#include "Es4allUpdate.h"

#ifdef ES4ALL_SELF_UPDATE

#include "EmulationStation.h"   // PROGRAM_VERSION_STRING
#include "utils/md5.h"          // 构建指纹改用 binary 的 md5
#include "ApiSystem.h"          // unzipFile
#include "HttpReq.h"
#include "Paths.h"
#include "Log.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "LocaleES.h"

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <cctype>
#include <unistd.h>
#include <sys/stat.h>

namespace
{
	// es4all 发布仓库与本 target 对应的产物名。
	const std::string kReleasesApi =
		"https://api.github.com/repos/w2xg2022/es4all/releases?per_page=50";

	std::string targetAssetName()
	{
#if defined(ES4ALL_TARGET_ROCKNIX)
		return "emulationstation-rocknix-aarch64.zip";
#elif defined(ES4ALL_TARGET_EMUELEC)
		return "emulationstation-emuelec-aarch64.zip";
#else
		return "emulationstation-armbian-aarch64.zip";
#endif
	}

	// 解析版本号: (major, minor, isPre)。接受可选前导 v, 如 "v1.1pre" / "1.1" / "1.2pre"。
	struct ParsedVersion { int major = 0; int minor = 0; bool pre = false; };

	ParsedVersion parseVersion(const std::string& raw)
	{
		ParsedVersion pv;
		std::string s = Utils::String::toLower(Utils::String::trim(raw));
		if (!s.empty() && s[0] == 'v')
			s = s.substr(1);

		size_t i = 0;
		std::string num;
		while (i < s.size() && std::isdigit((unsigned char)s[i])) num += s[i++];
		pv.major = num.empty() ? 0 : atoi(num.c_str());

		if (i < s.size() && s[i] == '.') i++;

		num.clear();
		while (i < s.size() && std::isdigit((unsigned char)s[i])) num += s[i++];
		pv.minor = num.empty() ? 0 : atoi(num.c_str());

		// 剩余部分含 "pre" 即预览版。
		pv.pre = (s.find("pre", i) != std::string::npos);
		return pv;
	}

	// 排序键: (major, minor, releaseFlag)。pre 的 releaseFlag=0 排在同号正式版(1)之前。
	int rankCompare(const ParsedVersion& a, const ParsedVersion& b)
	{
		if (a.major != b.major) return a.major < b.major ? -1 : 1;
		if (a.minor != b.minor) return a.minor < b.minor ? -1 : 1;
		int ra = a.pre ? 0 : 1;
		int rb = b.pre ? 0 : 1;
		if (ra != rb) return ra < rb ? -1 : 1;
		return 0;
	}

	// 从 Release 说明里抓构建指纹标记 "es4all-build-sha: <sha>"。
	std::string parseShaFromBody(const std::string& body)
	{
		const std::string marker = "es4all-build-sha:";
		size_t p = body.find(marker);
		if (p == std::string::npos)
			return "";
		p += marker.size();
		std::string rest = body.substr(p);
		// 取到行尾, 去空白。
		size_t nl = rest.find_first_of("\r\n");
		if (nl != std::string::npos)
			rest = rest.substr(0, nl);
		return Utils::String::trim(rest);
	}
}

namespace
{
	bool gNeedsFullReboot = false;
}

namespace Es4allUpdate
{
	bool needsFullReboot()
	{
		return gNeedsFullReboot;
	}

	std::string getInstalledVersion()
	{
		return PROGRAM_VERSION_STRING;
	}

	// es4all: 构建指纹 = **本机 binary 的 md5**(不再是编译期烤进去的 commit SHA)。
	//
	// 为什么改：CI 一次编三个 target、共用同一个 commit SHA，所以任何一次 push 都会让三个
	// target 的指纹一起变 —— 即使改动只在 `#if defined(ES4ALL_TARGET_ROCKNIX)` 守卫内、
	// 对 EmuELEC 的产物毫无影响，EmuELEC 也会被提示「有新版」(实机遇到过)。
	// 改用产物内容的 md5 后，只有该 target 的 binary 真的变了才会提示。
	//
	// 前提是相同源码要编出相同 binary：已移除 __DATE__/__TIME__(见 EmulationStation.h)与
	// -DES4ALL_BUILD_SHA，并用两次云端建置验证过 md5 完全一致。
	//
	// 读 /proc/self/exe 而不是 argv[0]：ROCKNIX/EMUELEC 上 binary 是 bind-mount 盖上去的，
	// 且滚动更新后原档可能已被 unlink(readlink 会带 " (deleted)" 后缀)——用 /proc/self/exe
	// 直接读「正在跑的这份内容」最准，不受路径与 deleted 影响。
	std::string getInstalledSha()
	{
		static std::string cached;
		static bool done = false;
		if (done)
			return cached;
		done = true;

		std::ifstream ifs("/proc/self/exe", std::ios::binary);
		if (!ifs)
		{
			LOG(LogWarning) << "Es4allUpdate: 无法读取 /proc/self/exe, 构建指纹留空(退化为纯版本号比较)";
			return cached;
		}
		std::ostringstream oss;
		oss << ifs.rdbuf();
		cached = Utils::String::toLower(md5(oss.str()));
		LOG(LogDebug) << "Es4allUpdate: 本机构建指纹(md5) = " << cached;
		return cached;
	}

	int compareVersion(const std::string& a, const std::string& b)
	{
		return rankCompare(parseVersion(a), parseVersion(b));
	}

	bool isNewer(const std::string& installedVer, const std::string& installedSha,
	             const std::string& candVer, const std::string& candSha)
	{
		int rc = compareVersion(candVer, installedVer);
		if (rc > 0) return true;    // 候选版本号更高
		if (rc < 0) return false;   // 候选版本号更低

		// 版本号(含 pre 标记)相同: 只有预览版才做滚动升级(靠构建指纹区分同版本号的不同次构建)。
		ParsedVersion pv = parseVersion(candVer);
		if (pv.pre && !candSha.empty() && candSha != installedSha)
			return true;

		return false;
	}

	bool findLatestApplicable(Es4allRelease& out)
	{
		LOG(LogInfo) << "Es4allUpdate: 查询 releases " << kReleasesApi;

		HttpReq req(kReleasesApi);
		if (!req.wait())
			return false;
		if (req.status() != HttpReq::REQ_SUCCESS)
		{
			LOG(LogError) << "Es4allUpdate: releases 查询失败 " << req.getErrorMsg();
			return false;
		}

		rapidjson::Document doc;
		doc.Parse(req.getContent().c_str());
		if (doc.HasParseError() || !doc.IsArray())
		{
			LOG(LogError) << "Es4allUpdate: releases JSON 解析失败";
			return false;
		}

		const std::string asset = targetAssetName();
		Es4allRelease best;
		ParsedVersion bestVer;

		for (auto& rel : doc.GetArray())
		{
			if (!rel.IsObject() || !rel.HasMember("tag_name") || !rel["tag_name"].IsString())
				continue;

			// 找本 target 对应的 zip 资产。
			// es4all: 同时抓本 target 的 zip 与其 **.md5**(构建指纹, 见下方 getInstalledSha 说明)。
			const std::string md5Asset = asset + ".md5";
			std::string url, md5Url;
			if (rel.HasMember("assets") && rel["assets"].IsArray())
			{
				for (auto& a : rel["assets"].GetArray())
				{
					if (!a.IsObject() || !a.HasMember("name") || !a["name"].IsString() ||
					    !a.HasMember("browser_download_url") || !a["browser_download_url"].IsString())
						continue;
					const std::string n = a["name"].GetString();
					if (n == asset)
						url = a["browser_download_url"].GetString();
					else if (n == md5Asset)
						md5Url = a["browser_download_url"].GetString();
				}
			}
			if (url.empty())
				continue;   // 这个 release 没有本 target 的产物

			Es4allRelease cand;
			cand.tag = rel["tag_name"].GetString();
			cand.version = cand.tag;
			if (!cand.version.empty() && (cand.version[0] == 'v' || cand.version[0] == 'V'))
				cand.version = cand.version.substr(1);
			cand.assetUrl = url;
			cand.md5Url = md5Url;
			// 旧版 release 只有 body 里的 commit SHA(无 .md5 资产)时仍读它, 保持向后兼容;
			// 新版以 .md5 为准(稍后在选定 best 之后才下载, 避免对每个 release 都发一次请求)。
			if (rel.HasMember("body") && rel["body"].IsString())
				cand.sha = parseShaFromBody(rel["body"].GetString());
			cand.valid = true;

			// 取排序键最高的一个作为"最新"。
			ParsedVersion cv = parseVersion(cand.version);
			if (!best.valid || rankCompare(cv, bestVer) > 0)
			{
				best = cand;
				bestVer = cv;
			}
		}

		if (!best.valid)
			return false;

		// es4all: 取回本 target 的构建指纹(binary 的 md5)。只对选定的最新 release 发这一次请求。
		// .md5 档内容是 `md5sum` 格式: "<hex>  <档名>"，只取前段。
		if (!best.md5Url.empty())
		{
			HttpReq md5req(best.md5Url);
			if (md5req.wait() && md5req.status() == HttpReq::REQ_SUCCESS)
			{
				std::string body = Utils::String::trim(md5req.getContent());
				size_t sp = body.find_first_of(" \t\r\n");
				if (sp != std::string::npos)
					body = body.substr(0, sp);
				if (!body.empty())
					best.sha = Utils::String::toLower(body);
			}
			else
				LOG(LogWarning) << "Es4allUpdate: 取 .md5 失败, 退回用 release body 的指纹";
		}

		if (!isNewer(getInstalledVersion(), getInstalledSha(), best.version, best.sha))
		{
			LOG(LogInfo) << "Es4allUpdate: 已是最新 (" << getInstalledVersion() << ")";
			return false;
		}

		LOG(LogInfo) << "Es4allUpdate: 发现更新 " << best.version
		             << " sha=" << best.sha << " <= 当前 " << getInstalledVersion()
		             << " sha=" << getInstalledSha();
		out = best;
		return true;
	}

	// -------------------------------------------------------------------------
	// 套用更新。按平台走各自最可靠的持久化(实机核对 .179 ROCKNIX / .165 EmuELEC)：
	//   ARMBIAN — rootfs 是可写 ext4, binary 就地 rename 覆盖(避开 ETXTBSY), 无需 hook。
	//   ROCKNIX — rootfs 只读 squashfs, binary 写 /storage + autostart 钩子(pre-ES, 同步执行)
	//             开机 bind-mount 盖到 /usr/bin/emulationstation。
	//   EMUELEC — rootfs 只读 squashfs, binary 写 /storage + systemd oneshot 服务(Before=ES,
	//             时序由核心保证; 不用 custom_start.sh——它被 emuelec_autostart.sh 背景执行会与 ES 竞态)。
	// resources 三平台都在可写的 ~/.emulationstation/resources(运行期解析), 直接覆盖即可, 无需 hook。
	// 回退: ARMBIAN 无自动回退(就地覆盖); ROCKNIX 删 autostart 钩子; EMUELEC `systemctl disable`。
	// -------------------------------------------------------------------------
	std::pair<std::string, int> apply(const Es4allRelease& rel,
	                                  const std::function<void(const std::string)>& func)
	{
#if !defined(ES4ALL_TARGET_ARMBIAN) && !defined(ES4ALL_TARGET_ROCKNIX) && !defined(ES4ALL_TARGET_EMUELEC)
		return std::make_pair(std::string("当前平台暂不支持自我更新"), 1);
#else
		if (!rel.valid || rel.assetUrl.empty())
			return std::make_pair(std::string("无可用更新"), 1);

		auto report = [&](const std::string& t, int pct)
		{
			if (func)
			{
				if (pct >= 0) func(t + " >>> " + std::to_string(pct) + "%");
				else          func(t);
			}
		};

		// 运行中的 binary 真实路径(即安装位置, 唯读平台上是 bind-mount 目标路径)。
		char exeBuf[4096] = {0};
		ssize_t n = readlink("/proc/self/exe", exeBuf, sizeof(exeBuf) - 1);
		if (n <= 0)
			return std::make_pair(std::string("无法定位当前程序路径"), 1);
		std::string realBin(exeBuf, n);
		// 关键: readlink 在 exe 的 inode 已被替换(滚动更新时旧 /storage 文件被 mv 覆盖, 运行中的
		// 进程仍持有旧 inode)会返回带 " (deleted)" 后缀的路径。若直接使用, 会污染挂载目标路径,
		// 且未转义的括号会让生成的 shell/systemd 命令语法错误(实测 EMUELEC selfmount 服务因此在
		// 重启后失败 → 退回镜像自带 ES)。这里剥掉该后缀, 取回干净的安装路径。
		{
			const std::string delSuffix = " (deleted)";
			if (realBin.size() > delSuffix.size() &&
			    realBin.compare(realBin.size() - delSuffix.size(), delSuffix.size(), delSuffix) == 0)
				realBin = realBin.substr(0, realBin.size() - delSuffix.size());
		}
		// ES 实际读取的 resources/locale = ~/.emulationstation/{resources,locale}, 运行期解析到可写目录。
		std::string userResDir = Paths::getUserEmulationStationPath() + "/resources";

		// 可写暂存基地: 唯读平台用 /storage, ARMBIAN 用可写的 home。
#if defined(ES4ALL_TARGET_ARMBIAN)
		std::string storeBase = Paths::getUserEmulationStationPath();
#else
		std::string storeBase = "/storage";
#endif

		// 1) 下载 zip。
		std::string tmpDir = storeBase + "/.es4all-update";
		Utils::FileSystem::createDirectory(tmpDir);
		std::string zipPath = tmpDir + "/es4all-update.zip";
		Utils::FileSystem::removeFile(zipPath);

		report(_("DOWNLOADING"), 0);
		HttpReq dl(rel.assetUrl, zipPath);
		while (dl.status() == HttpReq::REQ_IN_PROGRESS)
		{
			report(_("DOWNLOADING"), dl.getPercent());
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
		}
		if (dl.status() != HttpReq::REQ_SUCCESS)
			return std::make_pair(std::string("下载失败: ") + dl.getErrorMsg(), 1);

		// 2) 解压。
		report(_("EXTRACTING"), -1);
		std::string extractDir = tmpDir + "/new";
		Utils::FileSystem::deleteDirectoryFiles(extractDir, true);
		Utils::FileSystem::createDirectory(extractDir);
		if (!ApiSystem::getInstance()->unzipFile(zipPath, extractDir))
			return std::make_pair(std::string("解压失败"), 1);

		std::string newBin = extractDir + "/emulationstation";
		if (!Utils::FileSystem::exists(newBin))
			return std::make_pair(std::string("更新包内未找到 emulationstation"), 1);

		// 3) 落地。⚠️关键: 绝不在运行中的 ES 上破坏性替换它正在读取的 live 目录(resources/locale)。
		//    否则 ES 下一次载入资源/翻译就找不到 -> 崩溃黑屏(实机 .165 点更新即挂的根因: 旧代码
		//    在此 rm -rf 了运行中的 resources 目录)。改为: 全部暂存到 /storage 独立位置, 由开机钩子/
		//    服务在 ES 启动前(ES 已停)才 bind-mount 盖上; 唯读平台因此强制整机重开机。
		report(_("INSTALLING"), -1);

		std::string userLocaleDir = Paths::getUserEmulationStationPath() + "/locale";

#if defined(ES4ALL_TARGET_ARMBIAN)
		// armbian(用户要求暂略, 未测): 可写 rootfs。binary 就地 rename(避开 ETXTBSY); resources/
		// locale 就地 cp 覆盖(不 rm live 目录, 仅覆盖文件, 降低运行中 ES 崩溃风险)。重启 ES 即可。
		{
			std::string sh;
			sh += "set -e; ";
			sh += "cp -f '" + newBin + "' '" + realBin + ".new'; chmod 0755 '" + realBin + ".new'; mv -f '" + realBin + ".new' '" + realBin + "'; ";
			sh += "[ -d '" + extractDir + "/resources' ] && cp -a '" + extractDir + "/resources/.' '" + userResDir + "/' 2>/dev/null; ";
			sh += "[ -d '" + extractDir + "/locale' ] && cp -a '" + extractDir + "/locale/.' '" + userLocaleDir + "/' 2>/dev/null; ";
			sh += "true";
			if (system(sh.c_str()) != 0)
				return std::make_pair(std::string("覆盖程序失败"), 1);
		}
		gNeedsFullReboot = false;
#else
		// ROCKNIX/EMUELEC: 唯读 rootfs。binary/resources/locale 全部暂存到 /storage 独立位置,
		// 绝不碰 live 目录。开机钩子/服务在 ES 启动前 bind-mount 盖上。
		std::string stBin = "/storage/es4all-emulationstation";
		std::string stRes = "/storage/es4all-resources";
		std::string stLoc = "/storage/es4all-locale";
		{
			std::string sh;
			sh += "set -e; ";
			sh += "cp -f '" + newBin + "' '" + stBin + ".new'; chmod 0755 '" + stBin + ".new'; mv -f '" + stBin + ".new' '" + stBin + "'; ";
			sh += "if [ -d '" + extractDir + "/resources' ]; then rm -rf '" + stRes + ".new'; cp -a '" + extractDir + "/resources' '" + stRes + ".new'; rm -rf '" + stRes + "'; mv '" + stRes + ".new' '" + stRes + "'; fi; ";
			sh += "if [ -d '" + extractDir + "/locale' ]; then rm -rf '" + stLoc + ".new'; cp -a '" + extractDir + "/locale' '" + stLoc + ".new'; rm -rf '" + stLoc + "'; mv '" + stLoc + ".new' '" + stLoc + "'; fi";
			if (system(sh.c_str()) != 0)
				return std::make_pair(std::string("写入 /storage 失败"), 1);
		}
		// 唯读平台的 resources/locale 替换只能在 ES 停止时(开机钩子里)做 -> 必须整机重开机。
		gNeedsFullReboot = true;

		// 三个 bind-mount(binary + resources + locale)命令。paths 无空格故不加引号(便于嵌入
		// systemd ExecStart 的单引号内); grep 模式用双引号。bind-mount 非破坏性, 原目录藏在挂载
		// 点下, 删除钩子/服务即回退。
		std::string mountCmds =
			"grep -q \" " + realBin + " \" /proc/mounts || mount --bind " + stBin + " " + realBin + "; "
			"[ -d " + stRes + " ] && { grep -q \" " + userResDir + " \" /proc/mounts || mount --bind " + stRes + " " + userResDir + "; }; "
			"[ -d " + stLoc + " ] && { grep -q \" " + userLocaleDir + " \" /proc/mounts || mount --bind " + stLoc + " " + userLocaleDir + "; }; "
			"true";

#if defined(ES4ALL_TARGET_ROCKNIX)
		// 持久化: autostart 钩子(ROCKNIX 的 /usr/bin/autostart 在 ES 前同步执行)。
		std::string hookDir = "/storage/.config/autostart";
		Utils::FileSystem::createDirectory("/storage/.config");
		Utils::FileSystem::createDirectory(hookDir);
		std::string hookPath = hookDir + "/003-es4all-selfupdate";
		std::ofstream hook(hookPath);
		if (!hook.good())
			return std::make_pair(std::string("无法写入 autostart 钩子"), 1);
		hook << "#!/bin/sh\n";
		hook << "# es4all 自我更新覆盖钩子(自动生成)。删除本文件即回退到镜像自带 ES。\n";
		hook << mountCmds << "\n";
		hook << "exit 0\n";
		hook.close();
		chmod(hookPath.c_str(), 0755);
#else // EMUELEC
		// 持久化: systemd oneshot 服务, Before=emustation.service, 时序由核心保证。
		// EmuELEC 的 systemctl enable 会把 .wants 链接写进可写的 /storage/.config/system.d/, 不碰只读 /etc。
		Utils::FileSystem::createDirectory("/storage/.config/system.d");
		std::string unitPath = "/storage/.config/system.d/es4all-selfmount.service";
		std::ofstream unit(unitPath);
		if (!unit.good())
			return std::make_pair(std::string("无法写入 systemd 服务"), 1);
		unit << "[Unit]\n";
		unit << "Description=es4all: overlay updated ES (binary/resources/locale) before EmulationStation\n";
		unit << "ConditionPathExists=" << stBin << "\n";
		unit << "Before=emustation.service\n";
		unit << "RequiresMountsFor=/storage\n\n";
		unit << "[Service]\n";
		unit << "Type=oneshot\n";
		unit << "RemainAfterExit=yes\n";
		unit << "ExecStart=/bin/sh -c '" << mountCmds << "'\n\n";
		unit << "[Install]\n";
		unit << "WantedBy=emustation.service\n";
		unit.close();
		system("systemctl daemon-reload; systemctl enable es4all-selfmount.service");
#endif
#endif

		// 清理下载临时区。
		Utils::FileSystem::deleteDirectoryFiles(tmpDir, true);

		LOG(LogInfo) << "Es4allUpdate: 更新已就绪, 重启后生效 -> " << rel.version;
		report(_("UPDATE IS READY"), -1);
		return std::make_pair(std::string(""), 0);
#endif
	}
}

#endif // ES4ALL_SELF_UPDATE
