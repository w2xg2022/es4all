#include "ApiSystem.h"
#include "Settings.h"
#include "Log.h"
#include "HttpReq.h"
#include "AudioManager.h"
#include "VolumeControl.h"
#include "InputManager.h"
#include "EmulationStation.h"
#include "SystemConf.h"
#include "Sound.h"
#include "utils/Platform.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "utils/ThreadPool.h"
#include "resources/ResourceManager.h"   // parseAudioOutputs 读 :/audio_outputs.cfg
#include "RetroAchievements.h"
#include "Es4allUpdate.h"
#include "utils/ZipFile.h"
#include "Paths.h"
#include "utils/VectorEx.h"
#include "LocaleES.h"

#include <stdlib.h>
#include <unistd.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <algorithm>
#include <fstream>
#include <SDL.h>
#include <pugixml/src/pugixml.hpp>
#include <rapidjson/rapidjson.h>
#include <rapidjson/pointer.h>

#if WIN32
#include <Windows.h>
#define popen _popen
#define pclose _pclose
#define WIFEXITED(x) x
#define WEXITSTATUS(x) x
#include "Win32ApiSystem.h"
#else
#include <sys/statvfs.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#endif

/*
#define script_config "batocera-config"; // canupdate, overscan enable, overscan disable, storage 'X', storage current, storage list, forgetBT, getRootPassword, lsoutputs
#define script_overclock "batocera-overclock"; // list, set X
#define script_upgrade "batocera-upgrade";
#define script_sync "batocera-sync"; // sync
#define script_install "batocera-install"; // listDisks, listArchs, install X Y
#define script_scraper "batocera-scraper";
#define script_kodi "batocera-kodi";
#define script_wifi "batocera-wifi"; // scanlist, list, enable X Y, disable
#define script_bluetooth "batocera-bluetooth"; // trust, list, remove 
#define script_resolution "batocera-resolution"; // listModes
#define script_sync "batocera-sync";   // list
#define script_info "batocera-info"; // --full
#define script_systems "batocera-systems"; // --filter
#define script_suport "batocera-support";
#define script_gameforce "batocera-gameforce"; // buttonColorLed X, powerLed X
#define script_audio "batocera-audio"; // list, list-profiles, get-profile, set-profile 'X', get, set 'X'
#define script_bezelproject "batocera-es-thebezelproject"; // list, install X, remove X
#define script_format "batocera-format"; // listDisks, listFstypes
#define script_store "batocera-store"; // list, update, refresh, clean-all, install "X", remove "X"
#define script_preupdategamelists "batocera-preupdate-gamelists-hook";
#define script_timezones "batocera-timezone"; // get, detect, set "X"
#define script_padsinfos "batocera-padsinfo";
#define script_swissknife "batocera-es-swissknife"; // --emukill"
*/

ApiSystem::ApiSystem() { }

ApiSystem* ApiSystem::instance = nullptr;
ApiSystem::LED_TYPE ApiSystem::mSystemLedType = ApiSystem::LED_TYPE_NONE;

ApiSystem *ApiSystem::getInstance() 
{
	if (ApiSystem::instance == nullptr)
	{
#if WIN32
		ApiSystem::instance = new Win32ApiSystem();
#else
		ApiSystem::instance = new ApiSystem();
#endif
		
		IExternalActivity::Instance = ApiSystem::instance;
	}

	return ApiSystem::instance;
}

unsigned long ApiSystem::getFreeSpaceGB(std::string mountpoint) 
{
	LOG(LogDebug) << "ApiSystem::getFreeSpaceGB";

	int free = 0;

#if !WIN32
	struct statvfs fiData;
	if ((statvfs(mountpoint.c_str(), &fiData)) >= 0)
		free = (fiData.f_bfree * fiData.f_bsize) / (1024 * 1024 * 1024);
#endif

	return free;
}

std::string ApiSystem::getFreeSpaceUserInfo()
{
#ifdef _ENABLEEMUELEC
	return getFreeSpaceInfo("/storage/roms");
#else
	return getFreeSpaceInfo(Paths::getRootPath());
#endif
}

std::string ApiSystem::getFreeSpaceSystemInfo()
{
#ifdef _ENABLEEMUELEC
  return getFreeSpaceInfo("/emuelec");
#else
	return getFreeSpaceInfo("/boot");
#endif
}

std::string ApiSystem::getFreeSpaceInfo(const std::string mountpoint)
{
	LOG(LogDebug) << "ApiSystem::getFreeSpaceInfo";

	std::ostringstream oss;

#if !WIN32
	struct statvfs fiData;
	if ((statvfs(mountpoint.c_str(), &fiData)) < 0)
		return "";
		
	unsigned long long total = (unsigned long long) fiData.f_blocks * (unsigned long long) (fiData.f_bsize);
	unsigned long long free = (unsigned long long) fiData.f_bfree * (unsigned long long) (fiData.f_bsize);
	unsigned long long used = total - free;
	unsigned long percent = 0;
	
	if (total != 0) 
	{  //for small SD card ;) with share < 1GB
		percent = used * 100 / total;
		oss << Utils::FileSystem::megaBytesToString(used / (1024L * 1024L)) << "/" << Utils::FileSystem::megaBytesToString(total / (1024L * 1024L)) << " (" << percent << "%)";
	}
	else
		oss << "N/A";	
#endif

	return oss.str();
}

bool ApiSystem::isFreeSpaceLimit() 
{
#ifdef _ENABLEEMUELEC
	return getFreeSpaceGB("/storage/.update") < 2;
#else
	return getFreeSpaceGB(Paths::getRootPath()) < 2;
#endif
}

std::string ApiSystem::getVersion(bool extra)
{
	LOG(LogDebug) << "ApiSystem::getVersion";
#ifdef ES4ALL_SELF_UPDATE
	// es4all: 版本号以编进 binary 的 PROGRAM_VERSION_STRING 为准(自我更新据此比对),
	// 不走 batocera-version / version.info。extra(改机检测)一律返回 none。
	if (extra)
		return "none";
	return Es4allUpdate::getInstalledVersion();
#endif
#ifdef _ENABLEEMUELEC
	std::ifstream ifs("/usr/config/EE_VERSION");
#else
	std::ifstream ifs("/usr/share/batocera/batocera.version");
#endif

	if (isScriptingSupported(VERSIONINFO)) 
	{
		std::string command = "batocera-version";
		if (extra) 
			command += " --extra";

		auto res = executeEnumerationScript(command);
		if (res.size() > 0 && !res[0].empty())
			return res[0];
	}

	if (extra)
		return "none";

	std::string localVersionFile = Paths::getVersionInfoPath();
	if (!Utils::FileSystem::exists(localVersionFile))
		localVersionFile = Paths::findEmulationStationFile("version.info");

	if (Utils::FileSystem::exists(localVersionFile))
	{
		std::string localVersion = Utils::FileSystem::readAllText(localVersionFile);
		localVersion = Utils::String::replace(Utils::String::replace(localVersion, "\r", ""), "\n", "");
		return localVersion;
	}

	return PROGRAM_VERSION_STRING;	
}

std::string ApiSystem::getApplicationName()
{
	std::string localVersionFile = Paths::findEmulationStationFile("about.info");
	if (Utils::FileSystem::exists(localVersionFile))
	{
		std::string aboutInfo = Utils::FileSystem::readAllText(localVersionFile);
		aboutInfo = Utils::String::replace(Utils::String::replace(aboutInfo, "\r", ""), "\n", "");

		auto ver = ApiSystem::getInstance()->getVersion();
		auto cut = aboutInfo.find(" V" + ver);

		if (cut == std::string::npos)
			cut = aboutInfo.find(" " + ver);

		if (cut == std::string::npos)
			cut = aboutInfo.find(ver);

		if (cut != std::string::npos)
			aboutInfo = aboutInfo.substr(0, cut);

		return aboutInfo;
	}

#if BATOCERA
	return "BATOCERA";
#else
	return "EMULATIONSTATION";
#endif
}

bool ApiSystem::setOverscan(bool enable) 
{
	return executeScript("batocera-config overscan " + std::string(enable ? "enable" : "disable"));
}

bool ApiSystem::setOverclock(std::string mode) 
{
#ifdef _ENABLEEMUELEC
	return true;
#endif
	if (mode.empty())
		return false;

	return executeScript("batocera-overclock set " + mode);
}

std::vector<std::string> ApiSystem::getAvailableCpuGovernors()
{
	// 读第 0 个 policy 的可用 governor(同一 SoC 各 policy 通常一致)。因设备/内核而异
	// (RK3566 有 ondemand/powersave/performance/schedutil, 某些 Amlogic 只有 ondemand/performance),
	// 故动态读取, 避免菜单列出内核不支持的项。
	std::vector<std::string> govs;
	std::string out = Utils::Platform::getShOutput(
		"cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors 2>/dev/null");
	for (auto& g : Utils::String::split(out, ' ', true))
	{
		std::string t = Utils::String::trim(g);
		if (!t.empty())
			govs.push_back(t);
	}
	return govs;
}

void ApiSystem::setCpuGovernor(const std::string& gov)
{
	// 空 = AUTO(不强制, 保持内核默认), 不动。否则写到所有 CPU 的 scaling_governor。
	// EMUELEC 无发行版消费脚本, 由 ES 直接套用(需 root, EMUELEC 的 ES 以 root 运行)。
	if (gov.empty())
		return;

	std::string sh = "for f in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do "
	                 "[ -w \"$f\" ] && echo '" + gov + "' > \"$f\" 2>/dev/null; done";
	Utils::Platform::ProcessStartInfo(sh).run();
}

// es4all: 解析 resources/audio_outputs.cfg -> [(显示标签, "card,device")]，EMUELEC / ROCKNIX 共用。
// 格式与匹配规则见该文件头；取第一个「型号子串命中 /proc/device-tree/model」的行。
// 原本是 GuiMenu.cpp 的 file-static，移来这里是为了让开机还原(main.cpp)也用得到同一份逻辑。
std::vector<std::pair<std::string, std::string>> ApiSystem::parseAudioOutputs()
{
	std::vector<std::pair<std::string, std::string>> outs;

	std::string model = Utils::String::trim(Utils::Platform::getShOutput(
		"cat /proc/device-tree/model 2>/dev/null | tr -d '\\000'"));

	std::string cfgPath = ResourceManager::getInstance()->getResourcePath(":/audio_outputs.cfg");
	if (model.empty() || !Utils::FileSystem::exists(cfgPath))
		return outs;

	for (const std::string& raw : Utils::String::split(Utils::FileSystem::readAllText(cfgPath), '\n', true))
	{
		std::string line = Utils::String::trim(raw);
		if (line.empty() || line[0] == '#')
			continue;
		auto toks = Utils::String::splitAny(line, " \t", true);
		if (toks.empty() || model.find(toks[0]) == std::string::npos)
			continue;
		for (size_t i = 1; i < toks.size(); i++)
		{
			size_t c = toks[i].find(':');   // 每项 label:card,device
			if (c != std::string::npos)
				outs.push_back({ toks[i].substr(0, c), toks[i].substr(c + 1) });
		}
		break;
	}
	return outs;
}

#if defined(ES4ALL_TARGET_EMUELEC)
void ApiSystem::applyEmuelecAudioOutput(const std::string& dev)
{
	if (dev.empty() || dev == "auto")
		return;

	// 1) 应用层：改 asound.conf 的默认 PCM。emuelec-utils setauddev 【只】做这一件事
	//    (实作就一行 sed: `pcm "hw:..."` -> `pcm "hw:<dev>"`)。
	Utils::Platform::ProcessStartInfo("/usr/bin/emuelec-utils setauddev " + dev).run();

	// 2) 硬件层：开/关 HDMI 那条输出路径。
	//    ★实机 .165 定位★：只做第 1 步不够 —— 切到 AV 之后【HDMI 仍在同时出声】，因为
	//    ALSA 控制项 `Audio hdmi-out mute` 一直是 off、HDMI 输出路径从头到尾没人关过。
	//    (EmuELEC 原本预设就走 HDMI、没人切 AV，所以这个洞一直没暴露；是我们做出 AUDIO OUTPUT
	//     选单之后才浮现的。实测设 on 后 AV 独占、HDMI 静音；切回 HDMI 设 off 即恢复。)
	//    ★用【控制项名称】定址而非 numid★ —— numid 会随内核/机型变动，写死数字换台机器就错。
	//    非 HDMI 项(AV / 光纤等)一律静音 HDMI；找不到该机型的表(outs 空)时不动，避免误关。
	bool known = false, isHdmi = false;
	for (auto& o : parseAudioOutputs())
	{
		if (o.second == dev)
		{
			known = true;
			isHdmi = (Utils::String::toUpper(o.first) == "HDMI");
			break;
		}
	}
	if (!known)
		return;

	Utils::Platform::ProcessStartInfo(
		std::string("amixer -c 0 cset name='Audio hdmi-out mute' ") + (isHdmi ? "off" : "on")
		+ " >/dev/null 2>&1").run();
}

void ApiSystem::applyEmuelecVideoMode(const std::string& mode)
{
	// "Custom" 不是一个模式，而是「从 EE_VIDEO_MODE 档读自订字串」的逃生口，
	// 由 distro 的 check_res.sh 处理；这里不碰。
	if (mode.empty() || mode == "Custom")
		return;

	// ★实机 .165 定位：「视频模式设了没反应」的真因★
	//   设定其实一直存对、也读得到(get_ee_setting ee_videomode 回传正确值)，
	//   问题出在**套用被内核挡掉**：这颗内核把 /sys/class/display/mode 的写入锁住了，dmesg 明写
	//       warning, echo /sys/class/display/mode is disabled
	//       enable display/debug to set voutmode.
	//   必须先 `echo 1 > /sys/class/display/debug` 解锁才吃。
	//   而 EmuELEC 自己的 setres.sh **没做这一步** —— 它写完读回发现没变，就
	//   `[[ "${NEW_MODE}" != "${MODE}" ]] && exit 1` 直接放弃(实测手动跑 setres.sh 也是 exit=1)。
	//   开机链 emuelec_autostart.sh → check_res.sh → setres.sh 因此整条静默失效。
	//
	//   这里先开锁、再沿用 setres.sh(不自己写 mode，因为它除了写模式还会依分辨率调整 framebuffer)。
	//   ⚠️ 必须在建立视窗/renderer **之前**呼叫(check_res.sh 自己也注明
	//   "this has to be done before starting ES")，否则 ES 会以旧分辨率初始化。
	//   debug 开着不复位：留着 1 才不会有别的路径又被挡回去，且它本身只是解锁开关。
	//
	// ★另外两个实机踩到的坑，一并处理★
	//   ① setres.sh 偶尔首次失败：实测切换后读回还是旧模式，再跑一次才成功(驱动可能还在处理
	//      前一次切换)。故读回验证、最多重试 3 次 —— 这对**防呆的还原路径**尤其要紧：
	//      还原若默默失败，使用者就永远卡在看不到画面的模式里，防呆等于自废。
	//   ② **切完模式后 HDMI PHY 会停在 0**(= 输出直接被关掉)。这不是采集卡的问题，
	//      对真电视一样是黑屏 —— 先前多次「切了就再也没画面」有一部分就是它造成的。
	//      故结尾无条件补 `echo 1 > .../phy`(本来就是 1 时写入无害)。
	std::string sh =
		"for i in 1 2 3; do "
		  "echo 1 > /sys/class/display/debug 2>/dev/null; "
		  "/usr/bin/setres.sh " + mode + " >/dev/null 2>&1; "
		  "[ \"$(cat /sys/class/display/mode 2>/dev/null)\" = \"" + mode + "\" ] && break; "
		  "sleep 1; "
		"done; "
		"echo 1 > /sys/class/amhdmitx/amhdmitx0/phy 2>/dev/null; "
		"true";
	Utils::Platform::ProcessStartInfo(sh).run();
}
#endif

#if defined(ES4ALL_TARGET_ARMBIAN)
void ApiSystem::applyArmbianAudioOutput(const std::string& dev)
{
	// dev 形如 "card,device"(与 audio_outputs.cfg 一致)。
	size_t comma = dev.find(',');
	if (comma == std::string::npos)
		return;
	std::string card = Utils::String::trim(dev.substr(0, comma));
	std::string pcm  = Utils::String::trim(dev.substr(comma + 1));
	if (card.empty() || pcm.empty())
		return;

	// es4all: ARMBIAN 是**裸 ALSA**(实机 MD1000/Armbian 查证: PipeWire、PulseAudio 都没装)，
	// 所以切音源就是改 ALSA 的默认装置 —— 由 ES 直接写，不另外做 glue 脚本：
	//   ① 随 OTA 下发，不必重跑 es4all-1key 的安装脚本；
	//   ② 不必跨仓库协调(ROCKNIX 那边 glue 脚本漏装进韧体的教训还很新)。
	//
	// ★为什么写 ~/.asoundrc 而不是 /etc/asound.conf★:
	//   实机查证: es4all.service 写着 `User=game`(uid 1000)，而 /etc/asound.conf 是
	//   root:root 0644 —— ES 以 game 身份根本写不进去，ofstream 静默失败，
	//   选单看起来有反应、实际什么都没发生。game 也没有任何 sudo 权限(sudoers 查无此人)，
	//   要加 sudoers 就得回头改 es4all-1key(跨仓库，正是上面①②想避开的)。
	//   ALSA 的读取顺序是 /etc/asound.conf 之后才读 $HOME/.asoundrc，而这里三个节点
	//   都带 `!` 前缀(pcm.!default / pcm.!playback / ctl.!default)——
	//   `!` 是 ALSA 的「覆盖而非合并」运算符，能干净盖掉 1key 装的那份系统级配置。
	//   ES 与各模拟器都以 game 身份执行，这份 .asoundrc 对它们全体生效。
	//
	// ★为什么套 softvol 而不是直接 plughw★:
	//   本板 HDMI(card 0)**没有任何硬件混音器控件**(`amixer -c0 scontrols` 是空的)，
	//   ES 的 VolumeControl::isAvailable() 会回 false -> 整组「音量设置」菜单不显示。
	//   softvol 虚拟出一个名为 "PCM" 的控件挂到该卡上，音量选单才活得下来。
	//   这个结构沿用 es4all-1key 的 01-prep.sh 原本就部署的那份 asound.conf，只是把卡号参数化 ——
	//   两边形状一致，日后谁先写都不会互相打架。
	//   (AV/card1 的 RK809 本身有硬件 Master；softvol 叠在上面只是多一层衰减、不影响可用性，
	//    换来的是「不管切到哪张卡，ES 都有一个统一的音量控件」。)
	std::string conf =
		"# es4all: 由 ES 的 AUDIO OUTPUT 选单写入(ApiSystem::applyArmbianAudioOutput)。\n"
		"# 手动改这里会在下次切换音源时被覆盖。\n"
		"# 本档覆盖 /etc/asound.conf(由 es4all-1key 的 01-prep.sh 装的系统级预设)。\n"
		"pcm.!default {\n"
		"    type asym\n"
		"    playback.pcm \"playback\"\n"
		"    capture.pcm \"plughw:" + card + "\"\n"
		"}\n"
		"pcm.!playback {\n"
		"    type softvol\n"
		"    slave.pcm \"plughw:" + card + "," + pcm + "\"\n"
		"    control {\n"
		"        name \"PCM\"\n"
		"        card " + card + "\n"
		"    }\n"
		"}\n"
		"ctl.!default {\n"
		"    type hw\n"
		"    card " + card + "\n"
		"}\n";

	std::string rcPath = Paths::getHomePath() + "/.asoundrc";
	std::ofstream ofs(rcPath.c_str(), std::ios::trunc);
	if (!ofs)
	{
		LOG(LogError) << "es4all: 无法写入 " << rcPath << "(音源切换失败)";
		return;
	}
	ofs << conf;
	ofs.close();

	LOG(LogInfo) << "es4all: ARMBIAN audio output -> card " << card << ", device " << pcm;
}
#endif

// BusyComponent* ui
std::pair<std::string, int> ApiSystem::updateSystem(const std::function<void(const std::string)>& func)
{
	LOG(LogDebug) << "ApiSystem::updateSystem";

#ifdef ES4ALL_SELF_UPDATE
	// es4all: 走 GitHub Releases OTA, 替代不存在的 batocera-upgrade / emuelec-upgrade。
	{
		Es4allRelease rel;
		if (!Es4allUpdate::findLatestApplicable(rel))
			return std::make_pair(std::string("无可用更新"), 1);
		return Es4allUpdate::apply(rel, func);
	}
#endif

#ifdef _ENABLEEMUELEC
	std::string updatecommand = "emuelec-upgrade";
#else
	std::string updatecommand = "batocera-upgrade";
#endif

	FILE *pipe = popen(updatecommand.c_str(), "r");
	if (pipe == nullptr)
		return std::pair<std::string, int>(std::string("Cannot call update command"), -1);
	
	char line[1024] = "";
#ifdef _ENABLEEMUELEC
	FILE *flog = fopen(Utils::FileSystem::combine(Paths::getLogPath(), "emuelec-upgrade.log").c_str(), "w");
#else
	FILE *flog = fopen(Utils::FileSystem::combine(Paths::getLogPath(), "batocera-upgrade.log").c_str(), "w");
#endif
	while (fgets(line, 1024, pipe)) 
	{
	
		strtok(line, "\n");
		if (flog != nullptr) 
			fprintf(flog, "%s\n", line);

		if (func != nullptr)
			func(std::string(line));		
	}

	int exitCode = WEXITSTATUS(pclose(pipe));

	if (flog != NULL)
	{
		fprintf(flog, "Exit code : %d\n", exitCode);
		fclose(flog);
	}

	return std::pair<std::string, int>(std::string(line), exitCode);
}

std::pair<std::string, int> ApiSystem::backupSystem(BusyComponent* ui, std::string device) 
{
	LOG(LogDebug) << "ApiSystem::backupSystem";

	std::string updatecommand = "batocera-sync sync " + device;
	FILE* pipe = popen(updatecommand.c_str(), "r");
	if (pipe == NULL)
		return std::pair<std::string, int>(std::string("Cannot call sync command"), -1);

	char line[1024] = "";

	FILE* flog = fopen(Utils::FileSystem::combine(Paths::getLogPath(), "batocera-sync.log").c_str(), "w");
	while (fgets(line, 1024, pipe)) 
	{
		strtok(line, "\n");

		if (flog != NULL) 
			fprintf(flog, "%s\n", line);

		ui->setText(std::string(line));
	}

	if (flog != NULL) 
		fclose(flog);

	int exitCode = WEXITSTATUS(pclose(pipe));
	return std::pair<std::string, int>(std::string(line), exitCode);
}

std::pair<std::string, int> ApiSystem::installSystem(BusyComponent* ui, std::string device, std::string architecture) 
{
	LOG(LogDebug) << "ApiSystem::installSystem";

	std::string updatecommand = "batocera-install install " + device + " " + architecture;
	FILE *pipe = popen(updatecommand.c_str(), "r");
	if (pipe == NULL)
		return std::pair<std::string, int>(std::string("Cannot call install command"), -1);

	char line[1024] = "";

	FILE *flog = fopen(Utils::FileSystem::combine(Paths::getLogPath(), "batocera-install.log").c_str(), "w");
	while (fgets(line, 1024, pipe)) 
	{
		strtok(line, "\n");
		if (flog != NULL) fprintf(flog, "%s\n", line);
		ui->setText(std::string(line));
	}

	int exitCode = WEXITSTATUS(pclose(pipe));

	if (flog != NULL)
	{
		fprintf(flog, "Exit code : %d\n", exitCode);
		fclose(flog);
	}

	return std::pair<std::string, int>(std::string(line), exitCode);
}

std::pair<std::string, int> ApiSystem::scrape(BusyComponent* ui) 
{
	LOG(LogDebug) << "ApiSystem::scrape";

	FILE* pipe = popen("batocera-scraper", "r");
	if (pipe == nullptr)
		return std::pair<std::string, int>(std::string("Cannot call scrape command"), -1);

	char line[1024] = "";

#ifdef _ENABLEEMUELEC
	FILE* flog = fopen(Utils::FileSystem::combine(Paths::getLogPath(), "emuelec-scraper.log").c_str(), "w");
#else	
	FILE* flog = fopen(Utils::FileSystem::combine(Paths::getLogPath(), "batocera-scraper.log").c_str(), "w");
#endif
	while (fgets(line, 1024, pipe)) 
	{
		strtok(line, "\n");

		if (flog != NULL) 
			fprintf(flog, "%s\n", line);

		if (ui != nullptr && Utils::String::startsWith(line, "GAME: "))
			ui->setText(std::string(line));	
	}

	if (flog != nullptr)
		fclose(flog);

	int exitCode = WEXITSTATUS(pclose(pipe));
	return std::pair<std::string, int>(std::string(line), exitCode);
}

bool ApiSystem::ping() 
{
    // For mainland China (zh_CN), use China-reachable DNS servers
    if (Settings::getInstance()->getString("Language") == "zh_CN")
    {
        // AliDNS
        if (!executeScript("ping -c 1 -W 2 -t 255 223.5.5.5 > /dev/null 2>&1"))
        {
            // DNSPod
            return executeScript("ping -c 1 -W 2 -t 255 119.29.29.29 > /dev/null 2>&1");
        }

        return true;
    }

    // Google DNS
    if (!executeScript("ping -c 1 -W 2 -t 255 8.8.8.8 > /dev/null 2>&1"))
    {
        // Cloudflare DNS
        if (!executeScript("ping -c 1 -W 2 -t 255 1.1.1.1 > /dev/null 2>&1"))
        {
            // Quad9 DNS
            return executeScript("ping -c 1 -W 2 -t 255 9.9.9.9 > /dev/null 2>&1");
        }
    }

    return true;
}

bool ApiSystem::canUpdate(std::vector<std::string>& output) 
{
	LOG(LogDebug) << "ApiSystem::canUpdate";

#ifdef ES4ALL_SELF_UPDATE
	// es4all: 查询 GitHub Releases 判断是否有更新; 有则把目标版本号回填 output(供 UI 显示)。
	{
		Es4allRelease rel;
		if (Es4allUpdate::findLatestApplicable(rel))
		{
			output.push_back(rel.version);
			// es4all: 第二项(仅当有值时)= 目标建置时间戳, 供 GuiUpdate 区分同版本号的不同次建置。
			// 留空则不 push, 避免角落通知(NetworkThread 逐行拼接 msgtbl)多出一个空行。
			if (!rel.buildInfo.empty())
				output.push_back(rel.buildInfo);
			return true;
		}
		return false;
	}
#endif

	FILE *pipe = popen("batocera-config canupdate", "r");
	if (pipe == NULL)
		return false;

	char line[1024];
	while (fgets(line, 1024, pipe)) 
	{
		strtok(line, "\n");
		output.push_back(std::string(line));
	}

	int res = WEXITSTATUS(pclose(pipe));
	if (res == 0) 
	{
		LOG(LogInfo) << "Can update ";
		return true;
	}

	LOG(LogInfo) << "Cannot update ";
	return false;
}

void ApiSystem::launchExternalWindow_before(Window *window) 
{
	LOG(LogDebug) << "ApiSystem::launchExternalWindow_before";

	AudioManager::getInstance()->deinit();
	VolumeControl::getInstance()->deinit();
#ifdef _ENABLEEMUELEC	
	window->deinit(false);
#else
	window->deinit();
#endif

	LOG(LogDebug) << "ApiSystem::launchExternalWindow_before OK";
}

void ApiSystem::launchExternalWindow_after(Window *window) 
{
	LOG(LogDebug) << "ApiSystem::launchExternalWindow_after";

#ifdef _ENABLEEMUELEC
	window->init(false);
#else
	window->init();
#endif
	VolumeControl::getInstance()->init();
	AudioManager::getInstance()->init();
	window->normalizeNextUpdate();
	window->reactivateGui();

	AudioManager::getInstance()->playRandomMusic();

	LOG(LogDebug) << "ApiSystem::launchExternalWindow_after OK";
}

bool ApiSystem::launchKodi(Window *window) 
{
	LOG(LogDebug) << "ApiSystem::launchKodi";

	std::string commandline = InputManager::getInstance()->configureEmulators();
	std::string command = "batocera-kodi " + commandline;

	ApiSystem::launchExternalWindow_before(window);

	int exitCode = system(command.c_str());

	// WIFEXITED returns a nonzero value if the child process terminated normally with exit or _exit.
	// https://www.gnu.org/software/libc/manual/html_node/Process-Completion-Status.html
	if (WIFEXITED(exitCode))
		exitCode = WEXITSTATUS(exitCode);

	ApiSystem::launchExternalWindow_after(window);

	// handle end of kodi
	switch (exitCode) 
	{
	case 10: // reboot code
		Utils::Platform::quitES(Utils::Platform::QuitMode::REBOOT);
		return true;
		
	case 11: // shutdown code
		Utils::Platform::quitES(Utils::Platform::QuitMode::SHUTDOWN);
		return true;
	}

	return exitCode == 0;
}

bool ApiSystem::launchFileManager(Window *window) 
{
	LOG(LogDebug) << "ApiSystem::launchFileManager";

#ifdef _ENABLEEMUELEC
	std::string command = "/usr/bin/emuelec-utils filemanager";
#else
	std::string command = "filemanagerlauncher";
#endif

	ApiSystem::launchExternalWindow_before(window);

	int exitCode = system(command.c_str());
	if (WIFEXITED(exitCode))
		exitCode = WEXITSTATUS(exitCode);

	ApiSystem::launchExternalWindow_after(window);

	return exitCode == 0;
}

bool ApiSystem::enableWifi(std::string ssid, std::string key)
{
#if defined(ES4ALL_PATHS_ROCKNIX)
	// es4all: ROCKNIX 的 wifictl 把「开无线电」与「连线」拆成两个动词 ——
	//   enable  = 只做 rfkill unblock wifi，**不会连线**
	//   connect = 才真正建立连线(connect_wifi，自己从 system.cfg 读 wifi.ssid/wifi.key)
	// 原本只呼叫 enable，所以实机上凭证有写进 system.cfg、无线电也解封了，
	// 但 NetworkManager 里连个 WiFi profile 都没有、wlan0 永远 disconnected
	// (实机 .179 复现: 界面显示 WIFI 已启用 + SSID/密码都在，但「WIFI IP地址」永远是未连接)。
	// 故 enable 之后必须再 connect。ssid/key 由 wifictl 自行从 system.cfg 取，
	// 但仍原样传入以兼容其 "$2/$3 优先" 的取值顺序。
	if (!executeScript("wifictl enable \"" + ssid + "\" \"" + key + "\""))
		return false;
	return executeScript("wifictl connect \"" + ssid + "\" \"" + key + "\"");
#elif defined(_ENABLEEMUELEC)
	return executeScript("batocera-config wifi enable \"" + ssid + "\" \"" + key + "\"");
#else
	return executeScript("batocera-wifi enable \"" + ssid + "\" \"" + key + "\"");
#endif
}

bool ApiSystem::disableWifi()
{
#if defined(ES4ALL_PATHS_ROCKNIX)
	return executeScript("wifictl disable");
#elif defined(_ENABLEEMUELEC)
	return executeScript("batocera-config wifi disable");
#else
	return executeScript("batocera-wifi disable");
#endif
}

std::string ApiSystem::getIpAddress()
{
	LOG(LogDebug) << "ApiSystem::getIpAddress";

	// es4all: 优先取默认路由所在接口的 IP，避免多网卡(如同时接 eth0 + wlan0)时
	// queryIPAddress 抓到非主接口(例如显示 wlan0 的 192.168.1.x 而非 eth0 主网段)。
	std::string primary = Utils::Platform::getShOutput("ip route get 1.1.1.1 2>/dev/null | sed -n 's/.*src \\([0-9.]*\\).*/\\1/p'");
	primary.erase(std::remove(primary.begin(), primary.end(), '\n'), primary.end());
	primary.erase(std::remove(primary.begin(), primary.end(), '\r'), primary.end());
	primary.erase(std::remove(primary.begin(), primary.end(), ' '), primary.end());
	if (!primary.empty())
		return primary;

	std::string result = Utils::Platform::queryIPAddress(); // platform.h
	if (result.empty())
		return "NOT CONNECTED";

	return result;
}

std::string ApiSystem::getWifiIpAddress()
{
	return Utils::Platform::queryWifiIPAddress();
}

bool ApiSystem::enableBluetooth()
{
	return executeScript("batocera-bluetooth enable 2>&1 >/dev/null");
}

bool ApiSystem::disableBluetooth()
{
	return executeScript("batocera-bluetooth disable");
}

void ApiSystem::startBluetoothLiveDevices(const std::function<void(const std::string)>& func)
{
	executeScript("batocera-bluetooth live_devices", func);
}

void ApiSystem::stopBluetoothLiveDevices()
{
	executeScript("batocera-bluetooth stop_live_devices");
}

bool ApiSystem::pairBluetoothDevice(const std::string& deviceName)
{
	return executeScript("batocera-bluetooth trust " + deviceName);
}

bool ApiSystem::connectBluetoothDevice(const std::string& deviceName)
{
	return executeScript("batocera-bluetooth connect " + deviceName);
}

bool ApiSystem::disconnectBluetoothDevice(const std::string& deviceName)
{
	return executeScript("batocera-bluetooth disconnect " + deviceName);
}

bool ApiSystem::removeBluetoothDevice(const std::string& deviceName)
{
	return executeScript("batocera-bluetooth remove " + deviceName);
}

bool ApiSystem::scanNewBluetooth(const std::function<void(const std::string)>& func)
{
	return executeScript("batocera-bluetooth trust input", func).second == 0;
}

std::vector<std::string> ApiSystem::getPairedBluetoothDeviceList()
{
	return executeEnumerationScript("batocera-bluetooth list");
}

std::vector<std::string> ApiSystem::getAvailableStorageDevices() 
{
	return executeEnumerationScript("batocera-config storage list");
}

std::vector<std::string> ApiSystem::getVideoModes(const std::string output)
{
  if(output == "") {
    return executeEnumerationScript("batocera-resolution listModes");
  } else {
    return executeEnumerationScript("batocera-resolution --screen \"" + output + "\" listModes");
  }
}

std::vector<std::string> ApiSystem::getCustomRunners() 
{
	return executeEnumerationScript("batocera-wine-runners");
}

std::vector<std::string> ApiSystem::getAvailableBackupDevices() 
{
	return executeEnumerationScript("batocera-sync list");
}

std::vector<std::string> ApiSystem::getAvailableInstallDevices() 
{
	return executeEnumerationScript("batocera-install listDisks");
}

std::vector<std::string> ApiSystem::getAvailableInstallArchitectures() 
{
	return executeEnumerationScript("batocera-install listArchs");
}

std::vector<std::string> ApiSystem::getAvailableOverclocking() 
{
#ifdef _ENABLEEMUELEC
	return executeEnumerationScript("echo no");
#else
	return executeEnumerationScript("batocera-overclock list");
#endif
}

std::vector<std::string> ApiSystem::getSystemInformations() 
{
	return executeEnumerationScript("batocera-info --full");
}

std::vector<BiosSystem> ApiSystem::getBiosInformations(const std::string system) 
{
	std::vector<BiosSystem> res;
	BiosSystem current;
	bool isCurrent = false;

	std::string cmd = "batocera-systems";
	if (!system.empty())
		cmd += " --filter " + system;

	auto systems = executeEnumerationScript(cmd);
	for (auto line : systems)
	{
		if (Utils::String::startsWith(line, "> ")) 
		{
			if (isCurrent)
				res.push_back(current);

			isCurrent = true;
			current.name = std::string(std::string(line).substr(2));
			current.bios.clear();
		}
		else 
		{
			BiosFile biosFile;
			std::vector<std::string> tokens = Utils::String::split(line, ' ');
			if (tokens.size() >= 3) 
			{
				biosFile.status = tokens.at(0);
				biosFile.md5 = tokens.at(1);

				// concatenat the ending words
				std::string vname = "";
				for (unsigned int i = 2; i < tokens.size(); i++) 
				{
					if (i > 2) vname += " ";
					vname += tokens.at(i);
				}
				biosFile.path = vname;

				current.bios.push_back(biosFile);
			}
		}
	}

	if (isCurrent)
		res.push_back(current);

	return res;
}

bool ApiSystem::generateSupportFile() 
{
	return executeScript("batocera-support");
}

std::string ApiSystem::getCurrentStorage() 
{
	LOG(LogDebug) << "ApiSystem::getCurrentStorage";

#if WIN32
	return "DEFAULT";
#endif

	std::ostringstream oss;
	oss << "batocera-config storage current";
	FILE *pipe = popen(oss.str().c_str(), "r");
	char line[1024];

	if (pipe == NULL)
		return "";	

	if (fgets(line, 1024, pipe)) {
		strtok(line, "\n");
		pclose(pipe);
		return std::string(line);
	}
	return "INTERNAL";
}

bool ApiSystem::setStorage(std::string selected) 
{
	return executeScript("batocera-config storage " + selected);
}

bool ApiSystem::setButtonColorGameForce(std::string selected)
{
	return executeScript("batocera-gameforce buttonColorLed " + selected);
}

bool ApiSystem::setPowerLedGameForce(std::string selected)
{
	return executeScript("batocera-gameforce powerLed " + selected);
}

bool ApiSystem::forgetBluetoothControllers() 
{
	return executeScript("batocera-config forgetBT");
}

std::string ApiSystem::getRootPassword() 
{
	LOG(LogDebug) << "ApiSystem::getRootPassword";

	std::ostringstream oss;
	oss << "batocera-config getRootPassword";
	FILE *pipe = popen(oss.str().c_str(), "r");
	char line[1024];

	if (pipe == NULL) {
		return "";
	}

	if (fgets(line, 1024, pipe)) {
		strtok(line, "\n");
		pclose(pipe);
		return std::string(line);
	}
	return oss.str().c_str();
}

std::vector<std::string> ApiSystem::getAvailableVideoOutputDevices() 
{
	return executeEnumerationScript("batocera-config lsoutputs");
}

std::vector<std::string> ApiSystem::getAvailableAudioOutputDevices() 
{
#if WIN32
	std::vector<std::string> res;
	res.push_back("auto");
	return res;
#endif

	return executeEnumerationScript("batocera-audio list");
}

std::string ApiSystem::getCurrentAudioOutputDevice() 
{
#if WIN32
	return "auto";
#endif

	LOG(LogDebug) << "ApiSystem::getCurrentAudioOutputDevice";

	std::ostringstream oss;
	oss << "batocera-audio get";
	FILE *pipe = popen(oss.str().c_str(), "r");
	char line[1024];

	if (pipe == NULL)
		return "";	

	if (fgets(line, 1024, pipe)) 
	{
		strtok(line, "\n");
		pclose(pipe);
		return std::string(line);
	}

	return "";
}

bool ApiSystem::setAudioOutputDevice(std::string selected) 
{
	LOG(LogDebug) << "ApiSystem::setAudioOutputDevice";

	std::ostringstream oss;

	oss << "batocera-audio set" << " '" << selected << "'";
	int exitcode = system(oss.str().c_str());

	Sound::get(":/checksound.ogg")->play();

	return exitcode == 0;
}

std::vector<std::string> ApiSystem::getAvailableAudioOutputProfiles()
{
#if WIN32
	std::vector<std::string> res;
	res.push_back("auto");
	return res;
#endif

	return executeEnumerationScript("batocera-audio list-profiles");
}

std::string ApiSystem::getCurrentAudioOutputProfile() 
{
#if WIN32
	return "auto";
#endif

	LOG(LogDebug) << "ApiSystem::getCurrentAudioOutputProfile";

	std::ostringstream oss;
	oss << "batocera-audio get-profile";
	FILE *pipe = popen(oss.str().c_str(), "r");
	char line[1024];

	if (pipe == NULL)
		return "";	

	if (fgets(line, 1024, pipe)) 
	{
		strtok(line, "\n");
		pclose(pipe);
		return std::string(line);
	}

	return "";
}

bool ApiSystem::setAudioOutputProfile(std::string selected) 
{
	LOG(LogDebug) << "ApiSystem::setAudioOutputProfile";

	std::ostringstream oss;

	oss << "batocera-audio set-profile" << " '" << selected << "'";
	int exitcode = system(oss.str().c_str());
	
	Sound::get(":/checksound.ogg")->play();

	return exitcode == 0;
}

std::string ApiSystem::getUpdateUrl()
{
	auto systemsetting = SystemConf::getInstance()->get("global.updates.url");
	if (!systemsetting.empty())
		return systemsetting;

#ifdef _ENABLEEMUELEC
	return "https://raw.githubusercontent.com/EmuELEC/emuelec.github.io/master/";
#else
	return "https://updates.batocera.org";
#endif

}

std::string ApiSystem::getThemesUrl()
{
	auto systemsetting = SystemConf::getInstance()->get("global.themes.url");
	if (!systemsetting.empty())
		return systemsetting;

	return getUpdateUrl() + "/themes.json";
}

std::string ApiSystem::getGitRepositoryDefaultBranch(const std::string& url)
{
	std::string ret = "master";

	std::string statUrl = Utils::String::replace(url, "https://github.com/", "https://api.github.com/repos/");
	if (statUrl != url)
	{
		HttpReq statreq(statUrl);
		if (statreq.wait())
		{
			const std::string default_branch = "\"default_branch\": ";

			std::string content = statreq.getContent();
			auto pos = content.find(default_branch);
			if (pos != std::string::npos)
			{
				auto end = content.find(",", pos);
				if (end != std::string::npos)
				{
					ret = Utils::String::replace(content.substr(pos + default_branch.length(), end - pos - default_branch.length()), "\"", "");
				}
			}
		}
	}

	return ret;
}

bool ApiSystem::downloadGitRepository(const std::string& url, const std::string& branch, const std::string& fileName, const std::string& label, const std::function<void(const std::string)>& func, int64_t defaultDownloadSize)
{
	if (func != nullptr)
		func("Downloading " + label);

	int64_t downloadSize = defaultDownloadSize;
	if (downloadSize == 0)
	{
		std::string statUrl = Utils::String::replace(url, "https://github.com/", "https://api.github.com/repos/");
		if (statUrl != url)
		{
			HttpReq statreq(statUrl);
			if (statreq.wait())
			{
				std::string content = statreq.getContent();
				auto pos = content.find("\"size\": ");
				if (pos != std::string::npos)
				{
					auto end = content.find(",", pos);
					if (end != std::string::npos)
						downloadSize = atoi(content.substr(pos + 8, end - pos - 8).c_str()) * 1024LL;
				}
			}
		}
	}

	HttpReq httpreq(url + "/archive/"+ branch +".zip", fileName);

	int curPos = -1;
	while (httpreq.status() == HttpReq::REQ_IN_PROGRESS)
	{
		if (downloadSize > 0)
		{
			int64_t pos = httpreq.getPosition();
			if (pos > 0 && curPos != pos)
			{
				if (func != nullptr)
				{
					std::string pc = std::to_string((int)(pos * 100LL / downloadSize));
					func(std::string("Downloading " + label + " >>> " + pc + " %"));
				}

				curPos = pos;
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	if (httpreq.status() != HttpReq::REQ_SUCCESS)
		return false;

	return true;
}

bool ApiSystem::isThemeInstalled(const std::string& themeName, const std::string& url)
{
	std::string themeUrl = Utils::FileSystem::getFileName(url);

	std::vector<std::string> paths =
	{
		Paths::getUserThemesPath(),
		Paths::getThemesPath(),
		Paths::getUserEmulationStationPath() + "/themes"
#if !WIN32
		,"/etc/emulationstation/themes" // Backward compatibility with Retropie
#endif
	};

	for (auto path : VectorHelper::distinct(paths, [](auto x) { return x; }))
	{
		if (path.empty())
			continue;

		if (Utils::FileSystem::isDirectory(path + "/" + themeUrl))
			return true;

		if (Utils::FileSystem::isDirectory(path + "/" + themeUrl + "-master"))
			return true;

		if (Utils::FileSystem::isDirectory(path + "/" + themeName))
			return true;
	}

	return false;
}

extern std::string jsonString(const rapidjson::Value& val, const std::string& name);
extern int jsonInt(const rapidjson::Value& val, const std::string& name);

std::vector<BatoceraTheme> ApiSystem::getBatoceraThemesList()
{
	LOG(LogDebug) << "ApiSystem::getBatoceraThemesList";

	std::vector<BatoceraTheme> res;

	auto url = getThemesUrl();

	HttpReq httpreq(url);
	if (httpreq.wait())
	{
		rapidjson::Document doc;
		doc.Parse(httpreq.getContent().c_str());
		if (doc.HasParseError())
			return res;

		if (!doc.HasMember("data"))
			return res;

		for (auto& item : doc["data"].GetArray())
		{
			BatoceraTheme bt;
			bt.name = jsonString(item, "theme");
			bt.url = jsonString(item, "theme_url");
			bt.author = jsonString(item, "author");
			bt.lastUpdate = jsonString(item, "last_update");
			bt.upToDate = jsonInt(item, "up_to_date");
			bt.size = jsonInt(item, "size");
			bt.isInstalled = isThemeInstalled(bt.name, bt.url);

			auto screenShot = jsonString(item, "screenshot");
			if (!screenShot.empty())
				bt.image = Utils::FileSystem::getParent(url) + "/" + screenShot;

			res.push_back(bt);
		}
	}

	return res;	
}

std::pair<std::string, int> ApiSystem::installBatoceraTheme(std::string thname, const std::function<void(const std::string)>& func)
{
	for (auto theme : getBatoceraThemesList())
	{
		if (theme.name != thname)
			continue;

		std::string installFolder = Paths::getUserThemesPath();
		if (installFolder.empty())
			installFolder = Paths::getThemesPath();
		if (installFolder.empty())
			installFolder = Paths::getUserEmulationStationPath() + "/themes";

		std::string themeFileName = Utils::FileSystem::getFileName(theme.url);
		std::string extractionDirectory = installFolder + "/.tmp";
		std::string zipFile = extractionDirectory +"/" + themeFileName + ".zip";

		Utils::FileSystem::createDirectory(extractionDirectory);
		Utils::FileSystem::removeFile(zipFile);
		
		std::string branch = getGitRepositoryDefaultBranch(theme.url);

		if (downloadGitRepository(theme.url, branch, zipFile, thname, func, theme.size * 1024LL * 1024))
		{
			if (func != nullptr)
				func(_("Extracting") + " " + thname);

			unzipFile(zipFile, extractionDirectory);
			
			std::string folderName = extractionDirectory + "/" + themeFileName + "-" + branch;
			if (!Utils::FileSystem::exists(folderName))
				folderName = extractionDirectory + "/" + themeFileName;

			if (Utils::FileSystem::exists(folderName))
			{
				std::string finalfolderName = installFolder  + "/" + themeFileName;
				if (Utils::FileSystem::exists(finalfolderName))
					Utils::FileSystem::deleteDirectoryFiles(finalfolderName, true);

				Utils::FileSystem::renameFile(folderName, finalfolderName);
			}

			Utils::FileSystem::removeFile(zipFile);
			Utils::FileSystem::deleteDirectoryFiles(extractionDirectory, true);

			return std::pair<std::string, int>(std::string("OK"), 0);
		}

		Utils::FileSystem::deleteDirectoryFiles(extractionDirectory, true);
		return std::pair<std::string, int>(std::string(""), 1);
	}

	return std::pair<std::string, int>(std::string(""), 1);
}

std::pair<std::string, int> ApiSystem::uninstallBatoceraTheme(std::string thname, const std::function<void(const std::string)>& func)
{
	for (auto theme : getBatoceraThemesList())
	{
		if (!theme.isInstalled || theme.name != thname)
			continue;

		std::string installFolder = Paths::getUserThemesPath();
		if (installFolder.empty())
			installFolder = Paths::getThemesPath();
		if (installFolder.empty())
			installFolder = Paths::getUserEmulationStationPath() + "/themes";

		std::string themeFileName = Utils::FileSystem::getFileName(theme.url);

		std::string folderName = installFolder + "/" + themeFileName;
		if (!Utils::FileSystem::exists(folderName))
			folderName = folderName + "-master";

		if (Utils::FileSystem::exists(folderName))
		{
			Utils::FileSystem::deleteDirectoryFiles(folderName, true);
			return std::pair<std::string, int>("OK", 0);
		}

		break;
	}

	return std::pair<std::string, int>(std::string(""), 1);
}

std::vector<BatoceraBezel> ApiSystem::getBatoceraBezelsList()
{
	LOG(LogInfo) << "ApiSystem::getBatoceraBezelsList";

	std::vector<BatoceraBezel> res;

	auto lines = executeEnumerationScript("batocera-es-thebezelproject list");
	for (auto line : lines)
	{
		auto parts = Utils::String::splitAny(line, " \t");
		if (parts.size() < 2)
			continue;

		if (!Utils::String::startsWith(parts[0], "[I]") && !Utils::String::startsWith(parts[0], "[A]"))
			continue;

		BatoceraBezel bz;
		bz.isInstalled = (Utils::String::startsWith(parts[0], "[I]"));
		bz.name = parts[1];
		bz.url = parts.size() < 3 ? "" : (parts[2] == "-" ? parts[3] : parts[2]);
		bz.folderPath = parts.size() < 4 ? "" : parts[3];

		if (bz.name != "?")
			res.push_back(bz);
	}

	return res;
}

std::pair<std::string, int> ApiSystem::installBatoceraBezel(std::string bezelsystem, const std::function<void(const std::string)>& func)
{
	return executeScript("batocera-es-thebezelproject install " + bezelsystem, func);
}

std::pair<std::string, int> ApiSystem::uninstallBatoceraBezel(std::string bezelsystem, const std::function<void(const std::string)>& func)
{
	return executeScript("batocera-es-thebezelproject remove " + bezelsystem, func);
}

std::string ApiSystem::getMD5(const std::string fileName, bool fromZipContents)
{
	LOG(LogDebug) << "getMD5 >> " << fileName;

	// 7za x -so test.7z | md5sum
	std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(fileName));
	if (ext == ".zip" && fromZipContents)
	{
		Utils::Zip::ZipFile file;
		if (file.load(fileName))
		{
			std::string romName;

			for (auto name : file.namelist())
			{
				if (Utils::FileSystem::getExtension(name) != ".txt" && !Utils::String::endsWith(name, "/"))
				{
					if (!romName.empty())
					{
						romName = "";
						break;
					}

					romName = name;
				}
			}

			if (!romName.empty())
				return file.getFileMd5(romName);
		}
	}

#if !WIN32
	if (fromZipContents && ext == ".7z")
	{
		auto cmd = getSevenZipCommand() + " x -so \"" + fileName + "\" | md5sum";
		auto ret = executeEnumerationScript(cmd);
		if (ret.size() == 1 && ret.cbegin()->length() >= 32)
			return ret.cbegin()->substr(0, 32);
	}
#endif

	std::string contentFile = fileName;
	std::string ret;
	std::string tmpZipDirectory;

	if (fromZipContents && ext == ".7z")
	{
		tmpZipDirectory = Utils::FileSystem::combine(Utils::FileSystem::getTempPath(), Utils::FileSystem::getStem(fileName));
		Utils::FileSystem::deleteDirectoryFiles(tmpZipDirectory);

		if (unzipFile(fileName, tmpZipDirectory))
		{
			auto fileList = Utils::FileSystem::getDirContent(tmpZipDirectory, true);

			std::vector<std::string> res;
			std::copy_if(fileList.cbegin(), fileList.cend(), std::back_inserter(res), [](const std::string file) { return Utils::FileSystem::getExtension(file) != ".txt";  });
		
			if (res.size() == 1)
				contentFile = *res.cbegin();
		}

		// if there's no file or many files ? get md5 of archive
	}

	ret = Utils::FileSystem::getFileMd5(contentFile);

	if (!tmpZipDirectory.empty())
		Utils::FileSystem::deleteDirectoryFiles(tmpZipDirectory, true);

	LOG(LogDebug) << "getMD5 << " << ret;

	return ret;
}

std::string ApiSystem::getCRC32(std::string fileName, bool fromZipContents)
{
	LOG(LogDebug) << "getCRC32 >> " << fileName;

	std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(fileName));

	if (ext == ".7z" && fromZipContents)
	{
		LOG(LogDebug) << "getCRC32 is using 7z";

		std::string fn = Utils::FileSystem::getFileName(fileName);
		auto cmd = getSevenZipCommand() + " l -slt \"" + fileName + "\"";
		auto lines = executeEnumerationScript(cmd);
		for (std::string all : lines)
		{
			int idx = all.find("CRC = ");
			if (idx != std::string::npos)
				return all.substr(idx + 6);
			else if (all.find(fn) == (all.size() - fn.size()) && all.length() > 8 && all[9] == ' ')
				return all.substr(0, 8);
		}
	}
	else if (ext == ".zip" && fromZipContents)
	{
		LOG(LogDebug) << "getCRC32 is using ZipFile";

		Utils::Zip::ZipFile file;
		if (file.load(fileName))
		{
			std::string romName;

			for (auto name : file.namelist())
			{
				if (Utils::FileSystem::getExtension(name) != ".txt" && !Utils::String::endsWith(name, "/"))
				{
					if (!romName.empty())
					{
						romName = "";
						break;
					}

					romName = name;
				}
			}

			if (!romName.empty())
				return file.getFileCrc(romName);
		}
	}

	LOG(LogDebug) << "getCRC32 is using fileBuffer";
	return Utils::FileSystem::getFileCrc32(fileName);
}

bool ApiSystem::unzipFile(const std::string fileName, const std::string destFolder, const std::function<bool(const std::string)>& shouldExtract)
{
	LOG(LogDebug) << "unzipFile >> " << fileName << " to " << destFolder;

	if (!Utils::FileSystem::exists(destFolder))
		Utils::FileSystem::createDirectory(destFolder);
		
	if (Utils::String::toLower(Utils::FileSystem::getExtension(fileName)) == ".zip")
	{
		LOG(LogDebug) << "unzipFile is using ZipFile";

		Utils::Zip::ZipFile file;
		if (file.load(fileName))
		{
			for (auto name : file.namelist())
			{
				if (Utils::String::endsWith(name, "/"))
				{
					Utils::FileSystem::createDirectory(Utils::FileSystem::combine(destFolder, name.substr(0, name.length() - 1)));
					continue;
				}

				if (shouldExtract != nullptr && !shouldExtract(Utils::FileSystem::combine(destFolder, name)))
					continue;

				file.extract(name, destFolder);
			}

			LOG(LogDebug) << "unzipFile << OK";
			return true;
		}

		LOG(LogDebug) << "unzipFile << KO Bad format ?" << fileName;
		return false;
	}
	
	LOG(LogDebug) << "unzipFile is using 7z";

	std::string cmd = getSevenZipCommand() + " x \"" + Utils::FileSystem::getPreferredPath(fileName) + "\" -y -o\"" + Utils::FileSystem::getPreferredPath(destFolder) + "\"";
	bool ret = executeScript(cmd);
	LOG(LogDebug) << "unzipFile <<";
	return ret;
}

static std::string BACKLIGHT_BRIGHTNESS_NAME;
static std::string BACKLIGHT_BRIGHTNESS_MAX_NAME;

bool ApiSystem::getBrightness(int& value)
{	
	#if WIN32
	return false;
	#endif

	if (BACKLIGHT_BRIGHTNESS_NAME == "notfound")
		return false;

	if (BACKLIGHT_BRIGHTNESS_NAME.empty() || BACKLIGHT_BRIGHTNESS_MAX_NAME.empty())
	{
		for (auto file : Utils::FileSystem::getDirContent("/sys/class/backlight"))
		{				
			std::string brightnessPath = file + "/brightness";
			std::string maxBrightnessPath = file + "/max_brightness";

			if (Utils::FileSystem::exists(brightnessPath) && Utils::FileSystem::exists(maxBrightnessPath))
			{
				BACKLIGHT_BRIGHTNESS_NAME = brightnessPath;
				BACKLIGHT_BRIGHTNESS_MAX_NAME = maxBrightnessPath;

				LOG(LogInfo) << "ApiSystem::getBrightness > brightness path resolved to " << file;
				break;
			}
		}
	}

	if (BACKLIGHT_BRIGHTNESS_NAME.empty() || BACKLIGHT_BRIGHTNESS_MAX_NAME.empty())
	{
		LOG(LogInfo) << "ApiSystem::getBrightness > brightness path is not resolved";

		BACKLIGHT_BRIGHTNESS_NAME = "notfound";
		return false;
	}

	value = 0;

	int max = Utils::String::toInteger(Utils::FileSystem::readAllText(BACKLIGHT_BRIGHTNESS_MAX_NAME));
	if (max == 0)
		return false;

	if (Utils::FileSystem::exists(BACKLIGHT_BRIGHTNESS_NAME))
		value = Utils::String::toInteger(Utils::FileSystem::readAllText(BACKLIGHT_BRIGHTNESS_NAME));

	value = (uint32_t) ((value / (float)max * 100.0f) + 0.5f);
	return true;
}

void ApiSystem::setBrightness(int value)
{
#if WIN32	
	return;
#endif 

	if (BACKLIGHT_BRIGHTNESS_NAME.empty() || BACKLIGHT_BRIGHTNESS_NAME == "notfound")
		return;

	if (value < 1)
		value = 1;

	if (value > 100)
		value = 100;

	int max = Utils::String::toInteger(Utils::FileSystem::readAllText(BACKLIGHT_BRIGHTNESS_MAX_NAME));
	if (max == 0)
		return;

	float percent = (value / 100.0f * (float)max) + 0.5f;
		
	std::string content = std::to_string((uint32_t) percent) + "\n";
	Utils::FileSystem::writeAllText(BACKLIGHT_BRIGHTNESS_NAME, content);
}

static std::string LED_COLOUR_NAME;
static std::string LED_BRIGHTNESS_VALUE;
static std::string LED_MAX_BRIGHTNESS_VALUE;

bool ApiSystem::getLED(int& red, int& green, int& blue)
{	
#if WIN32
	return false;
#endif

	if (mSystemLedType != LED_TYPE_NONE)
		return true;

	auto entries = Utils::FileSystem::getDirContent("/sys/class/leds");
	bool found_addressable = false;

	for (const auto& entry : entries)
	{
		if (entry.find("multicolor") != std::string::npos || entry.find(":rgb:joystick_rings") != std::string::npos)
		{
			std::string ledColourPath = entry + "/multi_intensity";				
			if (Utils::FileSystem::exists(ledColourPath))
			{
				LED_COLOUR_NAME = ledColourPath;
				mSystemLedType = LED_TYPE_UNIFIED;
				LOG(LogInfo) << "ApiSystem::getLED > Found UNIFIED LED at " << entry;
				break;
			}
		}
		if (entry.find("l:b1") != std::string::npos)
		{
			found_addressable = true;
		}
	}

	if (mSystemLedType == LED_TYPE_NONE && found_addressable) {
		mSystemLedType = LED_TYPE_ADDRESSABLE;
		LOG(LogInfo) << "ApiSystem::getLED > Found ADDRESSABLE LED type";
	}

	if (mSystemLedType == LED_TYPE_NONE) {
		LED_COLOUR_NAME = "notfound";
		return false;
	}

    if (mSystemLedType == LED_TYPE_UNIFIED && Utils::FileSystem::exists(LED_COLOUR_NAME)) {
        std::string colourValue = Utils::FileSystem::readAllText(LED_COLOUR_NAME);
        std::stringstream ss(colourValue);
        std::string token;
        // Extract red value
        std::getline(ss, token, ' ');
        red = std::stoi(token);

        // Extract green value
        std::getline(ss, token, ' ');
        green = std::stoi(token);

        // Extract blue value
        std::getline(ss, token);
        blue = std::stoi(token);

        executeScript("batocera-led-handheld block_color_changes"); // temporarily prevent changes from external daemon
        LOG(LogInfo) << "ApiSystem::getLED > LED colours are:" << red << " " << green << " " << blue;

        return true;
    }
	else if (mSystemLedType == LED_TYPE_ADDRESSABLE) {
		getLEDColours(red, green, blue);
        executeScript("batocera-led-handheld block_color_changes");
		return true;
	}

	return false;
}

void ApiSystem::getLEDColours(int& red, int& green, int& blue)
{
	std::string colourValue = SystemConf::getInstance()->get("led.colour");
	if (colourValue.empty())
		colourValue = "255 0 165";

    std::stringstream ss(colourValue);
    std::string token;

	// Extract red value
    std::getline(ss, token, ' ');
    red = std::stoi(token);

	// Extract green value
    std::getline(ss, token, ' ');
    green = std::stoi(token);

	// Extract blue value
    std::getline(ss, token);
    blue = std::stoi(token);

	LOG(LogInfo) << "ApiSystem::getLEDColours > LED colours are: " << red << " " << green << " " << blue;
}

void ApiSystem::setLEDColours(int red, int green, int blue)
{
#if WIN32    
    return;
#endif 

	if (mSystemLedType == LED_TYPE_NONE)
		return;

    // Ensure RGB values are within valid range
	if (red < 0) red = 0;
    if (red > 255) red = 255;
    if (green < 0) green = 0;
    if (green > 255) green = 255;
    if (blue < 0) blue = 0;
    if (blue > 255) blue = 255;

	if (mSystemLedType == LED_TYPE_UNIFIED)
	{
		if (LED_COLOUR_NAME.empty() || LED_COLOUR_NAME == "notfound") return;
		std::string content = std::to_string(red) + " " + std::to_string(green) + " " + std::to_string(blue);
		Utils::FileSystem::writeAllText(LED_COLOUR_NAME, content);
	}
	else if (mSystemLedType == LED_TYPE_ADDRESSABLE)
	{
		static std::vector<std::string> r_files, g_files, b_files;
		if (r_files.empty()) {
			auto all_files = Utils::FileSystem::getDirContent("/sys/class/leds");
			for(const auto& file : all_files) {
				if (file.find(":r") != std::string::npos) r_files.push_back(file + "/brightness");
				if (file.find(":g") != std::string::npos) g_files.push_back(file + "/brightness");
				if (file.find(":b") != std::string::npos) b_files.push_back(file + "/brightness");
			}
		}

		for(const auto& file : r_files) Utils::FileSystem::writeAllText(file, std::to_string(red));
		for(const auto& file : g_files) Utils::FileSystem::writeAllText(file, std::to_string(green));
		for(const auto& file : b_files) Utils::FileSystem::writeAllText(file, std::to_string(blue));
	}
}

bool ApiSystem::getLEDBrightness(int& value)
{   
#if WIN32
    return false;
#endif
    if (mSystemLedType != LED_TYPE_UNIFIED)
        return false;

    if (LED_BRIGHTNESS_VALUE == "notfound")
        return false;

    if (LED_BRIGHTNESS_VALUE.empty() || LED_MAX_BRIGHTNESS_VALUE.empty())
    {
        auto directories = Utils::FileSystem::getDirContent("/sys/class/leds");

        for (const auto& directory : directories)
        {
            if (directory.find("multicolor") != std::string::npos || directory.find(":rgb:joystick_rings") != std::string::npos)
            {
                std::string ledBrightnessPath = directory + "/brightness";
                std::string ledMaxBrightnessPath = directory + "/max_brightness";

                if (Utils::FileSystem::exists(ledBrightnessPath) && Utils::FileSystem::exists(ledMaxBrightnessPath))
                {
                    LED_BRIGHTNESS_VALUE = ledBrightnessPath;
                    LED_MAX_BRIGHTNESS_VALUE = ledMaxBrightnessPath;

                    LOG(LogInfo) << "ApiSystem::getLEDBrightness > LED brightness path resolved to " << directory;
                    break;
                }
            }
        }
    }

    if (LED_BRIGHTNESS_VALUE.empty() || LED_MAX_BRIGHTNESS_VALUE.empty())
    {
        LOG(LogInfo) << "ApiSystem::getLEDBrightness > LED brightness path is not resolved";

        LED_BRIGHTNESS_VALUE = "notfound";
        return false;
    }

    value = 0;

    int max = Utils::String::toInteger(Utils::FileSystem::readAllText(LED_MAX_BRIGHTNESS_VALUE));
    if (max == 0)
        return false;

    if (Utils::FileSystem::exists(LED_BRIGHTNESS_VALUE))
        value = Utils::String::toInteger(Utils::FileSystem::readAllText(LED_BRIGHTNESS_VALUE));

    value = (uint32_t) ((value / (float)max * 100.0f) + 0.5f);
    return true;
}

void ApiSystem::setLEDBrightness(int value) 
{
#if WIN32
    return;
#endif
    if (mSystemLedType != LED_TYPE_UNIFIED)
		return;

    if (LED_BRIGHTNESS_VALUE.empty() || LED_BRIGHTNESS_VALUE == "notfound")
        return;

    if (value < 0) value = 0;
	if (value > 100) value = 100;

    int max = Utils::String::toInteger(Utils::FileSystem::readAllText(LED_MAX_BRIGHTNESS_VALUE));
    if (max == 0)
        return;

    float percent = static_cast<float>(value) / 100.0f;
	int brightnessValue = static_cast<int>(percent * max + 0.5f);
    std::string content = std::to_string(brightnessValue) + "\n";
    Utils::FileSystem::writeAllText(LED_BRIGHTNESS_VALUE, content);
}

bool ApiSystem::isLEDEnabled()
{
#if WIN32
	return false;
#else
	// Check batocera.conf for "led.enabled" setting, default to "1" (true) if not found
	return SystemConf::getInstance()->get("led.enabled") != "0";
#endif
}

void ApiSystem::setLEDEnabled(bool enabled)
{
#if WIN32
    return;
#else
	SystemConf::getInstance()->set("led.enabled", enabled ? "1" : "0");

	if (!enabled)
	{
		setLEDColours(0, 0, 0);
	}
	else
	{
		std::string lastColorStr = SystemConf::getInstance()->get("led.colour");
		if (lastColorStr.empty())
			lastColorStr = "255 0 165";

		std::stringstream ss(lastColorStr);
		int r, g, b;
		ss >> r >> g >> b;

		setLEDColours(r, g, b);
	}

	SystemConf::getInstance()->saveSystemConf();
#endif
}

std::vector<std::string> ApiSystem::getWifiNetworks(bool scan)
{
	// es4all: 扫描(scanlist)用 timeout 包住 —— 底层 `connmanctl scan wifi` 在某些机型(实测
	// E900V22C 的 uwe5631 wifi)会卡住 30s+ 不返回, 而扫描是在 UI 线程同步调用 → 整个 ES 冻死。
	// 加 8s 上限: 扫不到就返回空, 至少 UI 不会长时间无响应。list(读缓存)不阻塞、不用包。
#if defined(ES4ALL_PATHS_ROCKNIX)
	return executeEnumerationScript(scan ? "timeout 8 wifictl scanlist" : "wifictl list");
#else
	return executeEnumerationScript(scan ? "timeout 8 batocera-wifi scanlist" : "batocera-wifi list");
#endif
}

std::vector<std::string> ApiSystem::executeEnumerationScript(const std::string command)
{
	LOG(LogDebug) << "ApiSystem::executeEnumerationScript -> " << command;

	std::vector<std::string> res;

	FILE *pipe = popen(command.c_str(), "r");

	if (pipe == NULL)
		return res;

	char line[1024];
	while (fgets(line, 1024, pipe))
	{
		strtok(line, "\n");
		res.push_back(std::string(line));
	}

	pclose(pipe);
	return res;
}

std::pair<std::string, int> ApiSystem::executeScript(const std::string command, const std::function<void(const std::string)>& func)
{
	LOG(LogInfo) << "ApiSystem::executeScript -> " << command;

	FILE *pipe = popen(command.c_str(), "r");
	if (pipe == NULL)
	{
		LOG(LogError) << "Error executing " << command;
		return std::pair<std::string, int>("Error starting command : " + command, -1);
	}

	char line[1024];
	while (fgets(line, 1024, pipe))
	{
		strtok(line, "\n");

		if (func != nullptr)
			func(std::string(line));
	}

	int exitCode = WEXITSTATUS(pclose(pipe));
	return std::pair<std::string, int>(line, exitCode);
}

bool ApiSystem::executeScript(const std::string command)
{	
	LOG(LogInfo) << "Running " << command;

	if (system(command.c_str()) == 0)
		return true;
	
	LOG(LogError) << "Error executing " << command;
	return false;
}

bool ApiSystem::isScriptingSupported(ScriptId script)
{
	std::vector<std::string> executables;

	switch (script)
	{
	case ApiSystem::THEMESDOWNLOADER:
		return true;
	case ApiSystem::RETROACHIVEMENTS:
		// es4all: 原本未定义 CHEEVOS_DEV_LOGIN 时会 break 掉出 switch，
		// 而下方 `if (executables.size() == 0) return true;` 使其**回传 true** ——
		// 与原意(没编入 cheevos 开发者凭证就不支持)相反。
		// 结果: 游戏设置→ACCOUNTS/RETROACHIEVEMENT SETTINGS、系统设置→RETROACHIEVEMENT SETTINGS
		// 等菜单照常显示，但没有凭证根本无法向 RetroAchievements 认证 → 看得见却不能用。
		// 改为明确 return false，恢复原意；若要启用，编译时带 -DCHEEVOS_DEV_LOGIN 即可。
#ifdef CHEEVOS_DEV_LOGIN
		return true;
#else
		return false;
#endif
	case ApiSystem::KODI:
		executables.push_back("kodi");
		break;
	case ApiSystem::WIFI:
#if defined(ES4ALL_PATHS_ROCKNIX)
		// es4all: ROCKNIX 用 wifictl(nmcli 后端，命令与 batocera-wifi 兼容)取代 batocera-wifi，
		// 让「网络设置」一级选单出现(否则找不到 batocera-wifi 会被隐藏)。
		return Utils::FileSystem::exists("/usr/bin/wifictl");
#else
		executables.push_back("batocera-wifi");
#endif
		break;
	case ApiSystem::BLUETOOTH:
		executables.push_back("batocera-bluetooth");
		break;
	case ApiSystem::RESOLUTION:
		executables.push_back("batocera-resolution");
		break;
	case ApiSystem::BIOSINFORMATION:
		executables.push_back("batocera-systems");
		break;
	case ApiSystem::DISKFORMAT:
		executables.push_back("batocera-format");
		break;
	case ApiSystem::OVERCLOCK:
		executables.push_back("batocera-overclock");
		break;
	case ApiSystem::NETPLAY:
		executables.push_back("7zr");
		break;
	case ApiSystem::PDFEXTRACTION:
		executables.push_back("pdftoppm");
		executables.push_back("pdfinfo");
		break;
	case ApiSystem::BATOCERASTORE:
		executables.push_back("batocera-store");
		break;
	case ApiSystem::THEBEZELPROJECT:
		executables.push_back("batocera-es-thebezelproject");
		break;		
	case ApiSystem::PADSINFO:
		executables.push_back("batocera-padsinfo");
		break;
	case ApiSystem::EVMAPY:
		executables.push_back("evmapy");
		break;
	case ApiSystem::BATOCERAPREGAMELISTSHOOK:
		executables.push_back("batocera-preupdate-gamelists-hook");
		break;
	case ApiSystem::TIMEZONES:
		executables.push_back("batocera-timezone");
		break;
	case ApiSystem::AUDIODEVICE:
		executables.push_back("batocera-audio");
		break;		
	case ApiSystem::BACKUP:
		executables.push_back("batocera-sync");
		break;
	case ApiSystem::INSTALL:
		executables.push_back("batocera-install");
		break;	
	case ApiSystem::SUPPORTFILE:
		executables.push_back("batocera-support");
		break;
	case ApiSystem::UPGRADE:
#ifdef ES4ALL_SELF_UPDATE
		// es4all: 自我更新走 GitHub Releases OTA(Es4allUpdate)，getVersion/canUpdate/updateSystem
		// 早已改接该路径、**不依赖** batocera-upgrade / emuelec-upgrade 这两个发行版脚本。
		// 这道门却漏改、还在探测脚本是否存在 —— EmuELEC 自带 emuelec-upgrade 所以过得了，
		// 但 ROCKNIX 两个都没有 → 回传 false → 「更新和下载」里整个「软件更新」组被隐藏
		// (实机 .179 现象: 只剩「主题」一项)。既然不靠外部脚本，这里直接回 true。
		return true;
#elif defined(_ENABLEEMUELEC)
		executables.push_back("emuelec-upgrade");
#else
		executables.push_back("batocera-upgrade");
#endif
		break;
	case ApiSystem::SUSPEND:
		return (Utils::FileSystem::exists("/usr/sbin/pm-suspend") && Utils::FileSystem::exists("/usr/bin/pm-is-supported") && executeScript("/usr/bin/pm-is-supported --suspend"));
	case ApiSystem::VERSIONINFO:
		executables.push_back("batocera-version");
		break;
	case ApiSystem::READPLANEMODE:
	case ApiSystem::WRITEPLANEMODE:
		executables.push_back("batocera-planemode");
		break;
	case ApiSystem::SERVICES:
		executables.push_back("batocera-services");
		break;
	case ApiSystem::BACKGLASS:
		executables.push_back("batocera-backglass");
		break;
	}

	if (executables.size() == 0)
		return true;

	// es4all: 脚本查找路径改为依序 fallback。
	// 原本 #ifdef _ENABLEEMUELEC 只查 /usr/bin/batocera/(EmuELEC 布局)，#else 查 /usr/bin/；
	// 但三个 target(armbian/rocknix/emuelec)都带 -DENABLE_EMUELEC=1，故 #else 永不编译 ——
	// Armbian/ROCKNIX 把 batocera-* 脚本装在 /usr/bin 或 /usr/local/bin 时一律找不到，
	// 导致蓝牙/分辨率/BIOS/格式化/超频/音频/服务等菜单被静默隐藏。
	// 改为逐目录查找，任一命中即视为支持；三边通用，不再依赖 target 宏。
	static const std::vector<std::string> searchDirs =
	{
		"/usr/bin/batocera/",
		"/usr/bin/",
		"/usr/local/bin/"
	};

	for (auto executable : executables)
	{
		bool found = false;

		for (auto dir : searchDirs)
		{
			if (Utils::FileSystem::exists(dir + executable))
			{
				found = true;
				break;
			}
		}

		if (!found)
			return false;
	}

	return true;
}

bool ApiSystem::downloadFile(const std::string url, const std::string fileName, const std::string label, const std::function<void(const std::string)>& func)
{
	if (func != nullptr)
		func("Downloading " + label);

	HttpReq httpreq(url, fileName);
	while (httpreq.status() == HttpReq::REQ_IN_PROGRESS)
	{
		if (func != nullptr)
			func(std::string("Downloading " + label + " >>> " + std::to_string(httpreq.getPercent()) + " %"));

		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	if (httpreq.status() != HttpReq::REQ_SUCCESS)
		return false;

	return true;
}

void ApiSystem::setReadyFlag(bool ready)
{
	if (!ready)
	{
		Utils::FileSystem::removeFile("/tmp/emulationstation.ready");
		return;
	}

	FILE* fd = fopen("/tmp/emulationstation.ready", "w");
	if (fd != NULL) 
		fclose(fd);
}

bool ApiSystem::isReadyFlagSet()
{
	return Utils::FileSystem::exists("/tmp/emulationstation.ready");
}

std::vector<std::string> ApiSystem::getFormatDiskList()
{
#if WIN32 && _DEBUG
	std::vector<std::string> ret;
	ret.push_back("d:\\ DRIVE D:");
	ret.push_back("e:\\ DRIVE Z:");
	return ret;
#endif
	return executeEnumerationScript("batocera-format listDisks");
}

std::vector<std::string> ApiSystem::getFormatFileSystems()
{
#if WIN32 && _DEBUG
	std::vector<std::string> ret;
	ret.push_back("exfat");	
	ret.push_back("brfs");
	return ret;
#endif
	return executeEnumerationScript("batocera-format listFstypes");
}

int ApiSystem::formatDisk(const std::string disk, const std::string format, const std::function<void(const std::string)>& func)
{
	return executeScript("batocera-format format " + disk + " " + format, func).second;
}

int ApiSystem::getPdfPageCount(const std::string& fileName)
{
	auto lines = executeEnumerationScript("pdfinfo \"" + fileName + "\"");
	for (auto line : lines)
	{
		auto splits = Utils::String::split(line, ':', true);
		if (splits.size() == 2 && splits[0] == "Pages")
			return atoi(Utils::String::trim(splits[1]).c_str());
	}

	return 0;
}

std::vector<std::string> ApiSystem::extractPdfImages(const std::string& fileName, int pageIndex, int pageCount, int quality)
{
	auto pdfFolder = Utils::FileSystem::getPdfTempPath();

	std::vector<std::string> ret;

	if (pageIndex < 0)
	{
		Utils::FileSystem::deleteDirectoryFiles(pdfFolder);

		int hardWareCoreCount = std::thread::hardware_concurrency();
		if (hardWareCoreCount > 1)
		{
			int lastTime = SDL_GetTicks();

			int numberOfPagesToProcess = 1;
			if (hardWareCoreCount < 8)
				numberOfPagesToProcess = 2;

			int pc = getPdfPageCount(fileName);
			if (pc > 0)
			{
				Utils::ThreadPool pool(1);

				for (int i = 0; i < pc; i += numberOfPagesToProcess)
					pool.queueWorkItem([this, fileName, i, numberOfPagesToProcess] { extractPdfImages(fileName, i + 1, numberOfPagesToProcess); });

				pool.wait();

				int time = SDL_GetTicks() - lastTime;
				std::string timeText = std::to_string(time) + "ms";

				for (auto file : Utils::FileSystem::getDirContent(pdfFolder, false))
				{
					auto ext = Utils::String::toLower(Utils::FileSystem::getExtension(file));
					if (ext != ".jpg" && ext != ".png" && ext != ".ppm")
						continue;

					ret.push_back(file);
				}

				std::sort(ret.begin(), ret.end());
			}

			return ret;
		}
	}

	int lastTime = SDL_GetTicks();

	std::string page;

	std::string squality = Renderer::isSmallScreen() ? "96" : "125";
	if (quality > 0)
		squality = std::to_string(quality); // "300";

	std::string prefix = "extract";
	if (pageIndex >= 0)
	{
		char buffer[12];
		sprintf(buffer, "%08d", (uint32_t)pageIndex);
		
		if (pageIndex < 0)
			prefix = "page-" + squality + "-" + std::string(buffer) + "-pdf"; // page
		else
			prefix = Utils::FileSystem::getFileName(fileName) + "-" + squality + "-" + std::string(buffer) + "-pdf"; // page

		page = " -f " + std::to_string(pageIndex) + " -l " + std::to_string(pageIndex + pageCount - 1);
	}

#if WIN32
	executeEnumerationScript("pdftoppm -r "+ squality + page +" \"" + fileName + "\" \""+ pdfFolder +"/" + prefix +"\"");
#else
	executeEnumerationScript("pdftoppm -jpeg -r "+ squality +" -cropbox" + page + " \"" + fileName + "\" \"" + pdfFolder + "/" + prefix + "\"");
#endif

	int time = SDL_GetTicks() - lastTime;
	std::string text = std::to_string(time);
	
	for (auto file : Utils::FileSystem::getDirContent(pdfFolder, false))
	{
		auto ext = Utils::String::toLower(Utils::FileSystem::getExtension(file));
		if (ext != ".jpg" && ext != ".png" && ext != ".ppm")
			continue;

		if (pageIndex >= 0 && !Utils::String::startsWith(Utils::FileSystem::getFileName(file), prefix))
			continue;

		ret.push_back(file);
	}

	std::sort(ret.begin(), ret.end());
	return ret;
}


std::vector<PacmanPackage> ApiSystem::getBatoceraStorePackages()
{
	std::vector<PacmanPackage> packages;

	LOG(LogDebug) << "ApiSystem::getBatoceraStorePackages";

	auto res = executeEnumerationScript("batocera-store list");
	std::string data = Utils::String::join(res, "\n");
	if (data.empty())
	{
		LOG(LogError) << "Package list is empty";
		return packages;
	}

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_string(data.c_str());
	if (!result)
	{
		LOG(LogError) << "Unable to parse packages";
		return packages;
	}

	pugi::xml_node root = doc.child("packages");
	if (!root)
	{
		LOG(LogError) << "Could not find <packages> node";
		return packages;
	}

	for (pugi::xml_node pkgNode = root.child("package"); pkgNode; pkgNode = pkgNode.next_sibling("package"))
	{
		PacmanPackage package;

		for (pugi::xml_node node = pkgNode.first_child(); node; node = node.next_sibling())
		{
			std::string tag = node.name();
			if (tag == "name")
				package.name = node.text().get();
			if (tag == "repository")
				package.repository = node.text().get();
			if (tag == "available_version")
				package.available_version = node.text().get();
			if (tag == "description")
				package.description = node.text().get();
			if (tag == "group")
				package.group = node.text().get(); // groups.push_back(
			if (tag == "license")
				package.licenses.push_back(node.text().get());
			if (tag == "packager")
				package.packager = node.text().get();
			if (tag == "status")
				package.status = node.text().get();
			if (tag == "repository")
				package.repository = node.text().get();
			if (tag == "url")
				package.url = node.text().get();			
			if (tag == "arch")
				package.arch = node.text().get();
			if (tag == "download_size")
				package.download_size = node.text().as_llong();
			if (tag == "installed_size")
				package.installed_size = node.text().as_llong();
			if (tag == "preview_url")
				package.preview_url = node.text().get();
		}

		if (!package.name.empty())
			packages.push_back(package);		
	}

	return packages;
}

std::pair<std::string, int> ApiSystem::installBatoceraStorePackage(std::string name, const std::function<void(const std::string)>& func)
{
	return executeScript("batocera-store install \"" + name + "\"", func);
}

std::pair<std::string, int> ApiSystem::uninstallBatoceraStorePackage(std::string name, const std::function<void(const std::string)>& func)
{
	return executeScript("batocera-store remove \"" + name + "\"", func);
}

void ApiSystem::refreshBatoceraStorePackageList()
{
	executeScript("batocera-store refresh");
	executeScript("batocera-store clean-all");
}

void ApiSystem::callBatoceraPreGameListsHook()
{
	executeScript("batocera-preupdate-gamelists-hook");
}

void ApiSystem::updateBatoceraStorePackageList()
{
	executeScript("batocera-store update");
}

std::vector<std::string> ApiSystem::getShaderList(const std::string& systemName, const std::string& emulator, const std::string& core)
{
	Utils::FileSystem::FileSystemCacheActivator fsc;

	std::vector<std::string> ret;

	for (auto folder : { Paths::getUserShadersPath(), Paths::getShadersPath() })
	{
		for (auto file : Utils::FileSystem::getDirContent(folder, true))
		{
			if (Utils::FileSystem::getFileName(file) == "rendering-defaults.yml")
			{
				auto parent = Utils::FileSystem::getFileName(Utils::FileSystem::getParent(file));
				if (parent == "configs")
					continue;

				if (std::find(ret.cbegin(), ret.cend(), parent) == ret.cend())
					ret.push_back(parent);
			}
		}
	}

	std::sort(ret.begin(), ret.end());
	return ret;
}

std::vector<std::string> ApiSystem::getVideoFilterList(const std::string& systemName, const std::string& emulator, const std::string& core)
{
	Utils::FileSystem::FileSystemCacheActivator fsc;

	std::vector<std::string> ret;

	LOG(LogDebug) << "ApiSystem::getVideoFilterList";

	for (auto folder : { Paths::getUserVideoFilters(), Paths::getVideoFilters() })
	{
		for (auto file : Utils::FileSystem::getDirContent(folder, false))
		{
			auto videofilter = Utils::FileSystem::getFileName(file);
			if (videofilter.substr(videofilter.find_last_of('.') + 1) == "filt")
			{
				if (std::find(ret.cbegin(), ret.cend(), videofilter) == ret.cend())
					ret.push_back(videofilter.substr(0, videofilter.find_last_of('.')));
			}
		}
	}

	std::sort(ret.begin(), ret.end());
	return ret;
}

std::vector<std::string> ApiSystem::getRetroachievementsSoundsList()
{
	Utils::FileSystem::FileSystemCacheActivator fsc;

	std::vector<std::string> ret;

	LOG(LogDebug) << "ApiSystem::getRetroAchievementsSoundsList";

	for (auto folder : { Paths::getUserRetroachivementSounds(), Paths::getRetroachivementSounds() })
	{
		for (auto file : Utils::FileSystem::getDirContent(folder, false))
		{
			auto sound = Utils::FileSystem::getFileName(file);
			if (sound.substr(sound.find_last_of('.') + 1) == "ogg")
			{
				if (std::find(ret.cbegin(), ret.cend(), sound) == ret.cend())
				  ret.push_back(sound.substr(0, sound.find_last_of('.')));
			}
		}
	}

	std::sort(ret.begin(), ret.end());
	return ret;
}

std::vector<std::string> ApiSystem::getTimezones()
{
	std::vector<std::string> ret;

	LOG(LogDebug) << "ApiSystem::getTimezones";

	auto folder = Paths::getTimeZonesPath();
	if (Utils::FileSystem::isDirectory(folder))
	{
		for (auto continent : Utils::FileSystem::getDirContent(folder, false))
		{
			std::string short_continent = continent.substr(continent.find_last_of('/') + 1);
			if (short_continent == "Africa" || short_continent == "America"
				|| short_continent == "Antarctica" || short_continent == "Asia"
				|| short_continent == "Atlantic" || short_continent == "Australia"
				|| short_continent == "Etc" || short_continent == "Europe"
				|| short_continent == "Indian" || short_continent == "Pacific")
			{
				for (auto file : Utils::FileSystem::getDirContent(continent, false))
				{
					if (!Utils::FileSystem::isDirectory(file))
					{
						auto tz = Utils::FileSystem::getFileName(file);
						if (std::find(ret.cbegin(), ret.cend(), tz) == ret.cend())
						ret.push_back(short_continent + "/" + tz);
					}
				}
			}
		}
	}

	std::sort(ret.begin(), ret.end());
	return ret;
}

std::string ApiSystem::getCurrentTimezone()
{
	LOG(LogInfo) << "ApiSystem::getCurrentTimezone";
	auto cmd = executeEnumerationScript("batocera-timezone get");
	std::string tz = Utils::String::join(cmd, "");
	remove_if(tz.begin(), tz.end(), isspace);
	if (tz.empty()) {
		cmd = executeEnumerationScript("batocera-timezone detect");
		tz = Utils::String::join(cmd, "");
	}
	return tz;
}

bool ApiSystem::setTimezone(std::string tz)
{
	if (tz.empty())
		return false;
	return executeScript("batocera-timezone set \"" + tz + "\"");
}

std::vector<PadInfo> ApiSystem::getPadsInfo()
{
	LOG(LogInfo) << "ApiSystem::getPadsInfo";

	std::vector<PadInfo> ret;

	auto res = executeEnumerationScript("batocera-padsinfo");
	std::string data = Utils::String::join(res, "\n");
	if (data.empty())
	{
		LOG(LogError) << "Package list is empty";
		return ret;
	}

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_string(data.c_str());
	if (!result)
	{
		LOG(LogError) << "Unable to parse packages";
		return ret;
	}

	pugi::xml_node root = doc.child("pads");
	if (!root)
	{
		LOG(LogError) << "Could not find <pads> node";
		return ret;
	}

	for (pugi::xml_node pad = root.child("pad"); pad; pad = pad.next_sibling("pad"))
	{
		PadInfo pi;

		if (pad.attribute("device"))
			pi.device = pad.attribute("device").as_string();

		if (pad.attribute("id"))
			pi.id = Utils::String::toInteger(pad.attribute("id").as_string());

		if (pad.attribute("name"))
			pi.name = pad.attribute("name").as_string();

		if (pad.attribute("battery"))
			pi.battery = Utils::String::toInteger(pad.attribute("battery").as_string());

		if (pad.attribute("status"))
			pi.status = pad.attribute("status").as_string();

		if (pad.attribute("path"))
			pi.path = pad.attribute("path").as_string();

		ret.push_back(pi);
	}

	return ret;
}

std::string ApiSystem::getRunningArchitecture()
{
	auto res = executeEnumerationScript("uname -m");
	if (res.size() > 0)
		return res[0];

	return "";
}

std::string ApiSystem::getRunningBoard()
{
	auto res = executeEnumerationScript("cat /boot/boot/batocera.board");
	if (res.size() > 0)
		return res[0];

	return "";
}

std::string ApiSystem::getHostsName()
{
	// 优先读取系统实际设置的主机名（/etc/hostname），
	// 没有才回退到 SystemConf 记录的值，最后才显示 "ARMBIAN"
	char buf[256] = { 0 };
	if (gethostname(buf, sizeof(buf) - 1) == 0 && buf[0] != '\0')
		return std::string(buf);

	auto hostName = SystemConf::getInstance()->get("system.hostname");
	if (!hostName.empty())
		return hostName;

	return "ARMBIAN";
}

bool ApiSystem::emuKill()
{
	LOG(LogDebug) << "ApiSystem::emuKill";
	return executeScript("batocera-es-swissknife --emukill");
}

void ApiSystem::suspend()
{
	LOG(LogDebug) << "ApiSystem::suspend";
	executeScript("/usr/bin/batocera-shutdown gui");
}

void ApiSystem::replugControllers_sindenguns()
{
	LOG(LogDebug) << "ApiSystem::replugControllers_sindenguns";
	executeScript("/usr/bin/virtual-sindenlightgun-remap");
}

void ApiSystem::replugControllers_wiimotes()
{
	LOG(LogDebug) << "ApiSystem::replugControllers_wiimotes";
	executeScript("/usr/bin/virtual-wii-mouse-bar-remap");
}

void ApiSystem::replugControllers_steamdeckguns()
{
	LOG(LogDebug) << "ApiSystem::replugControllers_steamdeckguns";
	executeScript("/usr/bin/steamdeckgun-remap");
}

bool ApiSystem::isPlaneMode()
{
	auto res = executeEnumerationScript("batocera-planemode status");
	if (res.size() > 0)
		return res[0] == "on";

	return false;
}

bool ApiSystem::isReadPlaneModeSupported()
{
	return isScriptingSupported(READPLANEMODE);
}

bool ApiSystem::setPlaneMode(bool enable)
{
	LOG(LogDebug) << "ApiSystem::setPlaneMode";
	return executeScript("batocera-planemode " + std::string(enable ? "enable" : "disable"));
}

std::vector<Service> ApiSystem::getServices()
{
	std::vector<Service> services;

	LOG(LogDebug) << "ApiSystem::getServices";

	auto slines = executeEnumerationScript("batocera-services list");

	for (auto sline : slines) 
	{
		auto splits = Utils::String::split(sline, ';', true);
		if (splits.size() == 2) 
		{
			Service s;
			s.name = splits[0];
			s.enabled = (splits[1] == "*");
			services.push_back(s);
		}
	}
	return services;
}

std::vector<std::string> ApiSystem::backglassThemes() {
  std::vector<std::string> themes;

  LOG(LogDebug) << "ApiSystem::backglassThemes";

  auto slines = executeEnumerationScript("batocera-backglass list-themes");

  for (auto sline : slines) 
    {
      themes.push_back(sline);
    }
  return themes;
}

void ApiSystem::restartBackglass() {
  LOG(LogDebug) << "ApiSystem::restartBackglass";
  executeScript("/usr/bin/batocera-backglass restart");
}

bool ApiSystem::enableService(std::string name, bool enable) 
{
	std::string serviceName = name;
	if (serviceName.find(" ") != std::string::npos)
		serviceName = "\"" + serviceName + "\"";

	LOG(LogDebug) << "ApiSystem::enableService " << serviceName;

	bool res = executeScript("batocera-services " + std::string(enable ? "enable" : "disable") + " " + serviceName);
	if (res)
		res = executeScript("batocera-services " + std::string(enable ? "start" : "stop") + " " + serviceName);
	
	return res;
}
