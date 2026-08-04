#include "Es4allProfiles.h"

#ifdef ES4ALL_SELF_UPDATE

#include "EmulationStation.h"   // PROGRAM_VERSION_STRING
#include "Es4allUpdate.h"       // compareVersion(共用版本序, 别再写第二套)
#include "SystemConf.h"
#include "HttpReq.h"
#include "Paths.h"
#include "Log.h"
#include "utils/md5.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "utils/Platform.h"
#include "LocaleES.h"

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
	// ★写死, 不开放使用者自订★(见头档「安全性」)。用 raw. 而非 API: 免速率限制、免 token。
	const std::string kRepoRaw =
		"https://raw.githubusercontent.com/w2xg2022/es4all-profiles/main/";

	const std::string kManifestUrl = kRepoRaw + "manifest.json";

	// 本 ES 认得的 manifest 格式版本。仓库日后若改格式会 bump schema, 旧 ES 认不得就整包跳过,
	// 而不是拿新格式乱套一通。
	const int kSupportedSchema = 1;

	// 本 target 在仓库里的 scope 目录名。
	// ★仓库第一层用发行版全名, 不用 A/E/R 缩写★(2026-08-02 改):
	// 缩写省不了几个字, 却让人每次都要回想 A 是 Armbian 还是别的;
	// 目录名是给人读的, 不是给程式省字元的。
	std::string targetDir()
	{
#if defined(ES4ALL_TARGET_ROCKNIX)
		return "rocknix";
#elif defined(ES4ALL_TARGET_EMUELEC)
		return "emuelec";
#else
		return "armbian";
#endif
	}

	// 本机的 DEVICE(晶片家族)候选值。★收集成【集合】而不是取单一来源★:
	// 同一台机器的 DEVICE 在不同地方可能写得不一样 —— MD1000 的云编译 workflow 用
	// DEVICE=RK3566, 但本地曾用 DEVICE=MD1000 编过, 於是 /ee_arch 里躺的是 MD1000。
	// 只认一个来源就会「明明是这台却不命中」, 而且是静默的。任一来源对上就算命中。
	//   EmuELEC/CoreELEC: /etc/os-release 的 COREELEC_DEVICE、以及 /ee_arch
	//   ROCKNIX/LibreELEC 系: os-release 的 HW_DEVICE / DISTRO_DEVICE /
	//                          ROCKNIX_DEVICE / LIBREELEC_DEVICE
	//   Armbian: /etc/armbian-release 的 BOARDFAMILY
	//
	// ★HW_DEVICE / DISTRO_DEVICE 是 2026-08-04 实机 MD1000/ROCKNIX 补的★
	//   那台的 /etc/os-release 里【没有】ROCKNIX_DEVICE, 只有
	//       HW_DEVICE="RK3566"   DISTRO_DEVICE="RK3566"
	//   於是 deviceKeys() 回空集合, log 印的是 `DEVICE=[]` —— 机型层
	//   (rocknix/RK3566/MD1000/…)的档案一个都不会落地, 而 common 与 _common 照样成功,
	//   所以看起来「同步成功了」却独缺机型专属的东西。连带 emmc-layout.conf 也到不了,
	//   而 ES 正是拿它当「写入 eMMC」选单的显示条件 -> 那个选单永远不出现。
	//   ⚠️ 多认几个键名不会误命中: 这些值本来就是彼此不同的晶片家族名。
	std::vector<std::string> deviceKeys()
	{
		// ★每个来源【各呼叫一次】, 不要串成一条多行命令★(实机踩过 2026-08-03)
		//   getShOutput 会把**每一行的换行剥掉再串接**(那是为了修 /proc/device-tree/model
		//   没有尾随换行、旧写法会吃掉一个真实字元的坑, 见 Platform.cpp 的註解)。
		//   於是多行输出会黏成一坨 —— 两个来源都回 RK3566 时拿到的是 "RK3566RK3566",
		//   split('\n') 分不出来, 比对当然不命中。而且**完全静默**: 机型层的档一个都没套用,
		//   log 看起来还很正常(只有那个黏在一起的字串是唯一线索)。
		static const char* kCmds[] = {
			"sed -n 's/^\\(COREELEC_DEVICE\\|ROCKNIX_DEVICE\\|LIBREELEC_DEVICE\\|HW_DEVICE\\|DISTRO_DEVICE\\)=\"\\?\\([^\"]*\\)\"\\?$/\\2/p' /etc/os-release 2>/dev/null | head -1",
			"cat /ee_arch 2>/dev/null | head -1",
			"sed -n 's/^BOARDFAMILY=\"\\?\\([^\"]*\\)\"\\?$/\\1/p' /etc/armbian-release 2>/dev/null | head -1",
		};

		std::vector<std::string> out;
		for (auto cmd : kCmds)
		{
			std::string v = Utils::String::trim(Utils::Platform::getShOutput(cmd));
			if (v.empty())
				continue;
			if (std::find(out.cbegin(), out.cend(), v) == out.cend())
				out.push_back(v);
		}
		return out;
	}

	// 可写暂存基地。与 Es4allUpdate 同一套判断: 唯读平台用 /storage, ARMBIAN 用可写的 home。
	std::string storeBase()
	{
#if defined(ES4ALL_TARGET_ARMBIAN)
		return Paths::getUserEmulationStationPath();
#else
		return "/storage";
#endif
	}

	// 机型专属【脚本】的落点。与 storage-config 分开: 这里的档一律补执行位, 混在一起
	// 会误伤一般设定档。三个 target 都可写, 且消费端(发行版脚本)按固定路径找得到。
	std::string binDir()
	{
#if defined(ES4ALL_TARGET_ARMBIAN)
		return Paths::getHomePath() + "/.config/es4all/bin";
#else
		return "/storage/.config/es4all/bin";
#endif
	}

	// dest-root token -> 本机实际落点。★新增 token 要同时改这里和仓库 README 的对照表★,
	// 只加目录不加解析, 档案会被当成「不认得的落点」整批拒绝(见 resolveDest)。
	std::string resolveDestRoot(const std::string& token)
	{
		if (token == "es-resources")
			return Paths::getUserEmulationStationPath() + "/resources";

		if (token == "storage-config")
		{
#if defined(ES4ALL_TARGET_ARMBIAN)
			return Paths::getHomePath() + "/.config";
#else
			return "/storage/.config";
#endif
		}

		if (token == "bin")
			return binDir();

		// 使用者资料根本身(E/R 是 /storage, A 是 home)。
		// ★为什么需要它★: 不是所有落点都在 .config 底下 —— ROCKNIX 的 RA autoconfig
		// 出厂档要落 /storage/joypads(那是 /tmp/joypads 这个 overlay 的 upper, 写入即持久)。
		// 硬塞进 storage-config/ 会变成 /storage/.config/joypads, RA 根本不看那里, 静默失效。
		if (token == "storage")
			return storeBase();

		return "";   // 不认得
	}

	// 落在 bin/ 的档要补上执行位。★必须做★: Utils::FileSystem::copyFile 是纯位元组复制、
	// 不带模式位, 脚本落地会是 -rw-, 消费端的 `[ -x ]` 判断不成立 -> 静默走回内建实作,
	// 表现是「下发成功但完全没效果」且无任何错误讯息。
	bool needsExecBit(const std::string& destRootToken)
	{
		return destRootToken == "bin";
	}

	// 机型字串。与 ApiSystem::parseAudioOutputs 同一来源, 保持机型键写法一致。
	std::string deviceModel()
	{
		return Utils::String::trim(Utils::Platform::getShOutput(
			"cat /proc/device-tree/model 2>/dev/null | tr -d '\\000'"));
	}

	// 仓库路径 -> (scope 优先级, 落点绝对路径)。不适用本机则回 false。
	//
	// 目录结构(2026-08-02 定案, 与建置系统的 DEVICE/SUBDEVICE 同名同义):
	//   common/<落点>/…                              rank 0  三个发行版共用(目前空著)
	//   <target>/_common/<落点>/…                    rank 1  该发行版全机型
	//   <target>/<DEVICE>/_common/<落点>/…           rank 2  该晶片家族全机型
	//   <target>/<DEVICE>/<SUBDEVICE>/<落点>/…       rank 3  单一机型
	// 数字大的后套用 => 覆盖前者。
	//
	// ★DEVICE 与 SUBDEVICE 的比对规则【不同】, 是刻意的★:
	//   DEVICE   = 建置变数, 值本来就是精确字串(RK3566 / Amlogic-no) -> 全等比对
	//   SUBDEVICE= 拿 /proc/device-tree/model 比, 那是一长串描述 -> 子串比对
	// 反过来做都会错: DEVICE 用子串会让 "RK3566" 命中 "RK356x";
	// SUBDEVICE 用全等则永远对不上(model 从来不会刚好等於机型键)。
	//
	// ★DEVICE 的全等比对要【忽略大小写】★(2026-08-03 实机 Armbian/MD1000 查证):
	//   仓库目录名沿用建置变数的写法 RK3566(大写), 但 Armbian 的来源是
	//   /etc/armbian-release 的 BOARDFAMILY, 那里写的是**小写 rk3566** ——
	//   纯全等的话 A 版的机型层【一个档都不会命中】, 而且完全静默:
	//   同步显示成功、log 也正常, 只有机型专属的设定神秘地没出现。
	//   放宽成忽略大小写不会引入误命中: 这些值本来就是彼此不同的晶片家族名。
	bool resolveDest(const std::string& repoPath, const std::string& model,
	                 const std::vector<std::string>& devKeys,
	                 int& outRank, std::string& outAbs, std::string& outRootToken)
	{
		auto isDevice = [&](const std::string& s)
		{
			const std::string lower = Utils::String::toLower(s);
			for (auto it = devKeys.cbegin(); it != devKeys.cend(); it++)
				if (Utils::String::toLower(*it) == lower)
					return true;
			return false;
		};
		auto isSubDevice = [&](const std::string& s)
		{
			return !model.empty() && model.find(s) != std::string::npos;
		};

		std::vector<std::string> parts = Utils::String::split(repoPath, '/', true);
		if (parts.size() < 3)          // 至少 <scope>/<dest-root>/<档名>
			return false;

		const std::string& scope = parts[0];
		size_t rootIdx;

		if (scope == "common")
		{
			outRank = 0;
			rootIdx = 1;
		}
		else if (scope == targetDir())
		{
			if (parts.size() < 4)
				return false;

			const std::string& lvl2 = parts[1];
			if (lvl2 == "_common")
			{
				outRank = 1;
				rootIdx = 2;
			}
			else if (isDevice(lvl2))
			{
				// <target>/<DEVICE>/_common/… 或 <target>/<DEVICE>/<SUBDEVICE>/…
				if (parts.size() < 5)
					return false;
				const std::string& lvl3 = parts[2];
				if (lvl3 == "_common")
					outRank = 2;
				else if (isSubDevice(lvl3))
					outRank = 3;
				else
					return false;     // 同家族的别台机器
				rootIdx = 3;
			}
			else
				return false;         // 别的晶片家族, 不关本机的事
		}
		else
			return false;             // 别的 target

		outRootToken = parts[rootIdx];
		std::string root = resolveDestRoot(outRootToken);
		if (root.empty())
			return false;             // 不认得的落点 token

		std::string rel;
		for (size_t i = rootIdx + 1; i < parts.size(); i++)
			rel += (rel.empty() ? "" : "/") + parts[i];
		if (rel.empty())
			return false;

		outAbs = root + "/" + rel;
		return true;
	}

	// 仓库路径 -> 可直接用的 URL 路径。★逐段编码★:
	// HttpReq::urlEncode 会把 '/' 也编成 %2F, 整条丢进去会变成一个巨大的档名而不是路径。
	//
	// ★为什么非编不可★: 档名真的会有空格 —— RetroArch 的 autoconfig 按手柄名命名,
	// 而手柄名本来就带空格("Microsoft X-Box 360 pad.cfg")。没编码的话 GitHub raw
	// 直接回 404, 表现是「这一档下载失败 -> 整批放弃」, 而其他档看起来都好好的。
	std::string urlPath(const std::string& repoPath)
	{
		std::string out;
		for (auto& seg : Utils::String::split(repoPath, '/', true))
		{
			if (seg.empty())
				continue;
			if (!out.empty())
				out += "/";
			out += HttpReq::urlEncode(seg);
		}
		return out;
	}

	// 记「已套用的内容版本」的旁档。放可写使用者目录, 三个 target 都指得到。
	std::string versionStampPath()
	{
		return Paths::getUserEmulationStationPath() + "/es4all-profiles.version";
	}

	// ★校验用: 必须读【原始位元组】★。不能用 Utils::FileSystem::readAllText —— 它会
	// skipUtf8Bom(), 档案带 BOM 时算出的 md5 与仓库端 md5sum 的结果不同, 表现是明明没坏
	// 却每次都「校验失败, 整批放弃」, 而且只在带 BOM 的档上发生, 极难查。
	std::string readFileBytes(const std::string& p)
	{
		std::ifstream f(p, std::ios::binary);
		if (!f)
			return "";
		std::stringstream ss;
		ss << f.rdbuf();
		return ss.str();
	}

	std::string readFileTrimmed(const std::string& p)
	{
		if (!Utils::FileSystem::exists(p))
			return "";
		return Utils::String::trim(Utils::FileSystem::readAllText(p));
	}

	// 递归建立目录(Utils 的 createDirectory 不建中间层)。
	void createDirectoryTree(const std::string& dir)
	{
		if (dir.empty() || Utils::FileSystem::exists(dir))
			return;
		createDirectoryTree(Utils::FileSystem::getParent(dir));
		Utils::FileSystem::createDirectory(dir);
	}

	struct PlannedFile
	{
		std::string repoPath;   // 仓库内路径
		std::string md5;        // manifest 宣告的 md5
		std::string dest;       // 本机落点绝对路径
		std::string rootToken;  // 落点根 token(决定要不要补执行位)
		int rank = 0;           // scope 优先级
		std::string tmp;        // 下载暂存位置
	};
}

namespace Es4allProfiles
{
	void ensureDefaults()
	{
		// ★只在键【不存在】时种预设★: 使用者手动关掉后值是 "0", 不能被当成「没设定过」又打开,
		// 否则每次开机都会把使用者的选择改回来。
		if (SystemConf::getInstance()->get("system.profiles.enabled").empty())
		{
			SystemConf::getInstance()->setBool("system.profiles.enabled", true);
			SystemConf::getInstance()->saveSystemConf();
		}
	}

	bool isEnabled()
	{
		return SystemConf::getInstance()->getBool("system.profiles.enabled", true);
	}

	std::string installedVersion()
	{
		return readFileTrimmed(versionStampPath());
	}

	bool hasFile(const std::string& destRelPath)
	{
		const size_t slash = destRelPath.find('/');
		if (slash == std::string::npos)
			return false;

		const std::string root = resolveDestRoot(destRelPath.substr(0, slash));
		if (root.empty())
			return false;

		return Utils::FileSystem::exists(root + "/" + destRelPath.substr(slash + 1));
	}

	std::string scriptPath(const std::string& name)
	{
		const std::string p = binDir() + "/" + name;
		// ★同时验存在与可执行★: 只查 exists 会在执行位没补上时回报「有这个功能」,
		// 然后选单出现、点下去什么都没发生 —— 比不显示更糟。
		if (access(p.c_str(), X_OK) == 0)
			return p;
		return "";
	}

	bool sync(std::string* outMsg)
	{
		auto fail = [&](const std::string& m) -> bool
		{
			LOG(LogWarning) << "Es4allProfiles: " << m;
			if (outMsg) *outMsg = m;
			return false;
		};

		if (!isEnabled())
			return fail("机型专属配置下载已关闭");

		// 1) 取 manifest。
		HttpReq req(kManifestUrl);
		if (!req.wait() || req.status() != HttpReq::REQ_SUCCESS)
			return fail(std::string("取 manifest 失败: ") + req.getErrorMsg());

		rapidjson::Document doc;
		doc.Parse(req.getContent().c_str());
		if (doc.HasParseError() || !doc.IsObject())
			return fail("manifest 解析失败");

		if (!doc.HasMember("schema") || !doc["schema"].IsInt() || doc["schema"].GetInt() > kSupportedSchema)
			return fail("manifest 格式版本高于本 ES 所能处理, 已跳过");

		if (!doc.HasMember("version") || !doc["version"].IsString())
			return fail("manifest 缺 version");
		const std::string remoteVer = Utils::String::trim(doc["version"].GetString());

		// 2) min_es4all_version: 旧 ES 拿到不认得的内容会做出错误行为, 宁可不套。
		if (doc.HasMember("min_es4all_version") && doc["min_es4all_version"].IsString())
		{
			const std::string need = doc["min_es4all_version"].GetString();
			if (Es4allUpdate::compareVersion(PROGRAM_VERSION_STRING, need) < 0)
				return fail("本机 ES 版本低于配置要求的 " + need + ", 已跳过");
		}

		// 3) 内容版本没变就结束(设备端唯一的省流量判断, 故仓库端每次改动都必须 bump version)。
		if (!remoteVer.empty() && remoteVer == installedVersion())
		{
			if (outMsg) *outMsg = _("NO UPDATE AVAILABLE");
			return false;
		}

		if (!doc.HasMember("files") || !doc["files"].IsArray())
			return fail("manifest 缺 files");

		// 4) 挑出适用本机的档, 按 scope 优先级排序(小的先套, 大的后套 => 覆盖)。
		const std::string model = deviceModel();
		const std::vector<std::string> devKeys = deviceKeys();
		LOG(LogInfo) << "Es4allProfiles: target=" << targetDir()
		             << " DEVICE=[" << Utils::String::join(devKeys, ",") << "]"
		             << " model=\"" << model << "\"";
		std::vector<PlannedFile> plan;

		for (auto& f : doc["files"].GetArray())
		{
			if (!f.IsObject() || !f.HasMember("path") || !f["path"].IsString())
				continue;
			if (!f.HasMember("md5") || !f["md5"].IsString())
				continue;

			PlannedFile pf;
			pf.repoPath = f["path"].GetString();
			pf.md5 = Utils::String::toLower(Utils::String::trim(f["md5"].GetString()));

			if (!resolveDest(pf.repoPath, model, devKeys, pf.rank, pf.dest, pf.rootToken))
				continue;   // 不适用本机(别的 target / 别的机型 / 不认得的落点)

			plan.push_back(pf);
		}

		if (plan.empty())
		{
			// 不是错误: 本机型可能就是没有专属配置。仍记下版本, 免得每次开机都白抓一轮。
			Utils::FileSystem::writeAllText(versionStampPath(), remoteVer);
			if (outMsg) *outMsg = _("NO UPDATE AVAILABLE");
			return false;
		}

		std::stable_sort(plan.begin(), plan.end(),
			[](const PlannedFile& a, const PlannedFile& b) { return a.rank < b.rank; });

		// 5) 全部下载到暂存区并逐档校验 md5。★全有或全无★: 任一档不符就整批放弃 ——
		//    半套的配置(例如音源表更新了、消费脚本没更新)比整批旧的更糟。
		const std::string tmpDir = storeBase() + "/.es4all-profiles.tmp";
		Utils::FileSystem::deleteDirectoryFiles(tmpDir, true);
		createDirectoryTree(tmpDir);

		int idx = 0;
		for (auto& pf : plan)
		{
			pf.tmp = tmpDir + "/" + std::to_string(idx++);

			HttpReq dl(kRepoRaw + urlPath(pf.repoPath), pf.tmp);
			if (!dl.wait() || dl.status() != HttpReq::REQ_SUCCESS)
			{
				Utils::FileSystem::deleteDirectoryFiles(tmpDir, true);
				return fail("下载失败: " + pf.repoPath + " (" + dl.getErrorMsg() + ")");
			}

			const std::string got = Utils::String::toLower(md5(readFileBytes(pf.tmp)));
			if (got != pf.md5)
			{
				Utils::FileSystem::deleteDirectoryFiles(tmpDir, true);
				return fail("校验失败(整批放弃): " + pf.repoPath);
			}
		}

		// 6) 备份现有落点, 再套用。备份保留上一版, 出事可手动还原。
		const std::string bakDir = storeBase() + "/.profiles.bak";
		Utils::FileSystem::deleteDirectoryFiles(bakDir, true);
		createDirectoryTree(bakDir);

		int applied = 0;
		for (const auto& pf : plan)
		{
			if (Utils::FileSystem::exists(pf.dest))
			{
				// 备份路径沿用仓库内路径结构, 一眼看得出哪个档来自哪个 scope。
				const std::string bak = bakDir + "/" + pf.repoPath;
				createDirectoryTree(Utils::FileSystem::getParent(bak));
				Utils::FileSystem::copyFile(pf.dest, bak);
			}

			createDirectoryTree(Utils::FileSystem::getParent(pf.dest));
			if (!Utils::FileSystem::copyFile(pf.tmp, pf.dest))
			{
				Utils::FileSystem::deleteDirectoryFiles(tmpDir, true);
				return fail("写入失败: " + pf.dest + " (备份在 " + bakDir + ")");
			}

			if (needsExecBit(pf.rootToken))
				chmod(pf.dest.c_str(), 0755);

			applied++;
		}

		Utils::FileSystem::deleteDirectoryFiles(tmpDir, true);
		Utils::FileSystem::writeAllText(versionStampPath(), remoteVer);

		LOG(LogInfo) << "Es4allProfiles: 已套用 " << applied << " 个档, 版本 " << remoteVer;
		if (outMsg)
			*outMsg = _("DEVICE PROFILES UPDATED");
		return true;
	}
}

#else   // !ES4ALL_SELF_UPDATE

namespace Es4allProfiles
{
	void ensureDefaults() {}
	bool isEnabled() { return false; }
	std::string installedVersion() { return ""; }
	std::string scriptPath(const std::string&) { return ""; }
	bool hasFile(const std::string&) { return false; }
	bool sync(std::string*) { return false; }
}

#endif
