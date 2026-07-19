#include "Es4allUpdate.h"

#ifdef ES4ALL_SELF_UPDATE

#include "EmulationStation.h"   // PROGRAM_VERSION_STRING, ES4ALL_BUILD_SHA
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

	std::string getInstalledSha()
	{
		return ES4ALL_BUILD_SHA;   // 本地手动编译时为空字串
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
			std::string url;
			if (rel.HasMember("assets") && rel["assets"].IsArray())
			{
				for (auto& a : rel["assets"].GetArray())
				{
					if (a.IsObject() && a.HasMember("name") && a["name"].IsString() &&
					    asset == a["name"].GetString() &&
					    a.HasMember("browser_download_url") && a["browser_download_url"].IsString())
					{
						url = a["browser_download_url"].GetString();
						break;
					}
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
		// ES 实际读取的 resources = ~/.emulationstation/resources, 运行期解析到可写目录, 直接覆盖。
		std::string userResDir = Paths::getUserEmulationStationPath() + "/resources";

		// 判断这次是"首次安装"还是"滚动更新"(唯读平台专用, 决定重启 ES 够不够用还是要整机重开机)。
		// mount --bind 绑定的是当时的 inode; 若 realBin 此前已是 bind-mount 目标, 说明是滚动更新
		// ——稍后 mv 新文件进 /storage 只换 inode, 旧挂载仍指旧档, 只重启 ES 进程看不到新版本。
#if defined(ES4ALL_TARGET_ROCKNIX) || defined(ES4ALL_TARGET_EMUELEC)
		{
			std::string chk = "grep -q ' " + realBin + " ' /proc/mounts";
			gNeedsFullReboot = (system(chk.c_str()) == 0);
		}
#else
		gNeedsFullReboot = false;
#endif

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

		// 3) resources -> 可写 user 目录(三平台共用, 原子替换)。
		report(_("INSTALLING"), -1);
		{
			std::string sh;
			sh += "if [ -d '" + extractDir + "/resources' ]; then ";
			sh += "rm -rf '" + userResDir + ".new'; cp -a '" + extractDir + "/resources' '" + userResDir + ".new'; ";
			sh += "rm -rf '" + userResDir + "'; mv '" + userResDir + ".new' '" + userResDir + "'; fi";
			system(sh.c_str());
		}

		// 4) binary 落地(按平台)。
#if defined(ES4ALL_TARGET_ARMBIAN)
		// 可写 rootfs: 就地 rename 覆盖。rename 可替换正在运行的可执行文件(避开 ETXTBSY),
		// 旧 inode 供当前进程续用, 重启后 exec 到新档。
		{
			std::string sh;
			sh += "set -e; ";
			sh += "cp -f '" + newBin + "' '" + realBin + ".new'; chmod 0755 '" + realBin + ".new'; ";
			sh += "mv -f '" + realBin + ".new' '" + realBin + "'";
			if (system(sh.c_str()) != 0)
				return std::make_pair(std::string("覆盖程序失败"), 1);
		}
#else
		// 只读 rootfs(ROCKNIX/EMUELEC): 新 binary 写 /storage(原子替换, 即使正被 bind-mount 也安全)。
		std::string stBin = "/storage/es4all-emulationstation";
		{
			std::string sh;
			sh += "set -e; ";
			sh += "cp -f '" + newBin + "' '" + stBin + ".new'; chmod 0755 '" + stBin + ".new'; ";
			sh += "mv -f '" + stBin + ".new' '" + stBin + "'";
			if (system(sh.c_str()) != 0)
				return std::make_pair(std::string("写入 /storage 失败"), 1);
		}

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
		hook << "BIN=" << stBin << "\n";
		hook << "grep -q ' " << realBin << " ' /proc/mounts || mount --bind \"$BIN\" '" << realBin << "'\n";
		hook << "exit 0\n";
		hook.close();
		chmod(hookPath.c_str(), 0755);
#else // EMUELEC
		// 持久化: systemd oneshot 服务, Before=emustation.service, 时序由核心保证。
		// EmuELEC 的 systemctl enable 会把 .wants 链接写进可写的 /storage/.config/system.d/,
		// 不碰只读 /etc。(不用 custom_start.sh: emuelec_autostart.sh 里是 `custom_start.sh before &`
		// 背景执行, 与 ES 竞态不可靠。)
		Utils::FileSystem::createDirectory("/storage/.config/system.d");
		std::string unitPath = "/storage/.config/system.d/es4all-selfmount.service";
		std::ofstream unit(unitPath);
		if (!unit.good())
			return std::make_pair(std::string("无法写入 systemd 服务"), 1);
		unit << "[Unit]\n";
		unit << "Description=es4all: overlay updated ES binary before EmulationStation\n";
		unit << "ConditionPathExists=" << stBin << "\n";
		unit << "Before=emustation.service\n";
		unit << "RequiresMountsFor=/storage\n\n";
		unit << "[Service]\n";
		unit << "Type=oneshot\n";
		unit << "RemainAfterExit=yes\n";
		unit << "ExecStart=/bin/sh -c 'grep -q \" " << realBin << " \" /proc/mounts || mount --bind " << stBin << " " << realBin << "'\n\n";
		unit << "[Install]\n";
		unit << "WantedBy=emustation.service\n";
		unit.close();
		system("systemctl daemon-reload; systemctl enable es4all-selfmount.service");
#endif

		// 立即 bind-mount 一次: "重启 ES"(quitES RESTART, 只 re-exec 不重开机)也能马上用上新 binary;
		// 持久化(钩子/服务)负责整机重开机后。ES 以 root 运行, 可挂载。
		std::string mnt = "grep -q ' " + realBin + " ' /proc/mounts || mount --bind '" + stBin + "' '" + realBin + "'";
		system(mnt.c_str());
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
