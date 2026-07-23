#include "guis/GuiMenu.h"

#include "components/OptionListComponent.h"
#include "components/SliderComponent.h"
#include "components/SwitchComponent.h"
#include "guis/GuiCollectionSystemsOptions.h"
#include "guis/GuiDetectDevice.h"
#include "guis/GuiGeneralScreensaverOptions.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiScraperStart.h"
#include "guis/GuiHashStart.h"
#include "guis/GuiThemeInstaller.h"
#include "guis/GuiBatoceraStore.h"
#include "guis/GuiSettings.h"
#include "guis/GuiRetroAchievements.h"
#include "guis/GuiGamelistOptions.h"
#include "guis/GuiImageViewer.h"
#include "guis/GuiNetPlaySettings.h"
#include "guis/GuiRetroAchievementsSettings.h"
#include "guis/GuiSystemInformation.h"
#include "guis/GuiControllersSettings.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "EmulationStation.h"
#include "Scripting.h"
#include "SystemData.h"
#include "VolumeControl.h"
#include <SDL_events.h>
#include <algorithm>
#include "utils/Platform.h"

#include "SystemConf.h"
#include "ApiSystem.h"
#include "InputManager.h"
#include "AudioManager.h"
#include "FavoriteMusicManager.h"
#include "guis/GuiFavoriteMusicSelector.h"

#include <LibretroRatio.h>
#include "guis/GuiUpdate.h"
#include "Es4allUpdate.h"
#include "guis/GuiInstallStart.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "guis/GuiBackupStart.h"
#include "guis/GuiTextEditPopup.h"
#include "guis/GuiWifi.h"
#include "guis/GuiBluetoothPair.h"
#include "guis/GuiBluetoothDevices.h"
#include "scrapers/ThreadedScraper.h"
#include "FileSorts.h"
#include "ThreadedHasher.h"
#include "ThreadedBluetooth.h"
#include "views/gamelist/IGameListView.h"
#include "components/MultiLineMenuEntry.h"
#include "components/BatteryIndicatorComponent.h"
#include "GuiLoading.h"
#include "guis/GuiBios.h"
#include "guis/GuiKeyMappingEditor.h"
#include "Gamelist.h"
#include "TextToSpeech.h"
#include "Paths.h"
#include "resources/ResourceManager.h"
#include <set>

#if WIN32
#include "Win32ApiSystem.h"
#endif

// es4all: 页脚显示的「建置时间」，来源是 CI 写进 resources/build-info.txt 的一行 UTC 时间。
// ★为什么用独立档、不用编进 binary 的 __DATE__/__TIME__★：
//   自我更新的构建指纹要求「相同源码编出相同 binary」(见 EmulationStation.h / Es4allUpdate)，
//   __DATE__/__TIME__ 会让每次编译的 binary 都不同 -> md5 不可重现 -> 隔离方案破功。
//   改用独立档：CI 把时间写进 resources/build-info.txt，随 resources 一起部署；ES 读它显示。
//   而 CI 算整包指纹时**排除 build-info.txt**，所以时间戳变化既不影响 binary、也不误触发更新。
// 无此档(旧固件/本地编译)时回空字串，页脚就只显示版本号。static 缓存只读一次。
static std::string es4allBuildInfo()
{
	static std::string info;
	static bool done = false;
	if (done) return info;
	done = true;
	std::string p = ResourceManager::getInstance()->getResourcePath(":/build-info.txt");
	if (Utils::FileSystem::exists(p))
		info = Utils::String::trim(Utils::FileSystem::readAllText(p));
	return info;
}

#define fake_gettext_fade _("fade")
#define fake_gettext_fastfade _("fast fade")
#define fake_gettext_slide _("slide")
#define fake_gettext_fastslide _("fast slide")
#define fake_gettext_fadeslide _("fade & slide")
#define fake_gettext_instant _("instant")

#define fake_gettext_system       _("System")
#define fake_gettext_architecture _("Architecture")
#define fake_gettext_diskformat   _("Disk format")
#define fake_gettext_temperature  _("Temperature")
#define fake_gettext_avail_memory _("Available memory")
#define fake_gettext_battery      _("Battery")
#define fake_gettext_model        _("Model")
#define fake_gettext_cpu_model    _("Cpu model")
#define fake_gettext_cpu_number   _("Cpu number")
#define fake_gettext_cpu_frequency _("Cpu max frequency")
#define fake_gettext_cpu_feature  _("Cpu feature")

#define fake_gettext_available_memory               _("Available Memory")
#define fake_gettext_display_resolution             _("Display Resolution")
#define fake_gettext_display_refresh_rate           _("Display Refresh Rate")
#define fake_gettext_opengl_driver_version          _("OpenGL Driver Version")
#define fake_gettext_vulkan_driver_name             _("Vulkan Driver Name")
#define fake_gettext_vulkan_driver_version          _("Vulkan Driver Version")
#define fake_gettext_data_partition_format          _("Data Partition Format")
#define fake_gettext_data_partition_available_space _("Data Partition Available Space")
#define fake_gettext_network_ip_address             _("Network IP Address")
#define fake_gettext_uefi_boot                      _("UEFI Boot")
#define fake_gettext_secure_boot                    _("Secure Boot")

#define fake_gettext_simple_bilinear_simple	pgettext("game_options", "SHARP-BILINEAR-SIMPLE")
#define fake_gettext_scanlines				pgettext("game_options", "SCANLINES")
#define fake_gettext_retro					pgettext("game_options", "RETRO")
#define fake_gettext_enhanced				pgettext("game_options", "ENHANCED")
#define fake_gettext_curvature				pgettext("game_options", "CURVATURE")
#define fake_gettext_zfast					pgettext("game_options", "ZFAST")
#define fake_gettext_flatten_glow			pgettext("game_options", "FLATTEN-GLOW")
#define fake_gettext_rgascaling				pgettext("game_options", "RGA SCALING")

#define fake_gettext_glapi			_("GRAPHICS API")
#define fake_gettext_glvendor		_("VENDOR")
#define fake_gettext_glvrenderer	_("RENDERER")
#define fake_gettext_glversion		_("VERSION")
#define fake_gettext_glslversion	_("SHADERS")

#define fake_gettext_disk_internal _("INTERNAL")
#define fake_gettext_disk_external _("ANY EXTERNAL")

#define fake_gettext_resolution_max_1K  _("maximum 1920x1080")
#define fake_gettext_resolution_max_640 _("maximum 640x480")

#ifdef _ENABLEEMUELEC

static std::vector<std::string> explode(std::string sData, char delimeter=',')
{
	std::vector<std::string> arr;	
	std::stringstream ssData(sData);
	std::string datum;
	while(std::getline(ssData, datum, delimeter))
	{
			arr.push_back(datum);
	}
	return arr;
}

static std::vector<int> int_explode(std::string sData, char delimeter=',')
{
	std::vector<int> arr;	
	std::stringstream ssData(sData);
	std::string datum;
	while(std::getline(ssData, datum, delimeter))
	{
			arr.push_back( atoi(datum.c_str()));
	}
	return arr;
}

static std::string toupper(std::string s)
{
	std::for_each(s.begin(), s.end(), [](char & c){
	    c = ::toupper(c);
	});	
	return s;
}

int* getVideoModeDimensions(std::string videomode, std::vector<std::string> reslist) 
{
	static int screen[2];

	if (videomode == "480cvbs")
	{
		screen[0] = 720;
		screen[1] = 480;
		return screen;
  }
	else if (videomode == "576cvbs")
	{
		screen[0] = 720;
		screen[1] = 576;
		return screen;
  }
	
	int pos = videomode.find('x');
	std::string tmp = videomode;

	if (pos >= 0)
	{
		screen[0] = atoi(videomode.substr(0, pos).c_str());
		tmp = videomode.substr(pos+1);
	}
		
	pos = tmp.find('p');
	if (pos < 0)
		pos = tmp.find('i');
	if (pos >= 0)
	{
		screen[1] = atoi(tmp.substr(0, pos).c_str());					
	}

	if (screen[0] == 0) {
		for (auto it = reslist.cbegin(); it != reslist.cend(); it++) {
			int pos = (*it).find(" "+std::to_string(screen[1]));
			if (pos >= 0) {
				screen[0] = atoi((*it).substr(0,pos).c_str());
				break;
			}
		}
	}
	return screen;
}

#endif

GuiMenu::GuiMenu(Window *window, bool animate) : GuiComponent(window), mMenu(window, _("MAIN MENU").c_str()), mVersion(window)
{
	// MAIN MENU
	bool isFullUI = !UIModeController::getInstance()->isUIModeKid() && !UIModeController::getInstance()->isUIModeKiosk();
#ifdef _ENABLEEMUELEC
	bool isKidUI = UIModeController::getInstance()->isUIModeKid();
#endif

	// KODI >
	// GAMES SETTINGS >
	// CONTROLLER & BLUETOOTH >
	// USER INTERFACE SETTINGS >
	// SOUND SETTINGS >
	// NETWORK >
	// SCRAPER >
	// SYSTEM SETTINGS >
	// QUIT >

	// KODI
#ifdef _ENABLE_KODI_
	if (SystemConf::getInstance()->getBool("kodi.enabled", true) && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::KODI))
		addEntry(_("KODI MEDIA CENTER").c_str(), false, [this] 
	{ 
		Window *window = mWindow;
		delete this;
		if (!ApiSystem::getInstance()->launchKodi(window))
			LOG(LogWarning) << "Shutdown terminated with non-zero result!";

	}, "iconKodi");	
#endif

	// es4all: 「平台设置」单一入口。
	// 此前 _ENABLEEMUELEC 与 ES4ALL_TARGET_ROCKNIX 两块各自 addEntry，而三个 target 都带
	// -DENABLE_EMUELEC=1，导致 ROCKNIX 上出现【两个同名的「平台设置」】(翻译后完全一样，
	// 用户无从分辨)，且内容重叠(RETROARCH MENU / RA BEZELS / LOG LEVEL / EXTERNAL MOUNT)。
	// 改为互斥：ROCKNIX 走自己的 openPlatformSettings()(只含在 ROCKNIX 上真生效的项)，
	// 其余走 openEmuELECSettings()。后续将两者合并为单一全集函数(共用项 + CAP 门控的专属项)。
	if (isFullUI)
	{
#if defined(ES4ALL_TARGET_ROCKNIX)
		// ROCKNIX 主题没有 iconEmuelec，用主题实际存在的 iconAdvanced（否则图标空缺）
		addEntry(_("PLATFORM SETTINGS").c_str(), true, [this] { openPlatformSettings(); }, "iconAdvanced");
#elif defined(_ENABLEEMUELEC)
		addEntry(_("PLATFORM SETTINGS").c_str(), true, [this] { openEmuELECSettings(); }, "iconEmuelec");
#endif
	}

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RETROACHIVEMENTS) &&
		SystemConf::getInstance()->getBool("global.retroachievements") &&
		Settings::getInstance()->getBool("RetroachievementsMenuitem") && 
		SystemConf::getInstance()->get("global.retroachievements.username") != "")
		addEntry(_("RETROACHIEVEMENTS").c_str(), true, [this] {
				if (!checkNetwork())
					return;
				GuiRetroAchievements::show(mWindow); }, "iconRetroachievements");
	
	if (isFullUI)
	{
#if BATOCERA
		addEntry(_("GAME SETTINGS").c_str(), true, [this] { openGamesSettings(); }, "iconGames");
		addEntry(GuiControllersSettings::getControllersSettingsLabel(), true, [window] { GuiControllersSettings::openControllersSettings(window); }, "iconControllers");
		addEntry(_("USER INTERFACE SETTINGS").c_str(), true, [this] { openUISettings(); }, "iconUI");
		addEntry(_("GAME COLLECTION SETTINGS").c_str(), true, [this] { openCollectionSystemSettings(); }, "iconAdvanced");
		addEntry(_("SOUND SETTINGS").c_str(), true, [this] { openSoundSettings(); }, "iconSound");

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::WIFI))
			addEntry(_("NETWORK SETTINGS").c_str(), true, [this] { openNetworkSettings(); }, "iconNetwork");
#else
		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::GAMESETTINGS))
			addEntry(_("GAME SETTINGS").c_str(), true, [this] { openGamesSettings(); }, "iconGames");

		addEntry(_("USER INTERFACE SETTINGS").c_str(), true, [this] { openUISettings(); }, "iconUI");

		// es4all: 原本 GAMESETTINGS 为假时改显示「配置控制器」(CONFIGURE INPUT)，但
		// GAMESETTINGS 在 isScriptingSupported() 的 switch 中无 case → 恒真，该分支永不触发。
		addEntry(GuiControllersSettings::getControllersSettingsLabel(), true, [window] { GuiControllersSettings::openControllersSettings(window); }, "iconControllers");

		addEntry(_("SOUND SETTINGS").c_str(), true, [this] { openSoundSettings(); }, "iconSound");

// es4all: ROCKNIX 也显示 NETWORK SETTINGS（openNetworkSettings 函数本身未 guard、已可编；
// isScriptingSupported(WIFI) 在 ROCKNIX=检查 /usr/bin/wifictl 存在，为真）。
#if defined(_ENABLEEMUELEC) || defined(ES4ALL_TARGET_ROCKNIX)
if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::WIFI))
			addEntry(_("NETWORK SETTINGS").c_str(), true, [this] { openNetworkSettings(); }, "iconNetwork");
#endif

		addEntry(_("GAME COLLECTION SETTINGS").c_str(), true, [this] { openCollectionSystemSettings(); }, "iconAdvanced");

		// es4all: 原本 GAMESETTINGS 为假时显示「模拟器设置」一级入口，但 GAMESETTINGS 恒真
		// → 永不触发。逐系统挑 emulator/core 的功能由
		// 游戏设置 → 每个系统的高级设置 → popSystemConfigurationGui() 提供，
		// 故一并移除该入口及其孤儿函数 openEmulatorSettings()/openSystemEmulatorSettings()。
#endif

		addEntry(_("SCRAPER").c_str(), true, [this] { openScraperSettings(); }, "iconScraper");		

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::BATOCERASTORE) || ApiSystem::getInstance()->isScriptingSupported(ApiSystem::THEMESDOWNLOADER) ||
			(ApiSystem::getInstance()->isScriptingSupported(ApiSystem::THEBEZELPROJECT) && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::DECORATIONS)) ||
			ApiSystem::getInstance()->isScriptingSupported(ApiSystem::UPGRADE))
			addEntry(_("UPDATES & DOWNLOADS"), true, [this] { openUpdatesSettings(); }, "iconUpdates");

		addEntry(_("SYSTEM SETTINGS").c_str(), true, [this] { openSystemSettings(); }, "iconSystem");
	}
	// es4all: 原本 else 分支(受限模式下只显示「信息」+「解锁用户界面模式」)已移除 ——
	// Kid/Kiosk 已废除, isFullUI 恒真, 该分支不可达。

#ifdef WIN32
	addEntry(_("QUIT"), !Settings::getInstance()->getBool("ShowOnlyExit") || !Settings::getInstance()->getBool("ShowExit"), [this] { openQuitMenu(); }, "iconQuit");
#else
#ifdef _ENABLEEMUELEC
if (!isKidUI)
#endif
	addEntry(_("QUIT").c_str(), true, [this] { openQuitMenu(); }, "iconQuit");
#endif
	
	addChild(&mMenu);
	addVersionInfo();
	setSize(mMenu.getSize());

	if (animate)
	{
		if (Renderer::ScreenSettings::fullScreenMenus())
			animateTo(Vector2f((Renderer::getScreenWidth() - getSize().x()) / 2, (Renderer::getScreenHeight() - getSize().y()) / 2));
		else
			animateTo(Vector2f((Renderer::getScreenWidth() - mSize.x()) / 2, Renderer::getScreenHeight() * 0.15f));
	}
	else
	{
		if (Renderer::ScreenSettings::fullScreenMenus())
			setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, (Renderer::getScreenHeight() - mSize.y()) / 2);
		else
			setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, Renderer::getScreenHeight() * 0.15f);
	}
}
#ifdef _ENABLEEMUELEC

std::shared_ptr<OptionListComponent<std::string>> GuiMenu::createSplashLoadingOptionList(Window *window)
{
	auto emuelec_splash_loading_mode = std::make_shared< OptionListComponent<std::string> >(window, _("LOADING SPLASH OPTION"), false);
	std::vector<std::string> splashmode;
	splashmode.push_back(_("SHOW DEFAULT SPLASH")); // 0
	splashmode.push_back(_("SHOW CUSTOM SPLASH")); // 1
	splashmode.push_back(_("SHOW RANDOM SPLASH")); // 2
	splashmode.push_back(_("USE SCRAPED MEDIA")); // 3

	std::string str_index = SystemConf::getInstance()->get("ee_splashloading");
	int index = 0;
	if (!str_index.empty())
		index = atoi(str_index.c_str());
	if (index > 3)
		index = 0;

	int i=0;
	for (auto it = splashmode.cbegin(); it != splashmode.cend(); it++) {
		emuelec_splash_loading_mode->add(*it, std::to_string(i), index == i);
		i++;
	}

	return emuelec_splash_loading_mode;
}

std::shared_ptr<OptionListComponent<std::string>> GuiMenu::createSplashExitOptionList(Window *window)
{
	auto emuelec_splash_exit_mode = std::make_shared< OptionListComponent<std::string> >(window, _("EXIT SPLASH OPTION"), false);
	std::vector<std::string> splashmode;
	splashmode.push_back(_("SHOW DEFAULT SPLASH")); // 0
	splashmode.push_back(_("PLAY CUSTOM SPLASH")); // 1

	std::string str_index = SystemConf::getInstance()->get("ee_splashexit");
	int index = 0;
	if (!str_index.empty())
		index = atoi(str_index.c_str());
	if (index > 1)
		index = 0;

	int i=0;
	for (auto it = splashmode.cbegin(); it != splashmode.cend(); it++) {
		emuelec_splash_exit_mode->add(*it, std::to_string(i), index == i);
		i++;
	}

	return emuelec_splash_exit_mode;
}
#endif // 结束外层 `#ifdef _ENABLEEMUELEC`(338 行起)—— 该函式已移到 ApiSystem::parseAudioOutputs

// es4all: audio_outputs.cfg 的解析已移到 `ApiSystem::parseAudioOutputs()`。
//   原本是本档的 file-static，但**开机还原(main.cpp)也需要同一份逻辑**(EMUELEC 没有发行版
//   消费脚本，音源路由得由 ES 在启动时重新套用)，放 ApiSystem 才两边都取得到、不必复制一份。
//   顺带解决了旧守卫的麻烦：ApiSystem 三个 target 都会编，不再需要
//   `_ENABLEEMUELEC || ES4ALL_TARGET_ROCKNIX` 这种「定义漏在守卫内」的写法
//   (那类 bug 已出现过三次：GuiFileBrowser::AUDIO 11e8bc9、splash 白底 2f0b611、本函数 af15bca)。

#ifdef _ENABLEEMUELEC   // es4all: 重开外层守卫，下面 openEmuELECSettings 等仍是纯 EmuELEC
/* < emuelec */
void GuiMenu::openEmuELECSettings()
{
	auto s = new GuiSettings(mWindow, _("PLATFORM SETTINGS"));

	// es4all: ★VIDEO MODE 已移除（1.1 收敛决定）★
	//   透过采集卡无法可靠验证(卡不重新协商输入时序 -> 黑屏/角落/撕裂, 换一张卡坏法还不同),
	//   而效益偏低, 故 1.1 正式版三个 target 一律不出这个选项, 列为待办。
	//   ★后端与开机套用【保留】★: ApiSystem::applyEmuelecVideoMode() 仍会在开机时依
	//   ee_videomode 套用(含解开内核 display/debug 锁与 PHY 还原, 见该函式), 且防呆
	//   (试用未确认即还原)也还在 —— 所以既有设定值不会失效, 将来接回选单即可。

	// es4all: BUTTON LED COLOR(bl_rgb) / STATUS LED(gf_statusled) 已删除 ——
	// 两者包在 #ifdef _ENABLEGAMEFORCE 内，而 CI 三个 job 与 rocknix/emuelec 的 package.mk
	// 均未定义 ENABLE_GAMEFORCE，属彻底死码；且为 GameForce 掌机专属硬件功能。
	// es4all: AUDIO DEVICE 已从菜单隐藏(用户要求)。原因: 选项是写死的 ALSA card,device
	// (auto/0,0/0,1/1,0/1,1/0,2/1,2), 语义隐晦无法友善化 —— 连 aplay -l 给的都是
	// SPDIF-B-dummy / TDM-B-T9015 之类的隐晦名, 且各机型编号不同、HDMI 音频根本不在列表里。
	// 系统设置 → AUDIO OUTPUT 才是有抽象层的正牌入口, 但它 gate 在 batocera-audio(EMUELEC
	// 无此命令 → 该项在 EMUELEC 也不显示)。即隐藏本项后 EMUELEC 暂无音频切换入口; 若需要,
	// 后续应给 EMUELEC 补 batocera-audio 垫片让 AUDIO OUTPUT 生效, 而非留着这个隐晦项。
	// 代码保留(#if 0), 供日后加友善标签或补垫片时复用。
#if 0 && defined(ES4ALL_CAP_EMUELEC_PLATFORM) && !defined(_ENABLEGAMEFORCE) && !defined(ODROIDGOA)
		// es4all: 标题用 _() 才会翻译(原为裸字符串, 弹窗标题一直显示英文 AUDIO DEVICE)。
		auto emuelec_audiodev_def = std::make_shared< OptionListComponent<std::string> >(mWindow, _("AUDIO DEVICE"), false);
		std::vector<std::string> Audiodevices;
		Audiodevices.push_back("auto");
		Audiodevices.push_back("0,0");
		Audiodevices.push_back("0,1");
		Audiodevices.push_back("1,0");
		Audiodevices.push_back("1,1");
		Audiodevices.push_back("0,2");
		Audiodevices.push_back("1,2");
		
		auto AudiodevicesS = SystemConf::getInstance()->get("ee_audio_device");
		if (AudiodevicesS.empty())
		AudiodevicesS = "auto";
		
		// es4all: 值维持 ALSA card,device(底层 emuelec-utils setauddev 要吃), 仅标签友善化:
		// auto → 翻译为「自动」; card,device 要映射成 HDMI/耳机需按机型硬件(见后续建议)。
		for (auto it = Audiodevices.cbegin(); it != Audiodevices.cend(); it++)
		emuelec_audiodev_def->add(*it == "auto" ? _("AUTO") : *it, *it, AudiodevicesS == *it);
		
		s->addWithDescription(_("AUDIO DEVICE"), _("Changes will need an EmulationStation restart."), emuelec_audiodev_def);
        
        emuelec_audiodev_def->setSelectedChangedCallback([emuelec_audiodev_def](std::string name) { 
            if (SystemConf::getInstance()->set("ee_audio_device", name)) 
                SystemConf::getInstance()->saveSystemConf();
                std::string selectedaudio = emuelec_audiodev_def->getSelected();
                Utils::Platform::ProcessStartInfo("/usr/bin/emuelec-utils setauddev " +selectedaudio).run();
            });
#endif

// es4all: ★AUDIO OUTPUT 放宽给 ARMBIAN★(1.1 收敛: 三个 target 都要有这个选项)。
//   原本关在 ES4ALL_CAP_EMUELEC_PLATFORM 里, 而该旗标只在 target=emuelec 时定义 ——
//   于是 ARMBIAN 虽然走同一个 openEmuELECSettings(CI 给它带了 -DENABLE_EMUELEC=1),
//   却看不到这一项。选单本身是通用的(读 audio_outputs.cfg), 只有【套用的后端】各家不同:
//     EMUELEC = emuelec-utils setauddev + HDMI 硬件静音(同一张卡两个 device, 会一起出声)
//     ARMBIAN = 直接改写 /etc/asound.conf(裸 ALSA, 无 PipeWire/PulseAudio)
//   (ROCKNIX 有自己的 openPlatformSettings, 走 es4all-setauddev/PipeWire, 不在这块。)
#if defined(ES4ALL_CAP_EMUELEC_PLATFORM) || defined(ES4ALL_TARGET_ARMBIAN)
	// es4all: AUDIO OUTPUT —— 按机型只列该盒【实际有实体孔】的音源输出。
	//
	// 为什么要机型白名单(不纯动态): card,device 各机型编号不同, 且 ALSA 列出某 PCM ≠ 盒子有那个
	// 实体孔(Amlogic SoC 都支持 SPDIF/模拟, 但廉价盒常没接线)。故需实机 aplay -l + 逐一实听确认。
	// 映射表放 resources/audio_outputs.cfg(数据文件, 非代码): 加机型只改一行、随 OTA 下发, 见文件头。
	{
		// es4all: 机型->音源映射来自 resources/audio_outputs.cfg(数据文件, 非代码)，
		// 解析逻辑在 ApiSystem::parseAudioOutputs() 与 ROCKNIX / 开机还原共用。
		// 不在表内的机型: outs 为空 -> 下面 if 不成立 -> 不显示本选单(保持出厂默认音源, 最安全)。
		std::vector<std::pair<std::string, std::string>> outs = ApiSystem::getInstance()->parseAudioOutputs();

		if (!outs.empty())
		{
			auto audioout = std::make_shared< OptionListComponent<std::string> >(mWindow, _("AUDIO OUTPUT"), false);
			std::string cur = SystemConf::getInstance()->get("ee_audio_device");
			if (cur.empty() || cur == "auto")
				cur = outs.front().second;   // 未设时按白名单首项(HDMI)显示为已选

			for (auto& o : outs)
				audioout->add(_(o.first.c_str()), o.second, cur == o.second);
			s->addWithDescription(_("AUDIO OUTPUT"), _("Changes will need an EmulationStation restart."), audioout);
			audioout->setSelectedChangedCallback([audioout](std::string dev) {
				if (SystemConf::getInstance()->set("ee_audio_device", dev))
					SystemConf::getInstance()->saveSystemConf();
#if defined(ES4ALL_TARGET_ARMBIAN)
				// ARMBIAN: 裸 ALSA -> 直接改写 /etc/asound.conf 的默认装置。
				ApiSystem::getInstance()->applyArmbianAudioOutput(dev);
#else
				// es4all: 走 applyEmuelecAudioOutput 而不是直接呼叫 setauddev ——
				// setauddev 只改 asound.conf 的默认 PCM，不动硬件路由，会造成
				// 「选了 AV，HDMI 还在同时出声」(实机 .165 确认)。见该函式说明。
				ApiSystem::getInstance()->applyEmuelecAudioOutput(dev);
#endif
			});
		}
	}
#endif

	// es4all: START AT BOOT 为 EmuELEC 专属 —— 只写 ee_boot 这个键，
	// 实际生效靠 EmuELEC 的开机脚本读取；armbian/rocknix 不读它 → 选了没作用。
#if defined(ES4ALL_CAP_EMUELEC_PLATFORM)
		auto emuelec_boot_def = std::make_shared< OptionListComponent<std::string> >(mWindow, _("START AT BOOT"), false);
		std::vector<std::string> devices;
		devices.push_back("Emulationstation");
		devices.push_back("Retroarch");
		for (auto it = devices.cbegin(); it != devices.cend(); it++)
		emuelec_boot_def->add(_(it->c_str()), *it, SystemConf::getInstance()->get("ee_boot") == *it);
		s->addWithLabel(_("START AT BOOT"), emuelec_boot_def);
		s->addSaveFunc([emuelec_boot_def] {
			if (emuelec_boot_def->changed()) {
				std::string selectedBootMode = emuelec_boot_def->getSelected();
				SystemConf::getInstance()->set("ee_boot", selectedBootMode);
				SystemConf::getInstance()->saveSystemConf();
			}
		});
#endif

#if defined(ES4ALL_TARGET_EMUELEC) || defined(ES4ALL_TARGET_ARMBIAN)
	// es4all: CPU 调速器(governor)。移植自 ROCKNIX 平台设置。EMUELEC/ARMBIAN 都无发行版消费脚本
	// (ROCKNIX 靠 autostart/runemu.sh 读 system.cpugovernor 套用), 故由 ES 直接写 sysfs
	// (ApiSystem::setCpuGovernor)即时生效 + 开机重套(main.cpp)。列表动态读
	// scaling_available_governors, 只列内核实际支持的。
	//
	// es4all: ★ARMBIAN 多一道可写性检查★ —— sysfs 的 scaling_governor 是 root:root 0644，
	//   而 ARMBIAN 的 es4all.service 写着 User=game，写不进去时 setCpuGovernor 会静默跳过，
	//   选单就成了「点了没反应」的空壳。isCpuGovernorSettable() 探不到可写就整项不出。
	//   (EMUELEC/ROCKNIX 的 ES 以 root 跑，恒为真，行为不变。)
	{
		auto govs = ApiSystem::getInstance()->getAvailableCpuGovernors();
		if (!govs.empty() && ApiSystem::getInstance()->isCpuGovernorSettable())
		{
			auto cpugov = std::make_shared< OptionListComponent<std::string> >(mWindow, _("CPU GOVERNOR"), false);
			std::string curGov = SystemConf::getInstance()->get("system.cpugovernor");
			cpugov->add(_("AUTO"), "", curGov.empty());
			// es4all: 值保持内核 governor 名(小写, 写 sysfs 用); 显示用大写名过 _() 翻译
			// (ONDEMAND/PERFORMANCE/... 已加 zh_CN/zh_TW; 未翻的 governor 回退显示大写原名)。
			//
			// es4all: ★按性能由强到弱排序，最省电的排最下面(2026-07-23 用户要求，比照 R 版)★
			//   内核回报的顺序是字母序(conservative ondemand userspace powersave performance
			//   schedutil)，对使用者没有意义。这里给一份优先序，表内的照表排、表外的接在后面
			//   (不丢弃，别的板子可能有特有 governor)。
			// ★userspace 直接滤掉★: 它不是一种调速策略，而是「把频率交给使用者空间的程式决定」。
			//   没有搭配的守护程序时，选了等于把频率锁在当下那一档 —— 对使用者只有坏处，
			//   而且它没有翻译，画面上就是一串突兀的英文(实机 .198 截图可见)。
			static const char* kGovOrder[] = { "performance", "schedutil", "ondemand", "conservative", "powersave" };
			std::vector<std::string> ordered;
			for (auto* want : kGovOrder)
				for (auto& g : govs)
					if (g == want)
						ordered.push_back(g);
			for (auto& g : govs)
			{
				if (g == "userspace")
					continue;
				if (std::find(ordered.cbegin(), ordered.cend(), g) == ordered.cend())
					ordered.push_back(g);
			}
			for (auto& g : ordered)
				cpugov->add(_(Utils::String::toUpper(g).c_str()), g, curGov == g);
			s->addWithDescription(_("CPU GOVERNOR"), _("CPU frequency scaling policy."), cpugov);
			cpugov->setSelectedChangedCallback([cpugov](std::string val) {
				if (SystemConf::getInstance()->set("system.cpugovernor", val))
					SystemConf::getInstance()->saveSystemConf();
				ApiSystem::getInstance()->setCpuGovernor(val);
			});
		}
	}
#endif

#ifdef _ENABLEEMUELEC
		// es4all: ★RA 日志由布尔开关改成 RetroArch 自己的四级 + 关闭(2026-07-23 用户要求)★
		//   排列由轻到重: 关闭 → DEBUG(0) → INFO(1) → WARNING(2) → ERROR(3)，ERROR 在最下面。
		//   **预设关闭**。
		//
		// ★为什么级别另存一个键，而不是塞回 global.retroarchLogging★:
		//   E 的消费端 emuelecRunEmu.sh 只认该键的 "0"/非0，而 RA 的级别 0 正好是 DEBUG ——
		//   共用一个键的话，使用者选「DEBUG」会被那支脚本读成「关闭」，完全相反。
		//   故: global.retroarchLogging 维持 0/1 给 E 的脚本用，级别另存 global.retroarch.loglevel。
		auto ra_loglevel = std::make_shared< OptionListComponent<std::string> >(mWindow, _("RETROARCH LOGGING"), false);
		std::string raLevel = SystemConf::getInstance()->get("global.retroarch.loglevel");
		// 旧版只有布尔键时的迁移: 没有级别键就看旧的开关，开着就当 DEBUG(等同旧行为)。
		if (raLevel.empty())
			raLevel = (SystemConf::getInstance()->get("global.retroarchLogging") == "1") ? "0" : "off";
		ra_loglevel->addRange({ { _("OFF"), "off" }, { _("DEBUG"), "0" }, { _("INFO"), "1" },
		                        { _("WARNING"), "2" }, { _("ERROR"), "3" } }, raLevel);
		// es4all: 补上与 R 版同一句说明(2026-07-23) —— 原本 E/A 这项没有说明文字，
		// 使用者容易与开发者选项里的「ES 日志级别」(es_log.txt)搞混。三个 target 同标题同说明。
		s->addWithDescription(_("RETROARCH LOGGING"),
			_("RetroArch logs; enable when you need to debug."),
			ra_loglevel);
		s->addSaveFunc([ra_loglevel, raLevel] {
				const std::string sel = ra_loglevel->getSelected();
				const bool logging_enabled = (sel != "off");
				SystemConf::getInstance()->set("global.retroarch.loglevel", sel);
				SystemConf::getInstance()->set("global.retroarchLogging", logging_enabled ? "1" : "0");
				SystemConf::getInstance()->saveSystemConf();

				// es4all: ★必须比对旧值、只在真的改动时才写★
				//   本 addSaveFunc **没有 changed() 闸门**，选单一关就会无条件执行(原本只写个
				//   SystemConf 键，无所谓)。接上改写 retroarch.cfg 之后，光是「进平台设置看一眼
				//   再退出」就会把使用者自己在 RetroArch 里调过的 frontend_log_level 覆盖掉 ——
				//   实机 .198 上真的发生了(手动设的 "1" 被写成 "3")。故自行比对进选单时的初始值。
				if (sel != raLevel)
					ApiSystem::getInstance()->applyRetroarchLogging(sel);
			});
#endif

       // es4all: 此处原有一段注释掉的 ENABLE RA BEZELS(布尔版)已删除 ——
       // 该功能由 游戏设置 → DEFAULT GLOBAL SETTINGS 的三档版(AUTO/ON/OFF)统一提供。

       // es4all: ENABLE RA SPLASH(ee_splash.enabled = RetroArch 启动画面)曾在此处与
       // 「SPLASH SETTINGS」子菜单之间搬来搬去(子菜单内是 ES 自己的载入/退出动画
       // ee_splashloading/ee_splashexit/...，两者键不同)，**现已整项移除**(使用者要求)。

	// es4all: BOOT VIDEO 为 EmuELEC 专属 —— ee_bootvideo.enabled / ee_randombootvideo.enabled
	// 在 ES 内【没有任何读取者】，纯靠 EmuELEC 开机脚本读取，armbian/rocknix 上选了没作用。
	// (注意: SPLASH SETTINGS 与此不同，它是 ES 自己实作的跨平台功能，见下方，不可门控。)
#if defined(ES4ALL_CAP_EMUELEC_PLATFORM)

	// es4all: BOOT VIDEO —— 由原本两个互相干扰的 Switch 合并为单一三档 OptionList。
	// 原实现的问题:
	//   ALWAYS SHOW BOOT VIDEO  → ee_bootvideo.enabled
	//   RANDOMIZE BOOT VIDEO    → ee_randombootvideo.enabled，且开启时会偷偷把
	//                             ee_bootvideo.enabled 也设为 "1"，但上面那个 Switch 的
	//                             UI 状态不会跟着跳(要重进菜单才看得到)，用户会困惑。
	//   两键的四种组合里 (bootvideo=0, random=1) 是矛盾态，只能靠那行 if 硬修。
	// 三档把矛盾态从根本上消除；底层两个键不变，发行版脚本无须改动。
	auto bootvideo_mode = std::make_shared< OptionListComponent<std::string> >(mWindow, _("BOOT VIDEO"), false);
	bool bootEnabled = SystemConf::getInstance()->get("ee_bootvideo.enabled") == "1";
	bool randombootEnabled = SystemConf::getInstance()->get("ee_randombootvideo.enabled") == "1";
	std::string bootvideoCur = !bootEnabled ? "off" : (randombootEnabled ? "random" : "fixed");

	bootvideo_mode->add(_("OFF"), "off", bootvideoCur == "off");
	bootvideo_mode->add(_("FIXED"), "fixed", bootvideoCur == "fixed");
	bootvideo_mode->add(_("RANDOM"), "random", bootvideoCur == "random");
	s->addWithDescription(_("BOOT VIDEO"), _("Play a video at boot: a fixed one, or a random pick."), bootvideo_mode);

	s->addSaveFunc([bootvideo_mode] {
		if (!bootvideo_mode->changed())
			return;

		const std::string mode = bootvideo_mode->getSelected();
		SystemConf::getInstance()->set("ee_bootvideo.enabled", mode == "off" ? "0" : "1");
		SystemConf::getInstance()->set("ee_randombootvideo.enabled", mode == "random" ? "1" : "0");
		SystemConf::getInstance()->saveSystemConf();
	});

#endif // ES4ALL_CAP_EMUELEC_PLATFORM (BOOT VIDEO)

	// es4all: ★ENABLE RA SPLASH 已移除（使用者要求）★
	//   曾把它从第 3 层「启动画面设置」提到本层直接可见，现在整项拿掉 —— 平台设置只留真正常用的项。
	//   键 `ee_splash.enabled` 本身仍然有效(ES 内无读取者，由 EmuELEC 侧套用)，
	//   要改预设值请在 distro 的 `emuelec.conf` 设(交接单建议预设 0)，或直接编辑该档。
	//   ROCKNIX 侧本来就没有这一项，移除后两个 target 的平台设置更一致。

	// es4all: SPLASH SETTINGS —— 【跨平台，不可门控】。
	// 经查证其主体是 ES 自己实作的功能(本专案在 armbian 版上开发的):
	//   ENABLE LOADING SPLASH SCREEN → Settings "SplashScreen"
	//        → main.cpp / ViewController.cpp / Window.cpp 读取
	//   ENABLE EXIT SPLASH SCREEN    → Settings "SplashScreenExit" → main.cpp / Window.cpp
	//   自订图片                      → Settings "AlternateSplashScreen"
	//        → Window::getCustomSplashScreenImage()
	//   SPLASH EXIT DURATION         → SystemConf "ee_splash_exit_duration" → main.cpp
	// 仅其中少数项(ENABLE RA SPLASH / SPLASH LOADING OPTION 等 ee_splashloading 系列)
	// 在 ES 内无读取者、属 EmuELEC 侧，已在子菜单内各自门控。
	// 原「CONFIGURE SPLASH OPTIONS」改名为「SPLASH SETTINGS」，与子菜单标题一致。
	s->addEntry(_("SPLASH SETTINGS"), true, [this] {
		createConfigureSplash(mWindow);
	});


	// es4all: GAMEPAD CONFIG 入口已移除 —— 其内容实为 Wiimote 配对工具
	// (CONNECT WIIMOTE / ACTIVATE WIIMOTE WITH IR-SENSORBAR + ee_rumble_strength)，
	// 名称与「手柄设置」严重误导，且为 EmuELEC 专属。手柄相关设定统一在
	// 一级菜单的「手柄和蓝牙设置」(GuiControllersSettings)。

	// es4all: RETROARCH MENU(菜单样式, global.retroarch.menu_driver)已从平台设置移除 ——
	// 与「前端开发人员选项」里的 RETROARCH MENU DRIVER 是同一个键、重复。保留开发者选项那份
	// (面向进阶用户, 一般用户不易误触), 平台设置不再出现。

// es4all: ★ARMBIAN 不出「外部挂载选项」★(2026-07-23 实机 MD1000/Armbian 192.168.8.198 查证)
//   这整组功能是 EmuELEC 专属，只因 armbian 也带 -DENABLE_EMUELEC=1 才被顺带编进来，是个空壳:
//     ① 碟的列举写死 `find /var/media/ ...`，而 Armbian 上**没有 /var/media 这个目录** -> 恒为空，
//        选单永远只剩「自动 / 内部存储」两项；
//     ② 挂载后端 eemount / mount_romfs.sh 在 Armbian 上不存在；
//     ③ ROM 根是 /home/game/ROMs，不是 EmuELEC 的 /storage/roms，语义对不上。
//   而且该机**根本没有任何自动挂载机制**(udisks2/usbmount/pmount 都没装，fstab 也没写)，
//   插上的 USB 碟连挂都不会挂。给个点进去什么也做不到的选单只会误导，故直接不出。
//   ARMBIAN 的外接存储支持列为待办(见 [[es4all_features_todo]])，要做得连 es4all-1key 一起改。
#if !defined(ES4ALL_TARGET_ARMBIAN)
if (UIModeController::getInstance()->isUIModeFull())
	{
        //External Mount Options
        s->addEntry(_("EXTERNAL MOUNT OPTIONS"), true, [this] { openExternalMounts(mWindow, "global"); });
    }
#endif

    mWindow->pushGui(s);
}

#ifdef _ENABLEEMUELEC
void GuiMenu::createConfigureSplash(Window* mWindow, int menuIndex)
{
	auto s = new GuiSettings(mWindow, _("SPLASH SETTINGS"));

	s->setUpdateType(ComponentListFlags::UPDATE_ALWAYS);

	// es4all: ENABLE RA SPLASH(ee_splash.enabled)曾从本子菜单提到上一层「平台设置」，
	// **现已整项移除**(使用者要求)；键仍有效，要改预设值走 distro 的 emuelec.conf。
	// 本子菜单只保留 ES 自己的载入/退出动画各项。

		auto enable_splashscreen = std::make_shared<SwitchComponent>(mWindow);
	enable_splashscreen->setState(Settings::getInstance()->getBool("SplashScreen"));
	s->addWithLabel(_("ENABLE LOADING SPLASH SCREEN"), enable_splashscreen);
	s->addSaveFunc([enable_splashscreen] {
		Settings::getInstance()->setBool("SplashScreen", enable_splashscreen->getState());
	});
	enable_splashscreen->setOnChangedCallback([s, mWindow, enable_splashscreen]()
	{
		Settings::getInstance()->setBool("SplashScreen", enable_splashscreen->getState());
		// es4all: 这里**刻意不写档**。ES 的既定语意是「B(返回)=套用并存档、START(关闭)=取消不存档」
		// (GuiSettings::input: B 走 close()->save()->saveFile()；START 直接 delete 绕过 save)。
		// 曾在此补 saveFile() 让 START 也能存档，等于破坏了「取消」语意 —— 已撤销。
		int index = s->getMenu().getList()->getCursorIndex();
		delete s;
		createConfigureSplash(mWindow, index);
	});

	auto splashLoadingPlatformRoms = std::make_shared<SwitchComponent>(mWindow);
	splashLoadingPlatformRoms->setState(SystemConf::getInstance()->get("ee_splash_loading_platform_roms") != "0");

	auto splashLoadingTime = std::make_shared<SliderComponent>(mWindow, 0.f, 100.f, 1.f, "seconds");

	auto splashDuration = SystemConf::getInstance()->get("ee_splash_loading_duration");
	float fDuration = 0.f;
	if (!splashDuration.empty())
		fDuration = (float) atof(splashDuration.c_str());
	splashLoadingTime->setValue(fDuration);

	splashLoadingTime->setOnValueChanged([](const float &newVal) {
		auto val = std::to_string((int)Math::round(newVal));
		SystemConf::getInstance()->set("ee_splash_loading_duration", val);
	});

	auto splashLoadingOptionList = createSplashLoadingOptionList(mWindow);
	if (Settings::getInstance()->getBool("SplashScreen")) {
	s->addWithLabel(_("SPLASH LOADING OPTION"), splashLoadingOptionList);
	splashLoadingOptionList->setSelectedChangedCallback([=](std::string name) {
		SystemConf::getInstance()->set("ee_splashloading", name);
		delete s;
		createConfigureSplash(mWindow);
	});

	if (SystemConf::getInstance()->get("ee_splashloading") == "1") {
		// File picker for custom splash image
		s->addFileBrowser(_("SET CUSTOM SPLASH LOADING MEDIA FILE"), "ee_customsplash", (GuiFileBrowser::FileTypes) (GuiFileBrowser::FileTypes::IMAGES | GuiFileBrowser::FileTypes::VIDEO));
	}

	if (SystemConf::getInstance()->get("ee_splashloading") == "2") {
		// File picker for random splash media
		s->addFileBrowser(_("SET RANDOM SPLASH LOADING MEDIA FOLDER"), "ee_randomsplashpath", (GuiFileBrowser::FileTypes) (GuiFileBrowser::FileTypes::DIRECTORY));
	}

	if (SystemConf::getInstance()->get("ee_splashloading") == "3") {
		// options for gamelist.xml xml path scrape media
		auto emuelec_scrapepath_def = std::make_shared< OptionListComponent<std::string> >(mWindow, "SCRAPE XML PATH", false);
		std::vector<std::string> devices;
		devices.push_back("auto");
		devices.push_back("video");
		devices.push_back("image");
		devices.push_back("marquee");
		devices.push_back("fanart");
		devices.push_back("thumbnail");
		devices.push_back("random");
		for (auto it = devices.cbegin(); it != devices.cend(); it++)
		emuelec_scrapepath_def->add(*it, *it, SystemConf::getInstance()->get("ee_scrapedsplashpath") == *it);
		s->addWithLabel(_("SCRAPE XML PATH"), emuelec_scrapepath_def);
		s->addSaveFunc([emuelec_scrapepath_def] {
			if (emuelec_scrapepath_def->changed()) {
				std::string selectedScrapePath = emuelec_scrapepath_def->getSelected();
				SystemConf::getInstance()->set("ee_scrapedsplashpath", selectedScrapePath);
				SystemConf::getInstance()->saveSystemConf();
			}
		});
	}
	s->addWithLabel(_("SPLASH LOAD PLATFORMS AND ROMS"), splashLoadingPlatformRoms);
	s->addWithLabel(_("SPLASH LOADING DURATION"), splashLoadingTime);
	} // end if (SplashScreen)

	auto enable_splashscreen_exit = std::make_shared<SwitchComponent>(mWindow);
	enable_splashscreen_exit->setState(Settings::getInstance()->getBool("SplashScreenExit"));
	s->addWithLabel(_("ENABLE EXIT SPLASH SCREEN"), enable_splashscreen_exit);
	s->addSaveFunc([enable_splashscreen_exit] {
		Settings::getInstance()->setBool("SplashScreenExit", enable_splashscreen_exit->getState());
	});
	enable_splashscreen_exit->setOnChangedCallback([s, mWindow, enable_splashscreen_exit]()
	{
		Settings::getInstance()->setBool("SplashScreenExit", enable_splashscreen_exit->getState());
		int index = s->getMenu().getList()->getCursorIndex();
		delete s;
		createConfigureSplash(mWindow, index);
	});

	auto splashExitTime = std::make_shared<SliderComponent>(mWindow, 0.f, 100.f, 1.f, "seconds");

	splashDuration = SystemConf::getInstance()->get("ee_splash_exit_duration");
	fDuration = 0.f;
	if (!splashDuration.empty())
		fDuration = (float) atof(splashDuration.c_str());
	splashExitTime->setValue(fDuration);

	splashExitTime->setOnValueChanged([](const float &newVal) {
		auto val = std::to_string((int)Math::round(newVal));
		SystemConf::getInstance()->set("ee_splash_exit_duration", val);
	});

	auto splashExitOptionList = createSplashExitOptionList(mWindow);
	if (Settings::getInstance()->getBool("SplashScreenExit")) {
	s->addWithLabel(_("SPLASH EXIT OPTION"), splashExitOptionList);
	splashExitOptionList->setSelectedChangedCallback([=](std::string name) {
		SystemConf::getInstance()->set("ee_splashexit", name);
		int index = s->getMenu().getList()->getCursorIndex();
		delete s;
		createConfigureSplash(mWindow, index);
	});

	if (SystemConf::getInstance()->get("ee_splashexit") == "1") {
		// File picker for custom splash image/video
		s->addFileBrowser(_("SET CUSTOM SPLASH EXIT MEDIA FILE"), "ee_customexitsplash", (GuiFileBrowser::FileTypes) (GuiFileBrowser::FileTypes::IMAGES | GuiFileBrowser::FileTypes::VIDEO));
	}
	s->addWithLabel(_("SPLASH EXIT DURATION"), splashExitTime);
	} // end if (SplashScreenExit)

	s->addSaveFunc([=] {
		if (Settings::getInstance()->getBool("SplashScreen")) {
			if (splashLoadingOptionList->getSelected() == "2") {
				mWindow->displayNotificationMessage(_U("\uF011  ") + _("PUT RANDOM MEDIA IN '/storage/roms/splash/random'."));
			}
			SystemConf::getInstance()->set("ee_splashloading", splashLoadingOptionList->getSelected());
		} else {
			SystemConf::getInstance()->set("ee_splashloading", "4");
		}

		if (Settings::getInstance()->getBool("SplashScreenExit")) {
			SystemConf::getInstance()->set("ee_splashexit", splashExitOptionList->getSelected());
		} else {
			SystemConf::getInstance()->set("ee_splashexit", "2");
		}

		// es4all: \u79FB\u9664\u300C\u8BBE 0 \u4F1A\u64AD 3 \u79D2 / \u8BBE >0 \u4F1A\u53E0\u52A0\u65F6\u957F\u300D\u8FD9\u4E24\u6761\u63D0\u793A \u2014\u2014 \u6BCF\u6B21\u4ECE\u542F\u52A8\u753B\u9762\u8BBE\u7F6E\u8FD4\u56DE
		// \u90FD\u5FC5\u5F39\u5176\u4E2D\u4E00\u6761(\u503C\u4E0D\u662F 0 \u5C31\u662F\u975E 0), \u5C5E\u4E8E\u566A\u97F3\u3002\u89C4\u5219\u5DF2\u5728\u5404\u65F6\u957F\u9009\u9879\u7684\u8BF4\u660E\u91CC\u4EA4\u4EE3, \u65E0\u9700\u518D\u5F39\u901A\u77E5\u3002

		SystemConf::getInstance()->set("ee_splash_loading_duration", std::to_string((int)round(splashLoadingTime->getValue())));
		SystemConf::getInstance()->set("ee_splash_exit_duration", std::to_string((int)round(splashExitTime->getValue())));

		SystemConf::getInstance()->set("ee_splash_loading_platform_roms", splashLoadingPlatformRoms->getState() ? "1" : "0");

		SystemConf::getInstance()->saveSystemConf();
	});

	s->getMenu().getList()->setCursorIndex(menuIndex);
	mWindow->pushGui(s);	
}
#endif



void GuiMenu::openExternalMounts(Window* mWindow, std::string configName)
{

	GuiSettings* externalMounts = new GuiSettings(mWindow, _("EXTERNAL MOUNT OPTIONS").c_str());
    std::string a;
    
		auto emuelec_external_device_def = std::make_shared< OptionListComponent<std::string> >(mWindow, _("EXTERNAL DEVICE"), false);
		// es4all: ★global.externalmount 的真实语义(核对 eemount 源码 drive.c + mount_romfs.sh)★
		//   - 空字串        → 扫描任意「带标记档(roms/emuelecroms)」的外接碟, 无则用内部 /storage/roms。
		//                     ★这才是真正的「自动」★: 两个挂载后端(eemount / mount_romfs.sh)都只把【空】当自动。
		//   - "INTERNAL"    → 哨兵值, 没有任何 /var/media 下的碟叫这名字 → 扫描匹配不到 → 强制用内部
		//                     /storage/roms(=本机 eMMC 的 ROMS)。用于「插着外接碟也要切回本机」。
		//   - 具体 LABEL    → 只挂那颗。
		// 注意: ★绝不能存 "auto" 字串★ —— eemount(本机默认后端)没有 auto 特判, 会去找【名叫 auto 的碟】,
		//   找不到反而变纯内部, 且与 mount_romfs.sh 行为不一致。历史上的 "auto" 值在此迁移成空字串。
		const std::string kIntl = "INTERNAL";
		std::vector<std::string> devLabels;
		// es4all: ★排除系统盘自己的分区★ —— 原本只 prune 掉 EEROMS，但系统盘的其他分区
		// (EMUELEC=开机分区、STORAGE、CE_FLASH、CE_STORAGE)若也被挂到 /var/media/ 就会列出来，
		// 使用者误选当 ROM 盘就变 0 个游戏(实机遇到过)。这些 LABEL 一并 prune 掉。
		  for(std::stringstream ss(Utils::Platform::getShOutput(R"(find /var/media/ -mindepth 1 -maxdepth 1 -type d \( -iname EEROMS -o -iname EMUELEC -o -iname STORAGE -o -iname CE_FLASH -o -iname CE_STORAGE \) -prune -o -type d -exec basename {} \; | sed "s/$/,/g")")); getline(ss, a, ','); ) {
            if (!a.empty()) devLabels.push_back(a);
	    }

		auto extdevoptionsS = SystemConf::getInstance()->get("global.externalmount");
		if (extdevoptionsS == "auto")   // 迁移旧值: "auto" 其实等价于「自动」= 空字串。
			extdevoptionsS = "";

		// 自动(空值) + 本机(哨兵) + 各外接碟(LABEL)。前两项显示走 _() 中文化, LABEL 无法翻译。
		emuelec_external_device_def->add(_("AUTO"), "", extdevoptionsS.empty());
		emuelec_external_device_def->add(_("INTERNAL STORAGE"), kIntl, extdevoptionsS == kIntl);
		for (auto it = devLabels.cbegin(); it != devLabels.cend(); it++)
			emuelec_external_device_def->add(*it, *it, extdevoptionsS == *it);

        externalMounts->addWithDescription(_("EXTERNAL DEVICE"), _("Select where ROMS are read from: AUTO scans external drives, INTERNAL STORAGE forces the built-in storage, or pick a specific drive."), emuelec_external_device_def);

        emuelec_external_device_def->setSelectedChangedCallback([emuelec_external_device_def, kIntl](std::string name) {
       		if (SystemConf::getInstance()->set("global.externalmount", name)) {
			   // 只有选「具体外接碟」才需要打标记档(roms/emuelecroms), 让 eemount 认得它可当 ROM 盘。
			   // 自动(空)与本机(INTERNAL 哨兵)都不打标记。
			   const std::string sel = emuelec_external_device_def->getSelected();
			   if (!sel.empty() && sel != kIntl) {
                    std::string path = ("/var/media/" + sel + "/roms/emuelecroms").c_str();
                        if (!Utils::FileSystem::exists(path)) {
                            system((std::string("mkdir -p \"/var/media/") + sel + std::string("/roms\"")).c_str());
                            system((std::string("touch \"/var/media/") + sel + std::string("/roms/emuelecroms\"")).c_str());
                        }
                }
            SystemConf::getInstance()->saveSystemConf();
        }
        });
       
		auto emuelec_external_device_retry = std::make_shared< OptionListComponent<std::string> >(mWindow, _("RETRY TIMES"), false);
		emuelec_external_device_retry->addRange({ { _("AUTO"), "" },{ "1", "1" },{ "2", "2" },{ "3", "3" },{ "4", "4" },{ "5", "5" },{ "6", "6" },{ "7", "7" },{ "8", "8" },{ "9", "9" },{ "10", "10" },{ "11", "11" },{ "12", "12" },{ "13", "13" },{ "14", "14" },{ "15", "15" },{ "16", "16" },{ "17", "17" },{ "18", "18" },{ "19", "19" },{ "20", "20" },{ "21", "21" },{ "22", "22" },{ "23", "23" },{ "24", "24" },{ "25", "25" },{ "26", "26" },{ "27", "27" },{ "28", "28" },{ "29", "29" },{ "30", "30" } }, SystemConf::getInstance()->get("ee_mount.retry"));
        externalMounts->addWithDescription(_("RETRY TIMES"), _("How many times to retry the mount on boot."), emuelec_external_device_retry);
		emuelec_external_device_retry->setSelectedChangedCallback([emuelec_external_device_retry](std::string name) { 
            if (SystemConf::getInstance()->set("ee_mount.retry", name)) 
                SystemConf::getInstance()->saveSystemConf();
            });

		auto emuelec_external_device_retry_delay = std::make_shared< OptionListComponent<std::string> >(mWindow, _("DELAY BETWEEN TRIES"), false);
		emuelec_external_device_retry_delay->addRange({ { _("AUTO"), "" },{ "1", "1" },{ "2", "2" },{ "3", "3" },{ "4", "4" },{ "5", "5" },{ "6", "6" },{ "7", "7" },{ "8", "8" },{ "9", "9" },{ "10", "10" },{ "11", "11" },{ "12", "12" },{ "13", "13" },{ "14", "14" },{ "15", "15" },{ "16", "16" },{ "17", "17" },{ "18", "18" },{ "19", "19" },{ "20", "20" },{ "21", "21" },{ "22", "22" },{ "23", "23" },{ "24", "24" },{ "25", "25" },{ "26", "26" },{ "27", "27" },{ "28", "28" },{ "29", "29" },{ "30", "30" } }, SystemConf::getInstance()->get("ee_load.delay"));
        externalMounts->addWithDescription(_("DELAY BETWEEN TRIES"), _("How much delay in seconds between each retry."), emuelec_external_device_retry_delay);
		emuelec_external_device_retry_delay->setSelectedChangedCallback([emuelec_external_device_retry_delay](std::string name) { 
            if (SystemConf::getInstance()->set("ee_load.delay", name)) 
                SystemConf::getInstance()->saveSystemConf();
            });

        externalMounts->addEntry(_("FORCE MOUNT NOW"), true, [mWindow, emuelec_external_device_def, kIntl] {
            // es4all: 直接读选单当前选中的【值】(空=自动 / INTERNAL=本机 / LABEL=指定碟), 不再读 SystemConf
            // ——原本读 SystemConf 会因「默认自动没被持久化」而拿到空字串, 警告框显示 ""(实机遇到)。
            std::string selectedExternalDrive = emuelec_external_device_def->getSelected();
            // 显示名中文化: 空→自动、INTERNAL→本机、其余→原 LABEL。
            std::string disp = selectedExternalDrive.empty() ? _("AUTO")
                             : (selectedExternalDrive == kIntl ? _("INTERNAL STORAGE") : selectedExternalDrive);
            mWindow->pushGui(new GuiMsgBox(mWindow, (_("WARNING THIS WILL RESTART EMULATIONSTATION!\n\nSystem will try to mount the ROMS source selected ") + "\""+ disp + "\"" + _(". Make sure you have all the settings saved before running this.\n\nMOUNT AND RESTART?")).c_str(), _("YES"),
				[selectedExternalDrive] {
				// 把当前选中的值落盘, 确保后端(eemount/mount_romfs.sh)读到的与选单一致。
				SystemConf::getInstance()->set("global.externalmount", selectedExternalDrive);
				SystemConf::getInstance()->saveSystemConf();

                auto mountH = SystemConf::getInstance()->get("ee_mount.handler");
                if (mountH == "eemount" || mountH.empty()) {
                   Utils::Platform::ProcessStartInfo("eemount --esrestart " + selectedExternalDrive).run();
                } else if (mountH == "mount_romfs.sh") {
                   Utils::Platform::ProcessStartInfo("mount_romfs.sh yes " + selectedExternalDrive).run();
                } else {
                   Utils::Platform::ProcessStartInfo(mountH + selectedExternalDrive).run();
                }
				
                }, _("NO"), nullptr));
		});

mWindow->pushGui(externalMounts);
}


void GuiMenu::addFrameBufferOptions(Window* mWindow, GuiSettings* guiSettings, std::string configName, std::string header, std::string platform)
{
	if (!configName.empty())
		configName += ".";

	auto getVideoMode = [configName, platform] (){
		std::string ee_videomode = SystemConf::getInstance()->get("ee_videomode");

		if (Utils::FileSystem::exists("/storage/.config/EE_VIDEO_MODE"))
			ee_videomode = Utils::Platform::getShOutput(R"(cat /storage/.config/EE_VIDEO_MODE)");

		if (configName != "ee_es.") {
			ee_videomode = SystemConf::getInstance()->get(configName+"nativevideo");
		}

		if (!platform.empty() && (ee_videomode.empty() || ee_videomode == "auto")) {
			ee_videomode = SystemConf::getInstance()->get(platform+".nativevideo");
		}

		if (ee_videomode.empty() || ee_videomode == "auto") {
			ee_videomode = Utils::Platform::getShOutput(R"(cat /sys/class/display/mode)");
		}

		return ee_videomode;
	};
	
	std::string ee_videomode = getVideoMode();

	std::string ee_framebuffer = SystemConf::getInstance()->get(configName+"framebuffer");
	if (ee_framebuffer.empty()) {
		ee_framebuffer = "auto";
	}

	std::vector<std::string> reslist;
	std::string def_dimensions;
	for(std::stringstream ss(Utils::Platform::getShOutput(R"(/usr/bin/emuelec-utils dimensions)")); getline(ss, def_dimensions, ','); ) {
		reslist.push_back(def_dimensions.replace(def_dimensions.find("x"),1," "));
	}
	
	std::sort(reslist.begin(), reslist.end(), [](const std::string &lhs, const std::string &rhs)
	{
			size_t ll = lhs.length(); 
			size_t rl = rhs.length();
	    return (std::tie(ll, lhs) > std::tie(rl, rhs));
	});

	int* ee_dimensions = getVideoModeDimensions(ee_videomode, reslist);

	int fbWidth = ee_dimensions[0];
	int fbHeight = ee_dimensions[1];

	auto emuelec_frame_buffer = std::make_shared< OptionListComponent<std::string> >(mWindow, "VIDEO MODE", false);

	emuelec_frame_buffer->add("auto", "auto", ee_framebuffer == "auto");

	for (auto it = reslist.cbegin(); it != reslist.cend(); it++) {
		std::string lbl = *it;
		lbl = lbl.replace(lbl.find(" "),1,"x");
		emuelec_frame_buffer->add(lbl, *it, ee_framebuffer == *it);
	}
	guiSettings->addWithLabel(header+_("INTERNAL RESOLUTION DIMENSIONS"), emuelec_frame_buffer);

	auto fbSave = [mWindow, configName, emuelec_frame_buffer, fbWidth, fbHeight] (std::string selectedFB) {
		if (selectedFB == "auto")
			selectedFB = "";

		SystemConf::getInstance()->set(configName+"framebuffer", selectedFB);
		SystemConf::getInstance()->saveSystemConf();

		if (configName == "ee_es.") {
			Scripting::fireEvent("quit", "restart");
			Utils::Platform::quitES(Utils::Platform::QuitMode::QUIT);
		}
			//mWindow->displayNotificationMessage(_U("\uF011  ") + _("A REBOOT OF THE SYSTEM IS REQUIRED TO APPLY THE NEW CONFIGURATION"));
	};

	emuelec_frame_buffer->setSelectedChangedCallback([mWindow, configName, emuelec_frame_buffer, fbSave, fbWidth, fbHeight, getVideoMode](std::string name)
	{
		if (configName == "ee_es.") {
			mWindow->displayNotificationMessage(_U("\uF011  ") + _("A REBOOT OF THE SYSTEM WILL OCCUR TO APPLY THE NEW CONFIGURATION"));
		}
	});

	guiSettings->addSaveFunc([mWindow, configName, emuelec_frame_buffer, fbSave, fbWidth, fbHeight, getVideoMode]()
	{
		if (emuelec_frame_buffer->changed())
			fbSave(emuelec_frame_buffer->getSelected());
	});

	guiSettings->addEntry(header+_("ADJUST INTERNAL RESOLUTION BORDERS"), true, [mWindow, configName, ee_framebuffer, fbWidth, fbHeight] {
		sScreenBorders ee_borders;
		ee_borders.left = 0.0f;
		ee_borders.right = 0.0f;
		ee_borders.top = 0.0f;
		ee_borders.bottom = 0.0f;

		std::string str_ee_offsets = SystemConf::getInstance()->get(configName+"framebuffer_border");
		if (!str_ee_offsets.empty()) {
			std::vector<int> savedBorders = int_explode(str_ee_offsets, ' ');
			if (savedBorders.size() == 4) {
				ee_borders.left = (float) savedBorders[0];
				ee_borders.top = (float) savedBorders[1];
				ee_borders.right = (float) savedBorders[2];
				ee_borders.bottom = (float) savedBorders[3];
			}
		}

		GuiSettings* bordersConfig = new GuiSettings(mWindow, _("RESOLUTION BORDERS"));
		if (ee_framebuffer.empty())
			return;

		float width = (float)fbWidth;
		float height = (float)fbHeight;

		// borders
		// order: left, top, right, bottom.
		std::shared_ptr<SliderComponent> fb_borders[] = {
			std::make_shared<SliderComponent>(mWindow, 0.0f, width, 1.0f, "px"),
			std::make_shared<SliderComponent>(mWindow, 0.0f, height, 1.0f, "px"),
			std::make_shared<SliderComponent>(mWindow, 0.0f, width, 1.0f, "px"),
			std::make_shared<SliderComponent>(mWindow, 0.0f, height, 1.0f, "px")
		};

		fb_borders[0]->setValue(ee_borders.left);
		fb_borders[1]->setValue(ee_borders.top);
		fb_borders[2]->setValue(ee_borders.right);
		fb_borders[3]->setValue(ee_borders.bottom);

		fb_borders[0]->setOnValueChanged([fb_borders] (float val) {
			fb_borders[2]->setValue(val);
		});
		fb_borders[1]->setOnValueChanged([fb_borders] (float val) {
			fb_borders[3]->setValue(val);
		});
		bordersConfig->setUpdateType(ComponentListFlags::UPDATE_ALWAYS);
		bordersConfig->addWithLabel(_("LEFT BORDER"), fb_borders[0]);
		bordersConfig->addWithLabel(_("TOP BORDER"), fb_borders[1]);
		bordersConfig->addWithLabel(_("RIGHT BORDER"), fb_borders[2]);
		bordersConfig->addWithLabel(_("BOTTOM BORDER"), fb_borders[3]);

		bordersConfig->addSaveFunc([mWindow, configName, fb_borders, fbWidth, fbHeight]()
		{
			int borders[4] = {
				(int) fb_borders[0]->getValue(),
				(int) fb_borders[1]->getValue(),
				(int) fb_borders[2]->getValue(),
				(int) fb_borders[3]->getValue()};

			std::string result = "";
			if (!(borders[0] == 0 && 
					borders[1] == 0 &&
					borders[2] == 0 &&
					borders[3] == 0))
			{
				result = std::to_string(borders[0])+" "+
					std::to_string(borders[1])+" "+
					std::to_string(borders[2])+" "+
					std::to_string(borders[3]);					
			}

			SystemConf::getInstance()->set(configName+"framebuffer_border", result);
			SystemConf::getInstance()->saveSystemConf();

		});

		mWindow->pushGui(bordersConfig);
	});

}


/*  emuelec >*/
#endif

void GuiMenu::openScraperSettings()
{		
	mWindow->pushGui(new GuiScraperStart(mWindow));
}


void GuiMenu::addVersionInfo()
{
	std::string  buildDate = (Settings::getInstance()->getBool("Debug") ? std::string( "   (" + Utils::String::toUpper(PROGRAM_BUILT_STRING) + ")") : (""));

	auto theme = ThemeData::getMenuTheme();

	mVersion.setFont(theme->Footer.font);
	mVersion.setColor(theme->Footer.color);

	mVersion.setLineSpacing(0);

	std::string label;

	if (!ApiSystem::getInstance()->getVersion().empty())
	{
		if (ApiSystem::getInstance()->getApplicationName() == "BATOCERA")
			label = "BATOCERA.LINUX ES V" + ApiSystem::getInstance()->getVersion() + buildDate;
		else
		{
			// es4all: 版号统一以 PROGRAM_VERSION_STRING(EmulationStation.h)为唯一来源，不再拿
			// 各平台自己的底座版本文件(EmuELEC 的 version.info 等)当版号，避免误显示成
			// 4.8 这类底座版本号。三个编译目标(armbian/rocknix/emuelec)共用同一格式
			// "ES4All (<目标>) V<版本>, IP: <ip>"，只有目标名称随 ES4ALL_TARGET_* 宏而变。
#if defined(ES4ALL_TARGET_EMUELEC)
			std::string es4allTarget = "EmuELEC";
#elif defined(ES4ALL_TARGET_ROCKNIX)
			std::string es4allTarget = "ROCKNIX";
#else
			std::string es4allTarget = "Armbian";
#endif
			// es4all: 版本号后接建置时间戳，格式 V<版本>_<YYYYMMDDHHMMSS>(如 V1.1pre_20260722031422)，
			// 方便调试阶段确认装的是哪一次建置。时间戳来自 resources/build-info.txt(见 es4allBuildInfo)，
			// 独立于 binary、不破坏自我更新的 md5 可重现。build-info.txt 内容即那串 14 位数字。
			std::string bi = es4allBuildInfo();
			label = "ES4All (" + es4allTarget + ") V" + std::string(PROGRAM_VERSION_STRING)
			      + (bi.empty() ? "" : "_" + bi)
			      + ", IP: " + Utils::Platform::queryIPAddress();
		}
	}
		
	if (!label.empty())
	{
		if (Renderer::ScreenSettings::fullScreenMenus())
		{
			mMenu.setSubTitle(label);
			mMenu.addButton(_("BACK"), _("go back"), [&] { delete this; });
		}
		else
		{
			mVersion.setText(label);
		}
	}

	mVersion.setHorizontalAlignment(ALIGN_CENTER);
	mVersion.setVerticalAlignment(ALIGN_CENTER);
	addChild(&mVersion);
}

void GuiMenu::openScreensaverOptions() 
{
	mWindow->pushGui(new GuiGeneralScreensaverOptions(mWindow));
}

void GuiMenu::openCollectionSystemSettings() 
{
	if (ThreadedScraper::isRunning() || ThreadedHasher::isRunning())
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("THIS FUNCTION IS DISABLED WHILE THE SCRAPER IS RUNNING")));
		return;
	}

	mWindow->pushGui(new GuiCollectionSystemsOptions(mWindow));
}

void GuiMenu::onSizeChanged()
{
	GuiComponent::onSizeChanged();

	float h = mMenu.getButtonGridHeight();

	mVersion.setSize(mSize.x(), h);
	mVersion.setPosition(0, mSize.y() - h);
}

void GuiMenu::addEntry(const std::string& name, bool add_arrow, const std::function<void()>& func, const std::string iconName)
{
	auto theme = ThemeData::getMenuTheme();
	std::shared_ptr<Font> font = theme->Text.font;
	unsigned int color = theme->Text.color;

	ComponentListRow row;

	MenuComponent::addMenuIcon(mWindow, row, iconName);

	auto text = std::make_shared<TextComponent>(mWindow, name, font, color);
	row.addElement(text, true);

	if (EsLocale::isRTL())
		text->setHorizontalAlignment(Alignment::ALIGN_RIGHT);

	if (add_arrow)
	{
		std::shared_ptr<ImageComponent> bracket = makeArrow(mWindow);

		if (EsLocale::isRTL())
			bracket->setFlipX(true);

		row.addElement(bracket, false);
	}

	row.makeAcceptInputHandler(func);
	mMenu.addRow(row);
}

bool GuiMenu::input(InputConfig* config, Input input)
{
	if(GuiComponent::input(config, input))
		return true;

	if((config->isMappedTo(BUTTON_BACK, input) || config->isMappedTo("start", input)) && input.value != 0)
	{
		delete this;
		return true;
	}

	return false;
}

std::vector<HelpPrompt> GuiMenu::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt("up/down", _("CHOOSE"))); 
	prompts.push_back(HelpPrompt(BUTTON_OK, _("SELECT"))); 
	prompts.push_back(HelpPrompt("start", _("CLOSE"), [&] { delete this; }));
	return prompts;
}

class ExitKidModeMsgBox : public GuiSettings
{
	public: ExitKidModeMsgBox(Window* window, const std::string& title, const std::string& text) : GuiSettings(window, title) { addEntry(text); }

	bool input(InputConfig* config, Input input) override
	{
		Window* window = mWindow;
		if (UIModeController::getInstance()->listen(config, input))
		{
			// detect boot media (eMMC vs SD/USB) and show matching reboot entry
			return true;
		}

		return GuiComponent::input(config, input);
	}
};

void GuiMenu::exitKidMode()
{
	if (Settings::getInstance()->getString("UIMode") == "Basic")
		Settings::getInstance()->setString("UIMode", "Full");
	else
		mWindow->pushGui(new ExitKidModeMsgBox(mWindow, _("UNLOCK USER INTERFACE MODE"), _("ENTER THE CODE NOW TO UNLOCK")));
}

void GuiMenu::openSystemInformations()
{
	mWindow->pushGui(new GuiSystemInformation(mWindow));
}

void GuiMenu::openServicesSettings()
{
	auto s = new GuiSettings(mWindow, _("SERVICES").c_str());

	auto services = ApiSystem::getInstance()->getServices();
	for(unsigned int i = 0; i < services.size(); i++) {
	  auto service_enabled = std::make_shared<SwitchComponent>(mWindow);
	  service_enabled->setState(services[i].enabled);
	  s->addWithLabel(services[i].name, service_enabled);
	  service_enabled->setOnChangedCallback([services, i, service_enabled]()
	  {
	    ApiSystem::getInstance()->enableService(services[i].name, service_enabled->getState());
	  });
	}

	mWindow->pushGui(s);
}

void GuiMenu::openDmdSettings()
{
	auto s = new GuiSettings(mWindow, _("DMD").c_str());
	Window* window = mWindow;

	// server
	auto services = ApiSystem::getInstance()->getServices();
	std::string current_server = "";
	for(unsigned int i = 0; i < services.size(); i++) {
	  if(services[i].enabled) {
	    if(services[i].name == "dmd_real")      current_server = "dmd_real";
	    if(services[i].name == "dmd_simulator") current_server = "dmd_simulator";
	  }
	}
	auto server = std::make_shared< OptionListComponent<std::string> >(window, _("SERVER"), false);
	server->addRange({ { _("DISABLED"), "" }, { _("DMDSERVER (for real dmd)"), "dmd_real" }, { _("SIMULATOR (for web dmd)"), "dmd_simulator" } }, current_server);
	s->addWithDescription(_("SERVER"), _("dmd server"), server);

	// format
	auto format = std::make_shared< OptionListComponent<std::string> >(window, _("FORMAT"), false);
	std::string current_format = SystemConf::getInstance()->get("dmd.format");
	format->addRange({ { _("AUTO"), "" }, { "SD", "sd" }, { "HD", "hd" } }, current_format);
	s->addWithDescription(_("FORMAT"), _("dmd matrix size"), format);

	s->addGroup("ZEDMD");

	// zedmd.brightness
	auto zedmd_brightness = std::make_shared< OptionListComponent<std::string> >(window, _("BRIGHTNESS"), false);
	std::string current_zedmd_brightness = SystemConf::getInstance()->get("dmd.zedmd.brightness");
	zedmd_brightness->addRange({ { _("AUTO"), "" }, { "0", "0" }, { "1", "1" }, { "2", "2" }, { "3", "3" }, { "4", "4" }, { "5", "5" }, { "6", "6" }, { "7", "7" }, { "8", "8" }, { "9", "9" }, { "10", "10" }, { "11", "11" }, { "12", "12" }, { "13", "13" }, { "14", "14" }, { "15", "15" } }, current_zedmd_brightness);
	s->addWithLabel(_("BRIGHTNESS"), zedmd_brightness);

	s->addSaveFunc([window, server, format, zedmd_brightness, current_server, current_format, current_zedmd_brightness] {
	  bool needRestart = false;
	  bool needSave    = false;

	  if(current_format != format->getSelected()) {
	    SystemConf::getInstance()->set("dmd.format", format->getSelected());
	    needSave = true;
	  }
	  if(current_zedmd_brightness != zedmd_brightness->getSelected()) {
	    SystemConf::getInstance()->set("dmd.zedmd.brightness", zedmd_brightness->getSelected());
	    needRestart = true;
	    needSave = true;
	  }

	  if(server->getSelected() != current_server) {
	    needRestart = true;
	  }

	  if(needSave) {
	    SystemConf::getInstance()->saveSystemConf();
	  }

	  if(needRestart) {
	    bool stopped = false;
	    bool started = false;

	    // stop the existing server
	    if(current_server != "") {
	      ApiSystem::getInstance()->enableService(current_server, false);
	      stopped = true;
	    }
	    // start the new server
	    if(server->getSelected() != "") {
	      ApiSystem::getInstance()->enableService(server->getSelected(), true);
	      started = true;
	    }

	    if(stopped && !started) {
	      window->displayNotificationMessage(_U("\uF011  ") + _("DMDSERVER stopped"));
	    } else if(stopped && started) {
	      window->displayNotificationMessage(_U("\uF011  ") + _("DMDSERVER restarted"));
	    } else if(!stopped && started) {
	      window->displayNotificationMessage(_U("\uF011  ") + _("DMDSERVER started"));
	    }
	  }
	});

	window->pushGui(s);
}

void GuiMenu::openMultiScreensSettings()
{
	auto s = new GuiSettings(mWindow, _("MULTISCREENS").c_str());
	Window* window = mWindow;

#ifdef BATOCERA
	s->addGroup(_("BACKGLASS / INFORMATION SCREEN"));
	
	// video device2
	std::vector<std::string> availableVideo2 = ApiSystem::getInstance()->getAvailableVideoOutputDevices();
	if (availableVideo2.size())
	{
	        if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::BACKGLASS)) {
			// theme
			auto themes = ApiSystem::getInstance()->backglassThemes();
			auto selectedTheme = SystemConf::getInstance()->get("backglass.theme");
			auto theme = std::make_shared<OptionListComponent<std::string> >(mWindow, _("THEME"), false);
			
			std::vector<std::string> themeList;
			for (auto it = themes.begin(); it != themes.end(); it++)
			  themeList.push_back(*it);
			std::sort(themeList.begin(), themeList.end(), [](const std::string& a, const std::string& b) -> bool { return Utils::String::toLower(a).compare(Utils::String::toLower(b)) < 0; });

			theme->add(_("AUTO"), "auto", selectedTheme == "" || selectedTheme == "auto");
			for (auto themeName : themeList)
			  theme->add(themeName, themeName, themeName == selectedTheme);
			
			s->addWithLabel(_("THEME"), theme);
			s->addSaveFunc([theme]
			{
			  std::string oldTheme = SystemConf::getInstance()->get("backglass.theme");
			  if (oldTheme != theme->getSelected()) {
			    SystemConf::getInstance()->set("backglass.theme", theme->getSelected());
			    SystemConf::getInstance()->saveSystemConf();
			    ApiSystem::getInstance()->restartBackglass();
			  }
			});
		}

		auto optionsVideo2 = std::make_shared<OptionListComponent<std::string> >(mWindow, _("VIDEO OUTPUT"), false);
		std::string currentDevice2 = SystemConf::getInstance()->get("global.videooutput2");
		std::string currentDevice = SystemConf::getInstance()->get("global.videooutput");
		if (currentDevice2.empty()) currentDevice2 = "auto";

		bool vfound = false;
		for (auto it = availableVideo2.begin(); it != availableVideo2.end(); it++)
		{
		        if(currentDevice == (*it)) continue; // ignore the device of the first screen
			optionsVideo2->add((*it), (*it), currentDevice2 == (*it));
			if (currentDevice2 == (*it))
				vfound = true;
		}

		if (!vfound && currentDevice2 != "none")
			optionsVideo2->add(currentDevice2, currentDevice2, true);
		// add the none value
		optionsVideo2->add(_("NONE"), "none", currentDevice2 == "none");

		s->addWithLabel(_("VIDEO OUTPUT"), optionsVideo2);
		s->addSaveFunc([this, optionsVideo2, currentDevice2, s] 
		{
			if (optionsVideo2->changed()) 
			{
				SystemConf::getInstance()->set("global.videooutput2", optionsVideo2->getSelected());
				SystemConf::getInstance()->saveSystemConf();
				s->setVariable("exitreboot", true);
			}
		});

		// video resolution2
		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RESOLUTION)) {
		  auto videoModeOptionList2 = createVideoResolutionModeOptionList(mWindow, "es", "resolution2", optionsVideo2->getSelected() == "auto" ? "none" : optionsVideo2->getSelected());
		  s->addWithDescription(_("VIDEO MODE"), _("Sets the display's resolution."), videoModeOptionList2);
		  s->addSaveFunc([this, videoModeOptionList2, s] {
		    if(videoModeOptionList2->changed()) {
		      SystemConf::getInstance()->set("es.resolution2", videoModeOptionList2->getSelected());
		      SystemConf::getInstance()->saveSystemConf();
		      s->setVariable("exitreboot", true);
		    }
		  });
		}

		// video rotation2
		auto optionsRotation2 = std::make_shared<OptionListComponent<std::string> >(mWindow, _("ROTATION"), false);

		std::string selectedRotation2 = SystemConf::getInstance()->get("display.rotate2");
		if (selectedRotation2.empty())
		  selectedRotation2 = "auto";

		optionsRotation2->add(_("AUTO"),          "auto", selectedRotation2 == "auto");
		optionsRotation2->add(_("0 DEGREES"),        "0", selectedRotation2 == "0");
		optionsRotation2->add(_("90 DEGREES"),       "1", selectedRotation2 == "1");
		optionsRotation2->add(_("180 DEGREES"),      "2", selectedRotation2 == "2");
		optionsRotation2->add(_("270 DEGREES"),      "3", selectedRotation2 == "3");

		s->addWithLabel(_("SCREEN ROTATION"), optionsRotation2);

		s->addSaveFunc([this, optionsRotation2, selectedRotation2, s]
		{
		  if (optionsRotation2->changed()) {
		    SystemConf::getInstance()->set("display.rotate2", optionsRotation2->getSelected());
		    SystemConf::getInstance()->saveSystemConf();
		    s->setVariable("exitreboot", true);
		  }
		});
	}

	s->addGroup(_("DMD SCREEN"));

	// video device3
	std::vector<std::string> availableVideo3 = ApiSystem::getInstance()->getAvailableVideoOutputDevices();
	if (availableVideo3.size())
	{
		auto optionsVideo3 = std::make_shared<OptionListComponent<std::string> >(mWindow, _("VIDEO OUTPUT"), false);
		std::string currentDevice3 = SystemConf::getInstance()->get("global.videooutput3");
		std::string currentDevice = SystemConf::getInstance()->get("global.videooutput");
		if (currentDevice3.empty()) currentDevice3 = "auto";

		bool vfound = false;
		for (auto it = availableVideo3.begin(); it != availableVideo3.end(); it++)
		{
		        if(currentDevice == (*it)) continue; // ignore the device of the first screen
			optionsVideo3->add((*it), (*it), currentDevice3 == (*it));
			if (currentDevice3 == (*it))
				vfound = true;
		}

		if (!vfound)
			optionsVideo3->add(currentDevice3, currentDevice3, true);

		s->addWithLabel(_("VIDEO OUTPUT"), optionsVideo3);
		s->addSaveFunc([this, optionsVideo3, currentDevice3, s] 
		{
			if (optionsVideo3->changed()) 
			{
				SystemConf::getInstance()->set("global.videooutput3", optionsVideo3->getSelected());
				SystemConf::getInstance()->saveSystemConf();
				s->setVariable("exitreboot", true);
			}
		});

		// video resolution3
		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RESOLUTION)) {
		  auto videoModeOptionList3 = createVideoResolutionModeOptionList(mWindow, "es", "resolution3", optionsVideo3->getSelected() == "auto" ? "none" : optionsVideo3->getSelected());
		  s->addWithDescription(_("VIDEO MODE"), _("Sets the display's resolution."), videoModeOptionList3);
		  s->addSaveFunc([this, videoModeOptionList3, s] {
		    if(videoModeOptionList3->changed()) {
		      SystemConf::getInstance()->set("es.resolution3", videoModeOptionList3->getSelected());
		      SystemConf::getInstance()->saveSystemConf();
		      s->setVariable("exitreboot", true);
		    }
		  });
		}

		// video rotation3
		auto optionsRotation3 = std::make_shared<OptionListComponent<std::string> >(mWindow, _("ROTATION"), false);

		std::string selectedRotation3 = SystemConf::getInstance()->get("display.rotate3");
		if (selectedRotation3.empty())
		  selectedRotation3 = "auto";

		optionsRotation3->add(_("AUTO"),          "auto", selectedRotation3 == "auto");
		optionsRotation3->add(_("0 DEGREES"),        "0", selectedRotation3 == "0");
		optionsRotation3->add(_("90 DEGREES"),       "1", selectedRotation3 == "1");
		optionsRotation3->add(_("180 DEGREES"),      "2", selectedRotation3 == "2");
		optionsRotation3->add(_("270 DEGREES"),      "3", selectedRotation3 == "3");

		s->addWithLabel(_("SCREEN ROTATION"), optionsRotation3);

		s->addSaveFunc([this, optionsRotation3, selectedRotation3, s]
		{
		  if (optionsRotation3->changed()) 
		    {
		      SystemConf::getInstance()->set("display.rotate3", optionsRotation3->getSelected());
		      SystemConf::getInstance()->saveSystemConf();
		      s->setVariable("exitreboot", true);
		    }
		});
	}
#endif

	s->onFinalize([s, window]
	{
	  if (s->getVariable("exitreboot") && Settings::getInstance()->getBool("ExitOnRebootRequired"))
	    {
	      Utils::Platform::quitES(Utils::Platform::QuitMode::QUIT);
	      return;
	    }
	});

	window->pushGui(s);
}

void GuiMenu::openDeveloperSettings()
{
	Window *window = mWindow;

	auto s = new GuiSettings(mWindow, _("FRONTEND DEVELOPER OPTIONS").c_str());
	
	s->addGroup(_("VIDEO OPTIONS"));

	// maximum vram
	auto max_vram = std::make_shared<SliderComponent>(mWindow, 40.f, 1000.f, 10.f, "Mb");
	max_vram->setValue((float)(Settings::getInstance()->getInt("MaxVRAM")));
	s->addWithLabel(_("VRAM LIMIT"), max_vram);
	s->addSaveFunc([max_vram] { Settings::getInstance()->setInt("MaxVRAM", (int)round(max_vram->getValue())); });
	
	// es4all: 原描述为 "Also turns on the emulator's native FPS counter, if available."，
	// 那是继承自 batocera 的过时文案 —— 在 es4all 里 DrawFramerate 只在 Window.cpp 用于
	// 绘制 ES 前端自己的帧率，完全不会传给模拟器。游戏内的帧率显示是另一个独立设定:
	// 游戏设置 → DEFAULT GLOBAL SETTINGS → SHOW RETROARCH FPS (global.showFPS)。
	s->addSwitch(_("SHOW FRAMERATE"), _("Display EmulationStation's own framerate. Does not affect games."), "DrawFramerate", true, nullptr);
	s->addSwitch(_("VSYNC"), "VSync", true, [] { Renderer::setSwapInterval(); });

#ifdef BATOCERA
	// overscan
	auto overscan_enabled = std::make_shared<SwitchComponent>(mWindow);
	overscan_enabled->setState(Settings::getInstance()->getBool("Overscan"));
	s->addWithLabel(_("OVERSCAN"), overscan_enabled);
	s->addSaveFunc([overscan_enabled] 
	{
		if (Settings::getInstance()->getBool("Overscan") != overscan_enabled->getState()) 
		{
			Settings::getInstance()->setBool("Overscan", overscan_enabled->getState());
			ApiSystem::getInstance()->setOverscan(overscan_enabled->getState());
		}
	});
#endif

#ifdef _RPI_
	// Video Player - VideoOmxPlayer
	auto omx_player = std::make_shared<SwitchComponent>(mWindow);
	omx_player->setState(Settings::getInstance()->getBool("VideoOmxPlayer"));
	s->addWithLabel(_("USE OMX PLAYER (HARDWARE ACCELERATED)"), omx_player);
	s->addSaveFunc([omx_player, window]
	{
		// need to reload all views to re-create the right video components
		bool needReload = false;
		if (Settings::getInstance()->getBool("VideoOmxPlayer") != omx_player->getState())
			needReload = true;

		Settings::getInstance()->setBool("VideoOmxPlayer", omx_player->getState());

		if (needReload)
		{
			ViewController::get()->reloadAll(window);
			window->closeSplashScreen();
		}
	});
#endif

	s->addGroup(_("TOOLS"));	

#ifndef WIN32
	// GAME AT STARTUP
	if (!SystemConf::getInstance()->get("global.bootgame.path").empty())
	{		
		std::string gamelabel = SystemConf::getInstance()->get("global.bootgame.path");			
		gamelabel = Utils::FileSystem::getStem(gamelabel) + " [" + Utils::FileSystem::getStem(Utils::FileSystem::getParent(gamelabel)) + "]";

		s->addWithDescription(_("STOP LAUNCHING THIS GAME AT STARTUP"), gamelabel, nullptr, [s]
		{
			SystemConf::getInstance()->set("global.bootgame.path", "");
			SystemConf::getInstance()->set("global.bootgame.cmd", "");

			s->close();
		});
	}
#endif

	// WEB ACCESS
	auto webAccess = std::make_shared<SwitchComponent>(mWindow);
	webAccess->setState(Settings::getInstance()->getBool("PublicWebAccess"));
	s->addWithDescription(_("ENABLE PUBLIC WEB API ACCESS"), Utils::String::format(_("Allow public web access API using %s").c_str(), "http://IP:1234"), webAccess);
	s->addSaveFunc([webAccess, window, s]
	{ 
	  if (Settings::getInstance()->setBool("PublicWebAccess", webAccess->getState())) 
	  {
		  s->setVariable("reboot", true);
		  s->setVariable("exitreboot", true);
	  }
	});

	// log level
	// es4all: 改名 ES LOG LEVEL(原 LOG LEVEL) —— 与平台设置的「RETROARCH日志」区分:
	// 本项是 ES 前端【自身】日志(Settings LogLevel -> es_log.txt), 不是游戏/RA 日志。
	// (ROCKNIX 平台设置另有 system.loglevel 的 LOG LEVEL, 是不同键, 不受此改名影响。)
	auto logLevel = std::make_shared< OptionListComponent<std::string> >(mWindow, _("ES LOG LEVEL"), false);
	std::vector<std::string> modes;
	modes.push_back("default");
#ifdef _ENABLEEMUELEC 
	modes.push_back("minimal");
#else
	modes.push_back("disabled");
#endif
	modes.push_back("warning");
	modes.push_back("error");
	modes.push_back("debug");

	auto level = Settings::getInstance()->getString("LogLevel");
	if (level.empty())
		level = "default";

	for (auto it = modes.cbegin(); it != modes.cend(); it++)
		logLevel->add(_(it->c_str()), *it, level == *it);

	s->addWithDescription(_("ES LOG LEVEL"), _("Log verbosity of EmulationStation itself (es_log.txt); not games."), logLevel);
	s->addSaveFunc([this, logLevel]
	{
		if (Settings::getInstance()->setString("LogLevel", logLevel->getSelected() == "default" ? "" : logLevel->getSelected()))
		{
			Log::init();
		}
	});

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::SUPPORTFILE))
	{
		// support
		s->addEntry(_("CREATE A SUPPORT FILE"), true, [window] 
		{
			window->pushGui(new GuiMsgBox(window, _("CREATE A SUPPORT FILE? THIS INCLUDES ALL DATA IN YOUR SYSTEM FOLDER."), _("YES"),
				[window] 
				{
					if (ApiSystem::getInstance()->generateSupportFile())
						window->pushGui(new GuiMsgBox(window, _("SUPPORT FILE CREATED IN SAVES FOLDER"), _("OK")));
					else
						window->pushGui(new GuiMsgBox(window, _("SUPPORT FILE CREATION FAILED"), _("OK")));				
				}, 
				_("NO"), nullptr));
		});
	}

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::DISKFORMAT))
		s->addEntry(_("FORMAT A DISK"), true, [this] { openFormatDriveSettings(); });
	
	s->addWithDescription(_("CLEAN GAMELISTS & REMOVE UNUSED MEDIA"), _("Remove unused entries, and clean references to missing medias."), nullptr, [this, s]
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("ARE YOU SURE?"), _("YES"), [&]
		{
			int idx = 0;
			for (auto system : SystemData::sSystemVector)
			{
				mWindow->renderSplashScreen(_("Cleaning") + ": " + system->getFullName(), (float)idx / (float)SystemData::sSystemVector.size());
				cleanupGamelist(system);
				idx++;
			}

			mWindow->closeSplashScreen();
		}, _("NO"), nullptr));
	});

	s->addWithDescription(_("RESET GAMELISTS USAGE DATA"), _("Reset values of GameTime, PlayCount and LastPlayed metadata."), nullptr, [this, s]
		{
			mWindow->pushGui(new GuiMsgBox(mWindow, _("ARE YOU SURE?"), _("YES"), [&]
				{
					int idx = 0;
					for (auto system : SystemData::sSystemVector)
					{
						mWindow->renderSplashScreen(_("Cleaning") + ": " + system->getFullName(), (float)idx / (float)SystemData::sSystemVector.size());
						resetGamelistUsageData(system);
						idx++;
					}

					mWindow->closeSplashScreen();

					ViewController::reloadAllGames(mWindow, false);
				}, _("NO"), nullptr));
		});

	s->addWithDescription(_("RESET FILE EXTENSIONS"), _("Reset customized file extensions filters to default."), nullptr, [this, s]
	//s->addEntry(_("RESET FILE EXTENSIONS"), false, [this, s]
	{
		for (auto system : SystemData::sSystemVector)
			Settings::getInstance()->setString(system->getName() + ".HiddenExt", "");

		Settings::getInstance()->saveFile();
		ViewController::reloadAllGames(mWindow, false);
	});

	s->addEntry(_("REDETECT ALL GAMES' LANG/REGION"), false, [this]
	{
		Window* window = mWindow;
		window->pushGui(new GuiLoading<int>(window, _("PLEASE WAIT"), [](auto gui)
		{
			for (auto system : SystemData::sSystemVector)
			{
				if (system->isCollection() || !system->isGameSystem())
					continue;

				for (auto game : system->getRootFolder()->getFilesRecursive(GAME))
					game->detectLanguageAndRegion(true);
			}

			return 0;
		}));
	});

	s->addEntry(_("FIND ALL GAMES WITH NETPLAY/ACHIEVEMENTS"), false, [this] { ThreadedHasher::start(mWindow, ThreadedHasher::HASH_ALL, true); });

	s->addEntry(_("CLEAR CACHES"), true, [this, s]
		{
			ImageIO::clearImageCache();

			auto rootPath = Utils::FileSystem::getGenericPath(Paths::getUserEmulationStationPath());

			Utils::FileSystem::deleteDirectoryFiles(rootPath + "/tmp/");
			Utils::FileSystem::deleteDirectoryFiles(Utils::FileSystem::getTempPath());
			Utils::FileSystem::deleteDirectoryFiles(Utils::FileSystem::getPdfTempPath());

			ViewController::reloadAllGames(mWindow, false);
		});

	s->addEntry(_("BUILD IMAGE CACHE"), true, [this, s]
		{
			unsigned int x;
			unsigned int y;

			int idx = 0;
			for (auto sys : SystemData::sSystemVector)
			{
				if (sys->isCollection())
				{
					idx++;
					continue;
				}

				mWindow->renderSplashScreen(_("Building image cache") + ": " + sys->getFullName(), (float)idx / (float)SystemData::sSystemVector.size());

				for (auto file : sys->getRootFolder()->getFilesRecursive(GAME))
				{
					for (auto mdd : MetaDataList::getMDD())
					{
						if (mdd.id != MetaDataId::Image && mdd.id != MetaDataId::Thumbnail)
							continue;

						auto value = file->getMetadata(mdd.id);
						if (value.empty())
							continue;

						auto ext = Utils::String::toLower(Utils::FileSystem::getExtension(value));
						if (ext == ".jpg" || ext == ".png")
							ImageIO::loadImageSize(value.c_str(), &x, &y);
					}
				}

				idx++;
			}

			mWindow->closeSplashScreen();
		});

	s->addGroup(_("DISPLAY SETTINGS"));

	auto menuFontScale = std::make_shared< OptionListComponent<std::string> >(mWindow, _("MENU FONT SCALE"), false);
	menuFontScale->addRange({ { _("AUTO"), "" },{ "100%", "1.0" },{ "110%", "1.1" },{ "125%", "1.25" },{ "133%", "1.31" },{ "150%", "1.5" },{ "175%", "1.75" },{ "200%", "2" },{ "75%", "0.75" } ,{ "50%", "0.5" } },
		Settings::getInstance()->getString("MenuFontScale"));
	s->addWithLabel(_("MENU FONT SCALE"), menuFontScale);
	s->addSaveFunc([s, menuFontScale] { if (Settings::getInstance()->setString("MenuFontScale", menuFontScale->getSelected())) s->setVariable("reboot", true); });

	auto fontScale = std::make_shared< OptionListComponent<std::string> >(mWindow, _("FONT SCALE"), false);
	fontScale->addRange({ { _("AUTO"), "" },{ "100%", "1.0" },{ "110%", "1.1" },{ "125%", "1.25" },{ "133%", "1.31" },{ "150%", "1.5" },{ "175%", "1.75" },{ "200%", "2" },{ "75%", "0.75" } ,{ "50%", "0.5" } },
		Settings::getInstance()->getString("FontScale"));
	s->addWithLabel(_("THEME FONT SCALE"), fontScale);
	s->addSaveFunc([s, fontScale] { if (Settings::getInstance()->setString("FontScale", fontScale->getSelected())) s->setVariable("reboot", true); });

	auto fullScreenMenus = std::make_shared< OptionListComponent<std::string> >(mWindow, _("FULL SCREEN MENUS"), false);
	fullScreenMenus->addRange({ { _("AUTO"), "" },{ "YES", "true" },{ "NO", "false" } }, Settings::getInstance()->getString("FullScreenMenu"));
	s->addWithLabel(_("FULL SCREEN MENUS"), fullScreenMenus);
	s->addSaveFunc([s, fullScreenMenus] { if (Settings::getInstance()->setString("FullScreenMenu", fullScreenMenus->getSelected())) s->setVariable("reboot", true); });

	auto isSmallScreen = std::make_shared< OptionListComponent<std::string> >(mWindow, _("FORCE SMALL SCREEN THEMING"), false);
	isSmallScreen->addRange({ { _("AUTO"), "" },{ "YES", "true" },{ "NO", "false" } }, Settings::getInstance()->getString("ForceSmallScreen"));
	s->addWithLabel(_("FORCE SMALL SCREEN THEMING"), isSmallScreen);
	s->addSaveFunc([s, isSmallScreen] { if (Settings::getInstance()->setString("ForceSmallScreen", isSmallScreen->getSelected())) s->setVariable("reboot", true); });



	s->addGroup(_("DATA MANAGEMENT"));

	// ExcludeMultiDiskContent
	auto excludeMultiDiskContent = std::make_shared<SwitchComponent>(mWindow);
	excludeMultiDiskContent->setState(Settings::getInstance()->getBool("RemoveMultiDiskContent"));
	s->addWithLabel(_("IGNORE MULTI-FILE DISK CONTENT (CUE/GDI/CCD/M3U)"), excludeMultiDiskContent);
	s->addSaveFunc([excludeMultiDiskContent] { Settings::getInstance()->setBool("RemoveMultiDiskContent", excludeMultiDiskContent->getState()); });

	// enable filters (ForceDisableFilters)
	auto enable_filter = std::make_shared<SwitchComponent>(mWindow);
	enable_filter->setState(!Settings::getInstance()->getBool("ForceDisableFilters"));
	s->addWithDescription(_("ENABLE GAME FILTERING"), _("Whether to show or hide game filtering related settings in the view options."), enable_filter);
	s->addSaveFunc([this, enable_filter]
	{
		Settings::getInstance()->setBool("ForceDisableFilters", !enable_filter->getState());
	});

	// gamelist saving
	auto save_gamelists = std::make_shared<SwitchComponent>(mWindow);
	save_gamelists->setState(Settings::getInstance()->getBool("SaveGamelistsOnExit"));
	s->addWithLabel(_("SAVE METADATA ON EXIT"), save_gamelists);
	s->addSaveFunc([save_gamelists] { Settings::getInstance()->setBool("SaveGamelistsOnExit", save_gamelists->getState()); });

	// gamelist
	auto parse_gamelists = std::make_shared<SwitchComponent>(mWindow);
	parse_gamelists->setState(Settings::getInstance()->getBool("ParseGamelistOnly"));
	s->addWithDescription(_("PARSE GAMELISTS ONLY"), _("Debug tool: Don't check if the ROMs actually exist. Can cause problems!"), parse_gamelists);
	s->addSaveFunc([parse_gamelists] { Settings::getInstance()->setBool("ParseGamelistOnly", parse_gamelists->getState()); });

	// Local Art
	auto local_art = std::make_shared<SwitchComponent>(mWindow);
	local_art->setState(Settings::getInstance()->getBool("LocalArt"));
	s->addWithDescription(_("SEARCH FOR LOCAL ART"), _("If no image is specified in the gamelist, try to find media with the same filename to use."), local_art);
	s->addSaveFunc([local_art] { Settings::getInstance()->setBool("LocalArt", local_art->getState()); });

	s->addGroup(_("USER INTERFACE"));

	// carousel transition option
	auto move_carousel = std::make_shared<SwitchComponent>(mWindow);
	move_carousel->setState(Settings::getInstance()->getBool("MoveCarousel"));
	s->addWithLabel(_("CAROUSEL TRANSITIONS"), move_carousel);
	s->addSaveFunc([move_carousel] { Settings::getInstance()->setBool("MoveCarousel", move_carousel->getState()); });

	// quick system select (left/right in game list view)
	auto quick_sys_select = std::make_shared<SwitchComponent>(mWindow);
	quick_sys_select->setState(Settings::getInstance()->getBool("QuickSystemSelect"));
	s->addWithLabel(_("QUICK SYSTEM SELECT"), quick_sys_select);
	s->addSaveFunc([quick_sys_select] { Settings::getInstance()->setBool("QuickSystemSelect", quick_sys_select->getState()); });

	// quick jump next letter (R2/L2 in game list view)
	auto quick_jump_letter = std::make_shared<SwitchComponent>(mWindow);
	quick_jump_letter->setState(Settings::getInstance()->getBool("QuickJumpLetter"));
	s->addWithLabel(_("QUICK JUMP LETTER"), quick_jump_letter);
	s->addSaveFunc([quick_jump_letter] { Settings::getInstance()->setBool("QuickJumpLetter", quick_jump_letter->getState()); });

	// Enable OSK (On-Screen-Keyboard)
	auto osk_enable = std::make_shared<SwitchComponent>(mWindow);
	osk_enable->setState(Settings::getInstance()->getBool("UseOSK"));
	s->addWithLabel(_("ON-SCREEN KEYBOARD"), osk_enable);
	s->addSaveFunc([osk_enable] { Settings::getInstance()->setBool("UseOSK", osk_enable->getState()); });
	
#if defined(_WIN32) || defined(X86) || defined(X86_64)
	// Hide EmulationStation Window when running a game ( windows only )
	auto hideWindowScreen = std::make_shared<SwitchComponent>(mWindow);
	hideWindowScreen->setState(Settings::getInstance()->getBool("HideWindow"));
	s->addWithLabel(_("HIDE EMULATIONSTATION WHEN RUNNING A GAME"), hideWindowScreen);
	s->addSaveFunc([hideWindowScreen] { Settings::getInstance()->setBool("HideWindow", hideWindowScreen->getState()); });
#endif
	
#if defined(WIN32) && !defined(_DEBUG)
	// full exit
	auto fullExitMenu = std::make_shared<SwitchComponent>(mWindow);
	fullExitMenu->setState(!Settings::getInstance()->getBool("ShowOnlyExit"));
	s->addWithLabel(_("COMPLETE QUIT MENU"), fullExitMenu);
	s->addSaveFunc([fullExitMenu] { Settings::getInstance()->setBool("ShowOnlyExit", !fullExitMenu->getState()); });
#endif

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::GAMESETTINGS))
	{
		auto retroarchRgui = std::make_shared< OptionListComponent<std::string> >(mWindow, _("RETROARCH MENU DRIVER"), false);
		retroarchRgui->addRange({ { _("AUTO"), "" },{ "rgui", "rgui" },{ "xmb", "xmb" },{ "ozone", "ozone" },{ "glui", "glui" } }, SystemConf::getInstance()->get("global.retroarch.menu_driver"));
		s->addWithLabel(_("RETROARCH MENU DRIVER"), retroarchRgui);
		s->addSaveFunc([retroarchRgui] { SystemConf::getInstance()->set("global.retroarch.menu_driver", retroarchRgui->getSelected()); });
	}

	auto invertJoy = std::make_shared<SwitchComponent>(mWindow);
	invertJoy->setState(Settings::getInstance()->getBool("InvertButtons"));
	s->addWithDescription(_("SWITCH CONFIRM & CANCEL BUTTONS IN EMULATIONSTATION"), _("Switches the South and East buttons' functionality"), invertJoy);
	s->addSaveFunc([this, s, invertJoy]
	{
		if (Settings::getInstance()->setBool("InvertButtons", invertJoy->getState()))
		{
			InputConfig::AssignActionButtons();
			s->setVariable("reloadAll", true);
		}
	});

	// es4all: InvertGameButtons 开关已移到「手柄和蓝牙设置 → 手柄按键映射」旁（GuiControllersSettings）。

	auto invertLongPress = std::make_shared<SwitchComponent>(mWindow);
	invertLongPress->setState(Settings::getInstance()->getBool("GameOptionsAtNorth"));
	s->addWithDescription(_("ACCESS GAME OPTIONS WITH NORTH BUTTON"), _("Switches to short-press North for Savestates & long-press South button for Game Options"), invertLongPress);
	s->addSaveFunc([this, s, invertLongPress]
	{
		if (Settings::getInstance()->setBool("GameOptionsAtNorth", invertLongPress->getState()))
			s->setVariable("reloadAll", true);
	});

	auto firstJoystickOnly = std::make_shared<SwitchComponent>(mWindow);
	firstJoystickOnly->setState(Settings::getInstance()->getBool("FirstJoystickOnly"));
	s->addWithLabel(_("CONTROL EMULATIONSTATION WITH FIRST JOYSTICK ONLY"), firstJoystickOnly);
	s->addSaveFunc([this, firstJoystickOnly] { Settings::getInstance()->setBool("FirstJoystickOnly", firstJoystickOnly->getState()); });

//#if !defined(WIN32)
	{
	  auto gun_mt = std::make_shared<SliderComponent>(mWindow, 0.f, 10.f, 0.1f, "%");
	  gun_mt->setValue(Settings::getInstance()->getFloat("GunMoveTolerence"));
	  s->addWithLabel(_("GUN MOVE TOLERENCE"), gun_mt);
	  s->addSaveFunc([gun_mt] {
	    Settings::getInstance()->setFloat("GunMoveTolerence", gun_mt->getValue());
	  });
	}
//#endif

#if defined(BATOCERA)
	// PS3 controller enable
	auto enable_ps3 = std::make_shared<SwitchComponent>(mWindow);
	enable_ps3->setState(SystemConf::getInstance()->getBool("controllers.ps3.enabled"));
	s->addWithDescription(_("ENABLE PS3 CONTROLLER SUPPORT"), _("Might have negative impact on security."), enable_ps3);
	s->addSaveFunc([enable_ps3] {
		bool ps3Enabled = enable_ps3->getState();
		if (ps3Enabled != SystemConf::getInstance()->getBool("controllers.ps3.enabled"))
		{
			SystemConf::getInstance()->setBool("controllers.ps3.enabled", ps3Enabled);
			SystemConf::getInstance()->saveSystemConf();
			if(SystemConf::getInstance()->getBool("controllers.bluetooth.enabled"))
				ApiSystem::getInstance()->enableBluetooth();
		}
	});
#endif

#if defined(WIN32)

	auto hidJoysticks = std::make_shared<SwitchComponent>(mWindow);
	hidJoysticks->setState(Settings::getInstance()->getBool("HidJoysticks"));
	s->addWithLabel(_("ENABLE HID JOYSTICK DRIVERS"), hidJoysticks);
	s->addSaveFunc([this, hidJoysticks] { Settings::getInstance()->setBool("HidJoysticks", hidJoysticks->getState()); });
	
	// Network Indicator
	auto networkIndicator = std::make_shared<SwitchComponent>(mWindow);
	networkIndicator->setState(Settings::getInstance()->getBool("ShowNetworkIndicator"));
	s->addWithLabel(_("SHOW NETWORK INDICATOR"), networkIndicator);
	s->addSaveFunc([networkIndicator] { Settings::getInstance()->setBool("ShowNetworkIndicator", networkIndicator->getState()); });
#endif

	s->addGroup(_("OPTIMIZATIONS"));

	// preload UI
	auto preloadUI = std::make_shared<SwitchComponent>(mWindow);
	preloadUI->setState(Settings::getInstance()->getBool("PreloadUI"));
	s->addWithDescription(_("PRELOAD UI ELEMENTS ON BOOT"), _("Reduces lag when entering gamelists from the system menu, increases boot time"), preloadUI);
	s->addSaveFunc([preloadUI] { Settings::getInstance()->setBool("PreloadUI", preloadUI->getState()); });

	// preload Medias
	auto preloadMedias = std::make_shared<SwitchComponent>(mWindow);
	preloadMedias->setState(Settings::getInstance()->getBool("PreloadMedias"));
	s->addWithDescription(_("PRELOAD METADATA MEDIA ON BOOT"), _("Reduces lag when scrolling through a fully scraped gamelist, increases boot time"), preloadMedias);
	s->addSaveFunc([preloadMedias] { Settings::getInstance()->setBool("PreloadMedias", preloadMedias->getState()); });
	
	// threaded loading
	auto threadedLoading = std::make_shared<SwitchComponent>(mWindow);
	threadedLoading->setState(Settings::getInstance()->getBool("ThreadedLoading"));
	s->addWithLabel(_("THREADED LOADING"), threadedLoading);
	s->addSaveFunc([threadedLoading] { Settings::getInstance()->setBool("ThreadedLoading", threadedLoading->getState()); });

	// threaded loading
	auto asyncImages = std::make_shared<SwitchComponent>(mWindow);
	asyncImages->setState(Settings::getInstance()->getBool("AsyncImages"));
	s->addWithLabel(_("ASYNC IMAGE LOADING"), asyncImages);
	s->addSaveFunc([asyncImages] { Settings::getInstance()->setBool("AsyncImages", asyncImages->getState()); });

	// optimizeVram
	auto optimizeVram = std::make_shared<SwitchComponent>(mWindow);
	optimizeVram->setState(Settings::getInstance()->getBool("OptimizeVRAM"));
	s->addWithLabel(_("OPTIMIZE IMAGES VRAM USE"), optimizeVram);
	s->addSaveFunc([optimizeVram] { Settings::getInstance()->setBool("OptimizeVRAM", optimizeVram->getState()); });

	// optimizeVideo
	auto optimizeVideo = std::make_shared<SwitchComponent>(mWindow);
	optimizeVideo->setState(Settings::getInstance()->getBool("OptimizeVideo"));
	s->addWithLabel(_("OPTIMIZE VIDEO VRAM USAGE"), optimizeVideo);
	s->addSaveFunc([optimizeVideo] { Settings::getInstance()->setBool("OptimizeVideo", optimizeVideo->getState()); });
	
	s->onFinalize([s, window]
	{					
		if (s->getVariable("reboot"))
			window->displayNotificationMessage(_U("\uF011  ") + _("REBOOT REQUIRED TO APPLY THE NEW CONFIGURATION"));

		if (s->getVariable("reloadAll"))
		{
			ViewController::get()->reloadAll(window);
			window->closeSplashScreen();
		}
	});

	mWindow->pushGui(s);
}

void GuiMenu::openUpdatesSettings()
{
	GuiSettings *updateGui = new GuiSettings(mWindow, _("UPDATES & DOWNLOADS").c_str());

	updateGui->addGroup(_("DOWNLOADS"));

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::BATOCERASTORE))
	{
		updateGui->addEntry(_("CONTENT DOWNLOADER"), true, [this]
		{
			if (!checkNetwork())
				return;

			mWindow->pushGui(new GuiBatoceraStore(mWindow));
		});
	}

	// Themes installer/browser
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::THEMESDOWNLOADER))
	{
		updateGui->addEntry(_("THEMES"), true, [this]
		{
			if (!checkNetwork())
				return;

			mWindow->pushGui(new GuiThemeInstaller(mWindow));
		});
	}

	// es4all: THE BEZEL PROJECT 入口已移除（用户实测 EMUELEC 版就跑不起来）。
	// 它不是「解开门就能用」的东西 —— ES 只是驱动外部脚本 batocera-es-thebezelproject
	// (list / install <sys> / remove <sys>)，没有内建实作：
	//   - EMUELEC: 自带该脚本所以选单会出现，但实际下载/安装失败 -> 留着只是误导使用者；
	//   - ROCKNIX: 根本没有该脚本，而且它装出来的目录结构与 ROCKNIX 不相容
	//     (EmuELEC 脚本装成 bezels/<系统>/<rom>.png+.cfg；ROCKNIX 的 setsettings.sh
	//      check_decorations 期望 bezels/<套件名>/games/<系统>/<rom>.png+.info)，
	//     要支援得另写一支 ROCKNIX 版脚本 —— 评估后决定不做。
	// 边框功能本身仍在：游戏设置 → DEFAULT GLOBAL SETTINGS 的 ENABLE RA BEZELS(global.bezel)，
	// 使用者自备 bezel 档放 /storage/roms/bezels 即可，不受此处移除影响。

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::UPGRADE))
	{
		updateGui->addGroup(_("SOFTWARE UPDATES"));

		// Enable updates
		updateGui->addSwitch(_("CHECK FOR UPDATES"), "updates.enabled", false);

		auto updatesTypeList = std::make_shared<OptionListComponent<std::string> >(mWindow, _("UPDATE TYPE"), false);

#if BATOCERA
#define BETA_NAME "butterfly"
#else
#define BETA_NAME "beta"
#endif

		std::string updatesType = SystemConf::getInstance()->get("updates.type");

#if WIN32
		if (updatesType == "unstable")
			updatesTypeList->add("unstable", "unstable", updatesType == "unstable");
		else
#endif
			// es4all: 只维护单一 stable 更新线，不做 beta 通道(会造成困惑且无独立内容;
			// updatecheck.sh 里 stable/beta 本就指向同一个仓库)。强制归一到 stable 并持久化，
			// 顺带清掉历史上可能残留的 updates.type=beta。
			if (updatesType != "stable")
			{
				updatesType = "stable";
				if (SystemConf::getInstance()->set("updates.type", "stable"))
					SystemConf::getInstance()->saveSystemConf();
			}

		updatesTypeList->add("stable", "stable", updatesType == "stable");
		// es4all: BETA 选项已移除(原 updatesTypeList->add(BETA_NAME,...) 删除)。

#if WIN32
		// WIN32 有 unstable/stable 多选才显示更新类型选择器;其余平台(emuelec/armbian/
		// rocknix)只有单一 stable，无需选择器，整行隐藏。
		updateGui->addWithLabel(_("UPDATE TYPE"), updatesTypeList);
		updatesTypeList->setSelectedChangedCallback([](std::string name)
		{
			if (SystemConf::getInstance()->set("updates.type", name))
				SystemConf::getInstance()->saveSystemConf();
		});
#endif

		// Start update
		updateGui->addEntry(GuiUpdate::state == GuiUpdateState::State::UPDATE_READY ? _("APPLY UPDATE") : _("START UPDATE"), true, [this]
		{
			if (GuiUpdate::state == GuiUpdateState::State::UPDATE_READY)
			{
#ifdef ES4ALL_SELF_UPDATE
				// es4all: 滚动更新(同版本号 1.1pre 覆盖已挂载过的旧版)靠 bind-mount 换新 inode,
				// 只重启 ES 进程看不到新版本, 必须整机重开机重新走一次挂载钩子/服务。
				if (Es4allUpdate::needsFullReboot())
					Utils::Platform::quitES(Utils::Platform::QuitMode::REBOOT);
				else
#endif
					Utils::Platform::quitES(Utils::Platform::QuitMode::RESTART);
			}
			else if (GuiUpdate::state == GuiUpdateState::State::UPDATER_RUNNING)
				mWindow->pushGui(new GuiMsgBox(mWindow, _("UPDATER IS ALREADY RUNNING")));
			else
			{
				if (!checkNetwork())
					return;

				mWindow->pushGui(new GuiUpdate(mWindow));
			}
		});
	}

	mWindow->pushGui(updateGui);
}

bool GuiMenu::checkNetwork()
{
	if (ApiSystem::getInstance()->getIpAddress() == "NOT CONNECTED")
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("YOU ARE NOT CONNECTED TO A NETWORK"), _("OK"), nullptr));
		return false;
	}

	return true;
}

void GuiMenu::openSystemSettings() 
{
	Window *window = mWindow;

	auto s = new GuiSettings(mWindow, _("SYSTEM SETTINGS").c_str());
	bool isFullUI = UIModeController::getInstance()->isUIModeFull();

	s->addGroup(_("SYSTEM"));

	// System informations
	s->addEntry(_("INFORMATION"), true, [this] { openSystemInformations(); });

// es4all: ROCKNIX 也用这块时区（已改用 getTimezones() 读 /usr/share/zoneinfo，三边通用）。
#if defined(_ENABLEEMUELEC) || defined(ES4ALL_TARGET_ROCKNIX)
	auto emuelec_timezones = std::make_shared<OptionListComponent<std::string> >(mWindow, _("TIMEZONE"), false);
	std::string currentTimezone = SystemConf::getInstance()->get("system.timezone");
	// es4all: 用 ApiSystem::getTimezones()(直接读 /usr/share/zoneinfo，三边通用)取代
	// emuelec-utils(ROCKNIX 无此工具，原本 test 不 success 会让时区选项整个隐藏)。
	// 储存维持 system.timezone，各发行版启动时会读取并套用(ROCKNIX 走 es_settings + tz-data.service)。
	std::vector<std::string> tzList = ApiSystem::getInstance()->getTimezones();
	if (!tzList.empty()) {
		for (auto tz : tzList)
			emuelec_timezones->add(tz, tz, currentTimezone == tz);
		s->addWithLabel(_("TIMEZONE"), emuelec_timezones);
		s->addSaveFunc([emuelec_timezones] {
			if (emuelec_timezones->changed()) {
				std::string selectedTimezone = emuelec_timezones->getSelected();
				Utils::Platform::ProcessStartInfo("ln -sf /usr/share/zoneinfo/" + selectedTimezone + " $(readlink /etc/localtime)").run();
				SystemConf::getInstance()->set("system.timezone", selectedTimezone);
				SystemConf::getInstance()->saveSystemConf();
			}
		});
	}

#endif

	// language choice
	auto language_choice = std::make_shared<OptionListComponent<std::string> >(window, _("LANGUAGE"), false);

	std::string language = SystemConf::getInstance()->get("system.language");
	if (language.empty()) 
		language = "en_US";

	language_choice->add("ARABIC",               "ar_YE", language == "ar_YE");
	language_choice->add("CATALÀ",               "ca_ES", language == "ca_ES");
	language_choice->add("ČEŠTINA",                "cs_CZ", language == "cs_CZ");
	language_choice->add("CYMRAEG",              "cy_GB", language == "cy_GB");
	language_choice->add("DEUTSCH", 	     "de_DE", language == "de_DE");
	language_choice->add("GREEK",                "el_GR", language == "el_GR");
	language_choice->add("ENGLISH (US)", 	     "en_US", language == "en_US" || language == "en");
	language_choice->add("ENGLISH (UK)", 	     "en_GB", language == "en_GB");
	language_choice->add("ESPAÑOL", 	     "es_ES", language == "es_ES" || language == "es");
	language_choice->add("ESPAÑOL MEXICANO",     "es_MX", language == "es_MX");
	language_choice->add("EUSKARA",               "eu_ES", language == "eu_ES");
	language_choice->add("SUOMI",                "fi_FI", language == "fi_FI");
	language_choice->add("FRANÇAIS",             "fr_FR", language == "fr_FR" || language == "fr");
	language_choice->add("GALEGO",               "gl_ES", language == "gl_ES");
	language_choice->add("עברית",                "he_IL", language == "he_IL");
	language_choice->add("HUNGARIAN",            "hu_HU", language == "hu_HU");
	language_choice->add("BAHASA INDONESIA",     "id_ID", language == "id_ID");
	language_choice->add("ITALIANO",             "it_IT", language == "it_IT");
	language_choice->add("JAPANESE", 	     "ja_JP", language == "ja_JP");
	language_choice->add("KOREAN",   	     "ko_KR", language == "ko_KR" || language == "ko");
	language_choice->add("NORWEGIAN BOKMAL",     "nb_NO", language == "nb_NO");
	language_choice->add("DUTCH",                "nl_NL", language == "nl_NL");
	language_choice->add("NORWEGIAN",            "nn_NO", language == "nn_NO");
	language_choice->add("OCCITAN",              "oc_FR", language == "oc_FR");
	language_choice->add("POLISH",               "pl_PL", language == "pl_PL");
	language_choice->add("PORTUGUÊS BRASILEIRO", "pt_BR", language == "pt_BR");
	language_choice->add("PORTUGUÊS PORTUGAL",   "pt_PT", language == "pt_PT");
	language_choice->add("ROMÂNĂ",               "ro_RO", language == "ro_RO");
	language_choice->add("РУССКИЙ",              "ru_RU", language == "ru_RU");
	language_choice->add("SLOVENČINA", 	     "sk_SK", language == "sk_SK");
	language_choice->add("SVENSKA", 	     "sv_SE", language == "sv_SE");
	language_choice->add("TÜRKÇE",  	     "tr_TR", language == "tr_TR");
	language_choice->add("Українська",           "uk_UA", language == "uk_UA");
	language_choice->add("TIẾNG VIỆT",           "vi_VN", language == "vi_VN");
	language_choice->add("简体中文", 	     "zh_CN", language == "zh_CN");
	language_choice->add("繁體中文", 	     "zh_TW", language == "zh_TW");

	s->addWithLabel(_("LANGUAGE (REGION)"), language_choice);
	s->addSaveFunc([window, language_choice, language, s]
	{
		if (language_choice->changed() && SystemConf::getInstance()->set("system.language", language_choice->getSelected()))
		{
			FileSorts::reset();
			MetaDataList::initMetadata();

#ifdef HAVE_INTL
			s->setVariable("exitreboot", true);
#endif
			s->setVariable("reloadGuiMenu", true);
		}		
	});

// es4all: ROCKNIX 走上面 emuelec_timezones 那块，避免这里重复出现第二个时区项。
#if !defined(_ENABLEEMUELEC) && !defined(ES4ALL_TARGET_ROCKNIX)
	// Timezone
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::ScriptId::TIMEZONES))
	{
		VectorEx<std::string> availableTimezones = ApiSystem::getInstance()->getTimezones();
		if (availableTimezones.size() > 0)
		{
			std::string currentTZ = ApiSystem::getInstance()->getCurrentTimezone();
			if (currentTZ.empty() || !availableTimezones.any([currentTZ](const std::string& tz) { return tz == currentTZ; }))
				currentTZ = "Europe/Paris";

			auto tzChoices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("SELECT YOUR TIME ZONE"), false);

			for (auto tz : availableTimezones)
				tzChoices->add(_(Utils::String::toUpper(tz).c_str()), tz, currentTZ == tz);

			s->addWithLabel(_("TIME ZONE"), tzChoices);
			s->addSaveFunc([tzChoices] 
			{
				if (SystemConf::getInstance()->set("system.timezone", tzChoices->getSelected()))
					ApiSystem::getInstance()->setTimezone(tzChoices->getSelected());
			});
		}
	}

#endif
	// Clock time format (14:42 or 2:42 pm)
	s->addSwitch(_("SHOW CLOCK IN 12-HOUR FORMAT"), "ClockMode12", true);

	// power saver
	auto power_saver = std::make_shared< OptionListComponent<std::string> >(mWindow, _("POWER SAVING MODE"), false);
	power_saver->addRange({ { _("DISABLED"), "disabled" }, { _("DEFAULT"), "default" }, { _("ENHANCED"), "enhanced" }, { _("INSTANT"), "instant" }, }, Settings::PowerSaverMode());
	s->addWithDescription(_("POWER SAVING MODE"), _("Reduces power consumption when idle (useful for handhelds)."), power_saver);
	s->addSaveFunc([this, power_saver] 
	{
		if (Settings::PowerSaverMode() != "instant" && power_saver->getSelected() == "instant")
			Settings::getInstance()->setBool("EnableSounds", false);

		Settings::setPowerSaverMode(power_saver->getSelected());
		PowerSaver::init();
	});

#if defined(_ENABLE_TTS_) || defined(WIN32)
	if (TextToSpeech::getInstance()->isAvailable())
	{
			// tts
		auto tts = std::make_shared<SwitchComponent>(mWindow);
		tts->setState(Settings::getInstance()->getBool("TTS"));
		s->addWithLabel(_("SCREEN READER (TEXT TO SPEECH)"), tts);
		s->addSaveFunc([tts] {
			 if(TextToSpeech::getInstance()->isEnabled() != tts->getState()) {
			   TextToSpeech::getInstance()->enable(tts->getState());
			   Settings::getInstance()->setBool("TTS", tts->getState());
			 }});
	}
#endif

	// es4all: 「用户界面模式」(USER INTERFACE MODE) 已移除 —— Kid / Kiosk 受限界面模式不再提供，
	// 界面恒为「完整」(见 UIModeController::isUIModeFull() 等已定为常数)。
	// 连带: 主菜单的「信息」「解锁用户界面模式」(只在受限模式出现)也不再显示。
	// 原选单里的 "Basic" 本就选不到(mUIModes 中被注释掉)且 isUIModeBasic() 全专案零调用。

	// KODI SETTINGS
#ifdef _ENABLE_KODI_
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::KODI))
	{
		s->addEntry(_("KODI SETTINGS"), true, [this] 
		{
			GuiSettings* kodiGui = new GuiSettings(mWindow, _("KODI SETTINGS").c_str());
			kodiGui->addSwitch(_("ENABLE KODI"), "kodi.enabled", false);
			kodiGui->addSwitch(_("LAUNCH KODI AT BOOT"), "kodi.atstartup", false);
			mWindow->pushGui(kodiGui);
		});
	}
#endif

#ifdef BATOCERA
	s->addGroup(_("HARDWARE"));
#endif

	// brighness
	int brighness;
	if (ApiSystem::getInstance()->getBrightness(brighness))
	{
		auto brightnessComponent = std::make_shared<SliderComponent>(mWindow, 1.f, 100.f, 1.f, "%");
		brightnessComponent->setValue(brighness);
		brightnessComponent->setOnValueChanged([](const float &newVal)
		{
#ifdef _ENABLEEMUELEC
            auto thebright = std::to_string((int)Math::round(newVal));
            Utils::Platform::ProcessStartInfo("/usr/bin/odroidgoa_utils.sh bright " + thebright).run();
#else
			ApiSystem::getInstance()->setBrightness((int)Math::round(newVal));
#if !WIN32
			SystemConf::getInstance()->set("display.brightness", std::to_string((int)Math::round(newVal)));
#endif
#endif
		});

       s->addSaveFunc([this, brightnessComponent] {
            SystemConf::getInstance()->set("brightness.level", std::to_string((int)Math::round(brightnessComponent->getValue())));
       });
        
		s->addWithLabel(_("BRIGHTNESS"), brightnessComponent);
	}

#ifdef BATOCERA
	// video device
	std::vector<std::string> availableVideo = ApiSystem::getInstance()->getAvailableVideoOutputDevices();
	if (availableVideo.size())
	{
		auto optionsVideo = std::make_shared<OptionListComponent<std::string> >(mWindow, _("VIDEO OUTPUT"), false);
		std::string currentDevice = SystemConf::getInstance()->get("global.videooutput");
		if (currentDevice.empty()) currentDevice = "auto";

		bool vfound = false;
		for (auto it = availableVideo.begin(); it != availableVideo.end(); it++)
		{
			optionsVideo->add((*it), (*it), currentDevice == (*it));
			if (currentDevice == (*it))
				vfound = true;
		}

		if (!vfound)
			optionsVideo->add(currentDevice, currentDevice, true);

		s->addWithLabel(_("VIDEO OUTPUT"), optionsVideo);
		s->addSaveFunc([this, optionsVideo, currentDevice, s] 
		{
			if (optionsVideo->changed()) 
			{
				SystemConf::getInstance()->set("global.videooutput", optionsVideo->getSelected());
				SystemConf::getInstance()->saveSystemConf();				
				s->setVariable("exitreboot", true);
			}
		});
	}
	// es resolution
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RESOLUTION))
	{
	    auto videoModeOptionList = createVideoResolutionModeOptionList(mWindow, "es", "resolution");
	    s->addWithDescription(_("VIDEO MODE"), _("Sets the display's resolution for emulationstation."), videoModeOptionList);
	    s->addSaveFunc([this, videoModeOptionList] { SystemConf::getInstance()->set("es.resolution", videoModeOptionList->getSelected()); });
	}
#endif

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::AUDIODEVICE))
	{
		std::vector<std::string> availableAudio = ApiSystem::getInstance()->getAvailableAudioOutputDevices();
		if (availableAudio.size())
		{
			// audio device
			auto optionsAudio = std::make_shared<OptionListComponent<std::string> >(mWindow, _("AUDIO OUTPUT"), false);

			std::string selectedAudio = ApiSystem::getInstance()->getCurrentAudioOutputDevice();
			if (selectedAudio.empty())
				selectedAudio = "auto";

			bool afound = false;
			for (auto it = availableAudio.begin(); it != availableAudio.end(); it++)
			{
				std::vector<std::string> tokens = Utils::String::split(*it, '\t');

				if (selectedAudio == tokens.at(0))
					afound = true;

				if (tokens.size() >= 2)
				{
					// concatenat the ending words
					std::string vname = "";
					for (unsigned int i = 1; i < tokens.size(); i++)
					{
						if (i > 2) vname += " ";
						vname += tokens.at(i);
					}
					optionsAudio->add(vname, tokens.at(0), selectedAudio == tokens.at(0));
				}
				else
					optionsAudio->add((*it), (*it), selectedAudio == tokens.at(0));
			}

			if (!afound)
				optionsAudio->add(selectedAudio, selectedAudio, true);

			s->addWithLabel(_("AUDIO OUTPUT"), optionsAudio);

			s->addSaveFunc([this, optionsAudio, selectedAudio]
			{
				if (optionsAudio->changed())
				{
					SystemConf::getInstance()->set("audio.device", optionsAudio->getSelected());
					ApiSystem::getInstance()->setAudioOutputDevice(optionsAudio->getSelected());
				}
				SystemConf::getInstance()->saveSystemConf();
			});
		}

		// audio profile
		std::vector<std::string> availableAudioProfiles = ApiSystem::getInstance()->getAvailableAudioOutputProfiles();
		if (availableAudioProfiles.size())
		{
			auto optionsAudioProfile = std::make_shared<OptionListComponent<std::string> >(mWindow, _("AUDIO PROFILE"), false);

			std::string selectedAudioProfile = ApiSystem::getInstance()->getCurrentAudioOutputProfile();
			if (selectedAudioProfile.empty())
				selectedAudioProfile = "auto";

			bool afound = false;
			for (auto it = availableAudioProfiles.begin(); it != availableAudioProfiles.end(); it++)
			{
				std::vector<std::string> tokens = Utils::String::split(*it, '\t');

				if (selectedAudioProfile == tokens.at(0))
					afound = true;

				std::string vname = "";
				if (tokens.size() >= 2)
				{
					// Check if the profile contains "bluez_card" and remove it from the display name
					if (tokens.at(1).find("bluez_card") != std::string::npos)
					{
						// Skip the "bluez_card" token and use the remaining tokens for the display name
						for (unsigned int i = 2; i < tokens.size(); i++)
						{
							if (i > 2) vname += " ";
							vname += tokens.at(i);
						}
					}
					else
					{
						// Normal concatenation for other profiles
						for (unsigned int i = 1; i < tokens.size(); i++)
						{
							if (i > 1) vname += " ";
							vname += tokens.at(i);
						}
					}
					optionsAudioProfile->add(vname, tokens.at(0), selectedAudioProfile == tokens.at(0));
				}
				else
					optionsAudioProfile->add((*it), (*it), selectedAudioProfile == tokens.at(0));
			}

			if (afound == false)
				optionsAudioProfile->add(selectedAudioProfile, selectedAudioProfile, true);

			s->addWithDescription(_("AUDIO PROFILE"), _("Available options can change depending on current audio output."), optionsAudioProfile);

			s->addSaveFunc([this, optionsAudioProfile, selectedAudioProfile]
			{
				if (optionsAudioProfile->changed()) {
					SystemConf::getInstance()->set("audio.profile", optionsAudioProfile->getSelected());
					ApiSystem::getInstance()->setAudioOutputProfile(optionsAudioProfile->getSelected());
				}
				SystemConf::getInstance()->saveSystemConf();
			});
		}
	}

#ifdef BATOCERA
	// video rotation
	auto optionsRotation = std::make_shared<OptionListComponent<std::string> >(mWindow, _("SCREEN ROTATION"), false);

	std::string selectedRotation = SystemConf::getInstance()->get("display.rotate");
	if (selectedRotation.empty())
		selectedRotation = "auto";

	optionsRotation->add(_("AUTO"),              "auto", selectedRotation == "auto");
	optionsRotation->add(_("0 DEGREES"),        "0", selectedRotation == "0");
	optionsRotation->add(_("90 DEGREES"),       "1", selectedRotation == "1");
	optionsRotation->add(_("180 DEGREES"),    "2", selectedRotation == "2");
	optionsRotation->add(_("270 DEGREES"),        "3", selectedRotation == "3");

	s->addWithLabel(_("SCREEN ROTATION"), optionsRotation);

	s->addSaveFunc([this, optionsRotation, selectedRotation, s]
	{
	  if (optionsRotation->changed()) 
{
	    SystemConf::getInstance()->set("display.rotate", optionsRotation->getSelected());
	    SystemConf::getInstance()->saveSystemConf();
		s->setVariable("exitreboot", true);
	  }
	});

	// splash
	auto optionsSplash = std::make_shared<OptionListComponent<std::string> >(mWindow, _("BOOT SPLASH"), false);

	std::string enabledSplash = SystemConf::getInstance()->get("splash.screen.enabled");
	std::string soundSplash   = SystemConf::getInstance()->get("splash.screen.sound");

	std::string selectedSplash = "auto";
	if(enabledSplash == "1") {
	  selectedSplash = "splash";
	  if(soundSplash   == "0") selectedSplash = "silentsplash";
	} else {
	  if(enabledSplash == "0") selectedSplash = "nosplash";
	}

	optionsSplash->add(_("AUTO"), "auto", selectedSplash == "auto");
	optionsSplash->add(_("DEFAULT VIDEO/USER SET SPLASH"), "splash",       selectedSplash == "splash");
	optionsSplash->add(_("SILENT VIDEO/USER SET SPLASH"),  "silentsplash", selectedSplash == "silentsplash");
	optionsSplash->add(_("BATOCERA SPLASH IMAGE"),         "nosplash",     selectedSplash == "nosplash");

	s->addWithLabel(_("SPLASH SETTING"), optionsSplash);

	s->addSaveFunc([this, optionsSplash, selectedSplash]
	{
	  if (optionsSplash->changed()) {
	    if(optionsSplash->getSelected() == "auto") {
	      SystemConf::getInstance()->set("splash.screen.enabled", "");
	    } else {
	      if(optionsSplash->getSelected() == "nosplash") {
		SystemConf::getInstance()->set("splash.screen.enabled", "0");
	      } else {
		SystemConf::getInstance()->set("splash.screen.enabled", "1");
		if(optionsSplash->getSelected() == "silentsplash") {
		  SystemConf::getInstance()->set("splash.screen.sound", "0");
		} else {
		  SystemConf::getInstance()->set("splash.screen.sound", "1");
		}
	      }
	    }
	    SystemConf::getInstance()->saveSystemConf();
	  }
	});	
#else
	if (!ApiSystem::getInstance()->isScriptingSupported(ApiSystem::GAMESETTINGS))
	{
		// Retroachievements
		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RETROACHIVEMENTS))
			s->addEntry(_("RETROACHIEVEMENT SETTINGS"), true, [this] { openRetroachievementsSettings(); });

		if (SystemData::isNetplayActivated() && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::NETPLAY))
			s->addEntry(_("NETPLAY SETTINGS"), true, [this] { openNetplaySettings(); }, "iconNetplay");

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::BIOSINFORMATION))
		{
			s->addEntry(_("MISSING BIOS CHECK"), true, [this, s] { openMissingBiosSettings(); });
#ifndef _ENABLEEMUELEC
			s->addSwitch(_("CHECK BIOS FILES BEFORE RUNNING A GAME"), "CheckBiosesAtLaunch", true);
#endif
		}
	}
#endif
	std::shared_ptr<OptionListComponent<std::string>> overclock_choice;

#if ODROIDGOA || GAMEFORCE || RK3326
	// multimedia keys
	auto multimediakeys_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("MULTIMEDIA KEYS"));
	multimediakeys_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get("system.multimediakeys.enabled") != "0" && SystemConf::getInstance()->get("system.multimediakeys.enabled") != "1");
	multimediakeys_enabled->add(_("ON"), "1", SystemConf::getInstance()->get("system.multimediakeys.enabled") == "1");
	multimediakeys_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get("system.multimediakeys.enabled") == "0");
	s->addWithLabel(_("MULTIMEDIA KEYS"), multimediakeys_enabled);
	s->addSaveFunc([this, multimediakeys_enabled, s]
	{
		if (multimediakeys_enabled->changed())
		{
			SystemConf::getInstance()->set("system.multimediakeys.enabled", multimediakeys_enabled->getSelected());
			s->setVariable("reboot", true);
		}
	});
#endif

#if GAMEFORCE || RK3326
	auto buttonColor_GameForce = std::make_shared< OptionListComponent<std::string> >(mWindow, _("BUTTON LED COLOR"));
	buttonColor_GameForce->add(_("off"), "off", SystemConf::getInstance()->get("color_rgb") == "off" || SystemConf::getInstance()->get("color_rgb") == "");
	buttonColor_GameForce->add(_("red"), "red", SystemConf::getInstance()->get("color_rgb") == "red");
	buttonColor_GameForce->add(_("green"), "green", SystemConf::getInstance()->get("color_rgb") == "green");
	buttonColor_GameForce->add(_("blue"), "blue", SystemConf::getInstance()->get("color_rgb") == "blue");
	buttonColor_GameForce->add(_("white"), "white", SystemConf::getInstance()->get("color_rgb") == "white");
	buttonColor_GameForce->add(_("purple"), "purple", SystemConf::getInstance()->get("color_rgb") == "purple");
	buttonColor_GameForce->add(_("yellow"), "yellow", SystemConf::getInstance()->get("color_rgb") == "yellow");
	buttonColor_GameForce->add(_("cyan"), "cyan", SystemConf::getInstance()->get("color_rgb") == "cyan");		
	s->addWithLabel(_("BUTTON LED COLOR"), buttonColor_GameForce);
	s->addSaveFunc([buttonColor_GameForce] 
	{
		if (buttonColor_GameForce->changed()) {
			ApiSystem::getInstance()->setButtonColorGameForce(buttonColor_GameForce->getSelected());
			SystemConf::getInstance()->set("color_rgb", buttonColor_GameForce->getSelected());
		}
	});

	auto powerled_GameForce = std::make_shared< OptionListComponent<std::string> >(mWindow, _("POWER LED COLOR"));
	powerled_GameForce->add(_("heartbeat"), "heartbeat", SystemConf::getInstance()->get("option_powerled") == "heartbeat" || SystemConf::getInstance()->get("option_powerled") == "");
	powerled_GameForce->add(_("off"), "off", SystemConf::getInstance()->get("option_powerled") == "off");
	powerled_GameForce->add(_("on"), "on", SystemConf::getInstance()->get("option_powerled") == "on");	
	s->addWithLabel(_("POWER LED COLOR"), powerled_GameForce);
	s->addSaveFunc([powerled_GameForce] 
	{
		if (powerled_GameForce->changed()) {
			ApiSystem::getInstance()->setPowerLedGameForce(powerled_GameForce->getSelected());
			SystemConf::getInstance()->set("option_powerled", powerled_GameForce->getSelected());
		}
	});
#endif

	// Overclock choice
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::OVERCLOCK))
	{
		auto overclock_choice = std::make_shared<OptionListComponent<std::string>>(window, _("OVERCLOCK"), false);

		std::string currentOverclock = Settings::getInstance()->getString("Overclock");
		if (currentOverclock == "")
			currentOverclock = "none";

		std::vector<std::string> availableOverclocking = ApiSystem::getInstance()->getAvailableOverclocking();

		// Overclocking device
		bool isOneSet = false;
		for (auto it = availableOverclocking.begin(); it != availableOverclocking.end(); it++)
		{
			std::vector<std::string> tokens = Utils::String::split(*it, ' ');
			if (tokens.size() >= 2)
			{
				// concatenat the ending words
				std::string vname;
				for (unsigned int i = 1; i < tokens.size(); i++)
				{
					if (i > 1) vname += " ";
					vname += tokens.at(i);
				}
				bool isSet = currentOverclock == std::string(tokens.at(0));
				if (isSet)
					isOneSet = true;

				if (vname == "NONE" || vname == "none")
					vname = _("NONE");

				overclock_choice->add(vname, tokens.at(0), isSet);
			}
		}

		if (isOneSet == false)
		{
			if (currentOverclock == "none")
				overclock_choice->add(_("NONE"), currentOverclock, true);
			else
				overclock_choice->add(currentOverclock, currentOverclock, true);
		}

		// overclocking
		s->addWithLabel(_("OVERCLOCK"), overclock_choice);

		s->addSaveFunc([overclock_choice, window, s]
		{
			if (overclock_choice->changed() && Settings::getInstance()->setString("Overclock", overclock_choice->getSelected()))
			{
				ApiSystem::getInstance()->setOverclock(overclock_choice->getSelected());
				s->setVariable("reboot", true);
			}
		});
	}

#ifdef BATOCERA
	s->addEntry(_("DMD"), true, [this] { openDmdSettings(); });
#endif

#ifdef BATOCERA
        s->addEntry(_("MULTISCREENS"), true, [this] { openMultiScreensSettings(); });
#endif

#ifdef BATOCERA
	int red, green, blue;
	bool ledSupported = ApiSystem::getInstance()->getLED(red, green, blue);

	if (ledSupported) {
		s->addGroup(_("LED HARDWARE"));

		auto led_enabled_switch = std::make_shared<SwitchComponent>(mWindow);
		bool isEnabled = ApiSystem::getInstance()->isLEDEnabled();
		led_enabled_switch->setState(isEnabled);
		s->addWithLabel(_("ENABLE LED"), led_enabled_switch);
		
		std::string colourString = SystemConf::getInstance()->get("led.colour");
		if (colourString.empty())
			colourString = "255 0 165";

		std::stringstream ss(colourString);
		ss >> red >> green >> blue; 

		auto redLEDComponent = std::make_shared<SliderComponent>(mWindow, 0.f, 255.f, 1.f);
		auto greenLEDComponent = std::make_shared<SliderComponent>(mWindow, 0.f, 255.f, 1.f);
		auto blueLEDComponent = std::make_shared<SliderComponent>(mWindow, 0.f, 255.f, 1.f);

		redLEDComponent->setValue(red);
		redLEDComponent->setOnValueChanged([greenLEDComponent, blueLEDComponent](const float &newVal) {
			int redInt = static_cast<int>(newVal);
			int greenInt = static_cast<int>(greenLEDComponent->getValue());
			int blueInt = static_cast<int>(blueLEDComponent->getValue());
			ApiSystem::getInstance()->setLEDColours(redInt, greenInt, blueInt);
			std::string colourString = std::to_string(redInt) + " " + std::to_string(greenInt) + " " + std::to_string(blueInt);
			SystemConf::getInstance()->set("led.colour", colourString);
		});
		s->addWithLabel(_("RED"), redLEDComponent);

		greenLEDComponent->setValue(green);
		greenLEDComponent->setOnValueChanged([redLEDComponent, blueLEDComponent](const float &newVal) {
			int redInt = static_cast<int>(redLEDComponent->getValue());
			int greenInt = static_cast<int>(newVal);
			int blueInt = static_cast<int>(blueLEDComponent->getValue());
			ApiSystem::getInstance()->setLEDColours(redInt, greenInt, blueInt);
			std::string colourString = std::to_string(redInt) + " " + std::to_string(greenInt) + " " + std::to_string(blueInt);
			SystemConf::getInstance()->set("led.colour", colourString);
		});
		s->addWithLabel(_("GREEN"), greenLEDComponent);

		blueLEDComponent->setValue(blue);
		blueLEDComponent->setOnValueChanged([redLEDComponent, greenLEDComponent](const float &newVal) {
			int redInt = static_cast<int>(redLEDComponent->getValue());
			int greenInt = static_cast<int>(greenLEDComponent->getValue());
			int blueInt = static_cast<int>(newVal);
			ApiSystem::getInstance()->setLEDColours(redInt, greenInt, blueInt);
			std::string colourString = std::to_string(redInt) + " " + std::to_string(greenInt) + " " + std::to_string(blueInt);
			SystemConf::getInstance()->set("led.colour", colourString);
		});
		s->addWithLabel(_("BLUE"), blueLEDComponent);

		s->addSaveFunc([led_enabled_switch] {
			bool state = led_enabled_switch->getState();
			if (state != (SystemConf::getInstance()->get("led.enabled") != "0")) {
				ApiSystem::getInstance()->setLEDEnabled(state);
			}
		});

	}
	
	// LED brightness
	int ledBrightness;
	if (ApiSystem::getInstance()->getLEDBrightness(ledBrightness)) {
		auto ledBrightnessComponent = std::make_shared<SliderComponent>(mWindow, 0.f, 100.f, 1.f, "%");
		ledBrightnessComponent->setValue(ledBrightness);
		ledBrightnessComponent->setOnValueChanged([](const float &newVal)
		{
			ApiSystem::getInstance()->setLEDBrightness((int)Math::round(newVal));
			SystemConf::getInstance()->set("led.brightness", std::to_string((int)Math::round(newVal)));
		});

		s->addWithLabel(_("LED BRIGHTNESS"), ledBrightnessComponent);
	}
#endif

#ifdef BATOCERA
	s->addGroup(_("STORAGE"));

	// Storage device
	std::vector<std::string> availableStorage = ApiSystem::getInstance()->getAvailableStorageDevices();
	if (availableStorage.size())
	{		
		std::string selectedStorage = ApiSystem::getInstance()->getCurrentStorage();

		auto optionsStorage = std::make_shared<OptionListComponent<std::string> >(window, _("STORAGE DEVICE"), false);
		for (auto it = availableStorage.begin(); it != availableStorage.end(); it++)
		{
				if (Utils::String::startsWith(*it, "DEV"))
				{
					std::vector<std::string> tokens = Utils::String::split(*it, ' ');

					if (tokens.size() >= 3) {
						// concatenat the ending words
						std::string vname = "";
						for (unsigned int i = 2; i < tokens.size(); i++) {
							if (i > 2) vname += " ";
							vname += tokens.at(i);
						}
						optionsStorage->add(vname, (*it), selectedStorage == std::string("DEV " + tokens.at(1)));
					}
				} else {
				  std::vector<std::string> tokens = Utils::String::split(*it, ' ');
				  if (tokens.size() == 1) {
					optionsStorage->add((*it), (*it), selectedStorage == (*it));
				  } else {
				    // concatenat the ending words
				    std::string vname = "";
				    for (unsigned int i = 1; i < tokens.size(); i++) {
				      if (i > 1) vname += " ";
				      vname += tokens.at(i);
				    }
				    optionsStorage->add(_(vname.c_str()), tokens.at(0), selectedStorage == tokens.at(0));
				  }
				}
		}

		s->addWithLabel(_("STORAGE DEVICE"), optionsStorage);
		s->addSaveFunc([optionsStorage, selectedStorage, s]
		{
			if (optionsStorage->changed())
			{
				ApiSystem::getInstance()->setStorage(optionsStorage->getSelected());
				s->setVariable("reboot", true);
			}
		});
	}

	// backup
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::BACKUP))
		s->addEntry(_("BACKUP USER DATA"), true, [this] { mWindow->pushGui(new GuiBackupStart(mWindow)); });

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::INSTALL))
		s->addEntry(_("INSTALL ON A NEW DISK"), true, [this] { mWindow->pushGui(new GuiInstallStart(mWindow)); });
	
	s->addGroup(_("ADVANCED"));

	if(ApiSystem::getInstance()->isScriptingSupported(ApiSystem::SERVICES)) {
	  // Services
	  if (isFullUI)
	    s->addEntry(_("SERVICES"), true, [this] { openServicesSettings(); });
	}

	// Security
	s->addEntry(_("SECURITY"), true, [this, s] 
	{
		GuiSettings *securityGui = new GuiSettings(mWindow, _("SECURITY").c_str());
		auto securityEnabled = std::make_shared<SwitchComponent>(mWindow);
		securityEnabled->setState(SystemConf::getInstance()->get("system.security.enabled") == "1");
		securityGui->addWithDescription(_("ENFORCE SECURITY"), _("Require a password for accessing the network share."), securityEnabled);

		auto rootpassword = std::make_shared<TextComponent>(mWindow, ApiSystem::getInstance()->getRootPassword(), ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color);
		securityGui->addWithLabel(_("ROOT PASSWORD"), rootpassword);

		securityGui->addSaveFunc([this, securityEnabled, s] 
		{
			Window* window = this->mWindow;

			if (securityEnabled->changed()) 
			{
				SystemConf::getInstance()->set("system.security.enabled", securityEnabled->getState() ? "1" : "0");
				SystemConf::getInstance()->saveSystemConf();
				s->setVariable("reboot", true);				
			}
		});
		mWindow->pushGui(securityGui);
	});
#else
	if (isFullUI)
	{
		s->addGroup(_("ADVANCED"));

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::SERVICES) && ApiSystem::getInstance()->getServices().size())
			s->addEntry(_("SERVICES"), true, [this] { openServicesSettings(); });
	}
#endif
	
	// Developer options
	if (isFullUI)
		s->addEntry(_("FRONTEND DEVELOPER OPTIONS"), true, [this] { openDeveloperSettings(); });

	auto pthis = this;
	s->onFinalize([s, pthis, window]
	{
		if (s->getVariable("exitreboot") && Settings::getInstance()->getBool("ExitOnRebootRequired"))
		{
			Utils::Platform::quitES(Utils::Platform::QuitMode::QUIT);
			return;
		}
		
		if (s->getVariable("reboot") || s->getVariable("exitreboot"))
			window->displayNotificationMessage(_U("\uF011  ") + _("REBOOT REQUIRED TO APPLY THE NEW CONFIGURATION"));

		if (s->getVariable("reloadGuiMenu"))
		{
			delete pthis;
			window->pushGui(new GuiMenu(window, false));
		}
	});

	mWindow->pushGui(s);
}

void GuiMenu::openLatencyReductionConfiguration(Window* mWindow, std::string configName)
{
	GuiSettings* guiLatency = new GuiSettings(mWindow, _("LATENCY REDUCTION").c_str());

	// run-ahead
	auto runahead_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("RUN-AHEAD FRAMES"));
	runahead_enabled->addRange({ { _("AUTO"), "" }, { _("NONE"), "0" }, { "1", "1" }, { "2", "2" }, { "3", "3" }, { "4", "4" }, { "5", "5" }, { "6", "6" } }, SystemConf::getInstance()->get(configName + ".runahead"));
	guiLatency->addWithDescription(_("RUN-AHEAD FRAMES"), _("High numbers can result in visible jitter."), runahead_enabled);
	guiLatency->addSaveFunc([configName, runahead_enabled] { SystemConf::getInstance()->set(configName + ".runahead", runahead_enabled->getSelected()); });

	// second instance
	auto secondinstance = std::make_shared<OptionListComponent<std::string>>(mWindow, _("USE SECOND INSTANCE FOR RUN-AHEAD"));
	secondinstance->addRange({ { _("AUTO"), "" }, { _("ON"), "1" }, { _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".secondinstance"));
	guiLatency->addWithDescription(_("USE SECOND INSTANCE FOR RUN-AHEAD"), _("Can prevent audio skips on button presses."), secondinstance);
	guiLatency->addSaveFunc([configName, secondinstance] { SystemConf::getInstance()->set(configName + ".secondinstance", secondinstance->getSelected()); });

	// auto frame delay
	auto video_frame_delay_auto = std::make_shared<OptionListComponent<std::string>>(mWindow, _("AUTOMATIC FRAME DELAY"));
	video_frame_delay_auto->addRange({ { _("AUTO"), "" }, { _("ON"), "1" }, { _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".video_frame_delay_auto"));
	guiLatency->addWithDescription(_("AUTO FRAME DELAY"), _("Automatically decrease frame delay temporarily to prevent frame drops. Can introduce stuttering."), video_frame_delay_auto);
	guiLatency->addSaveFunc([configName, video_frame_delay_auto] { SystemConf::getInstance()->set(configName + ".video_frame_delay_auto", video_frame_delay_auto->getSelected()); });

	// variable refresh rate (freesync, gsync, etc.)
	auto vrr_runloop_enable = std::make_shared<OptionListComponent<std::string>>(mWindow, _("VARIABLE REFRESH RATE (G-SYNC, FREESYNC)"));
	vrr_runloop_enable->addRange({ { _("AUTO"), "" }, { _("ON"), "1" }, { _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".vrr_runloop_enable"));
	guiLatency->addWithDescription(_("VARIABLE REFRESH RATE"), _("Don't deviate from core requested timing. G-Sync, FreeSync, HDMI 2.1 VRR."), vrr_runloop_enable);
	guiLatency->addSaveFunc([configName, vrr_runloop_enable] { SystemConf::getInstance()->set(configName + ".vrr_runloop_enable", vrr_runloop_enable->getSelected()); });

	mWindow->pushGui(guiLatency);
}

void GuiMenu::openRetroachievementsSettings()
{
	mWindow->pushGui(new GuiRetroAchievementsSettings(mWindow));
}

void GuiMenu::openNetplaySettings()
{
	mWindow->pushGui(new GuiNetPlaySettings(mWindow));	
}

void GuiMenu::addDecorationSetOptionListComponent(Window* window, GuiSettings* parentWindow, const std::vector<DecorationSetInfo>& sets, const std::string& configName)
{
	auto decorations = std::make_shared<OptionListComponent<std::string> >(window, _("DECORATION SET"), false);
	decorations->setRowTemplate([window, sets](std::string data, ComponentListRow& row) { createDecorationItemTemplate(window, sets, data, row); });

	std::vector<std::string> items = { _("AUTO"), _("NONE") };
	for (auto set : sets)
		items.push_back(set.name);

	std::string bezel = SystemConf::getInstance()->get(configName + ".bezel");

	for (auto item : items)
		decorations->add(item, item, (bezel == item) || (bezel == "none" && item == _("NONE")) || (bezel == "" && item == _("AUTO")));

	if (!decorations->hasSelection())
		decorations->selectFirstItem();

	parentWindow->addWithLabel(_("DECORATION SET"), decorations);
	parentWindow->addSaveFunc([decorations, configName]
	{
		SystemConf::getInstance()->set(configName + ".bezel", decorations->getSelected() == _("NONE") ? "none" : decorations->getSelected() == _("AUTO") ? "" : decorations->getSelected());
	});
}



void GuiMenu::addFeatureItem(Window* window, GuiSettings* settings, const CustomFeature& feat, const std::string& configName, const std::string& system, const std::string& emulator, const std::string& core)
{	
	if (feat.preset == "hidden")
		return;

	std::string storageName = configName + "." + feat.value;
	
	if (configName == "global" && Utils::String::startsWith(feat.value, configName + "."))
		storageName = feat.value;
			
	if (feat.preset == "input")
	{
		settings->addInputTextRow(pgettext("game_options", feat.name.c_str()), storageName, false);
		return;
	}
	
	if (feat.preset == "password")
	{
		settings->addInputTextRow(pgettext("game_options", feat.name.c_str()), storageName, true);
		return;
	}
	
	if (feat.preset == "image")
	{
		settings->addFileBrowser(pgettext("game_options", feat.name.c_str()), storageName, GuiFileBrowser::IMAGES);
		return;
	}

	if (feat.preset == "video")
	{
		settings->addFileBrowser(pgettext("game_options", feat.name.c_str()), storageName, GuiFileBrowser::VIDEO);
		return;
	}

	if (feat.preset == "folder")
	{
		settings->addFileBrowser(pgettext("game_options", feat.name.c_str()), storageName, GuiFileBrowser::DIRECTORY);
		return;
	}

	if (feat.preset == "document")
	{
		settings->addFileBrowser(pgettext("game_options", feat.name.c_str()), storageName, GuiFileBrowser::MANUALS);
		return;
	}

	if (feat.preset == "files")
	{
		settings->addFileBrowser(pgettext("game_options", feat.name.c_str()), storageName, GuiFileBrowser::FILES);
		return;
	}

	std::string storedValue = SystemConf::getInstance()->get(storageName);
	
	std::string inheritedValue;
	if (!Utils::String::startsWith(storageName, "global."))
	{
		std::string systemSetting = storageName;

		bool querySystemSetting = false;

		// Look if we are using a "per-game" setting, then compute the system setting name
		auto gameInfoStart = storageName.find("[\"");
		if (gameInfoStart != std::string::npos)
		{
			auto gameInfoEnd = storageName.find("\"]");
			if (gameInfoEnd != std::string::npos)
			{
				systemSetting = storageName.substr(0, gameInfoStart) + storageName.substr(gameInfoEnd + 2);
				querySystemSetting = true;
			}
		}
	
		// First find the global option
		auto dotPos = systemSetting.find(".");
		if (dotPos != std::string::npos)
		{
			std::string globalSetting = "global." + systemSetting.substr(dotPos + 1);
			std::string globalStoredValue = SystemConf::getInstance()->get(globalSetting);
			if (!globalStoredValue.empty() && globalStoredValue != "auto" && globalStoredValue != storedValue)
				inheritedValue = globalStoredValue;
		}

		// Then take the system option
		if (querySystemSetting)
		{
			std::string systemStoredValue = SystemConf::getInstance()->get(systemSetting);
			if (!systemStoredValue.empty() && systemStoredValue != "auto" && systemStoredValue != storedValue)
				inheritedValue = systemStoredValue;
		}
	}

	if (feat.preset == "switch" || feat.preset == "switch_default_off")
	{
		auto switchComponent = std::make_shared<SwitchComponent>(window);
		switchComponent->setState(storedValue == "1");

		if (!feat.description.empty())
			settings->addWithDescription(pgettext("game_options", feat.name.c_str()), pgettext("game_options", feat.description.c_str()), switchComponent);
		else
			settings->addWithLabel(pgettext("game_options", feat.name.c_str()), switchComponent);

		settings->addSaveFunc([storageName, switchComponent] { SystemConf::getInstance()->set(storageName, switchComponent->getState() ? "1" : ""); });
		return;
	}

	if (feat.preset == "switchauto")
	{
		auto switchComponent = std::make_shared<SwitchComponent>(window);
		switchComponent->setHasAuto(true);
		switchComponent->setAutoState(storedValue == "");
		switchComponent->setState(storedValue == "1");

		if (!feat.description.empty())
			settings->addWithDescription(pgettext("game_options", feat.name.c_str()), pgettext("game_options", feat.description.c_str()), switchComponent);
		else
			settings->addWithLabel(pgettext("game_options", feat.name.c_str()), switchComponent);

		settings->addSaveFunc([storageName, switchComponent] { SystemConf::getInstance()->set(storageName, switchComponent->getAutoState() ? "" : (switchComponent->getState() ? "1" : "0")); });
		return;
	}

	if (feat.preset == "switchon" || feat.preset == "switch_default_on")
	{
		auto switchComponent = std::make_shared<SwitchComponent>(window);
		switchComponent->setState(storedValue != "0");

		if (!feat.description.empty())
			settings->addWithDescription(pgettext("game_options", feat.name.c_str()), pgettext("game_options", feat.description.c_str()), switchComponent);
		else
			settings->addWithLabel(pgettext("game_options", feat.name.c_str()), switchComponent);

		settings->addSaveFunc([storageName, switchComponent] { SystemConf::getInstance()->set(storageName, switchComponent->getState() ? "" : "0"); });
		return;
	}

	if (feat.preset == "switchoff" || feat.preset == "switch_default_off_reverse_value")
	{
		auto switchComponent = std::make_shared<SwitchComponent>(window);
		switchComponent->setState(storedValue != "1");

		if (!feat.description.empty())
			settings->addWithDescription(pgettext("game_options", feat.name.c_str()), pgettext("game_options", feat.description.c_str()), switchComponent);
		else
			settings->addWithLabel(pgettext("game_options", feat.name.c_str()), switchComponent);

		settings->addSaveFunc([storageName, switchComponent] { SystemConf::getInstance()->set(storageName, switchComponent->getState() ? "" : "1"); });
		return;
	}

	if (feat.preset == "slider")
	{
		std::vector<std::string> tokens = Utils::String::split(feat.preset_parameters, ' ');

		float slider_from    =   0.0f;
		float slider_to      = 100.0f;
		float slider_step    =   1.0f;
		float slider_default =   0.0f;
		std::string slider_suffix = "";

		if (tokens.size() >= 1) slider_from    = Utils::String::toFloat(tokens.at(0));
		if (tokens.size() >= 2) slider_to      = Utils::String::toFloat(tokens.at(1));
		if (tokens.size() >= 3) slider_step    = Utils::String::toFloat(tokens.at(2));
		if (tokens.size() >= 4) slider_default = Utils::String::toFloat(tokens.at(3));
		if (tokens.size() >= 5) slider_suffix  = tokens.at(4);

		auto sliderComponent = std::make_shared<SliderComponent>(window, slider_from, slider_to, slider_step, slider_suffix);
		if(storedValue == "") {
		  sliderComponent->setValue(slider_default);
		} else {
		  sliderComponent->setValue(Utils::String::toFloat(storedValue));
		}

		if (!feat.description.empty())
			settings->addWithDescription(pgettext("game_options", feat.name.c_str()), pgettext("game_options", feat.description.c_str()), sliderComponent);
		else
			settings->addWithLabel(pgettext("game_options", feat.name.c_str()), sliderComponent);

		settings->addSaveFunc([storageName, sliderComponent] { SystemConf::getInstance()->set(storageName, std::to_string(sliderComponent->getValue())); });
		return;
	}

	if (feat.preset == "sliderauto")
	{
		std::vector<std::string> tokens = Utils::String::split(feat.preset_parameters, ' ');
		float slider_from = 0.0f;
		float slider_to = 100.0f;
		float slider_step = 1.0f;
		std::string slider_suffix = "";

		// Parse parameters from the preset
		if (tokens.size() >= 1) slider_from = Utils::String::toFloat(tokens.at(0));
		if (tokens.size() >= 2) slider_to = Utils::String::toFloat(tokens.at(1));
		if (tokens.size() >= 3) slider_step = Utils::String::toFloat(tokens.at(2));
		if (tokens.size() >= 4) slider_suffix = tokens.at(3);

		auto sliderComponent = std::make_shared<SliderComponent>(window, slider_from, slider_to, slider_step, slider_suffix, true);

		if (storedValue.empty())
		{
			// Set to AUTO if no saved value exists
			sliderComponent->setAuto(true);
		}
		else
		{
			// Set to the stored value
			sliderComponent->setValue(Utils::String::toFloat(storedValue));
		}

		// Add the slider to the settings menu
		if (!feat.description.empty())
			settings->addWithDescription(pgettext("game_options", feat.name.c_str()), pgettext("game_options", feat.description.c_str()), sliderComponent);
		else
			settings->addWithLabel(pgettext("game_options", feat.name.c_str()), sliderComponent);

		// Save the slider value
		settings->addSaveFunc([storageName, sliderComponent] {
			float value = sliderComponent->getValue();

			// If the value is AUTO, save an empty string
			if (sliderComponent->getAuto())
			{
				SystemConf::getInstance()->set(storageName, "");
			}
			else
			{
				// Save the actual slider value
				SystemConf::getInstance()->set(storageName, std::to_string(value));
			}
			});

		return;
	}

	auto item = std::make_shared<OptionListComponent<std::string>>(window, pgettext("game_options", feat.name.c_str()));

	if (feat.preset == "shaders" || feat.preset == "shaderset")
	{
		item->add(_("AUTO"), "auto", storedValue.empty() || storedValue == "auto");

		auto shaders = ApiSystem::getInstance()->getShaderList(configName != "global" ? system : "", configName != "global" ? emulator : "", configName != "global" ? core : "");
		if (shaders.size() > 0)
		{
			item->add(_("NONE"), "none", storedValue == "none");

			for (auto shader : shaders)
			  item->add(pgettext("game_options", Utils::String::toUpper(shader).c_str()), shader, storedValue == shader);
		}
	}
	else if (feat.preset == "videofilters")
	{
		item->add(_("AUTO"), "auto", storedValue.empty() || storedValue == "auto");

		auto videofilters = ApiSystem::getInstance()->getVideoFilterList(configName != "global" ? system : "", configName != "global" ? emulator : "", configName != "global" ? core : "");
		if (videofilters.size() > 0)
		{
			item->add(_("NONE"), "none", storedValue == "none");

			for (auto videofilter : videofilters)
				item->add(pgettext("game_options", Utils::String::toUpper(videofilter).c_str()), videofilter, storedValue == videofilter);
		}
	}
	else if (feat.preset == "decorations" || feat.preset == "bezel")
	{
		item->add(_("AUTO"), "auto", storedValue.empty() || storedValue == "auto");

		auto sets = GuiMenu::getDecorationsSets(ViewController::get()->getState().getSystem());
		if (sets.size() > 0)
		{
			item->setRowTemplate([window, sets](std::string data, ComponentListRow& row) { createDecorationItemTemplate(window, sets, data, row); });
			item->add(_("NONE"), "none", storedValue == "none");

			for (auto set : sets)
				item->add(set.name, set.name, storedValue == set.name);
		}
	}
	else if (feat.preset == "videomodes" || feat.preset == "videomode")
	{
		item->add(_("AUTO"), "auto", storedValue.empty() || storedValue == "auto");

		auto modes = ApiSystem::getInstance()->getVideoModes();
		for (auto videoMode : modes)
		{
			std::vector<std::string> tokens = Utils::String::split(videoMode, ':');
			if (tokens.size() == 0)
				continue;

			std::string vname;
			for (unsigned int i = 1; i < tokens.size(); i++)
			{
				if (i > 1)
					vname += ":";

				vname += tokens.at(i);
			}

			item->add(_(vname.c_str()), tokens.at(0), storedValue == tokens.at(0));
		}
	}
	else if (feat.preset == "runners")
	{
		item->add(_("AUTO"), "auto", storedValue.empty() || storedValue == "auto");

		auto runners = ApiSystem::getInstance()->getCustomRunners();
		for (auto customRunner : runners)
		{
			item->add(_(customRunner.c_str()), customRunner, storedValue == customRunner);
		}
	}
	else
	{
		item->add(_("AUTO"), "", storedValue.empty() || storedValue == "auto");

		for (auto fval : feat.choices)
			item->add(pgettext("game_options", fval.name.c_str()), fval.value, storedValue == fval.value);
	}

	if (!item->hasSelection())
		item->selectFirstItem();

	std::string desc = pgettext("game_options", feat.description.c_str());

	if (!inheritedValue.empty())
	{
		auto displayName = item->getItemDisplayName(inheritedValue);
		if (!displayName.empty())
		{
			if (desc.empty())
				desc = _("Current setting") + " : " + displayName;
			else
				desc = desc + "\r\n" + _("Current setting") + " : " + displayName;
		}
	}

	if (!desc.empty())
		settings->addWithDescription(pgettext("game_options", feat.name.c_str()), desc, item);
	else
		settings->addWithLabel(pgettext("game_options", feat.name.c_str()), item);

	settings->addSaveFunc([item, storageName] { SystemConf::getInstance()->set(storageName, item->getSelected()); });
}

static bool hasGlobalFeature(const std::string& name)
{
	return CustomFeatures::GlobalFeatures.hasGlobalFeature(name);
}

static std::string getFeatureMenuDescription(const std::string& configName, const VectorEx<CustomFeature>& items)
{
	std::string description;

	for (auto item : items)
	{
		std::string storageName = configName + "." + item.value;
		std::string storedValue = SystemConf::getInstance()->get(storageName);
		if (!storedValue.empty())
		{
			std::string text = pgettext("game_options", item.name.c_str());

			for (auto ch : item.choices)
			{
				if (ch.value == storedValue)
				{
					storedValue = ch.name;
					break;
				}
			}

			if (item.preset == "switchoff" || item.preset == "switch_default_off_reverse_value")
			{
				if (storedValue == "0")
					storedValue = _("ON");
				else if (storedValue == "1")
					storedValue = _("OFF");
			}
			else if (Utils::String::startsWith(item.preset, "switch"))
			{
				if (storedValue == "0")
					storedValue = _("OFF");
				else if (storedValue == "1")
					storedValue = _("ON");
			}

			text += " : " + Utils::String::toUpper(storedValue);

			if (description.empty())
				description = text;
			else
				description = description + "\r\n" + text;
		}
	}

	return description;
}


void GuiMenu::addFeatures(const VectorEx<CustomFeature>& features, Window* window, GuiSettings* settings, const std::string& configName, const std::string& system, const std::string& emulator, const std::string& core, const std::string& defaultGroupName, bool addDefaultGroupOnlyIfNotFirst)
{
	bool firstGroup = true;
	
	auto groups = features.groupBy([](auto x) { return x.group; });
	for (auto group : groups)
	{
		settings->removeLastRowIfGroup();

		if (!group.first.empty())
			settings->addGroup(pgettext("game_options", group.first.c_str()));
		else if (!defaultGroupName.empty())
		{
			if (!addDefaultGroupOnlyIfNotFirst || !firstGroup)
				settings->addGroup(defaultGroupName); // _("DEFAULT GLOBAL SETTINGS")
		}

		firstGroup = false;

		std::set<std::string> processed;

		for (auto feat : group.second)
		{
			if (feat.submenu.empty())
			{
				addFeatureItem(window, settings, feat, configName, system, emulator, core);
				continue;
			}

			if (processed.find(feat.submenu) != processed.cend())
				continue;

			processed.insert(feat.submenu);

			auto items = features.where([feat](auto x) { return x.preset != "hidden" && x.submenu == feat.submenu; });
			if (items.size() > 0)
			{
				std::string label = Utils::String::toUpper(pgettext("game_options", feat.submenu.c_str()));
				std::string description = getFeatureMenuDescription(configName, items);

				std::shared_ptr<MultiLineMenuEntry> entry = std::make_shared<MultiLineMenuEntry>(window, label, description, true);

				ComponentListRow row;
				row.addElement(entry, true);

				auto arrow = makeArrow(window);
				if (EsLocale::isRTL()) arrow->setFlipX(true);
				row.addElement(arrow, false);

				row.makeAcceptInputHandler([window, configName, feat, items, system, emulator, core, settings, entry]
				{
					GuiSettings* groupSettings = new GuiSettings(window, pgettext("game_options", feat.submenu.c_str()));

					for (auto feat : items)
						addFeatureItem(window, groupSettings, feat, configName, system, emulator, core);

					groupSettings->addSaveFunc([settings, entry, configName, items]
					{
						if (entry != nullptr)
						{
							std::string newDesc = getFeatureMenuDescription(configName, items);
							if (newDesc != entry->getDescription())
							{
								entry->setDescription(newDesc);
								settings->updateSize();
							}
						}
					});

					window->pushGui(groupSettings);
				});

				settings->addRow(row);

				/*
				settings->addEntry(pgettext("game_options", feat.submenu.c_str()), true, [window, configName, feat, items, system, emulator, core]
				{
					GuiSettings* groupSettings = new GuiSettings(window, pgettext("game_options", feat.submenu.c_str()));

					for (auto feat : items)
						addFeatureItem(window, groupSettings, feat, configName, system, emulator, core);

					window->pushGui(groupSettings);
				});*/
			}
		}
	}
}

void GuiMenu::openGamesSettings() 
{
	Window* window = mWindow;

	auto s = new GuiSettings(mWindow, _("GAME SETTINGS").c_str());

	s->addGroup(_("TOOLS"));

	// Game List Update
	s->addEntry(_("UPDATE GAMELISTS"), false, [this, window] { updateGameLists(window); });

	if (SystemConf::getInstance()->getBool("global.retroachievements") && !Settings::getInstance()->getBool("RetroachievementsMenuitem") && SystemConf::getInstance()->get("global.retroachievements.username") != "")
	{
		s->addEntry(_("RETROACHIEVEMENTS").c_str(), true, [this] 
		{ 
			if (!checkNetwork())
				return;

			GuiRetroAchievements::show(mWindow); 
		});
	}
	

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RETROACHIVEMENTS) || (SystemData::isNetplayActivated() && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::NETPLAY)))
		s->addGroup(_("ACCOUNTS"));

	// Retroachievements
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RETROACHIVEMENTS))
		s->addEntry(_("RETROACHIEVEMENT SETTINGS"), true, [this] { openRetroachievementsSettings(); });

	// Netplay
	if (SystemData::isNetplayActivated() && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::NETPLAY))
		s->addEntry(_("NETPLAY SETTINGS"), true, [this] { openNetplaySettings(); }, "iconNetplay");

	// Missing Bios
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::BIOSINFORMATION))
	{
		s->addGroup(_("BIOS SETTINGS"));
		s->addEntry(_("MISSING BIOS CHECK"), true, [this, s] { openMissingBiosSettings(); });

		auto checkBiosesAtLaunch = std::make_shared<SwitchComponent>(mWindow);
		checkBiosesAtLaunch->setState(Settings::getInstance()->getBool("CheckBiosesAtLaunch"));
		s->addWithLabel(_("CHECK BIOS FILES BEFORE RUNNING A GAME"), checkBiosesAtLaunch);
		s->addSaveFunc([checkBiosesAtLaunch] { Settings::getInstance()->setBool("CheckBiosesAtLaunch", checkBiosesAtLaunch->getState()); });
	}

	// Custom config for systems
	s->addGroup(_("SAVESTATES"));

	// AUTO SAVE/LOAD
	auto autosave_enabled = std::make_shared<SwitchComponent>(mWindow);
	autosave_enabled->setState(SystemConf::getInstance()->get("global.autosave") == "1");
	s->addWithDescription(_("AUTO SAVE/LOAD"), _("Load latest savestate on game launch and savestate when exiting game."), autosave_enabled);
	s->addSaveFunc([autosave_enabled] { SystemConf::getInstance()->set("global.autosave", autosave_enabled->getState() ? "1" : ""); });

	// INCREMENTAL SAVESTATES
	auto incrementalSaveStates = std::make_shared<OptionListComponent<std::string>>(mWindow, _("INCREMENTAL SAVESTATES"));
	incrementalSaveStates->addRange({
		{ _("INCREMENT PER SAVE"), _("Never overwrite old savestates, always make new ones."), "" }, // Don't use 1 -> 1 is YES, auto too
		{ _("INCREMENT SLOT"), _("Increment slot on a new game."), "0" },
		{ _("DO NOT INCREMENT"), _("Use current slot on a new game."), "2" } },
		SystemConf::getInstance()->get("global.incrementalsavestates"));

	s->addWithLabel(_("INCREMENTAL SAVESTATES"), incrementalSaveStates);
	s->addSaveFunc([incrementalSaveStates] { SystemConf::getInstance()->set("global.incrementalsavestates", incrementalSaveStates->getSelected()); });

	// SHOW SAVE STATES
	auto showSaveStates = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW SAVESTATE MANAGER"));
	showSaveStates->addRange({ { _("NO"), "auto" },{ _("ALWAYS") , "1" },{ _("IF NOT EMPTY") , "2" } }, SystemConf::getInstance()->get("global.savestates"));
	s->addWithDescription(_("SHOW SAVESTATE MANAGER"), _("Display savestate manager before launching a game."), showSaveStates);
	s->addSaveFunc([showSaveStates] { SystemConf::getInstance()->set("global.savestates", showSaveStates->getSelected()); });

	s->addGroup(_("DEFAULT GLOBAL SETTINGS"));

	// es4all: SHOW RETROARCH FPS —— 在所有 RetroArch 核心游戏画面显示帧数。
	// 写入 global.showFPS，由启动脚本套用到 retroarch.cfg 的 fps_show。跨 target 共用。
	auto showFPS_enabled = std::make_shared<SwitchComponent>(mWindow);
	showFPS_enabled->setState(SystemConf::getInstance()->get("global.showFPS") == "1");
	s->addWithDescription(_("SHOW RETROARCH FPS"), _("Display the framerate on-screen in RetroArch games."), showFPS_enabled);
	s->addSaveFunc([showFPS_enabled] { SystemConf::getInstance()->set("global.showFPS", showFPS_enabled->getState() ? "1" : "0"); });

	// Screen ratio choice
	if (!hasGlobalFeature("ratio"))
	{
		auto ratio_choice = createRatioOptionList(mWindow, "global");
		s->addWithDescription(_("GAME ASPECT RATIO"), _("Force the game to render in this aspect ratio."), ratio_choice);
		s->addSaveFunc([ratio_choice] { SystemConf::getInstance()->set("global.ratio", ratio_choice->getSelected()); });
	}

#ifndef _ENABLEEMUELEC
	// video resolution mode
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RESOLUTION) && !hasGlobalFeature("videomode"))
	{
		auto videoModeOptionList = createVideoResolutionModeOptionList(mWindow, "global");
		s->addWithDescription(_("VIDEO MODE"), _("Sets the display's resolution. Does not affect the rendering resolution."), videoModeOptionList);
		s->addSaveFunc([this, videoModeOptionList] { SystemConf::getInstance()->set("global.videomode", videoModeOptionList->getSelected()); });
	}
#endif

	// smoothing	
	if (!hasGlobalFeature("smooth"))
	{
		auto smoothing_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SMOOTH GAMES (BILINEAR FILTERING)"));
		smoothing_enabled->addRange({ { _("AUTO"), "auto" },{ _("ON") , "1" },{ _("OFF") , "0" } }, SystemConf::getInstance()->get("global.smooth"));
		s->addWithLabel(_("SMOOTH GAMES (BILINEAR FILTERING)"), smoothing_enabled);
		s->addSaveFunc([smoothing_enabled] { SystemConf::getInstance()->set("global.smooth", smoothing_enabled->getSelected()); });
	}
#ifdef _ENABLEEMUELEC
	// bezel
	auto bezel_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("ENABLE RA BEZELS"));
	bezel_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get("global.bezel") != "0" && SystemConf::getInstance()->get("global.bezel") != "1");
	bezel_enabled->add(_("ON"), "1", SystemConf::getInstance()->get("global.bezel") == "1");
	bezel_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get("global.bezel") == "0");
	s->addWithLabel(_("ENABLE RA BEZELS"), bezel_enabled);
    s->addSaveFunc([bezel_enabled] { SystemConf::getInstance()->set("global.bezel", bezel_enabled->getSelected()); });
	
	//maxperf
	auto maxperf_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("ENABLE MAX PERFORMANCE"));
	maxperf_enabled->add(_("ON"), "1", SystemConf::getInstance()->get("global.maxperf") == "1" || SystemConf::getInstance()->get("global.maxperf") != "0");
	maxperf_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get("global.maxperf") == "0");
	s->addWithLabel(_("ENABLE MAX PERFORMANCE"), maxperf_enabled);
    s->addSaveFunc([maxperf_enabled] { SystemConf::getInstance()->set("global.maxperf", maxperf_enabled->getSelected()); });
#endif

	// rewind
	if (!hasGlobalFeature("rewind"))
	{
		auto rewind_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("REWIND"));
		rewind_enabled->addRange({ { _("AUTO"), "auto" },{ _("ON") , "1" },{ _("OFF") , "0" } }, SystemConf::getInstance()->get("global.rewind"));
		s->addWithDescription(_("REWIND"), _("Store past states to rewind to in realtime, if the core supports it."), rewind_enabled);
		s->addSaveFunc([rewind_enabled] { SystemConf::getInstance()->set("global.rewind", rewind_enabled->getSelected()); });
	}
	
	// Integer scale
	if (!hasGlobalFeature("integerscale"))
	{
		auto integerscale_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("INTEGER SCALING (PIXEL PERFECT)"));
		integerscale_enabled->addRange({ { _("AUTO"), "auto" },{ _("ON") , "1" },{ _("OFF") , "0" } }, SystemConf::getInstance()->get("global.integerscale"));
		s->addWithLabel(_("INTEGER SCALING (PIXEL PERFECT)"), integerscale_enabled);
		s->addSaveFunc([integerscale_enabled] { SystemConf::getInstance()->set("global.integerscale", integerscale_enabled->getSelected()); });
	}

#ifdef _ENABLEEMUELEC
	// Integer scale overscale
	auto integerscaleoverscale_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("INTEGER SCALING (OVERSCALE)"));
	integerscaleoverscale_enabled->addRange({ { _("AUTO"), "auto" },{ _("ON") , "1" },{ _("SMART") , "2" },{ _("OFF") , "0" } }, SystemConf::getInstance()->get("global.integerscaleoverscale"));
	s->addWithLabel(_("INTEGER SCALING (OVERSCALE)"), integerscaleoverscale_enabled);
	s->addSaveFunc([integerscaleoverscale_enabled] { SystemConf::getInstance()->set("global.integerscaleoverscale", integerscaleoverscale_enabled->getSelected()); });
#endif
	// Shaders preset
#ifndef _ENABLEEMUELEC	
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::SHADERS) && !hasGlobalFeature("shaderset"))
	{
		auto installedShaders = ApiSystem::getInstance()->getShaderList("", "", "");
		if (installedShaders.size() > 0)
		{
#endif
			std::string currentShader = SystemConf::getInstance()->get("global.shaderset");

			auto shaders_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("SHADER SET"), false);			
			shaders_choices->add(_("AUTO"), "auto", currentShader.empty() || currentShader == "auto");
			shaders_choices->add(_("NONE"), "none", currentShader == "none");

#ifdef _ENABLEEMUELEC	
	std::string a;
	for(std::stringstream ss(Utils::Platform::getShOutput(R"(/usr/bin/emuelec-utils getshaders)")); getline(ss, a, ','); )
		shaders_choices->add(a, a, currentShader == a); // emuelec
#else
			for (auto shader : installedShaders)
				shaders_choices->add(_(Utils::String::toUpper(shader).c_str()), shader, currentShader == shader);
			
			if (!shaders_choices->hasSelection())
				shaders_choices->selectFirstItem();

#endif
			s->addWithLabel(_("SHADER SET"), shaders_choices);
			s->addSaveFunc([shaders_choices] { SystemConf::getInstance()->set("global.shaderset", shaders_choices->getSelected()); });
#ifndef _ENABLEEMUELEC			
		}
	}
#endif

	// Video Filters
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::VIDEOFILTERS) && !hasGlobalFeature("videofilters"))
	{
		auto installedVideofilters = ApiSystem::getInstance()->getVideoFilterList("", "", "");
		if (installedVideofilters.size() > 0)
		{
			std::string currentVideofilter = SystemConf::getInstance()->get("global.videofilters");

			auto videofilters_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("VIDEO FILTER"), false);
			videofilters_choices->add(_("AUTO"), "auto", currentVideofilter.empty() || currentVideofilter == "auto");
			videofilters_choices->add(_("NONE"), "none", currentVideofilter == "none");

			for (auto videofilter : installedVideofilters)
				videofilters_choices->add(_(Utils::String::toUpper(videofilter).c_str()), videofilter, currentVideofilter == videofilter);

			if (!videofilters_choices->hasSelection())
				videofilters_choices->selectFirstItem();

			s->addWithLabel(_("VIDEO FILTER"), videofilters_choices);
			s->addSaveFunc([videofilters_choices] { SystemConf::getInstance()->set("global.videofilters", videofilters_choices->getSelected()); });
		}
	}

#ifdef _ENABLEEMUELEC 
#if defined(ODROIDGOA) || defined(_ENABLEGAMEFORCE)
	// RGA SCALING
	auto rgascale_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("RGA SCALING"));
	rgascale_enabled->addRange({ { _("AUTO"), "auto" },{ _("ON") , "1" },{ _("OFF") , "0" } }, SystemConf::getInstance()->get("global.rgascale"));
	s->addWithLabel(_("RGA SCALING"), rgascale_enabled);
	s->addSaveFunc([rgascale_enabled] { SystemConf::getInstance()->set("global.rgascale", rgascale_enabled->getSelected()); });
#endif
#endif
#ifndef _ENABLEEMUELEC
	// decorations
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::DECORATIONS) && !hasGlobalFeature("bezel"))
	{		
		auto sets = GuiMenu::getDecorationsSets(ViewController::get()->getState().getSystem());
		if (sets.size() > 0)
		{
#ifdef BATOCERA
			s->addEntry(_("DECORATIONS"), true, [this, sets]
			{
				GuiSettings *decorations_window = new GuiSettings(mWindow, _("DECORATIONS").c_str());

				addDecorationSetOptionListComponent(mWindow, decorations_window, sets);

				// stretch bezels
				auto bezel_stretch_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("STRETCH BEZELS (4K & ULTRAWIDE)"));
				bezel_stretch_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get("global.bezel_stretch") != "0" && SystemConf::getInstance()->get("global.bezel_stretch") != "1");
				bezel_stretch_enabled->add(_("ON"), "1", SystemConf::getInstance()->get("global.bezel_stretch") == "1");
				bezel_stretch_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get("global.bezel_stretch") == "0");
				decorations_window->addWithLabel(_("STRETCH BEZELS (4K & ULTRAWIDE)"), bezel_stretch_enabled);
				decorations_window->addSaveFunc([bezel_stretch_enabled] {
						if (bezel_stretch_enabled->changed()) {
						SystemConf::getInstance()->set("global.bezel_stretch", bezel_stretch_enabled->getSelected());
						SystemConf::getInstance()->saveSystemConf();
						}
						});

				// tattoo and controller overlays
				auto bezel_tattoo = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW TATTOO OVER BEZEL"));
				bezel_tattoo->add(_("AUTO"), "auto", SystemConf::getInstance()->get("global.bezel.tattoo") != "0"
						&& SystemConf::getInstance()->get("global.bezel.tattoo") != "system"
						&& SystemConf::getInstance()->get("global.bezel.tattoo") != "custom");
				bezel_tattoo->add(_("NO"), "0", SystemConf::getInstance()->get("global.bezel.tattoo") == "0");
				bezel_tattoo->add(_("SYSTEM CONTROLLERS"), "system", SystemConf::getInstance()->get("global.bezel.tattoo") == "system");
				bezel_tattoo->add(_("CUSTOM IMAGE"), "custom", SystemConf::getInstance()->get("global.bezel.tattoo") == "custom");
				decorations_window->addWithDescription(_("SHOW TATTOO OVER BEZEL"), _("Show an image overlaid on top of the bezel."), bezel_tattoo);
				decorations_window->addSaveFunc([bezel_tattoo] {
						if (bezel_tattoo->changed()) {
						SystemConf::getInstance()->set("global.bezel.tattoo", bezel_tattoo->getSelected());
						SystemConf::getInstance()->saveSystemConf();
						}
						});

				auto bezel_tattoo_corner = std::make_shared<OptionListComponent<std::string>>(mWindow, _("TATTOO CORNER"));
				bezel_tattoo_corner->add(_("AUTO"), "auto", SystemConf::getInstance()->get("global.bezel.tattoo_corner") != "NW"
						&& SystemConf::getInstance()->get("global.bezel.tattoo_corner") != "NE"
						&& SystemConf::getInstance()->get("global.bezel.tattoo_corner") != "SE"
						&& SystemConf::getInstance()->get("global.bezel.tattoo_corner") != "SW");
				bezel_tattoo_corner->add(_("NORTH WEST"), "NW", SystemConf::getInstance()->get("global.bezel.tattoo_corner") == "NW");
				bezel_tattoo_corner->add(_("NORTH EAST"), "NE", SystemConf::getInstance()->get("global.bezel.tattoo_corner") == "NE");
				bezel_tattoo_corner->add(_("SOUTH EAST"), "SE", SystemConf::getInstance()->get("global.bezel.tattoo_corner") == "SE");
				bezel_tattoo_corner->add(_("SOUTH WEST"), "SW", SystemConf::getInstance()->get("global.bezel.tattoo_corner") == "SW");
				decorations_window->addWithLabel(_("TATTOO CORNER"), bezel_tattoo_corner);
				decorations_window->addSaveFunc([bezel_tattoo_corner] {
						if (bezel_tattoo_corner->changed()) {
						SystemConf::getInstance()->set("global.bezel.tattoo_corner", bezel_tattoo_corner->getSelected());
						SystemConf::getInstance()->saveSystemConf();
						}
						});
				decorations_window->addInputTextRow(_("CUSTOM .PNG IMAGE PATH"), "global.bezel.tattoo_file", false);

				auto bezel_resize_tattoo = std::make_shared<SwitchComponent>(mWindow);
				bezel_resize_tattoo->setState(SystemConf::getInstance()->getBool("global.bezel.resize_tattoo"));
				decorations_window->addWithDescription(_("RESIZE TATTOO"), _("Shrink/expand tattoo to fit within the bezel's border."), bezel_resize_tattoo);
				decorations_window->addSaveFunc([bezel_resize_tattoo]
				{
					if (SystemConf::getInstance()->getBool("global.bezel.resize_tattoo") != bezel_resize_tattoo->getState()) {
						SystemConf::getInstance()->setBool("global.bezel.resize_tattoo", bezel_resize_tattoo->getState());
					}
				});

				mWindow->pushGui(decorations_window);
			});			
#else
			addDecorationSetOptionListComponent(mWindow, s, sets);
#endif
		}
	}
	
#endif	
	// latency reduction
	if (!hasGlobalFeature("runahead"))
		s->addEntry(_("LATENCY REDUCTION"), true, [this] { openLatencyReductionConfiguration(mWindow, "global"); });

	//AI-enabled translations
	if (!hasGlobalFeature("ai_service_enabled"))
	{
		s->addEntry(_("AI GAME TRANSLATION"), true, [this]
		{
			GuiSettings *ai_service = new GuiSettings(mWindow, _("AI GAME TRANSLATION").c_str());

			// AI service enabled?
			auto ai_service_enabled = std::make_shared<SwitchComponent>(mWindow);
			ai_service_enabled->setState(
				SystemConf::getInstance()->get("global.ai_service_enabled") == "1");
			ai_service->addWithLabel(_("ENABLE AI TRANSLATION SERVICE"), ai_service_enabled);

			// Target language - order is: popular languages in the Batocera community first
			// then alphabetical order of the 2-char lang code (because the strings are localized)
			auto lang_choices = std::make_shared<OptionListComponent<std::string> >(mWindow,
				_("TARGET LANGUAGE"), false);
			std::string currentLang = SystemConf::getInstance()->get("global.ai_target_lang");
			if (currentLang.empty())
				currentLang = std::string("En");
			lang_choices->add("ENGLISH", "En", currentLang == "En");
			lang_choices->add("FRANÇAIS", "Fr", currentLang == "Fr");
			lang_choices->add("PORTUGUÊS", "Pt", currentLang == "Pt");
			lang_choices->add("DEUTSCH", "De", currentLang == "De");
			lang_choices->add("GREEK", "El", currentLang == "El");
			lang_choices->add("ESPAÑOL", "Es", currentLang == "Es");
			lang_choices->add("CZECH", "Cs", currentLang == "Cs");
			lang_choices->add("DANISH", "Da", currentLang == "Da");
			lang_choices->add("CROATIAN", "Hr", currentLang == "Hr");
			lang_choices->add("HUNGARIAN", "Hu", currentLang == "Hu");
			lang_choices->add("ITALIANO", "It", currentLang == "It");
			lang_choices->add("JAPANESE", "Ja", currentLang == "Ja");
			lang_choices->add("KOREAN", "Ko", currentLang == "Ko");
			lang_choices->add("DUTCH", "Nl", currentLang == "Nl");
			lang_choices->add("NORWEGIAN", "Nn", currentLang == "Nn");
			lang_choices->add("POLISH", "Po", currentLang == "Po");
			lang_choices->add("ROMANIAN", "Ro", currentLang == "Ro");
			lang_choices->add("РУССКИЙ", "Ru", currentLang == "Ru");
			lang_choices->add("SVENSKA", "Sv", currentLang == "Sv");
			lang_choices->add("TÜRKÇE", "Tr", currentLang == "Tr");
			lang_choices->add("简体中文", "Zh", currentLang == "Zh");
			ai_service->addWithLabel(_("TARGET LANGUAGE"), lang_choices);

			// Service  URL
			ai_service->addInputTextRow(_("AI TRANSLATION SERVICE URL"), "global.ai_service_url", false);

			// Pause game for translation?
			auto ai_service_pause = std::make_shared<SwitchComponent>(mWindow);
			ai_service_pause->setState(
				SystemConf::getInstance()->get("global.ai_service_pause") == "1");
			ai_service->addWithLabel(_("PAUSE ON TRANSLATED SCREEN"), ai_service_pause);

			ai_service->addSaveFunc([ai_service_enabled, lang_choices, ai_service_pause] {
				if (ai_service_enabled->changed())
					SystemConf::getInstance()->set("global.ai_service_enabled",
						ai_service_enabled->getState() ? "1" : "0");
				if (lang_choices->changed())
					SystemConf::getInstance()->set("global.ai_target_lang",
						lang_choices->getSelected());
				if (ai_service_pause->changed())
					SystemConf::getInstance()->set("global.ai_service_pause",
						ai_service_pause->getState() ? "1" : "0");
				SystemConf::getInstance()->saveSystemConf();
			});

			mWindow->pushGui(ai_service);
		});
	}
	
	// Load global custom features
	addFeatures(CustomFeatures::GlobalFeatures, window, s, "global", "", "", "", _("DEFAULT GLOBAL SETTINGS"));
	
	if (!hasGlobalFeature("disableautocontrollers") && SystemData::sSystemVector.any([](auto sys) { return !sys->getCompatibleCoreNames(EmulatorFeatures::autocontrollers).empty(); }))
	{
		auto autoControllers = std::make_shared<SwitchComponent>(mWindow);
		autoControllers->setState(SystemConf::getInstance()->get("global.disableautocontrollers") != "1");
		s->addWithLabel(_("AUTOCONFIGURE CONTROLLERS"), autoControllers);
		s->addSaveFunc([autoControllers] { SystemConf::getInstance()->set("global.disableautocontrollers", autoControllers->getState() ? "" : "1"); });
	}

	s->addGroup(_("SYSTEM SETTINGS"));

	// Custom config for systems
	s->addEntry(_("PER SYSTEM ADVANCED CONFIGURATION"), true, [this, s, window]
		{
			s->save();
			GuiSettings* configuration = new GuiSettings(window, _("PER SYSTEM ADVANCED CONFIGURATION").c_str());

			// For each activated system
			std::vector<SystemData*> systems = SystemData::sSystemVector;
			for (auto system : systems)
			{
				if (system->isCollection() || !system->isGameSystem())
					continue;

#if defined(ES4ALL_TARGET_ROCKNIX)
				// es4all: ROCKNIX 尊重 HiddenSystems（pico-8 等被隐藏的系统不出现在
				// 此进阶设定列表；isVisible() 在 mHidden 时为 false）。
				if (!system->isVisible())
					continue;
#endif

				if (system->hasPlatformId(PlatformIds::PLATFORM_IGNORE))
					continue;

				if (!system->hasFeatures() && !system->hasEmulatorSelection())
					continue;

				configuration->addEntry(system->getFullName(), true, [this, system, window] {
					popSystemConfigurationGui(window, system);
					});
			}

			window->pushGui(configuration);
		});

	mWindow->pushGui(s);
}

void GuiMenu::openMissingBiosSettings()
{
	GuiBios::show(mWindow);
}

void GuiMenu::updateGameLists(Window* window, bool confirm)
{
	if (ThreadedScraper::isRunning())
	{
		window->pushGui(new GuiMsgBox(window, _("SCRAPER IS RUNNING. DO YOU WANT TO STOP IT?"),
			_("YES"), [] { ThreadedScraper::stop(); }, 
			_("NO"), nullptr));

		return;
	}

	if (ThreadedHasher::isRunning())
	{
		window->pushGui(new GuiMsgBox(window, _("GAME HASHING IS RUNNING. DO YOU WANT TO STOP IT?"),
			_("YES"), [] { ThreadedHasher::stop(); },
			_("NO"), nullptr));

		return;
	}
	
	if (!confirm)
	{
		ViewController::reloadAllGames(window, true, true);
		return;
	}

	window->pushGui(new GuiMsgBox(window, _("REALLY UPDATE GAMELISTS?"), _("YES"), [window]
		{
			Scripting::fireEvent("update-gamelists");
			ViewController::reloadAllGames(window, true, true);
		}, 
		_("NO"), nullptr));
}



struct ThemeConfigOption
{
	std::string defaultSettingName;
	std::string subset;
	std::shared_ptr<OptionListComponent<std::string>> component;
};

void GuiMenu::openThemeConfiguration(Window* mWindow, GuiComponent* s, std::shared_ptr<OptionListComponent<std::string>> theme_set, const std::string systemTheme)
{
	if (theme_set != nullptr && Settings::getInstance()->getString("ThemeSet") != theme_set->getSelected())
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("YOU MUST APPLY THE THEME BEFORE EDITING ITS CONFIGURATION"), _("OK")));
		return;
	}

	Window* window = mWindow;

	auto system = ViewController::get()->getState().getSystem();
	auto theme = system->getTheme();

	auto themeconfig = new GuiSettings(mWindow, (systemTheme.empty() ? _("THEME CONFIGURATION") : _("VIEW CUSTOMIZATION")).c_str());

	auto themeSubSets = theme->getSubSets();

	std::string viewName;
	bool showGridFeatures = true;
	if (!systemTheme.empty())
	{
		auto glv = ViewController::get()->getGameListView(system);
		viewName = glv->getName();
		std::string baseType = theme->getCustomViewBaseType(viewName);

		showGridFeatures = (viewName == "grid" || baseType == "grid");
	}

	// gamelist_style
	std::shared_ptr<OptionListComponent<std::string>> gamelist_style = nullptr;

	if (systemTheme.empty() || showGridFeatures && system != NULL && theme->hasView("grid"))
		themeconfig->addGroup(_("GAMELIST STYLE"));

	if (systemTheme.empty())
	{
		gamelist_style = std::make_shared< OptionListComponent<std::string> >(mWindow, _("GAMELIST VIEW STYLE"), false);

		std::vector<std::pair<std::string, std::string>> styles;
		styles.push_back(std::pair<std::string, std::string>("automatic", _("automatic")));

		bool showViewStyle = true;

		if (system != NULL)
		{
			auto mViews = theme->getViewsOfTheme();

			showViewStyle = mViews.size() > 1;

			for (auto it = mViews.cbegin(); it != mViews.cend(); ++it)
			{
				if (it->first == "basic" || it->first == "detailed" || it->first == "grid" || it->first == "video" || it->first == "gamecarousel")
					styles.push_back(std::pair<std::string, std::string>(it->first, _(it->first.c_str())));
				else
					styles.push_back(*it);
			}
		}
		else
		{
			styles.push_back(std::pair<std::string, std::string>("basic", _("basic")));
			styles.push_back(std::pair<std::string, std::string>("detailed", _("detailed")));
		}

		auto viewPreference = systemTheme.empty() ? Settings::getInstance()->getString("GamelistViewStyle") : system->getSystemViewMode();
		if (!theme->hasView(viewPreference))
			viewPreference = "automatic";

		for (auto it = styles.cbegin(); it != styles.cend(); it++)
			gamelist_style->add(it->second, it->first, viewPreference == it->first);

		if (!gamelist_style->hasSelection())
			gamelist_style->selectFirstItem();

		if (showViewStyle)
			themeconfig->addWithLabel(_("GAMELIST VIEW STYLE"), gamelist_style);
	}

	// Default grid size
	std::shared_ptr<OptionListComponent<std::string>> mGridSize = nullptr;
	if (showGridFeatures && system != NULL && theme->hasView("grid"))
	{
		Vector2f gridOverride =
			systemTheme.empty() ? Vector2f::parseString(Settings::getInstance()->getString("DefaultGridSize")) :
			system->getGridSizeOverride();

		auto ovv = std::to_string((int)gridOverride.x()) + "x" + std::to_string((int)gridOverride.y());

		mGridSize = std::make_shared<OptionListComponent<std::string>>(mWindow, _("DEFAULT GRID SIZE"), false);

		bool found = false;
		for (auto it = GuiGamelistOptions::gridSizes.cbegin(); it != GuiGamelistOptions::gridSizes.cend(); it++)
		{
			bool sel = (gridOverride == Vector2f(0, 0) && *it == "automatic") || ovv == *it;
			if (sel)
				found = true;

			mGridSize->add(_(it->c_str()), *it, sel);
		}

		if (!found)
			mGridSize->selectFirstItem();

		themeconfig->addWithLabel(_("DEFAULT GRID SIZE"), mGridSize);
	}



	std::map<std::string, ThemeConfigOption> options;

	auto subsetNames = theme->getSubSetNames(viewName);
	
	// push appliesTo at end of list
	std::stable_sort(subsetNames.begin(), subsetNames.end(), [themeSubSets](const std::string& a, const std::string& b) -> bool
	{ 
		auto sa = ThemeData::getSubSet(themeSubSets, a);
		auto sb = ThemeData::getSubSet(themeSubSets, b);

		bool aHasApplies = sa.size() > 0 && !sa.cbegin()->appliesTo.empty();
		bool bHasApplies = sb.size() > 0 && !sb.cbegin()->appliesTo.empty();

		return aHasApplies < bHasApplies;
	});

	bool hasThemeOptionGroup = false;
	bool hasApplyToGroup = false;
	for (std::string subset : subsetNames) // theme->getSubSetNames(viewName)
	{
		std::string settingName = "subset." + subset;
		std::string perSystemSettingName = systemTheme.empty() ? "" : "subset." + systemTheme + "." + subset;

		if (subset == "colorset") settingName = "ThemeColorSet";
		else if (subset == "iconset") settingName = "ThemeIconSet";
		else if (subset == "menu") settingName = "ThemeMenu";
		else if (subset == "systemview") settingName = "ThemeSystemView";
		else if (subset == "gamelistview") settingName = "ThemeGamelistView";
		else if (subset == "region") settingName = "ThemeRegionName";

		auto themeColorSets = ThemeData::getSubSet(themeSubSets, subset);

		if (themeColorSets.size() > 0)
		{
			auto selectedColorSet = themeColorSets.end();
			auto selectedName = !perSystemSettingName.empty() ? Settings::getInstance()->getString(perSystemSettingName) : Settings::getInstance()->getString(settingName);

			if (!perSystemSettingName.empty() && selectedName.empty())
				selectedName = Settings::getInstance()->getString(settingName);

			for (auto it = themeColorSets.begin(); it != themeColorSets.end() && selectedColorSet == themeColorSets.end(); it++)
				if (it->name == selectedName)
					selectedColorSet = it;

			std::string displayName;
			if (!themeColorSets.empty())
				displayName = themeColorSets.cbegin()->subSetDisplayName;

			std::shared_ptr<OptionListComponent<std::string>> item = std::make_shared<OptionListComponent<std::string> >(mWindow, displayName.empty() ? _(("THEME " + Utils::String::toUpper(subset)).c_str()) : displayName, false);
			item->setTag(!perSystemSettingName.empty() ? perSystemSettingName : settingName);

			std::string defaultName;
			for (auto it = themeColorSets.begin(); it != themeColorSets.end(); it++)
			{
				std::string displayName = it->displayName;

				if (!systemTheme.empty())
				{
					std::string defaultValue = Settings::getInstance()->getString(settingName);
					if (defaultValue.empty())
						defaultValue = system->getTheme()->getDefaultSubSetValue(subset);

					if (it->name == defaultValue)
					{
						defaultName = Utils::String::toUpper(displayName);
						// displayName = displayName + " (" + _("DEFAULT") + ")";
					}
				}

				item->add(displayName, it->name, it == selectedColorSet);
			}

			if (selectedColorSet == themeColorSets.end())
				item->selectFirstItem();

			if (!themeColorSets.empty())
			{				
				if (!displayName.empty())
				{
					bool hasApplyToSubset = themeColorSets.cbegin()->appliesTo.size() > 0;

					std::string prefix;

					if (systemTheme.empty())
					{
						for (auto subsetName : themeColorSets.cbegin()->appliesTo)
						{
							std::string pfx = theme->getViewDisplayName(subsetName);
							if (!pfx.empty())
							{
								if (prefix.empty())
									prefix = pfx;
								else
									prefix = prefix + ", " + pfx;
							}
						}

						prefix = Utils::String::toUpper(prefix);
					}

					if (hasApplyToSubset && !hasApplyToGroup)
					{
						hasApplyToGroup = true;
						themeconfig->addGroup(_("GAMELIST THEME OPTIONS"));
					}
					else if (!hasApplyToSubset && !hasThemeOptionGroup)
					{
						hasThemeOptionGroup = true;
						themeconfig->addGroup(_("THEME OPTIONS"));
					}

					if (displayName == "-" && item->size() <= 1)
					{
						ComponentListRow row;
						row.selectable = false;

						auto font = ThemeData::getMenuTheme()->TextSmall.font;
						auto text = std::make_shared<TextComponent>(mWindow, "", font, 0); 						
						text->setLineSpacing(1.0f);
						row.addElement(text, true);						

						themeconfig->addRow(row);
					}
					else if (!prefix.empty())
						themeconfig->addWithDescription(displayName, prefix, item);
					else if (!defaultName.empty())
						themeconfig->addWithDescription(displayName, _("DEFAULT VALUE") + " : " + defaultName, item);
					else 
						themeconfig->addWithLabel(displayName + prefix, item);
				}
				else
				{
					if (!hasThemeOptionGroup)
					{
						hasThemeOptionGroup = true;
						themeconfig->addGroup(_("THEME OPTIONS"));
					}

					themeconfig->addWithLabel(_(("THEME " + Utils::String::toUpper(subset)).c_str()), item);
				}
			}

			ThemeConfigOption opt;
			opt.component = item;
			opt.subset = subset;
			opt.defaultSettingName = settingName;
			options[!perSystemSettingName.empty() ? perSystemSettingName : settingName] = opt;
		}
		else
		{
			ThemeConfigOption opt;
			opt.component = nullptr;
			options[!perSystemSettingName.empty() ? perSystemSettingName : settingName] = opt;
		}
	}
	

	if (!systemTheme.empty())
	{
		themeconfig->addGroup(_("GAMELIST OPTIONS"));

		// Show favorites first in gamelists
		auto fav = Settings::getInstance()->getString(system->getName() + ".FavoritesFirst");
		auto favoritesFirst = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW FAVORITES ON TOP"), false);
		std::string defFav = Settings::getInstance()->getBool("FavoritesFirst") ? _("YES") : _("NO");
		favoritesFirst->add(_("AUTO"), "", fav == "" || fav == "auto");
		favoritesFirst->add(_("YES"), "1", fav == "1");
		favoritesFirst->add(_("NO"), "0", fav == "0");
		themeconfig->addWithDescription(_("SHOW FAVORITES ON TOP"), _("DEFAULT VALUE") + " : " + defFav, favoritesFirst);
		themeconfig->addSaveFunc([themeconfig, favoritesFirst, system]
		{
			if (Settings::getInstance()->setString(system->getName() + ".FavoritesFirst", favoritesFirst->getSelected()))
				themeconfig->setVariable("reloadAll", true);
		});

		// Show favorites first in gamelists
		auto defHid = Settings::ShowHiddenFiles() ? _("YES") : _("NO");
		auto curhid = Settings::getInstance()->getString(system->getName() + ".ShowHiddenFiles");
		auto hiddenFiles = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW HIDDEN FILES"), false);
		hiddenFiles->add(_("AUTO"), "", curhid == "" || curhid == "auto");
		hiddenFiles->add(_("YES"), "1", curhid == "1");
		hiddenFiles->add(_("NO"), "0", curhid == "0");
		themeconfig->addWithDescription(_("SHOW HIDDEN FILES"), _("DEFAULT VALUE") + " : " + defHid, hiddenFiles);
		themeconfig->addSaveFunc([themeconfig, hiddenFiles, system]
		{
			if (Settings::getInstance()->setString(system->getName() + ".ShowHiddenFiles", hiddenFiles->getSelected()))
				themeconfig->setVariable("reloadAll", true);
		});

		// Folder View Mode
		auto folderView = Settings::getInstance()->getString("FolderViewMode");
		auto defFol = folderView.empty() ? "" : Utils::String::toUpper(_(folderView.c_str()));
		auto curFol = Settings::getInstance()->getString(system->getName() + ".FolderViewMode");

		auto foldersBehavior = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW FOLDERS"), false);
		foldersBehavior->add(_("AUTO"), "", curFol == "" || curFol == "auto"); //  + " (" + defFol + ")"
		foldersBehavior->add(_("always"), "always", curFol == "always");
		foldersBehavior->add(_("never"), "never", curFol == "never");
		foldersBehavior->add(_("having multiple games"), "having multiple games", curFol == "having multiple games");

		themeconfig->addWithDescription(_("SHOW FOLDERS"), _("DEFAULT VALUE") + " : " + defFol, foldersBehavior);
		themeconfig->addSaveFunc([themeconfig, foldersBehavior, system]
		{
			if (Settings::getInstance()->setString(system->getName() + ".FolderViewMode", foldersBehavior->getSelected()))
				themeconfig->setVariable("reloadAll", true);
		});
		
		// Show parent folder in gamelists
		auto defPf = Settings::getInstance()->getBool("ShowParentFolder") ? _("YES") : _("NO");
		auto curPf = Settings::getInstance()->getString(system->getName() + ".ShowParentFolder");
		auto parentFolder = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW '..' PARENT FOLDER"), false);
		parentFolder->add(_("AUTO"), "", curPf == "" || curPf == "auto");
		parentFolder->add(_("YES"), "1", curPf == "1");
		parentFolder->add(_("NO"), "0", curPf == "0");
		themeconfig->addWithDescription(_("SHOW '..' PARENT FOLDER"), _("DEFAULT VALUE") + " : " + defPf, parentFolder);
		themeconfig->addSaveFunc([themeconfig, parentFolder, system]
		{
			if (Settings::getInstance()->setString(system->getName() + ".ShowParentFolder", parentFolder->getSelected()))
				themeconfig->setVariable("reloadAll", true);
		});

		// Show flags

		auto defSF = Settings::getInstance()->getString("ShowFlags");
		if (defSF == "1")
			defSF = _("BEFORE NAME");
		else if (defSF == "2")
			defSF = _("AFTER NAME");
		else 
			defSF = _("NO");
		
		auto curSF = Settings::getInstance()->getString(system->getName() + ".ShowFlags");
		auto showRegionFlags = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW REGION FLAG"), false);

		showRegionFlags->addRange({ 
			{ _("AUTO"), "auto" },
			{ _("NO"), "0" },
			{ _("BEFORE NAME") , "1" },
			{ _("AFTER NAME"), "2" } }, 
			curSF);

		themeconfig->addWithDescription(_("SHOW REGION FLAG"), _("DEFAULT VALUE") + " : " + defSF, showRegionFlags);
		themeconfig->addSaveFunc([themeconfig, showRegionFlags, system]
		{
			if (Settings::getInstance()->setString(system->getName() + ".ShowFlags", showRegionFlags->getSelected()))
				themeconfig->setVariable("reloadAll", true);
		});
		
		// Show SaveStates
		auto defSS = Settings::getInstance()->getBool("ShowSaveStates") ? _("YES") : _("NO");
		auto curSS = Settings::getInstance()->getString(system->getName() + ".ShowSaveStates");
		auto showSaveStates = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW SAVESTATE ICON"), false);
		showSaveStates->add(_("AUTO"), "", curSS == "" || curSS == "auto");
		showSaveStates->add(_("YES"), "1", curSS == "1");
		showSaveStates->add(_("NO"), "0", curSS == "0");
		themeconfig->addWithDescription(_("SHOW SAVESTATE ICON"), _("DEFAULT VALUE") + " : " + defSS, showSaveStates);
		themeconfig->addSaveFunc([themeconfig, showSaveStates, system]
		{
			if (Settings::getInstance()->setString(system->getName() + ".ShowSaveStates", showSaveStates->getSelected()))
				themeconfig->setVariable("reloadAll", true);
		});

		// Show Manual
		auto defMM = Settings::getInstance()->getBool("ShowManualIcon") ? _("YES") : _("NO");
		auto curMM = Settings::getInstance()->getString(system->getName() + ".ShowManualIcon");
		auto showManual = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW MANUAL ICON"), false);
		showManual->add(_("AUTO"), "", curMM == "" || curMM == "auto");
		showManual->add(_("YES"), "1", curMM == "1");
		showManual->add(_("NO"), "0", curMM == "0");
		themeconfig->addWithDescription(_("SHOW MANUAL ICON"), _("DEFAULT VALUE") + " : " + defMM, showManual);
		themeconfig->addSaveFunc([themeconfig, showManual, system]
		{
			if (Settings::getInstance()->setString(system->getName() + ".ShowManualIcon", showManual->getSelected()))
				themeconfig->setVariable("reloadAll", true);
		});

#ifdef _ENABLEEMUELEC
	auto enable_hideSortName = std::make_shared<SwitchComponent>(window);
	bool hideSortNameEnabled = SystemConf::getInstance()->get(system->getName() + ".hideSortNames") == "1";
	enable_hideSortName->setState(hideSortNameEnabled);
	themeconfig->addWithLabel(_("HIDE SORTNAMES IN GAMELIST"), enable_hideSortName);

	themeconfig->addSaveFunc([enable_hideSortName, mWindow, system, themeconfig] {
		bool hideSortNameEnabled = enable_hideSortName->getState();
		bool hideSortNameEnabled2 = SystemConf::getInstance()->get(system->getName() + ".hideSortNames") == "1";
		if (hideSortNameEnabled != hideSortNameEnabled2)
			themeconfig->setVariable("reloadAll", true);

		SystemConf::getInstance()->set(system->getName() + ".hideSortNames", hideSortNameEnabled ? "1" : "");
	});
#endif

		// Show Cheevos
		auto defCI = Settings::getInstance()->getBool("ShowCheevosIcon") ? _("YES") : _("NO");
		auto curCI = Settings::getInstance()->getString(system->getName() + ".ShowCheevosIcon");
		auto showCheevos = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW RETROACHIEVEMENTS ICON"), false);
		showCheevos->add(_("AUTO"), "", curCI == "" || curCI == "auto");
		showCheevos->add(_("YES"), "1", curCI == "1");
		showCheevos->add(_("NO"), "0", curCI == "0");
		themeconfig->addWithDescription(_("SHOW RETROACHIEVEMENTS ICON"), _("DEFAULT VALUE") + " : " + defCI, showCheevos);
		themeconfig->addSaveFunc([themeconfig, showCheevos, system]
			{
				if (Settings::getInstance()->setString(system->getName() + ".ShowCheevosIcon", showCheevos->getSelected()))
					themeconfig->setVariable("reloadAll", true);
			});

		// Show gun icons
		auto defGI = Settings::getInstance()->getBool("ShowGunIconOnGames") ? _("YES") : _("NO");
		auto curGI = Settings::getInstance()->getString(system->getName() + ".ShowGunIconOnGames");
		auto showGun = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW GUN ICON"), false);
		showGun->add(_("AUTO"), "", curGI == "" || curGI == "auto");
		showGun->add(_("YES"), "1", curGI == "1");
		showGun->add(_("NO"), "0", curGI == "0");
		themeconfig->addWithDescription(_("SHOW GUN ICON"), _("DEFAULT VALUE") + " : " + defGI, showGun);
		themeconfig->addSaveFunc([themeconfig, showGun, system]
			{
				if (Settings::getInstance()->setString(system->getName() + ".ShowGunIconOnGames", showGun->getSelected()))
					themeconfig->setVariable("reloadAll", true);
			});

		// Show wheel icons
		auto defWI = Settings::getInstance()->getBool("ShowWheelIconOnGames") ? _("YES") : _("NO");
		auto curWI = Settings::getInstance()->getString(system->getName() + ".ShowWheelIconOnGames");
		auto showWheel = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW WHEEL ICON"), false);
		showWheel->add(_("AUTO"), "", curWI == "" || curWI == "auto");
		showWheel->add(_("YES"), "1", curWI == "1");
		showWheel->add(_("NO"), "0", curWI == "0");
		themeconfig->addWithDescription(_("SHOW WHEEL ICON"), _("DEFAULT VALUE") + " : " + defWI, showWheel);
		themeconfig->addSaveFunc([themeconfig, showWheel, system]
			{
				if (Settings::getInstance()->setString(system->getName() + ".ShowWheelIconOnGames", showWheel->getSelected()))
					themeconfig->setVariable("reloadAll", true);
			});

		// Show trackball icons
		auto defTI = Settings::getInstance()->getBool("ShowTrackballIconOnGames") ? _("YES") : _("NO");
		auto curTI = Settings::getInstance()->getString(system->getName() + ".ShowTrackballIconOnGames");
		auto showTrackball = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW TRACKBALL ICON"), false);
		showTrackball->add(_("AUTO"), "", curTI == "" || curTI == "auto");
		showTrackball->add(_("YES"), "1", curTI == "1");
		showTrackball->add(_("NO"), "0", curTI == "0");
		themeconfig->addWithDescription(_("SHOW TRACKBALL ICON"), _("DEFAULT VALUE") + " : " + defTI, showTrackball);
		themeconfig->addSaveFunc([themeconfig, showTrackball, system]
			{
				if (Settings::getInstance()->setString(system->getName() + ".ShowTrackballIconOnGames", showTrackball->getSelected()))
					themeconfig->setVariable("reloadAll", true);
			});

		// Show spinner icons
		auto defSI = Settings::getInstance()->getBool("ShowSpinnerIconOnGames") ? _("YES") : _("NO");
		auto curSI = Settings::getInstance()->getString(system->getName() + ".ShowSpinnerIconOnGames");
		auto showSpinner = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW SPINNER ICON"), false);
		showSpinner->add(_("AUTO"), "", curSI == "" || curSI == "auto");
		showSpinner->add(_("YES"), "1", curSI == "1");
		showSpinner->add(_("NO"), "0", curSI == "0");
		themeconfig->addWithDescription(_("SHOW SPINNER ICON"), _("DEFAULT VALUE") + " : " + defSI, showSpinner);
		themeconfig->addSaveFunc([themeconfig, showSpinner, system]
			{
				if (Settings::getInstance()->setString(system->getName() + ".ShowSpinnerIconOnGames", showSpinner->getSelected()))
					themeconfig->setVariable("reloadAll", true);
			});

		// Show filenames
		auto defFn = Settings::getInstance()->getBool("ShowFilenames") ? _("YES") : _("NO");
		auto curFn = Settings::getInstance()->getString(system->getName() + ".ShowFilenames");

		auto showFilenames = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW FILENAMES INSTEAD"), false);
		showFilenames->add(_("AUTO"), "", curFn == "");
		showFilenames->add(_("YES"), "1", curFn == "1");
		showFilenames->add(_("NO"), "0", curFn == "0");
		themeconfig->addWithDescription(_("SHOW FILENAMES INSTEAD"), _("DEFAULT VALUE") + " : " + defFn, showFilenames);
		themeconfig->addSaveFunc([themeconfig, showFilenames, system]
		{
			if (Settings::getInstance()->setString(system->getName() + ".ShowFilenames", showFilenames->getSelected()))
			{
				SystemData::resetSettings();
				FileData::resetSettings();

		//		themeconfig->setVariable("reloadCollections", true);
				themeconfig->setVariable("reloadAll", true);				
			}
		});
		

		// File extensions
		if (!system->isCollection() && system->isGameSystem())
		{
			auto hiddenExts = Utils::String::split(Settings::getInstance()->getString(system->getName() + ".HiddenExt"), ';');

			auto hiddenCtrl = std::make_shared<OptionListComponent<std::string>>(mWindow, _("FILE EXTENSIONS"), true);

			for (auto ext : system->getExtensions())
			{
				std::string extid = Utils::String::toLower(Utils::String::replace(ext, ".", ""));
				hiddenCtrl->add(ext, extid, std::find(hiddenExts.cbegin(), hiddenExts.cend(), extid) == hiddenExts.cend());
			}

			themeconfig->addWithLabel(_("FILE EXTENSIONS"), hiddenCtrl);
			themeconfig->addSaveFunc([themeconfig, system, hiddenCtrl]
			{
				std::string hiddenSystems;

				std::vector<std::string> sel = hiddenCtrl->getSelectedObjects();

				for (auto ext : system->getExtensions())
				{
					std::string extid = Utils::String::toLower(Utils::String::replace(ext, ".", ""));
					if (std::find(sel.cbegin(), sel.cend(), extid) == sel.cend())
					{
						if (hiddenSystems.empty())
							hiddenSystems = extid;
						else
							hiddenSystems = hiddenSystems + ";" + extid;
					}
				}

				if (Settings::getInstance()->setString(system->getName() + ".HiddenExt", hiddenSystems))
				{
					Settings::getInstance()->saveFile();

					themeconfig->setVariable("reloadAll", true);
					themeconfig->setVariable("forceReloadGames", true);
				}
			});
		}
	}

	if (systemTheme.empty())
	{
		themeconfig->addGroup(_("TOOLS"));

		themeconfig->addEntry(_("RESET CUSTOMIZATIONS"), false, [s, themeconfig, window]
		{
			themeconfig->setVariable("resetTheme", true);
			themeconfig->setVariable("reloadAll", true);
			themeconfig->close();
		});
	}

	//  theme_colorset, theme_iconset, theme_menu, theme_systemview, theme_gamelistview, theme_region,
	themeconfig->addSaveFunc([systemTheme, system, themeconfig, options, gamelist_style, mGridSize, window]
	{
		bool reloadAll = false;

		for (auto option : options)
		{
			ThemeConfigOption& opt = option.second;

			std::string value;

			if (opt.component != nullptr)
			{
				value = opt.component->getSelected();

				if (!systemTheme.empty() && !value.empty())
				{
					std::string defaultValue = Settings::getInstance()->getString(opt.defaultSettingName);
					if (defaultValue.empty())
						defaultValue = system->getTheme()->getDefaultSubSetValue(opt.subset);

					if (value == defaultValue)
						value = "";
				}
				else if (systemTheme.empty() && value == system->getTheme()->getDefaultSubSetValue(opt.subset))
					value = "";
			}

			if (value != Settings::getInstance()->getString(option.first))
				reloadAll |= Settings::getInstance()->setString(option.first, value);
		}

		Vector2f gridSizeOverride(0, 0);

		if (mGridSize != nullptr)
		{
			std::string str = mGridSize->getSelected();
			std::string value = "";

			size_t divider = str.find('x');
			if (divider != std::string::npos)
			{
				std::string first = str.substr(0, divider);
				std::string second = str.substr(divider + 1, std::string::npos);

				gridSizeOverride = Vector2f((float)atof(first.c_str()), (float)atof(second.c_str()));
				value = Utils::String::replace(Utils::String::replace(gridSizeOverride.toString(), ".000000", ""), "0 0", "");
			}

			if (systemTheme.empty())
				reloadAll |= Settings::getInstance()->setString("DefaultGridSize", value);
		}
		else if (systemTheme.empty())
			reloadAll |= Settings::getInstance()->setString("DefaultGridSize", "");

		if (systemTheme.empty())
			reloadAll |= Settings::getInstance()->setString("GamelistViewStyle", gamelist_style == nullptr ? "" : gamelist_style->getSelected());
		else
		{
			std::string viewMode = gamelist_style == nullptr ? system->getSystemViewMode() : gamelist_style->getSelected();
			reloadAll |= system->setSystemViewMode(viewMode, gridSizeOverride);
		}

		if (themeconfig->getVariable("resetTheme"))
		{
			Settings::getInstance()->setString("GamelistViewStyle", "");
			Settings::getInstance()->setString("DefaultGridSize", "");
			Settings::getInstance()->setString("ThemeRegionName", "");
			Settings::getInstance()->setString("ThemeColorSet", "");
			Settings::getInstance()->setString("ThemeIconSet", "");
			Settings::getInstance()->setString("ThemeMenu", "");
			Settings::getInstance()->setString("ThemeSystemView", "");
			Settings::getInstance()->setString("ThemeGamelistView", "");
			Settings::getInstance()->setString("GamelistViewStyle", "");
			Settings::getInstance()->setString("DefaultGridSize", "");

			for (auto sm : Settings::getInstance()->getStringMap())
				if (Utils::String::startsWith(sm.first, "subset."))
					Settings::getInstance()->setString(sm.first, "");

			for (auto system : SystemData::sSystemVector)
			{
				system->setSystemViewMode("automatic", Vector2f(0, 0));

				Settings::getInstance()->setString(system->getName() + ".FavoritesFirst", "");
				Settings::getInstance()->setString(system->getName() + ".ShowHiddenFiles", "");
				Settings::getInstance()->setString(system->getName() + ".FolderViewMode", "");
				Settings::getInstance()->setString(system->getName() + ".ShowFilenames", "");
				Settings::getInstance()->setString(system->getName() + ".ShowParentFolder", "");
			}

			Settings::getInstance()->saveFile();
			std::string path = Paths::getUserEmulationStationPath() + "/themesettings/" + Settings::getInstance()->getString("ThemeSet") + ".cfg";
			if (Utils::FileSystem::exists(path))
				Utils::FileSystem::removeFile(path);
		}

		if (reloadAll || themeconfig->getVariable("reloadAll"))
		{
			if (themeconfig->getVariable("forceReloadGames"))
			{
				ViewController::reloadAllGames(window, false);
			}
			else if (systemTheme.empty())
			{				
				CollectionSystemManager::get()->updateSystemsList();
				ViewController::get()->reloadAll(window);
				window->closeSplashScreen();
			}
			else
			{
				system->loadTheme();
				system->resetFilters();

				ViewController::get()->reloadSystemListViewTheme(system);
				ViewController::get()->reloadGameListView(system);
			}
		}
	});

	mWindow->pushGui(themeconfig);
}

void GuiMenu::openUISettings() 
{
	auto pthis = this;
	Window* window = mWindow;

	auto s = new GuiSettings(mWindow, _("USER INTERFACE SETTINGS").c_str());

	// theme set
	auto theme = ThemeData::getMenuTheme();
	auto themeSets = ThemeData::getThemeSets();
	auto system = ViewController::get()->getState().getSystem();

	s->addGroup(_("APPEARANCE"));

	if (system != nullptr && !themeSets.empty())
	{		
		auto selectedSet = themeSets.find(Settings::getInstance()->getString("ThemeSet"));
		if (selectedSet == themeSets.end())
			selectedSet = themeSets.begin();

		auto theme_set = std::make_shared<OptionListComponent<std::string> >(mWindow, _("THEME SET"), false);

		std::vector<std::string> themeList;
		for (auto it = themeSets.begin(); it != themeSets.end(); it++)
			themeList.push_back(it->first);

		std::sort(themeList.begin(), themeList.end(), [](const std::string& a, const std::string& b) -> bool { return Utils::String::toLower(a).compare(Utils::String::toLower(b)) < 0; });

		for (auto themeName : themeList)
			theme_set->add(themeName, themeName, themeName == selectedSet->first);

		//for (auto it = themeSets.begin(); it != themeSets.end(); it++)
		//	theme_set->add(it->first, it->first, it == selectedSet);

		s->addWithLabel(_("THEME SET"), theme_set);
		s->addSaveFunc([s, theme_set, pthis, window, system]
		{
			std::string oldTheme = Settings::getInstance()->getString("ThemeSet");
			if (oldTheme != theme_set->getSelected())
			{			
				saveSubsetSettings();

				Settings::getInstance()->setString("ThemeSet", theme_set->getSelected());

				// theme changed without setting options, forcing options to avoid crash/blank theme
				Settings::getInstance()->setString("ThemeRegionName", "");
				Settings::getInstance()->setString("ThemeColorSet", "");
				Settings::getInstance()->setString("ThemeIconSet", "");
				Settings::getInstance()->setString("ThemeMenu", "");
				Settings::getInstance()->setString("ThemeSystemView", "");
				Settings::getInstance()->setString("ThemeGamelistView", "");
				Settings::getInstance()->setString("GamelistViewStyle", "");
				Settings::getInstance()->setString("DefaultGridSize", "");

				for(auto sm : Settings::getInstance()->getStringMap())
					if (Utils::String::startsWith(sm.first, "subset."))
						Settings::getInstance()->setString(sm.first, "");

				for (auto sysIt = SystemData::sSystemVector.cbegin(); sysIt != SystemData::sSystemVector.cend(); sysIt++)
					(*sysIt)->setSystemViewMode("automatic", Vector2f(0,0));

				loadSubsetSettings(theme_set->getSelected());

				s->setVariable("reloadCollections", true);
				s->setVariable("reloadAll", true);
				s->setVariable("reloadGuiMenu", true);

				Scripting::fireEvent("theme-changed", theme_set->getSelected(), oldTheme);				
			}
		});

		bool showThemeConfiguration = system->getTheme()->hasSubsets() || system->getTheme()->hasView("grid");
		if (showThemeConfiguration)
		{
			s->addSubMenu(_("THEME CONFIGURATION"), [this, s, theme_set]() { openThemeConfiguration(mWindow, s, theme_set); });
		}
		else // GameList view style only, acts like Retropie for simple themes
		{
			auto gamelist_style = std::make_shared< OptionListComponent<std::string> >(mWindow, _("GAMELIST VIEW STYLE"), false);
			std::vector<std::pair<std::string, std::string>> styles;
			styles.push_back(std::pair<std::string, std::string>("automatic", _("automatic")));

			auto system = ViewController::get()->getState().getSystem();
			if (system != NULL)
			{
				auto mViews = system->getTheme()->getViewsOfTheme();
				for (auto it = mViews.cbegin(); it != mViews.cend(); ++it)
					styles.push_back(*it);
			}
			else
			{
				styles.push_back(std::pair<std::string, std::string>("basic", _("basic")));
				styles.push_back(std::pair<std::string, std::string>("detailed", _("detailed")));
				styles.push_back(std::pair<std::string, std::string>("video", _("video")));
				styles.push_back(std::pair<std::string, std::string>("grid", _("grid")));				
			}

			auto viewPreference = Settings::getInstance()->getString("GamelistViewStyle");
			if (!system->getTheme()->hasView(viewPreference))
				viewPreference = "automatic";

			for (auto it = styles.cbegin(); it != styles.cend(); it++)
				gamelist_style->add(it->second, it->first, viewPreference == it->first);

			s->addWithLabel(_("GAMELIST VIEW STYLE"), gamelist_style);
			s->addSaveFunc([s, gamelist_style, window] {
				if (Settings::getInstance()->setString("GamelistViewStyle", gamelist_style->getSelected()))
				{
					s->setVariable("reloadAll", true);
					s->setVariable("reloadGuiMenu", true);
				}
			});
		}		
	}

	s->addGroup(_("DISPLAY OPTIONS"));
	s->addEntry(_("SCREENSAVER SETTINGS"), true, std::bind(&GuiMenu::openScreensaverOptions, this));
	s->addOptionList(_("LIST TRANSITION"), { { _("auto"), "auto" },{ _("fade"), "fade" },{ _("slide"), "slide" },{ _("fade & slide"), "fade & slide" },{ _("instant"), "instant" } }, "TransitionStyle", true);
	s->addOptionList(_("GAME LAUNCH TRANSITION"), { { _("auto"), "auto" },{ _("fade"), "fade" },{ _("fast fade"), "fast fade" },{ _("slide"), "slide" },{ _("fast slide"), "fast slide" },{ _("instant"), "instant" } }, "GameTransitionStyle", true);

	s->addSwitch(_("GAME MEDIAS DURING FAST SCROLLING"), "ScrollLoadMedias", false); 

	s->addSwitch(_("SHOW CLOCK"), "DrawClock", true);
	s->addSwitch(_("ON-SCREEN HELP"), "ShowHelpPrompts", true, [s] { s->setVariable("reloadAll", true); });

	if (Utils::Platform::queryBatteryInformation().hasBattery)
		s->addOptionList(_("SHOW BATTERY STATUS"), { { _("NO"), "" },{ _("ICON"), "icon" },{ _("ICON AND TEXT"), "text" } }, "ShowBattery", true);

	s->addGroup(_("GAMELIST OPTIONS"));
	s->addSwitch(_("SHOW FAVORITES ON TOP"), "FavoritesFirst", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW HIDDEN FILES"), "ShowHiddenFiles", true, [s] { s->setVariable("reloadAll", true); });
	s->addOptionList(_("SHOW FOLDERS"), { { _("always"), "always" },{ _("never") , "never" },{ _("having multiple games"), "having multiple games" } }, "FolderViewMode", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW FOLDERS FIRST"), "ShowFoldersFirst", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW '..' PARENT FOLDER"), "ShowParentFolder", true, [s] { s->setVariable("reloadAll", true); });
	s->addOptionList(_("SHOW REGION FLAG"), { { _("NO"), "auto" },{ _("BEFORE NAME") , "1" },{ _("AFTER NAME"), "2" } }, "ShowFlags", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW SAVESTATE ICON"), "ShowSaveStates", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW MANUAL ICON"), "ShowManualIcon", true, [s] { s->setVariable("reloadAll", true); });	
	s->addSwitch(_("SHOW RETROACHIEVEMENTS ICON"), "ShowCheevosIcon", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW GUN ICON"), "ShowGunIconOnGames", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW WHEEL ICON"), "ShowWheelIconOnGames", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW TRACKBALL ICON"), "ShowTrackballIconOnGames", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW SPINNER ICON"), "ShowSpinnerIconOnGames", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW FILENAMES INSTEAD"), "ShowFilenames", true, [s] 
		{
			SystemData::resetSettings();
			FileData::resetSettings();

			s->setVariable("reloadCollections", true);
			s->setVariable("reloadAll", true); 
		});
	s->addSwitch(_("IGNORE LEADING ARTICLES WHEN SORTING"), _("Ignore 'The' and 'A' if at the start."), "IgnoreLeadingArticles", true, [s] { s->setVariable("reloadAll", true); });
	
	s->onFinalize([s, pthis, window]
	{
		if (s->getVariable("reloadCollections"))
			CollectionSystemManager::get()->updateSystemsList();

		if (s->getVariable("reloadAll"))
		{
			ViewController::get()->reloadAll(nullptr);
			window->closeSplashScreen();
		}

		if (s->getVariable("reloadGuiMenu"))
		{
			delete pthis;
			window->pushGui(new GuiMenu(window));
		}
	});

	mWindow->pushGui(s);
}

void GuiMenu::openSoundSettings()
{
	auto s = new GuiSettings(mWindow, _("SOUND SETTINGS").c_str());
	
#ifdef _ENABLEEMUELEC
	s->setUpdateType(ComponentListFlags::UPDATE_ALWAYS);
#endif


	if (VolumeControl::getInstance()->isAvailable())
	{
		s->addGroup(_("VOLUME"));

		// volume
		auto volume = std::make_shared<SliderComponent>(mWindow, 0.f, 100.f, 1.f, "%");
#ifdef _ENABLEEMUELEC
		std::string cfgAudioVolume = SystemConf::getInstance()->get("audio.volume");
		if (!cfgAudioVolume.empty()) {
			VolumeControl::getInstance()->setVolume((int)atoi(cfgAudioVolume.c_str()));
		}
#endif
		volume->setValue((float)VolumeControl::getInstance()->getVolume());
		volume->setOnValueChanged([](const float &newVal) { VolumeControl::getInstance()->setVolume((int)Math::round(newVal)); });
		s->addWithLabel(_("SYSTEM VOLUME"), volume);
		s->addSaveFunc([this, volume]
		{
			VolumeControl::getInstance()->setVolume((int)Math::round(volume->getValue()));
#if !WIN32
			SystemConf::getInstance()->set("audio.volume", std::to_string((int)round(volume->getValue())));
#endif
#ifdef _ENABLEEMUELEC
			SystemConf::getInstance()->saveSystemConf();
#endif
		});


		// Music Volume
		auto musicVolume = std::make_shared<SliderComponent>(mWindow, 0.f, 100.f, 1.f, "%");
		musicVolume->setValue(Settings::getInstance()->getInt("MusicVolume"));
		musicVolume->setOnValueChanged([](const float &newVal) { Settings::getInstance()->setInt("MusicVolume", (int)round(newVal)); });
		s->addWithLabel(_("MUSIC VOLUME"), musicVolume);

		s->addSwitch(_("SHOW OVERLAY WHEN VOLUME CHANGES"), "VolumePopup", true);
	}

	s->addGroup(_("MUSIC"));

	s->addSwitch(_("FRONTEND MUSIC"), "audio.bgmusic", true, []
	{
		if (Settings::getInstance()->getBool("audio.bgmusic"))
			AudioManager::getInstance()->playRandomMusic();
		else
			AudioManager::getInstance()->stopMusic();
	});
	
	s->addSwitch(_("DISPLAY SONG TITLES"), "audio.display_titles", true);
 
	// how long to display the song titles?
	auto titles_time = std::make_shared<SliderComponent>(mWindow, 2.f, 120.f, 2.f, "s");
	titles_time->setValue(Settings::getInstance()->getInt("audio.display_titles_time"));
	s->addWithLabel(_("SONG TITLE DISPLAY DURATION"), titles_time);
	s->addSaveFunc([titles_time] {
		Settings::getInstance()->setInt("audio.display_titles_time", (int)Math::round(titles_time->getValue()));
	});

	s->addSwitch(_("ONLY PLAY SYSTEM-SPECIFIC MUSIC FOLDER"), "audio.persystem", true, [] { AudioManager::getInstance()->changePlaylist(ViewController::get()->getState().getSystem()->getTheme(), true); } );
	s->addSwitch(_("PLAY SYSTEM-SPECIFIC MUSIC"), "audio.thememusics", true, [] { AudioManager::getInstance()->changePlaylist(ViewController::get()->getState().getSystem()->getTheme(), true); });	
	s->addSwitch(_("LOWER MUSIC WHEN PLAYING VIDEO"), "VideoLowersMusic", true);


    auto favoriteSwitch = std::make_shared<SwitchComponent>(mWindow);
    std::string favoritesFile = FavoriteMusicManager::getFavoriteMusicFilePath();
    bool hasFavorites = false;
    if (Utils::FileSystem::exists(favoritesFile))
    {
        auto favorites = FavoriteMusicManager::loadFavoriteSongs(favoritesFile);
        hasFavorites = !favorites.empty();
    }
    bool shouldUseFavorites = Settings::getInstance()->getBool("audio.useFavoriteMusic") && hasFavorites;
    if (Settings::getInstance()->getBool("audio.useFavoriteMusic") && !hasFavorites)
    {
        Settings::getInstance()->setBool("audio.useFavoriteMusic", false);
        Settings::getInstance()->saveFile();
    }
    favoriteSwitch->setState(shouldUseFavorites);
    s->addWithDescription(_("PLAY ONLY SONGS FROM YOUR FAVORITES PLAYLIST"), "", favoriteSwitch, nullptr);
    s->addSaveFunc([favoriteSwitch, hasFavorites]() 
    {
        bool useFavorite = favoriteSwitch->getState();
        if (useFavorite && !hasFavorites)
        {
            useFavorite = false;
        }
        Settings::getInstance()->setBool("audio.useFavoriteMusic", useFavorite);
        Settings::getInstance()->saveFile();
        AudioManager::getInstance()->playRandomMusic(useFavorite);
    });

    s->addEntry(_("SELECTION OF FAVORITE SONGS"), true, [this] {
        GuiFavoriteMusicSelector::openSelectFavoriteSongs(mWindow, false, true);
    });

	s->addGroup(_("SOUNDS"));

	// es4all: 移除多余的 #ifdef _ENABLEEMUELEC 守卫 —— 本项与 EmuELEC 无关。
	// ee_menuscrollsound 是 es-core/components/ComponentList.cpp 读取并由 ES 自己播放的
	// 菜单滚动音效, 三边皆完全可用(只是键名沿用了带误导性的 ee_ 前缀)。
	// 原守卫因三个 target 都带 -DENABLE_EMUELEC=1 而恰好都成立, 属无效守卫。
	s->addFileBrowser(_("CUSTOM MENU SCROLL SOUND"), "ee_menuscrollsound", GuiFileBrowser::AUDIO);
	s->addSwitch(_("ENABLE NAVIGATION SOUNDS"), "EnableSounds", true, []
	{
		if (Settings::getInstance()->getBool("EnableSounds") && PowerSaver::getMode() == PowerSaver::INSTANT)
		{
			Settings::getInstance()->setPowerSaverMode("default");
			PowerSaver::init();
		}
	});

	s->addSwitch(_("ENABLE VIDEO PREVIEW AUDIO"), "VideoAudio", true);
	
	mWindow->pushGui(s);
}

void GuiMenu::openWifiSettings(Window* win, std::string title, std::string data, const std::function<void(std::string)>& onsave)
{
	win->pushGui(new GuiWifi(win, title, data, onsave));
}

void GuiMenu::openNetworkSettings(bool selectWifiEnable)
{
	bool baseWifiEnabled = SystemConf::getInstance()->getBool("wifi.enabled");

	auto theme = ThemeData::getMenuTheme();
	std::shared_ptr<Font> font = theme->Text.font;
	unsigned int color = theme->Text.color;

	Window *window = mWindow;

	auto s = new GuiSettings(mWindow, _("NETWORK SETTINGS").c_str());
	s->addGroup(_("INFORMATION"));

	auto ip = std::make_shared<TextComponent>(mWindow, ApiSystem::getInstance()->getIpAddress(), font, color);
	s->addWithLabel(_("IP ADDRESS"), ip);

	auto status = std::make_shared<TextComponent>(mWindow, ApiSystem::getInstance()->ping() ? _("CONNECTED") : _("NOT CONNECTED"), font, color);
	s->addWithLabel(_("INTERNET STATUS"), status);

#if !WIN32
	auto hostname = std::make_shared<TextComponent>(mWindow, ApiSystem::getInstance()->getHostsName(), font, color);
	s->addWithLabel(_("HOSTNAME"), hostname);
#endif

	// Network Indicator
	auto networkIndicator = std::make_shared<SwitchComponent>(mWindow);
	networkIndicator->setState(Settings::getInstance()->getBool("ShowNetworkIndicator"));
	s->addWithLabel(_("SHOW NETWORK INDICATOR"), networkIndicator);
	s->addSaveFunc([networkIndicator] { Settings::getInstance()->setBool("ShowNetworkIndicator", networkIndicator->getState()); });

	s->addGroup(_("SETTINGS"));

	// Wifi enable
	auto enable_wifi = std::make_shared<SwitchComponent>(mWindow);	
	enable_wifi->setState(baseWifiEnabled);
	s->addWithLabel(_("ENABLE WIFI"), enable_wifi, selectWifiEnable);

	// window, title, settingstring,
	const std::string baseSSID = SystemConf::getInstance()->get("wifi.ssid");
	const std::string baseKEY = SystemConf::getInstance()->get("wifi.key");

	if (baseWifiEnabled)
	{
		s->addInputTextRow(_("WIFI SSID"), "wifi.ssid", false, false, &openWifiSettings);
		s->addInputTextRow(_("WIFI KEY"), "wifi.key", true);

		std::string wifiIpStr = ApiSystem::getInstance()->getWifiIpAddress();
		auto wifiIp = std::make_shared<TextComponent>(mWindow, wifiIpStr.empty() ? _("NOT CONNECTED") : wifiIpStr, font, color);
		s->addWithLabel(_("WIFI IP ADDRESS"), wifiIp);
	}
	
	s->addSaveFunc([baseWifiEnabled, baseSSID, baseKEY, enable_wifi, window]
	{
		bool wifienabled = enable_wifi->getState();

		SystemConf::getInstance()->setBool("wifi.enabled", wifienabled);

		if (wifienabled) 
		{
			std::string newSSID = SystemConf::getInstance()->get("wifi.ssid");
			std::string newKey = SystemConf::getInstance()->get("wifi.key");

			if (baseSSID != newSSID || baseKEY != newKey || !baseWifiEnabled)
			{
				if (ApiSystem::getInstance()->enableWifi(newSSID, newKey)) 
					window->pushGui(new GuiMsgBox(window, _("WIFI ENABLED")));
				else 
					window->pushGui(new GuiMsgBox(window, _("WIFI CONFIGURATION ERROR")));
			}
		}
		else if (baseWifiEnabled)
			ApiSystem::getInstance()->disableWifi();
	});
	

	enable_wifi->setOnChangedCallback([this, s, baseWifiEnabled, enable_wifi]()
	{
		bool wifienabled = enable_wifi->getState();
		if (baseWifiEnabled != wifienabled)
		{
			SystemConf::getInstance()->setBool("wifi.enabled", wifienabled);

			if (wifienabled)
				ApiSystem::getInstance()->enableWifi(SystemConf::getInstance()->get("wifi.ssid"), SystemConf::getInstance()->get("wifi.key"));
			else
				ApiSystem::getInstance()->disableWifi();

			delete s;
			openNetworkSettings(true);
		}
	});

	mWindow->pushGui(s);
}

#if defined(ES4ALL_TARGET_ROCKNIX)
// es4all: ROCKNIX 专属 PLATFORM SETTINGS —— 全新独立实现，只放 ROCKNIX 上真生效的平台级项，
// 完全不依赖 openEmuELECSettings/createConfigureSplash 等 _ENABLEEMUELEC 里那团 Amlogic/emuelec 代码。
void GuiMenu::openPlatformSettings()
{
	auto s = new GuiSettings(mWindow, _("PLATFORM SETTINGS"));
	Window* window = mWindow;

	// es4all: 本选单**不分组、平铺**，且项目顺序比照 EMUELEC 的 openEmuELECSettings
	// (VIDEO MODE → AUDIO OUTPUT → CPU GOVERNOR → 日志 → SPLASH SETTINGS → EXTERNAL MOUNT)，
	// 让两个 target 的使用者看到一致的排列。EMUELEC 那边本来就没有 addGroup。
	//
	// es4all: RETROARCH MENU(菜单样式, global.retroarch.menu_driver)已从平台设置移除 ——
	// 与「前端开发人员选项」里的 RETROARCH MENU DRIVER 同键重复。保留开发者那份(面向进阶
	// 用户, 一般用户不易误触); 键不变, ROCKNIX setsettings.sh 照常读 retroarch.menu_driver。
	//
	// es4all: ENABLE RA BEZELS 已移除 —— 与 游戏设置 → DEFAULT GLOBAL SETTINGS 的同名项
	// 重复(同一个 global.bezel 键)。保留游戏设置那个三档版(AUTO/ON/OFF)，因为:
	// 1) 位置更合理(全局游戏设定，与 SHOW RETROARCH FPS 同组)；
	// 2) 此处的布尔 Switch 只写 "1"/"0"，一旦操作会把三档版的 "auto" 值毁掉。

	// es4all: ★VIDEO MODE 已移除（1.1 收敛决定）★
	//   透过采集卡无法可靠验证(卡本身不重新协商输入时序, 会黑屏/角落/撕裂), 而效益偏低,
	//   故 1.1 正式版三个 target 一律不出这个选项, 列为待办。
	//   后端与开机还原【保留】: ROCKNIX 的 glue 脚本 es4all-setvideomode(wlr-randr)、
	//   EMUELEC 的 ApiSystem::applyEmuelecVideoMode(解 display/debug 锁 + PHY 还原) 都还在,
	//   将来要恢复选单，接回来即可。


	// 音源输出 —— 与 EMUELEC 共用 resources/audio_outputs.cfg 映射表(见 ApiSystem::parseAudioOutputs)，
	// 但后端不同：EMUELEC 是裸 ALSA(emuelec-utils setauddev 改 asound.conf 默认 PCM)，
	// ROCKNIX 走 PipeWire(aplay -L 的 default 指向 PipeWire Media Server)，写 asound.conf 无效，
	// 必须改 PipeWire 默认 sink -> 交给 glue 脚本 es4all-setauddev(按 ALSA card 号解析 sink id，
	// 因为 wpctl 的数字 id 每次开机会变)。键用 system.audiooutput(开机 glue 重新套用)。
	//
	// ⚠️ 机型不在 audio_outputs.cfg 里就不显示本选单。MD1000 未列入 —— 实机验证它只有 HDMI 可用：
	// 板载 3.5mm 是 AV 孔，但设备树里没有任何模拟 codec 节点(只有 rockchip,rk3568-spdif + spdif-dit
	// 这个数字 dummy codec)，实听 card1 无声、HDMI 正常。只剩一个可选项的选单没有意义。
	{
		std::vector<std::pair<std::string, std::string>> outs = ApiSystem::getInstance()->parseAudioOutputs();
		if (!outs.empty())
		{
			auto audioout = std::make_shared< OptionListComponent<std::string> >(mWindow, _("AUDIO OUTPUT"), false);
			std::string cur = SystemConf::getInstance()->get("system.audiooutput");
			if (cur.empty() || cur == "auto")
				cur = outs.front().second;

			for (auto& o : outs)
				audioout->add(_(o.first.c_str()), o.second, cur == o.second);
			s->addWithDescription(_("AUDIO OUTPUT"), _("Changes will need an EmulationStation restart."), audioout);
			audioout->setSelectedChangedCallback([audioout](std::string dev) {
				if (SystemConf::getInstance()->set("system.audiooutput", dev))
					SystemConf::getInstance()->saveSystemConf();
				Utils::Platform::ProcessStartInfo("/usr/bin/es4all-setauddev " + dev).run();
			});
		}
	}

	// CPU 调速器 —— system.cpugovernor
	auto gov = std::make_shared< OptionListComponent<std::string> >(mWindow, _("CPU GOVERNOR"), false);
	std::string curGov = SystemConf::getInstance()->get("system.cpugovernor");
	// es4all: 显示名过 _() 翻译(与 EmuELEC 版 openEmuELECSettings 的做法一致，
	// ONDEMAND/PERFORMANCE/SCHEDUTIL/POWERSAVE 的 zh_CN/zh_TW 早已存在)；值维持内核 governor 原名。
	gov->addRange({ { _("AUTO"), "" }, { _("PERFORMANCE"), "performance" }, { _("SCHEDUTIL"), "schedutil" }, { _("ONDEMAND"), "ondemand" }, { _("POWERSAVE"), "powersave" } }, curGov);
	s->addWithDescription(_("CPU GOVERNOR"), _("CPU frequency scaling policy."), gov);
	s->addSaveFunc([gov] {
		if (gov->changed() && SystemConf::getInstance()->set("system.cpugovernor", gov->getSelected()))
			SystemConf::getInstance()->saveSystemConf();
	});

	// 日志等级 —— system.loglevel（ROCKNIX runemu.sh 原生读取:off / verbose / 默认normal，零 shell 改动）
	// 位置对应 EMUELEC 的 RETROARCH LOGGING（同样排在 CPU GOVERNOR 之后）。
	//
	// es4all: ★标题改用与 E 版同一个字串 RETROARCH LOGGING(2026-07-23 用户要求对标)★
	//   原本叫「日志级别」，与开发者选项里的「ES 日志级别」(Settings LogLevel -> es_log.txt)
	//   只差三个字，使用者分不出哪个管游戏、哪个管前端 —— 实机截图上就被误认成 ES 的日志开关。
	//   这里管的是 runemu.sh(游戏启动脚本)那条: system.loglevel 决定 exec.log 记不记、
	//   verbose 再额外开脚本自身的除错讯息，跟 ES 自己的 es_log.txt 无关。
	//   与 E 版的差别只在档位: E 是布尔开关，R 保留 runemu.sh 原生的三档(默认/关闭/详细)。
	auto loglevel = std::make_shared< OptionListComponent<std::string> >(mWindow, _("RETROARCH LOGGING"), false);
	std::string curLog = SystemConf::getInstance()->get("system.loglevel");
	// es4all: ROCKNIX 出厂/原生可能写 "none"，runemu.sh 的 case 是 `off|none)` 两者等价；
	// 不归一化的话 OptionList 匹配不到任何项，会退回显示第一项「默认」，与实际(关闭)不符。
	if (curLog == "none") curLog = "off";
	// es4all: 顺序照「记录量由少到多」排 —— 关闭 → 默认 → 详细(2026-07-23 用户要求)。
	//   原本是 默认/关闭/详细，把记录量最少的「关闭」摆中间，反直觉。
	//   三档对应 runemu.sh:133 的三个分支:
	//     off|none  -> LOG=false                 完全不记
	//     (空)      -> LOG=true                  记 exec.log，但不开 VERBOSE
	//     verbose   -> LOG=true; VERBOSE=true    另外再记 runemu.sh 自身的步骤讯息
	//   即「默认」确实比「详细」少一层，夹在中间是对的。
	loglevel->addRange({ { _("OFF"), "off" }, { _("DEFAULT"), "" }, { _("VERBOSE"), "verbose" } }, curLog);
	s->addWithDescription(_("RETROARCH LOGGING"), _("RetroArch logs; enable when you need to debug."), loglevel);
	s->addSaveFunc([loglevel] {
		if (loglevel->changed() && SystemConf::getInstance()->set("system.loglevel", loglevel->getSelected()))
			SystemConf::getInstance()->saveSystemConf();
	});

	// 启动画面（ES 自己的载入/退出过场，不是 RA SPLASH）——
	// ★不能调用 createConfigureSplash★：它在 `#ifdef _ENABLEEMUELEC` 里，而 ROCKNIX 固件
	// build 没定义该宏(rocknix package.mk 只有 -DES4ALL_TARGET=rocknix -DROCKNIX=1)，
	// 且其函式体依赖 createSplashLoadingOptionList 等一堆 emuelec-only 符号、放宽守卫会连环爆。
	// 故 ROCKNIX 用自家精简版 openRocknixSplashSettings()，只做 ES 自己的两个开关(纯 Settings)。
	s->addEntry(_("SPLASH SETTINGS"), true, [this] { openRocknixSplashSettings(mWindow); });

	// 外接挂载 —— 子菜单（走 ROCKNIX 原生 system.automount 机制）
	s->addEntry(_("EXTERNAL MOUNT OPTIONS"), true, [this, window] { openRocknixExternalMount(window); });

	mWindow->pushGui(s);
}

// es4all: ROCKNIX 专属的「启动画面设置」—— createConfigureSplash 的精简替身。
// 只做 ES 自己的两个开关(SplashScreen / SplashScreenExit，纯 Settings、零 emuelec 依赖)，
// 不碰 ee_splash* 那些发行版专属项(那些在 ROCKNIX 上没有消费端)。
// 存档语意与其他选单一致：B(返回)=套用存档、START(关闭)=取消不存档(见 GuiSettings::input)。
void GuiMenu::openRocknixSplashSettings(Window* win)
{
	auto s = new GuiSettings(win, _("SPLASH SETTINGS"));

	auto loading = std::make_shared<SwitchComponent>(win);
	loading->setState(Settings::getInstance()->getBool("SplashScreen"));
	s->addWithLabel(_("ENABLE LOADING SPLASH SCREEN"), loading);
	s->addSaveFunc([loading] {
		Settings::getInstance()->setBool("SplashScreen", loading->getState());
	});

	auto exitSplash = std::make_shared<SwitchComponent>(win);
	exitSplash->setState(Settings::getInstance()->getBool("SplashScreenExit"));
	s->addWithLabel(_("ENABLE EXIT SPLASH SCREEN"), exitSplash);
	s->addSaveFunc([exitSplash] {
		Settings::getInstance()->setBool("SplashScreenExit", exitSplash->getState());
	});

	win->pushGui(s);
}

void GuiMenu::openRocknixExternalMount(Window* win)
{
	auto s = new GuiSettings(win, _("EXTERNAL MOUNT OPTIONS"));

	// 自动挂载外接盘 —— system.automount（automount 脚本读，默认 1）
	auto automount = std::make_shared<SwitchComponent>(win);
	automount->setState(SystemConf::getInstance()->get("system.automount") != "0");
	s->addWithDescription(_("AUTOMOUNT EXTERNAL"), _("Auto-detect and mount an external drive for games at boot."), automount);
	s->addSaveFunc([automount] {
		SystemConf::getInstance()->set("system.automount", automount->getState() ? "1" : "0");
		SystemConf::getInstance()->saveSystemConf();
	});

	// 游戏装置 —— system.gamesdevice（用 blkid 枚举可挂载分区）
	auto gamesdev = std::make_shared< OptionListComponent<std::string> >(win, _("GAMES DEVICE"), false);
	std::string curDev = SystemConf::getInstance()->get("system.gamesdevice");
	gamesdev->add(_("AUTO"), "", curDev.empty());
	std::string devLine;
	// es4all: ★排除系统碟自己★ —— 原本 blkid 会把挂载 /storage 的系统盘(及其兄弟分区)也列出来，
	// 使用者误选就变 0 个游戏(实机遇到过：列出 STORAGE=系统盘、EMUELEC=插着的 EmuELEC 盘开机分区)。
	// 先从 /proc/mounts 找出 /storage 的来源装置、去掉分区号得基础装置(sda2->sda, mmcblk0p2->mmcblk0)，
	// 再从枚举里剔掉该基础装置的所有分区。sysbase 取不到时用占位符，避免 grep -v 空 pattern 误删全部。
	// ★分隔符用 R"SH(...)SH" 而非 R"(...)"★：脚本里的 grep 正则含 `)"` 序列
	// (…(p?[0-9]|$)" )，会被当成 R"(…)" 的结束符导致 raw string 提前结束、编译报语法错。
	std::string devs = Utils::Platform::getShOutput(R"SH(
		sysdev=$(awk '$2=="/storage"{print $1; exit}' /proc/mounts)
		sysbase=$(echo "$sysdev" | sed -E 's#p?[0-9]+$##')
		[ -z "$sysbase" ] && sysbase="/dev/__nomatch__"
		blkid 2>/dev/null | awk -F: '/ext4|btrfs|vfat|exfat|ntfs/ {print $1}' \
		  | grep -E 'mmcblk|sd|nvme' | grep -vE "^${sysbase}(p?[0-9]|$)" | sort -u
	)SH");
	for (std::stringstream ss(devs); std::getline(ss, devLine); )
	{
		while (!devLine.empty() && (devLine.back() == '\r' || devLine.back() == ' ')) devLine.pop_back();
		if (!devLine.empty())
			gamesdev->add(devLine, devLine, curDev == devLine);
	}
	s->addWithDescription(_("GAMES DEVICE"), _("Force a specific partition for /storage/roms (else auto-detect)."), gamesdev);
	s->addSaveFunc([gamesdev] {
		if (gamesdev->changed() && SystemConf::getInstance()->set("system.gamesdevice", gamesdev->getSelected()))
			SystemConf::getInstance()->saveSystemConf();
	});

	// 主储存 —— system.merged.device
	auto primary = std::make_shared< OptionListComponent<std::string> >(win, _("PRIMARY STORAGE"), false);
	std::string curPrim = SystemConf::getInstance()->get("system.merged.device");
	primary->addRange({ { _("AUTO"), "" }, { _("INTERNAL"), "internal" }, { _("EXTERNAL"), "external" } }, curPrim);
	s->addWithDescription(_("PRIMARY STORAGE"), _("Which storage is primary when merging internal + external."), primary);
	s->addSaveFunc([primary] {
		if (primary->changed() && SystemConf::getInstance()->set("system.merged.device", primary->getSelected()))
			SystemConf::getInstance()->saveSystemConf();
	});

	// 合并储存 —— system.merged.storage（overlay 叠加内部+外接 ROM）
	auto merged = std::make_shared<SwitchComponent>(win);
	merged->setState(SystemConf::getInstance()->get("system.merged.storage") == "1");
	s->addWithDescription(_("MERGED STORAGE"), _("Overlay internal and external ROMs together."), merged);
	s->addSaveFunc([merged] {
		SystemConf::getInstance()->set("system.merged.storage", merged->getState() ? "1" : "0");
		SystemConf::getInstance()->saveSystemConf();
	});

	// 应用（外接挂载在开机时套用，需重启）
	s->addEntry(_("REBOOT TO APPLY"), false, [win] {
		win->pushGui(new GuiMsgBox(win, _("Storage changes are applied at boot. Reboot now?"),
			_("YES"), [] {
				SystemConf::getInstance()->saveSystemConf();
				Utils::Platform::ProcessStartInfo("reboot").run();
			},
			_("NO"), nullptr));
	});

	win->pushGui(s);
}
#endif

void GuiMenu::openQuitMenu()
{
  GuiMenu::openQuitMenu_static(mWindow);
}

void GuiMenu::openQuitMenu_static(Window *window, bool quickAccessMenu, bool animate)
{
#ifdef WIN32
	if (!quickAccessMenu && Settings::getInstance()->getBool("ShowOnlyExit") && Settings::getInstance()->getBool("ShowExit"))
	{
		Utils::Platform::quitES(Utils::Platform::QuitMode::QUIT);
		return;
	}
#endif

	auto s = new GuiSettings(window, (quickAccessMenu ? _("QUICK ACCESS") : _("QUIT")).c_str());
	s->setCloseButton("select");

	
	if (quickAccessMenu)
	{
    		s->addGroup(_("QUICK ACCESS"));
            if (AudioManager::getInstance()->isSongPlaying())
            {
                std::string songName = AudioManager::getInstance()->getSongName();
                std::string currentSongPath = AudioManager::getInstance()->getCurrentSongPath();

                if (!songName.empty())
                {
                    s->addWithDescription(_("SKIP TO THE NEXT SONG"),
                                          _("NOW PLAYING") + ": " + songName,
                                          nullptr,
                                          [s, window]()
                                          {
                                              Window* w = window;
                                              AudioManager::getInstance()->playRandomMusic(false);
                                              delete s;
                                              GuiMenu::openQuitMenu_static(w, true, false);
                                          },
                                          "iconSound");

                    std::string favoritesFile = FavoriteMusicManager::getFavoriteMusicFilePath();
                    auto favorites = FavoriteMusicManager::loadFavoriteSongs(favoritesFile);

                    bool inFavorites = false;
                    for (const auto& fav : favorites)
                    {
                        if (fav.first == currentSongPath)
                        {
                            inFavorites = true;
                            break;
                        }
                    }

                    std::string fileNameWithoutExt = Utils::FileSystem::getFileName(currentSongPath);
                    size_t lastDot = fileNameWithoutExt.find_last_of('.');
                    if (lastDot != std::string::npos) {
                        fileNameWithoutExt = fileNameWithoutExt.substr(0, lastDot);
                    }

                    if (inFavorites)
                    {
                        s->addWithDescription(_("REMOVE CURRENT SONG FROM THE FAVORITES PLAYLIST"), "",
                                          nullptr,
                                          [s, window, currentSongPath, fileNameWithoutExt]()
                                          {
                                              Window* w = window;
                                              if (FavoriteMusicManager::getInstance().removeSongFromFavorites(currentSongPath, fileNameWithoutExt, window))
                                              {
                                                  AudioManager::getInstance()->playRandomMusic(true);
                                                  delete s;
                                                  GuiMenu::openQuitMenu_static(w, true, false);
                                              }
                                          },
                                          "iconSound");
                    }
                    else
                    {
                        s->addWithDescription(_("SAVE CURRENT SONG TO THE FAVORITES PLAYLIST"), "",
                                          nullptr,
                                          [s, window, currentSongPath, fileNameWithoutExt]()
                                          {
                                              Window* w = window;
                                              if (FavoriteMusicManager::getInstance().saveSongToFavorites(currentSongPath, fileNameWithoutExt, window))
                                              {
                                                  Settings::getInstance()->saveFile();
                                                  AudioManager::getInstance()->playRandomMusic(true);
                                                  delete s;
                                                  GuiMenu::openQuitMenu_static(w, true, false);
                                              }
                                          },
                                          "iconSound");
                    }
                }
            }
					
		s->addEntry(_("LAUNCH SCREENSAVER"), false, [s, window]
			{
				Window* w = window;
				window->postToUiThread([w]()
					{
						w->startScreenSaver();
						w->renderScreenSaver();
					});
				delete s;

			}, "iconScraper", true);

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::ScriptId::PDFEXTRACTION) && Utils::FileSystem::exists(Paths::getUserManualPath()))
		{
			s->addEntry(_("VIEW USER MANUAL"), false, [s, window]
				{
					GuiImageViewer::showPdf(window, Paths::getUserManualPath());
					delete s;
				}, "iconManual");
		}

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::WRITEPLANEMODE))
		{
			if (ApiSystem::getInstance()->isPlaneMode())
			{
				s->addEntry(_("DISABLE PLANE MODE"), false, [window, s]
					{
						ApiSystem::getInstance()->setPlaneMode(false);
						delete s;
					}, "iconPlanemode");
			}
			else 
			{
				s->addEntry(_("ENABLE PLANE MODE"), false, [window, s]
					{
						ApiSystem::getInstance()->setPlaneMode(true);
						delete s;
					}, "iconPlanemode");
			}
		}
	}
	
// es4all: ★「重启 EMULATIONSTATION」放宽给 ROCKNIX★(1.1 收敛)。
//   这一项本身跟发行版无关 —— 只是 fireEvent + quitES(QUIT)，由各家的服务管理器把 ES 拉回来
//   (EMUELEC=emustation.service、ROCKNIX=essway)，三边都成立。原本却被关在 `_ENABLEEMUELEC`
//   里，而 ROCKNIX 固件的 package.mk 没有该旗标，导致 ROCKNIX 的退出选单只有「重启/关机/快速关机」，
//   连重启前端都得整机重开，很不合理(实机 MD1000 确认)。
#if defined(_ENABLEEMUELEC) || defined(ES4ALL_TARGET_ROCKNIX)
	s->addEntry(_("RESTART EMULATIONSTATION"), false, [window] {
		window->pushGui(new GuiMsgBox(window, _("REALLY RESTART EMULATIONSTATION?"), _("YES"),
			[] {
    		   /*Utils::Platform::ProcessStartInfo("systemctl restart emustation.service", "", nullptr);*/
    		   Scripting::fireEvent("quit", "restart");
			   Utils::Platform::quitES(Utils::Platform::QuitMode::QUIT);
		}, _("NO"), nullptr));
	}, "iconRestart");
#endif

// es4all: ★「重启到 USB/SD」评估后决定【不做】（1.1 收敛）★
//   实读 eMMC 上的 u-boot 二进位: boot_targets = nvme mmc1 mmc0 usb0 pxe dhcp,
//   其中 mmc1=SD/TF 卡槽(fe2b0000, cap-sd-highspeed)、mmc0=eMMC(fe310000, non-removable)。
//   => TF/SD 排在 eMMC 【之前】: 插上可开机的卡本来就会自动进去, ES 做不做这个选项都一样;
//      USB 排在 eMMC 【之后】: eMMC 有 boot.scr 时永远轮不到, 给按钮也切不过去。
//   两个方向都不需要(或无法)由 ES 介入, 故不提供 —— 与其放个语意含糊的按钮, 不如不放。
//   ⚠️ 顺带更正 installtoemmc 注解里「u-boot 读不了 USB」的说法: 那是错的,
//      u-boot 编入了 bootcmd_usb0, 真正原因是扫描顺序。将来若要让 USB 可开机,
//      正解是改我们自己维护的 boot.scr(加 usb start + 标记档), 不必动 u-boot。详见交接单。


#ifdef _ENABLEEMUELEC
	bool isFullUI = UIModeController::getInstance()->isUIModeFull();
	if (isFullUI)
	{
		if (SystemConf::getInstance()->getBool("extra_quit_menu.enabled", true))
		{
			// es4all: 探测开机媒体(eMMC vs SD/USB)决定显示「重启到 USB/SD」还是「重启到 eMMC」。
			// 原实现用 lsblk + findmnt —— EMUELEC(busybox)两个命令都没装, 整段失效,
			// bootedFromEmmc 恒为默认 true → 无论实际从哪开机都显示「重启到USB/SD」(与实际相反)。
			// 改用纯 busybox 可用的 /proc/mounts + /sys/block/<disk>/removable:
			//   优先取 /flash 的设备(EMUELEC/CoreELEC 开机分区; / 在 EMUELEC 是 squashfs loop,
			//   无法直接判开机媒体), 否则取 /; 若是 loop 设备再回退 /storage。
			//   剥掉分区号得基础盘(sda1→sda, mmcblk0p1→mmcblk0), 读 removable(0=eMMC,1=SD/USB)。
			bool bootedFromEmmc = true;

			FILE* pipe = popen(
				R"SH(dev=$(awk '$2=="/flash"{print $1;exit}' /proc/mounts); [ -z "$dev" ] && dev=$(awk '$2=="/"{print $1;exit}' /proc/mounts); case "$dev" in /dev/loop*) dev=$(awk '$2=="/storage"{print $1;exit}' /proc/mounts);; esac; d=$(basename "$dev" 2>/dev/null); case "$d" in mmcblk*) d=${d%p[0-9]*};; *) d=$(echo "$d" | sed 's/[0-9]*$//');; esac; cat "/sys/block/$d/removable" 2>/dev/null)SH", "r");
			if (pipe != nullptr)
			{
				char buf[8] = "";
				if (fgets(buf, sizeof(buf), pipe) != nullptr)
					bootedFromEmmc = (Utils::String::trim(std::string(buf)) == "0");
				pclose(pipe);
			}

			if (bootedFromEmmc)
			{
				s->addEntry(_("REBOOT TO USB/SD"), false, [window] {
					window->pushGui(new GuiMsgBox(window, _("REALLY REBOOT TO USB/SD?"), _("YES"),
						[] {
						Scripting::fireEvent("quit", "usb");
						// es4all: 实际切到外接(USB/SD)开机。Amlogic/EMUELEC u-boot bootcmd:
						// bootfromnand=0 时依序试 SD→USB→eMMC(外接优先)。原本只 systemctl reboot
						// 不改 env、fireEvent quit 又无处理器, 于是重启回原媒体(动作无效)。
						// 带 command -v 守卫: 无 fw_setenv 的平台(非 Amlogic)自动退回普通重启。
						Utils::Platform::ProcessStartInfo("command -v fw_setenv >/dev/null 2>&1 && fw_setenv bootfromnand 0").run();
						Utils::Platform::ProcessStartInfo("sync").run();
						Utils::Platform::ProcessStartInfo("systemctl reboot").run();
						Utils::Platform::quitES(Utils::Platform::QuitMode::QUIT);
					}, _("NO"), nullptr));
				}, "iconAdvanced");
			}
			else
			{
				s->addEntry(_("REBOOT TO EMMC"), false, [window] {
					window->pushGui(new GuiMsgBox(window, _("REALLY REBOOT TO EMMC?"), _("YES"),
						[] {
						Scripting::fireEvent("quit", "emmc");
						// es4all: 实际切到内建 eMMC 开机。用 EMUELEC/CoreELEC 自带 rebootfromnand
						// (设 bootfromnand=1, 并处理 whereToBootFrom / FireTV 特例)。它本身除 FireTV
						// 外不重启, 故随后 systemctl reboot。带 command -v 守卫, 非 Amlogic 平台退回普通重启。
						Utils::Platform::ProcessStartInfo("command -v rebootfromnand >/dev/null 2>&1 && rebootfromnand").run();
						Utils::Platform::ProcessStartInfo("sync").run();
						Utils::Platform::ProcessStartInfo("systemctl reboot").run();
						Utils::Platform::quitES(Utils::Platform::QuitMode::QUIT);
					}, _("NO"), nullptr));
				}, "iconAdvanced");
			}
		}
}

	s->setUpdateType(ComponentListFlags::UPDATE_ALWAYS);
	// AUTO SHUTDOWN TIMEOUT
	auto shutdownSlider = std::make_shared<SliderComponent>(window, 0.0f, 1440.0f, 10.0f, "min");

	int timeout = 0;
	try {
		timeout = std::stoi(SystemConf::getInstance()->get("ee_auto_shutdown_timeout"));
	} catch (...) {
		timeout = 0;
	}
	shutdownSlider->setValue((float)timeout);
	s->addWithDescription(_("SHUTDOWN AFTER INACTIVITY"), _("Shuts down the system if no controller activity occurs within the timer."), shutdownSlider, nullptr, "iconAutoShutdown");
	s->addSaveFunc([shutdownSlider] {
		int value = (int)shutdownSlider->getValue();
		if (value > 0) {
			system("killall ee_asd > /dev/null 2>&1"); // avoid double instance
			system(("ee_asd -t " + std::to_string((int)value)).c_str());
		} else {
			system("killall ee_asd  > /dev/null 2>&1");
		}
		SystemConf::getInstance()->set("ee_auto_shutdown_timeout", std::to_string(value));
		SystemConf::getInstance()->saveSystemConf();
	});

	s->addSwitch(_("Persistent Autoshutdown"), _("When enabled, the inactivity timer keeps counting in all menus, not just during gameplay."), "ee_auto_shutdown_persistent", false, nullptr);
#endif

	if (quickAccessMenu)
		s->addGroup(_("QUIT"));

	s->addEntry(_("RESTART SYSTEM"), false, [window] {
		window->pushGui(new GuiMsgBox(window, _("REALLY RESTART?"), 
			_("YES"), [] { Utils::Platform::quitES(Utils::Platform::QuitMode::REBOOT); },
			_("NO"), nullptr));
	}, "iconRestart");

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::SUSPEND))
	{
		s->addEntry(_("SUSPEND SYSTEM"), false, [window, s] {
			window->pushGui(new GuiMsgBox(window, _("REALLY SUSPEND ?"),
				_("YES"), [s] { s->close(); ApiSystem::getInstance()->suspend(); },
				_("NO"), nullptr));
		}, "iconFastShutdown");
	}

	s->addEntry(_("SHUTDOWN SYSTEM"), false, [window] {
		window->pushGui(new GuiMsgBox(window, _("REALLY SHUTDOWN?"), 
			_("YES"), [] { Utils::Platform::quitES(Utils::Platform::QuitMode::SHUTDOWN); },
			_("NO"), nullptr));
	}, "iconShutdown");

#ifndef _ENABLEEMUELEC
	s->addWithDescription(_("FAST SHUTDOWN SYSTEM"),_("Shutdown without saving metadata."), nullptr, [window] {
		window->pushGui(new GuiMsgBox(window, _("REALLY SHUTDOWN WITHOUT SAVING METADATA?"), 
			_("YES"), [] { Utils::Platform::quitES(Utils::Platform::QuitMode::FAST_SHUTDOWN); },
			_("NO"), nullptr));
	}, "iconFastShutdown");
#endif

#ifdef WIN32
	if (Settings::getInstance()->getBool("ShowExit"))
	{
		s->addEntry(_("QUIT EMULATIONSTATION"), false, [window] {
			window->pushGui(new GuiMsgBox(window, _("REALLY QUIT?"), 
				_("YES"), [] { Utils::Platform::quitES(Utils::Platform::QuitMode::QUIT); },
				_("NO"), nullptr));
		}, "iconQuit");
	}
#endif

	if (quickAccessMenu && animate)
		s->getMenu().animateTo(Vector2f((Renderer::getScreenWidth() - s->getMenu().getSize().x()) / 2, (Renderer::getScreenHeight() - s->getMenu().getSize().y()) / 2));
	else if (quickAccessMenu)
		s->getMenu().setPosition((Renderer::getScreenWidth() - s->getMenu().getSize().x()) / 2, (Renderer::getScreenHeight() - s->getMenu().getSize().y()) / 2);

	window->pushGui(s);
}

void GuiMenu::createDecorationItemTemplate(Window* window, std::vector<DecorationSetInfo> sets, std::string data, ComponentListRow& row)
{
	Vector2f maxSize(Renderer::getScreenWidth() * 0.14, Renderer::getScreenHeight() * 0.14);

	int IMGPADDING = Renderer::getScreenHeight()*0.01f;

	auto theme = ThemeData::getMenuTheme();
	std::shared_ptr<Font> font = theme->Text.font;
	unsigned int color = theme->Text.color;

	// spacer between icon and text
	auto spacer = std::make_shared<GuiComponent>(window);
	spacer->setSize(IMGPADDING, 0);
	row.addElement(spacer, false);

	std::string label = data;
	if (data.empty())
		label = _("AUTO");
	else if (data == "none")
		label = _("NONE");
	else
		label = Utils::String::toUpper(Utils::String::replace(data, "_", " "));
		
	row.addElement(std::make_shared<TextComponent>(window, label, font, color, ALIGN_LEFT), true);

	std::string imageUrl;

	for (auto set : sets)
		if (set.name == data)
			imageUrl = set.imageUrl;

	// image
	if (!imageUrl.empty())
	{
		auto icon = std::make_shared<ImageComponent>(window);
		icon->setImage(imageUrl, false, maxSize);
		icon->setMaxSize(maxSize);
		icon->setColorShift(theme->Text.color);
		icon->setPadding(IMGPADDING);
		row.addElement(icon, false);
	}
}

void GuiMenu::popSystemConfigurationGui(Window* mWindow, SystemData* systemData) 
{  
	popSpecificConfigurationGui(mWindow, 
		systemData->getFullName(), 
		systemData->getName(), 
		systemData, 
		nullptr);
}

void GuiMenu::popGameConfigurationGui(Window* mWindow, FileData* fileData)
{
	popSpecificConfigurationGui(mWindow,
		fileData->getName(),
		fileData->getConfigurationName(),
		fileData->getSourceFileData()->getSystem(),
		fileData);
}

// TODO 

#ifdef _ENABLEEMUELEC
// Button Remapper currently AdvMame supported only.

std::shared_ptr<OptionListComponent<std::string>> GuiMenu::btn_choice = nullptr;
std::shared_ptr<OptionListComponent<std::string>> GuiMenu::del_choice = nullptr;
std::shared_ptr<OptionListComponent<std::string>> GuiMenu::edit_choice = nullptr;

std::shared_ptr<OptionListComponent<std::string>> GuiMenu::createJoyBtnRemapOptionList(Window *window, std::string prefixName, std::string remapName, int btnIndex)
{
	auto btn_choice = std::make_shared< OptionListComponent<std::string> >(window, _("BUTTON REMAP CONFIG"), false);

	std::string joy_btns = SystemConf::getInstance()->get(prefixName + ".joy_btn_defaults");
	std::vector<std::string> arr_joy_btn(explode(joy_btns));
	
	if (joy_btns.empty()) {
		btn_choice->add("NONE", "-1", true);
		return btn_choice;
	}

	std::string btn_names = SystemConf::getInstance()->get(prefixName + ".joy_btn_button_names."+remapName);
	if (btn_names.empty())
		btn_names = SystemConf::getInstance()->get(prefixName + ".joy_btn_button_names");
	std::vector<std::string> arr_btn_names(explode(btn_names));

	std::vector<int> iOrders = {0,1,2,3,4,5,6,7};
	std::string sOrder = SystemConf::getInstance()->get(prefixName + ".joy_btn_order." + remapName);
	if (!sOrder.empty())
		iOrders = int_explode(sOrder, ' ');

	int index = 0;
	btn_choice->add("NONE", "-1", false);
	int i;
	for (auto it = arr_joy_btn.cbegin(); it != arr_joy_btn.cend(); it++) {
		i = iOrders[index++];
		if (i >= arr_btn_names.size())
			continue;
		btn_choice->add(arr_btn_names[i], std::to_string(i), btnIndex == i);
	}

	return btn_choice;
}

void GuiMenu::editJoyBtnRemapOptionList(Window *window, GuiSettings *systemConfiguration, std::string prefixName)
{
	const std::function<void(std::string)> editFunc([window, systemConfiguration, prefixName] (std::string remapName) {
		int editIndex = GuiMenu::edit_choice->getSelectedIndex();
		if (editIndex <= 0)
			return;

		GuiSettings* systemConfiguration = new GuiSettings(window, _("EDIT REMAP"));

		if (!remapName.empty())
			GuiMenu::createBtnJoyCfgRemap(window, systemConfiguration, prefixName, remapName, 0);

		GuiMenu::edit_choice->selectFirstItem();
		GuiMenu::del_choice->selectFirstItem();

		window->pushGui(systemConfiguration);
	});

	edit_choice->setSelectedChangedCallback([window, systemConfiguration, editFunc, prefixName](std::string s) {
		long unsigned int m1 = (long unsigned int) &(*window->peekGui());
		long unsigned int m2 = (long unsigned int) &(*systemConfiguration);
		if (m1 == m2)
			return;

		std::string sn = GuiMenu::edit_choice->getSelectedName();
		editFunc(sn);
	});

	systemConfiguration->addSaveFunc([window, systemConfiguration, editFunc, prefixName] {
		std::string sn = GuiMenu::edit_choice->getSelectedName();
		editFunc(sn);
	});
}

void GuiMenu::createBtnJoyCfgRemap(Window *window, GuiSettings *systemConfiguration,
	std::string prefixName, std::string remapName, int orderIndex)
{
	std::vector<std::shared_ptr<OptionListComponent<std::string>>> remap_choice;

	std::string btnNames = SystemConf::getInstance()->get(prefixName + ".joy_btn_defaults");
	std::vector<std::string> arr_btn_names(explode(btnNames));

	int btnCount = static_cast<int>(std::count(btnNames.begin(), btnNames.end(), ',')+1);

	std::vector<int> iOrders = {0,1,2,3,4,5,6,7};
	std::string sOrder;
	sOrder = SystemConf::getInstance()->get(prefixName + ".joy_btn_order." + remapName);
	if (!sOrder.empty())
		iOrders = int_explode(sOrder, ' ');

	for (int index=0; index < iOrders.size(); ++index)
	{
		auto remap = createJoyBtnRemapOptionList(window, prefixName, remapName, iOrders[index]);
		remap_choice.push_back(remap);
		systemConfiguration->addWithLabel(arr_btn_names[index], remap);
	}

	// Loops through remaps assigns Event to make sure no remap duplicates exist.
	for(auto it = remap_choice.cbegin(); it != remap_choice.cend(); ++it) {
		auto self = (*it);
		self->setSelectedChangedCallback([self, window, systemConfiguration, remap_choice] (std::string choice) {
			long unsigned int m1 = (long unsigned int) &(*window->peekGui());
			long unsigned int m2 = (long unsigned int) &(*systemConfiguration);
			if (m1 == m2)
				return;
			
			std::string choice2;
			if (choice == "-1")
				return;
			for(auto it = remap_choice.cbegin(); it != remap_choice.cend(); ++it) {
				auto remap = *it;
				choice2 = remap->getSelected();
				if (choice2 == "-1")
					continue;
				if (self != remap && choice == choice2) {
					remap->selectFirstItem();
					continue;
				}
			}
		});
	}


	systemConfiguration->addSaveFunc([window, systemConfiguration, remap_choice, prefixName, remapName, btnCount, orderIndex] {
		// Hack to avoid over-writing defaults.
		/*if (btnIndex != -1 && editIndex > 0 && editIndex <= 2)
		{
			window->pushGui(new GuiMsgBox(window,  _("CANNOT SAVE DEFAULT BUTTON MAPS."),
				_("OK")));
			edit_choice->selectFirstItem();
			return;
		}*/

		int err = 0;
		if (btnCount == 0)
			err = 1;

		// Loops through remap values and makes sure no conflicts or empty fields.
		[&] {
			for(auto it = remap_choice.cbegin(); it != remap_choice.cend(); ++it) {
				auto remap = *it;
				std::string choice = remap->getSelected();
				if (choice == "-1") {
					err=1;
					break;
				}
				for(auto it2 = remap_choice.cbegin(); it2 != remap_choice.cend(); ++it2) {
					auto remap2 = *it2;
					std::string choice2 = remap2->getSelected();
					if (choice2 == "-1") {
						err=1;
						return;
					}
					if (remap != remap2 && choice == choice2) {
						err=1;
						return;
					}
				}
			}
		}();

		if (err > 0)
		{
			window->pushGui(new GuiMsgBox(window, _("ERROR - Remap is not configured properly, All buttons must be assigned and no duplicates."),
			_("OK")));
			return;
		}
		
		const std::function<void()> addRemaps([remap_choice, orderIndex, prefixName, remapName] {
			std::string sRemap = "";
			for(auto it = remap_choice.cbegin(); it != remap_choice.cend(); ++it) {
				if (it != remap_choice.cbegin())
					sRemap += " ";
				sRemap += (*it)->getSelected();
			}
			SystemConf::getInstance()->set(prefixName + ".joy_btn_index", remapName);
			SystemConf::getInstance()->set(prefixName + ".joy_btn_order." + remapName, sRemap);
			SystemConf::getInstance()->saveSystemConf();
		});

		if (orderIndex == -1)
		{
			window->pushGui(new GuiMsgBox(window, _("ARE YOU SURE YOU WANT TO CREATE THE REMAP?"),
				_("YES"), [addRemaps, remap_choice, prefixName, remapName, btnCount]
			{
				std::string remapNames = SystemConf::getInstance()->get(prefixName + ".joy_btn_remap_names");
				remapNames = remapNames.empty() ? remapName : (remapNames + "," + remapName);
				SystemConf::getInstance()->set(prefixName + ".joy_btn_remap_names", remapNames);

				std::string sRemap = "";
				for(int i=0; i < btnCount; ++i) {
					if (i > 0)
						sRemap += " ";
					sRemap += remap_choice[i]->getSelected();
				}

				addRemaps();

				// Counts amount of commas in remap names.
				int remapCount = static_cast<int>(std::count(remapNames.begin(), remapNames.end(), ',')+1);

				GuiMenu::addJoyBtnEntry(remapName, std::to_string(remapCount));
			}, _("NO"), nullptr));
		}
		else {
			addRemaps();
		}
	});
}

void GuiMenu::createBtnJoyCfgName(Window *window, GuiSettings *systemConfiguration,
	std::string prefixName)
{
	InputConfig* inputCfg = nullptr;
	if (InputManager::getInstance()->getNumJoysticks() > 0) {
		auto configList = InputManager::getInstance()->getInputConfigs();
		inputCfg = configList[0];
	}

	if (inputCfg == nullptr)
		return;

	auto theme = ThemeData::getMenuTheme();

	ComponentListRow row;

	auto createText = std::make_shared<TextComponent>(window, _("CREATE BUTTON REMAP"), theme->Text.font, theme->Text.color);
	row.addElement(createText, true);
	
	auto updateVal = [window, prefixName](const std::string& newVal)
	{
		if (newVal.empty()) return;
		if(newVal.find(',') != std::string::npos || newVal.find(' ') != std::string::npos) {
			window->pushGui(new GuiMsgBox(window, _("YOU CANNOT HAVE COMMAS OR SPACES IN REMAP NAME"), _("OK"), nullptr));
		  return;
		}

		std::string btnOrder = SystemConf::getInstance()->get(prefixName + ".joy_btn_order."+newVal);
		if (!btnOrder.empty()) {
			window->pushGui(new GuiMsgBox(window, _("REMAP NAME MUST BE UNIQUE"), _("OK"), nullptr));
			return;
		}

		GuiSettings* systemConfiguration = new GuiSettings(window, "CREATE REMAP");
		window->pushGui(new GuiMsgBox(window, _("All buttons must be assigned and no duplicates."), _("OK"),
		[window, systemConfiguration, prefixName, newVal] {
			GuiMenu::createBtnJoyCfgRemap(window, systemConfiguration, prefixName, newVal);
			window->pushGui(systemConfiguration);
		}));
	};

	row.makeAcceptInputHandler([window, createText, updateVal]
	{
		if (Settings::getInstance()->getBool("UseOSK"))
			window->pushGui(new GuiTextEditPopupKeyboard(window, _("REMAP NAME"), "", updateVal, false));
		else
			window->pushGui(new GuiTextEditPopup(window, _("REMAP NAME"), "", updateVal, false));
	});
	
	systemConfiguration->addRow(row);
}

void GuiMenu::removeJoyBtnEntry(int index) {
	del_choice->removeIndex(index);
	edit_choice->removeIndex(index);
	btn_choice->removeIndex(index);
}

void GuiMenu::addJoyBtnEntry(std::string name, std::string val) {
	del_choice->add(name, val, false);
	edit_choice->add(name, val, false);
	btn_choice->add(name, val, false);
}

void GuiMenu::deleteBtnJoyCfg(Window *window, GuiSettings *systemConfiguration,
	 std::string prefixName)
{
	const std::function<void()> saveFunc([window, prefixName] {
		int delIndex = GuiMenu::del_choice->getSelectedIndex();
		int remapIndex = delIndex-1;

		// Protect default maps (mk and sf).
		if (delIndex <= 2)
		{
			window->pushGui(new GuiMsgBox(window,  _("CANNOT DELETE DEFAULT BUTTON MAPS."),
				_("OK"), nullptr));
			GuiMenu::del_choice->selectFirstItem();
			return;
		}

		// Delete does not remove the existing button maps so any game/emulator references will still work.
		std::string remapNames = SystemConf::getInstance()->get(prefixName + ".joy_btn_remap_names");

		std::string remapName = GuiMenu::del_choice->getSelectedName();
		remapNames = Utils::String::replace(remapNames, ","+remapName, "");
		remapNames = Utils::String::replace(remapNames, remapName+",", "");
		SystemConf::getInstance()->set(prefixName + ".joy_btn_remap_names", remapNames);

		SystemConf::getInstance()->set(prefixName + ".joy_btn_order."+remapName, "");

		int btnIndex = GuiMenu::btn_choice->getSelectedIndex();
		GuiMenu::removeJoyBtnEntry(delIndex);

		if (btnIndex == delIndex)
			btnIndex = 0;
		if (btnIndex >= GuiMenu::del_choice->size() && GuiMenu::del_choice->size() > 0)
			btnIndex--;

		GuiMenu::del_choice->selectFirstItem();
		GuiMenu::edit_choice->selectFirstItem();
		GuiMenu::btn_choice->selectIndex(btnIndex);
	});

	del_choice->setSelectedChangedCallback(
		[window, systemConfiguration, saveFunc, prefixName] (std::string s) {
		long unsigned int m1 = (long unsigned int) &(*window->peekGui());
		long unsigned int m2 = (long unsigned int) &(*systemConfiguration);
		if (m1 == m2)
			return;

		int delIndex = GuiMenu::del_choice->getSelectedIndex();
		if (delIndex <= 0)
			return;

		window->pushGui(new GuiMsgBox(window,  _("ARE YOU SURE YOU WANT TO DELETE THE REMAP?"),
			_("YES"), saveFunc, _("NO"), nullptr));
	});

	systemConfiguration->addSaveFunc([window, saveFunc, prefixName] {
		int delIndex = GuiMenu::del_choice->getSelectedIndex();
		if (delIndex <= 0)
			return;

		window->pushGui(new GuiMsgBox(window,  _("ARE YOU SURE YOU WANT TO DELETE THE REMAP?"),
			_("YES"), saveFunc, _("NO"), nullptr));
	});
}

std::shared_ptr<OptionListComponent<std::string>> GuiMenu::createJoyBtnOptionList(Window *window, std::string prefixName,
	std::string title, int selectId)
{
	auto btn_cfg = std::make_shared< OptionListComponent<std::string> >(window, title, false);

	std::string btnNames = SystemConf::getInstance()->get(prefixName + ".joy_btn_remap_names");
	std::vector<std::string> btn_names(explode(btnNames));

	if (prefixName == "auto" || prefixName.empty() || btn_names.size() == 0) {
		btn_cfg->add(_("NONE"), "-1", true);
		return btn_cfg;
	}

	if (selectId >= btn_names.size())
		selectId = -1;

	int i = 0;
	btn_cfg->add(_("NONE"), "-1", selectId == -1);
	for (auto it = btn_names.cbegin(); it != btn_names.cend(); it++) {
		btn_cfg->add(*it, *it, selectId == i++);
	}
	return btn_cfg;
}

#endif

void GuiMenu::popSpecificConfigurationGui(Window* mWindow, std::string title, std::string configName, SystemData *systemData, FileData* fileData, bool selectCoreLine)
{
	// The system configuration
	GuiSettings* systemConfiguration = new GuiSettings(mWindow, title.c_str());

	if (fileData != nullptr)
		systemConfiguration->setSubTitle(systemData->getFullName());

	std::string currentEmulator = fileData != nullptr ? fileData->getEmulator(false) : systemData->getEmulator(false);
	std::string currentCore = fileData != nullptr ? fileData->getCore(false) : systemData->getCore(false);

	if (systemData->hasEmulatorSelection())
	{
		auto emulChoice = std::make_shared<OptionListComponent<std::string>>(mWindow, _("Emulator"), false);
		emulChoice->add(_("AUTO"), "", false);
		for (auto& emul : systemData->getEmulators())
		{
			if (emul.cores.size() == 0)
				emulChoice->add(emul.name, emul.name, emul.name == currentEmulator);
			else
			{
				for (auto& core : emul.cores)
				{
					bool selected = (emul.name == currentEmulator && core.name == currentCore);

					if (emul.name == core.name)
						emulChoice->add(emul.name, emul.name + "/" + core.name, selected);
					else
						emulChoice->add(emul.name + ": " + Utils::String::replace(core.name, "_", " "), emul.name + "/" + core.name, selected);
				}
			}
		}

		if (!emulChoice->hasSelection())
			emulChoice->selectFirstItem();

		emulChoice->setSelectedChangedCallback([mWindow, title, systemConfiguration, systemData, fileData, configName, emulChoice](std::string s)
		{
			std::string newEmul;
			std::string newCore;

			auto values = Utils::String::split(emulChoice->getSelected(), '/');
			if (values.size() > 0)
				newEmul = values[0];

			if (values.size() > 1)
				newCore = values[1];

			if (fileData != nullptr)
			{
				fileData->setEmulator(newEmul);
				fileData->setCore(newCore);
			}
			else
			{
				SystemConf::getInstance()->set(configName + ".emulator", newEmul);
				SystemConf::getInstance()->set(configName + ".core", newCore);
			}

			popSpecificConfigurationGui(mWindow, title, configName, systemData, fileData);
			delete systemConfiguration;

		});

		systemConfiguration->addWithLabel(_("Emulator"), emulChoice);
	}

	auto customFeatures = systemData->getCustomFeatures(currentEmulator, currentCore);

#ifdef _ENABLEEMUELEC
		// Conf gptokeyb.
		if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::gptokeyb) || currentEmulator == "ports")
		{
			auto emuelec_virtual_kb = std::make_shared< OptionListComponent<std::string> >(mWindow, "Virtual Keyboard", false);
			std::vector<std::string> virtual_kb;

			std::string def_vkb;
			for(std::stringstream ss(Utils::Platform::getShOutput(R"(/usr/bin/emuelec-utils get_filenames_no_ext /emuelec/configs/gptokeyb)")); getline(ss, def_vkb, ','); ) {
				if (!std::count(virtual_kb.begin(), virtual_kb.end(), def_vkb)) {
					 virtual_kb.push_back(def_vkb);
				}
			}

			std::string index = SystemConf::getInstance()->get(configName + ".gptokeyb");
			if (index.empty())
				index = "auto";

			emuelec_virtual_kb->add(_("AUTO"), "auto", index == "auto");
			for (auto it = virtual_kb.cbegin(); it != virtual_kb.cend(); it++) {
				emuelec_virtual_kb->add(*it, *it, index == *it);
			}
		
			systemConfiguration->addWithLabel(_("VIRTUAL KEYBOARD"), emuelec_virtual_kb);

			systemConfiguration->addSaveFunc([mWindow, configName, emuelec_virtual_kb] {
				std::string vkb_choice = emuelec_virtual_kb->getSelected();

				if (vkb_choice == "auto")
					vkb_choice = "";

				SystemConf::getInstance()->set(configName + ".gptokeyb", vkb_choice);
			});
		}
#endif

#ifdef _ENABLEEMUELEC
	// NATIVE VIDEO.

	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::nativevideo))
	{
		auto videoNativeResolutionMode_choice = createNativeVideoResolutionModeOptionList(mWindow, configName);
		systemConfiguration->addWithLabel(_("NATIVE VIDEO"), videoNativeResolutionMode_choice);

		const std::function<void()> video_changed([mWindow, configName, videoNativeResolutionMode_choice] {

			std::string def_video;
			std::string video_choice = videoNativeResolutionMode_choice->getSelected();
			bool safe_video = false;

			if (video_choice == "auto")
				safe_video = true;
			else {
				for(std::stringstream ss(Utils::Platform::getShOutput(R"(/usr/bin/emuelec-utils resolutions)")); getline(ss, def_video, ','); ) {
					if (video_choice == def_video) {
						safe_video = true;
						break;
					}
				}
			}

			const std::function<void()> saveFunc([configName, videoNativeResolutionMode_choice] {
				SystemConf::getInstance()->set(configName + ".nativevideo", videoNativeResolutionMode_choice->getSelected());
				SystemConf::getInstance()->saveSystemConf();				
			});

			const std::function<void()> abortFunc([configName, videoNativeResolutionMode_choice] {
				videoNativeResolutionMode_choice->selectFirstItem();
				SystemConf::getInstance()->set(configName + ".nativevideo", "");
				SystemConf::getInstance()->saveSystemConf();
			});

			if (!safe_video) {
				mWindow->pushGui(new GuiMsgBox(mWindow,  video_choice + _(" UNSAFE RESOLUTION DETECTED, CONTINUE?"),
					_("YES"), saveFunc, _("NO"), abortFunc));
			}
			else {
				saveFunc();
			}
		});

		systemConfiguration->addSaveFunc([mWindow, video_changed] {
			video_changed();
		});
	}
#endif

#ifdef _ENABLEEMUELEC
	// JOY BUTTON REMAP.

	std::string tEmulator = fileData != nullptr ? fileData->getEmulator(true) : systemData->getEmulator(true);
	if (tEmulator == "auto")
		tEmulator = systemData->getEmulator(true);
	if (!tEmulator.empty() && systemData->isFeatureSupported(tEmulator, currentCore, EmulatorFeatures::joybtnremap))
	{
		[&] {
			std::string prefixName = tEmulator;
			if (SystemConf::getInstance()->get(prefixName + ".joy_btn_defaults").empty())
				SystemConf::getInstance()->set(prefixName + ".joy_btn_defaults", "B button (S),A button (E),Y button (W),X button (N),L1 button,R1 button,L2 button,R2 button");
			if (SystemConf::getInstance()->get(prefixName + ".joy_btn_button_names").empty())
				SystemConf::getInstance()->set(prefixName + ".joy_btn_button_names" , "Button1,Button2,Button3,Button4,Button5,Button6,Button7,Button8" );
			if (SystemConf::getInstance()->get(prefixName + ".joy_btn_button_names.mk").empty())
				SystemConf::getInstance()->set(prefixName + ".joy_btn_button_names.mk" , "HK,B1,LP,HP,LK,B2,L2,R2" );
			if (SystemConf::getInstance()->get(prefixName + ".joy_btn_button_names.sf").empty())
				SystemConf::getInstance()->set(prefixName + ".joy_btn_button_names.sf" , "MP,HP,FK,MK,FP,HK,L2,R2" );
			if (SystemConf::getInstance()->get(prefixName + ".joy_btn_remap_names").empty())
				SystemConf::getInstance()->set(prefixName + ".joy_btn_remap_names", "mk,sf" );
			if (SystemConf::getInstance()->get(prefixName + ".joy_btn_order.mk").empty())
				SystemConf::getInstance()->set(prefixName + ".joy_btn_order.mk", "2 4 3 0 1 5 6 7" );
			if (SystemConf::getInstance()->get(prefixName + ".joy_btn_order.sf").empty())
				SystemConf::getInstance()->set(prefixName + ".joy_btn_order.sf", "2 3 4 0 1 5 6 7" );

			std::string remapName = SystemConf::getInstance()->get(configName + ".joy_btn_index");
			std::string btnNames = SystemConf::getInstance()->get(prefixName + ".joy_btn_remap_names");
			std::vector<std::string> btn_names(explode(btnNames));
			int btnId = -1;
			if (!remapName.empty()) {
				for (int i = 0; i < btn_names.size(); ++i) {
					if (remapName == btn_names[i])
					{
						btnId = i;
						break;
					}
				}				
			}

			btn_choice = createJoyBtnOptionList(mWindow, prefixName, _("BUTTON REMAP"), btnId);
			edit_choice = createJoyBtnOptionList(mWindow, prefixName, _("EDIT REMAP"));
			del_choice = createJoyBtnOptionList(mWindow, prefixName, _("DELETE REMAP"));

			systemConfiguration->addWithLabel(_("BUTTON REMAP"), btn_choice);
			systemConfiguration->addWithLabel(_("EDIT REMAP"), edit_choice);
			systemConfiguration->addWithLabel(_("DELETE REMAP"), del_choice);

			systemConfiguration->addSaveFunc([configName, prefixName] {
				std::string remapName = GuiMenu::btn_choice->getSelectedName();
				if (remapName == "NONE")
					remapName = "";
				SystemConf::getInstance()->set(configName + ".joy_btn_index", remapName);				
			});

			GuiMenu::editJoyBtnRemapOptionList(mWindow, systemConfiguration, prefixName);
			GuiMenu::createBtnJoyCfgName(mWindow, systemConfiguration, prefixName);
			GuiMenu::deleteBtnJoyCfg(mWindow, systemConfiguration, prefixName);
		}();
	}

 // PR - HLE BIOS.

	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::hlebios))
	{
		systemConfiguration->addSwitch(_("Use HLE BIOS"), configName + ".hlebios", false);
	}
#endif

	// Screen ratio choice
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::ratio))
	{
		auto ratio_choice = createRatioOptionList(mWindow, configName);
		systemConfiguration->addWithDescription(_("GAME ASPECT RATIO"), _("Force the game to render in this aspect ratio."), ratio_choice);
		systemConfiguration->addSaveFunc([configName, ratio_choice] { SystemConf::getInstance()->set(configName + ".ratio", ratio_choice->getSelected()); });
	}

	// video resolution mode
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::videomode))
	{
		auto videoResolutionMode_choice = createVideoResolutionModeOptionList(mWindow, configName);
		systemConfiguration->addWithDescription(_("VIDEO MODE"), _("Sets the display's resolution. Does not affect the rendering resolution."), videoResolutionMode_choice);
		systemConfiguration->addSaveFunc([configName, videoResolutionMode_choice] { SystemConf::getInstance()->set(configName + ".videomode", videoResolutionMode_choice->getSelected()); });
	}

	// smoothing
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::smooth))
	{
		auto smoothing_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SMOOTH GAMES (BILINEAR FILTERING)"));
		smoothing_enabled->addRange({ { _("AUTO"), "auto" },{ _("ON") , "1" },{ _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".smooth"));
		systemConfiguration->addWithLabel(_("SMOOTH GAMES (BILINEAR FILTERING)"), smoothing_enabled);
		systemConfiguration->addSaveFunc([configName, smoothing_enabled] { SystemConf::getInstance()->set(configName + ".smooth", smoothing_enabled->getSelected()); });
	}

	// Rewind
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::rewind))
	{
		auto rewind_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("REWIND"));
		rewind_enabled->addRange({ { _("AUTO"), "auto" }, { _("ON") , "1" }, { _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".rewind"));
		systemConfiguration->addWithLabel(_("REWIND"), rewind_enabled);
		systemConfiguration->addSaveFunc([configName, rewind_enabled] { SystemConf::getInstance()->set(configName + ".rewind", rewind_enabled->getSelected()); });
	}

	// AUTO SAVE/LOAD
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::autosave) && !customFeatures.hasFeature("autosave"))
	{
		auto autosave_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("AUTO SAVE/LOAD ON GAME LAUNCH"));
		autosave_enabled->addRange({ { _("AUTO"), "auto" }, { _("ON") , "1" }, { _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".autosave"));
		systemConfiguration->addWithDescription(_("AUTO SAVE/LOAD ON GAME LAUNCH"), _("Load latest save state on game launch and save state when exiting game."), autosave_enabled);
		systemConfiguration->addSaveFunc([configName, autosave_enabled] { SystemConf::getInstance()->set(configName + ".autosave", autosave_enabled->getSelected()); });
	}
#ifdef _ENABLEEMUELEC
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::cloudsave))
	{
		auto enable_cloudsave = std::make_shared<SwitchComponent>(mWindow);
		bool cloudSaveEnabled = SystemConf::getInstance()->get(configName + ".cloudsave") == "1";
		enable_cloudsave->setState(cloudSaveEnabled);
		systemConfiguration->addWithLabel(_("ENABLE CLOUD SAVE"), enable_cloudsave);

		systemConfiguration->addSaveFunc([enable_cloudsave, mWindow, configName] {
			bool cloudSaveEnabled = enable_cloudsave->getState();
			SystemConf::getInstance()->set(configName + ".cloudsave", cloudSaveEnabled ? "1" : "0");
			SystemConf::getInstance()->saveSystemConf();
		});
	}

	// Shaders preset
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::SHADERS) &&
		systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::shaders))
	{
        std::string a;
		auto shaders_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("SHADERS SET"),false);
		std::string currentShader = SystemConf::getInstance()->get(configName + ".shaderset");
		if (currentShader.empty()) {
			currentShader = std::string("auto");
		}

		shaders_choices->add(_("AUTO"), "auto", currentShader == "auto");
		shaders_choices->add(_("NONE"), "none", currentShader == "none");
		for(std::stringstream ss(Utils::Platform::getShOutput(R"(/usr/bin/emuelec-utils getshaders)")); getline(ss, a, ','); )
		shaders_choices->add(a, a, currentShader == a); // emuelec
		systemConfiguration->addWithLabel(_("SHADERS SET"), shaders_choices);
		systemConfiguration->addSaveFunc([shaders_choices, configName] { SystemConf::getInstance()->set(configName + ".shaderset", shaders_choices->getSelected()); });
	}

#if defined(ODROIDGOA) || defined(_ENABLEGAMEFORCE)
	// RGA SCALING
		auto rgascale_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("RGA SCALING"));
		rgascale_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".rgascale") != "0" && SystemConf::getInstance()->get(configName + ".rgascale") != "1");
		rgascale_enabled->add(_("ON"), "1", SystemConf::getInstance()->get(configName + ".rgascale") == "1");
		rgascale_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get(configName + ".rgascale") == "0");
		systemConfiguration->addWithLabel(_("RGA SCALING"), rgascale_enabled);
		systemConfiguration->addSaveFunc([configName, rgascale_enabled] { SystemConf::getInstance()->set(configName + ".rgascale", rgascale_enabled->getSelected()); });
#endif

	// Vertical Game
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::vertical))
	{
		auto vertical_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("ENABLE VERTICAL"));
		vertical_enabled->add(_("OFF"), "auto", SystemConf::getInstance()->get(configName + ".vertical") != "1");
		vertical_enabled->add(_("ON"), "1", SystemConf::getInstance()->get(configName + ".vertical") == "1");
		systemConfiguration->addWithLabel(_("ENABLE VERTICAL"), vertical_enabled);
		systemConfiguration->addSaveFunc([configName, vertical_enabled] { SystemConf::getInstance()->set(configName + ".vertical", vertical_enabled->getSelected()); });
        
        auto vert_aspect_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("VERTICAL ASPECT RATIO"));
		vert_aspect_enabled->addRange({ { _("16:9") , "1" }, { _("3:2") , "7" }, { _("21:9"), "4" }, { _("4:3") , "0" } }, SystemConf::getInstance()->get(configName + ".vert_aspect"));
		systemConfiguration->addWithLabel(_("VERTICAL ASPECT RATIO"), vert_aspect_enabled);
		systemConfiguration->addSaveFunc([configName, vert_aspect_enabled] { SystemConf::getInstance()->set(configName + ".vert_aspect", vert_aspect_enabled->getSelected()); });
	}
#else
	
	// Shaders preset
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::SHADERS) &&
		systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::shaders))
	{
		auto installedShaders = ApiSystem::getInstance()->getShaderList(systemData->getName(), currentEmulator, currentCore);
		if (installedShaders.size() > 0)
		{
			std::string currentShader = SystemConf::getInstance()->get(configName + ".shaderset");

			auto shaders_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("SHADER SET"), false);
			shaders_choices->add(_("AUTO"), "auto", currentShader.empty() || currentShader == "auto");
			shaders_choices->add(_("NONE"), "none", currentShader == "none");

			for (auto shader : installedShaders)
				shaders_choices->add(_(Utils::String::toUpper(shader).c_str()), shader, currentShader == shader);

			if (!shaders_choices->hasSelection())
				shaders_choices->selectFirstItem();

			systemConfiguration->addWithLabel(_("SHADER SET"), shaders_choices);
			systemConfiguration->addSaveFunc([configName, shaders_choices] { SystemConf::getInstance()->set(configName + ".shaderset", shaders_choices->getSelected()); });
		}
	}
#endif

	// Video Filters preset
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::VIDEOFILTERS) &&
		systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::videofilters))
	{
		auto installedVideofilters = ApiSystem::getInstance()->getVideoFilterList(systemData->getName(), currentEmulator, currentCore);
		if (installedVideofilters.size() > 0)
		{
			std::string currentVideofilter = SystemConf::getInstance()->get(configName + ".videofilter");

			auto videofilters_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("VIDEO FILTER"), false);
			videofilters_choices->add(_("AUTO"), "auto", currentVideofilter.empty() || currentVideofilter == "auto");
			videofilters_choices->add(_("NONE"), "none", currentVideofilter == "none");

			for (auto videofilter : installedVideofilters)
				videofilters_choices->add(_(Utils::String::toUpper(videofilter).c_str()), videofilter, currentVideofilter == videofilter);

			if (!videofilters_choices->hasSelection())
				videofilters_choices->selectFirstItem();

			systemConfiguration->addWithLabel(_("VIDEO FILTER"), videofilters_choices);
			systemConfiguration->addSaveFunc([configName, videofilters_choices] { SystemConf::getInstance()->set(configName + ".videofilter", videofilters_choices->getSelected()); });
		}
	}

	// Integer scale
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::pixel_perfect))
	{
		auto integerscale_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("INTEGER SCALING (PIXEL PERFECT)"));
		integerscale_enabled->addRange({ { _("AUTO"), "auto" },{ _("ON") , "1" },{ _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".integerscale"));
		systemConfiguration->addWithLabel(_("INTEGER SCALING (PIXEL PERFECT)"), integerscale_enabled);
		systemConfiguration->addSaveFunc([integerscale_enabled, configName] { SystemConf::getInstance()->set(configName + ".integerscale", integerscale_enabled->getSelected()); });
#ifdef _ENABLEEMUELEC		
        auto integerscaleoverscale_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("INTEGER SCALING (OVERSCALE)"));
		integerscaleoverscale_enabled->addRange({ { _("AUTO"), "auto" },{ _("ON") , "1" },{ _("SMART") , "2" },{ _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".integerscaleoverscale"));
		systemConfiguration->addWithLabel(_("INTEGER SCALING (OVERSCALE)"), integerscaleoverscale_enabled);
		systemConfiguration->addSaveFunc([integerscaleoverscale_enabled, configName] { SystemConf::getInstance()->set(configName + ".integerscaleoverscale", integerscaleoverscale_enabled->getSelected()); });
#endif
	}

#ifdef _ENABLEEMUELEC
	// bezel
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::decoration))
	{
		auto bezel_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("BEZEL"));
		bezel_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".bezel") != "0" && SystemConf::getInstance()->get(configName + ".bezel") != "1");
		bezel_enabled->add(_("YES"), "1", SystemConf::getInstance()->get(configName + ".bezel") == "1");
		bezel_enabled->add(_("NO"), "0", SystemConf::getInstance()->get(configName + ".bezel") == "0");
		systemConfiguration->addWithLabel(_("BEZEL"), bezel_enabled);
		systemConfiguration->addSaveFunc([bezel_enabled, configName] { SystemConf::getInstance()->set(configName + ".bezel", bezel_enabled->getSelected()); });
	}

	// maxperf
		auto maxperf_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("ENABLE MAX PERFORMANCE"));
		maxperf_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".maxperf") != "0" && SystemConf::getInstance()->get(configName + ".maxperf") != "1");
		maxperf_enabled->add(_("YES"), "1", SystemConf::getInstance()->get(configName + ".maxperf") == "1");
		maxperf_enabled->add(_("NO"), "0", SystemConf::getInstance()->get(configName + ".maxperf") == "0");
		systemConfiguration->addWithLabel(_("ENABLE MAX PERFORMANCE"), maxperf_enabled);
		systemConfiguration->addSaveFunc([maxperf_enabled, configName] { SystemConf::getInstance()->set(configName + ".maxperf", maxperf_enabled->getSelected()); });
#else
	// decorations
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::DECORATIONS) && systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::decoration))
	{
		auto sets = GuiMenu::getDecorationsSets(systemData);
		if (sets.size() > 0)
		{
#ifdef BATOCERA
			systemConfiguration->addEntry(_("DECORATIONS"), true, [mWindow, configName, systemData, sets]
			{
				GuiSettings* decorations_window = new GuiSettings(mWindow, _("DECORATIONS").c_str());

				addDecorationSetOptionListComponent(mWindow, decorations_window, sets, configName);
				
				// stretch bezels
				auto bezel_stretch_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("STRETCH BEZELS (4K & ULTRAWIDE)"));
				bezel_stretch_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".bezel_stretch") != "0" && SystemConf::getInstance()->get(configName + ".bezel_stretch") != "1");
				bezel_stretch_enabled->add(_("ON"), "1", SystemConf::getInstance()->get(configName + ".bezel_stretch") == "1");
				bezel_stretch_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get(configName + ".bezel_stretch") == "0");
				decorations_window->addWithLabel(_("STRETCH BEZELS (4K & ULTRAWIDE)"), bezel_stretch_enabled);
				decorations_window->addSaveFunc([bezel_stretch_enabled, configName] {
					if (bezel_stretch_enabled->changed()) {
						SystemConf::getInstance()->set(configName + ".bezel_stretch", bezel_stretch_enabled->getSelected());
						SystemConf::getInstance()->saveSystemConf();
					}
				});

				// tattoo and controller overlays
				auto bezel_tattoo = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW CONTROLLER OVERLAYS"));
				bezel_tattoo->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".bezel.tattoo") != "0"
					&& SystemConf::getInstance()->get(configName + ".bezel.tattoo") != "system"
					&& SystemConf::getInstance()->get(configName + ".bezel.tattoo") != "custom");
				bezel_tattoo->add(_("NO"), "0", SystemConf::getInstance()->get(configName + ".bezel.tattoo") == "0");
				bezel_tattoo->add(_("SYSTEM CONTROLLERS"), "system", SystemConf::getInstance()->get(configName + ".bezel.tattoo") == "system");
				bezel_tattoo->add(_("CUSTOM .PNG IMAGE"), "custom", SystemConf::getInstance()->get(configName + ".bezel.tattoo") == "custom");
				decorations_window->addWithLabel(_("SHOW CONTROLLER OVERLAYS"), bezel_tattoo);
				decorations_window->addSaveFunc([bezel_tattoo, configName] {
					if (bezel_tattoo->changed()) {
						SystemConf::getInstance()->set(configName + ".bezel.tattoo", bezel_tattoo->getSelected());
						SystemConf::getInstance()->saveSystemConf();
					}
				});

				auto bezel_tattoo_corner = std::make_shared<OptionListComponent<std::string>>(mWindow, _("OVERLAY CORNER"));
				bezel_tattoo_corner->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".bezel.tattoo_corner") != "NW"
					&& SystemConf::getInstance()->get(configName + ".bezel.tattoo_corner") != "NE"
					&& SystemConf::getInstance()->get(configName + ".bezel.tattoo_corner") != "SE"
					&& SystemConf::getInstance()->get(configName + ".bezel.tattoo_corner") != "SW");
				bezel_tattoo_corner->add(_("NORTH WEST"), "NW", SystemConf::getInstance()->get(configName + ".bezel.tattoo_corner") == "NW");
				bezel_tattoo_corner->add(_("NORTH EAST"), "NE", SystemConf::getInstance()->get(configName + ".bezel.tattoo_corner") == "NE");
				bezel_tattoo_corner->add(_("SOUTH EAST"), "SE", SystemConf::getInstance()->get(configName + ".bezel.tattoo_corner") == "SE");
				bezel_tattoo_corner->add(_("SOUTH WEST"), "SW", SystemConf::getInstance()->get(configName + ".bezel.tattoo_corner") == "SW");
				decorations_window->addWithLabel(_("OVERLAY CORNER"), bezel_tattoo_corner);
				decorations_window->addSaveFunc([bezel_tattoo_corner, configName] {
					if (bezel_tattoo_corner->changed()) {
						SystemConf::getInstance()->set(configName + ".bezel.tattoo_corner", bezel_tattoo_corner->getSelected());
						SystemConf::getInstance()->saveSystemConf();
					}
				});

				std::string tatpath = configName + ".bezel.tattoo_file";
				const char *bezelpath = const_cast<char*>(tatpath.data());
				decorations_window->addInputTextRow(_("CUSTOM .PNG IMAGE PATH"), bezelpath, false);

				mWindow->pushGui(decorations_window);
			});
#else
			addDecorationSetOptionListComponent(mWindow, systemConfiguration, sets, configName);
#endif		
		}
	}	

#endif
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::latency_reduction))	
		systemConfiguration->addEntry(_("LATENCY REDUCTION"), true, [mWindow, configName] { openLatencyReductionConfiguration(mWindow, configName); });

	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::colorization))
	{
		// gameboy colorize
		auto colorizations_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("COLORIZATION"), false);
		std::string currentColorization = SystemConf::getInstance()->get(configName + "-renderer.colorization");
		if (currentColorization.empty())
			currentColorization = std::string("auto");
		
		colorizations_choices->add(_("AUTO"), "auto", currentColorization == "auto");
		colorizations_choices->add(_("NONE"), "none", currentColorization == "none");
#ifdef _ENABLEEMUELEC
        colorizations_choices->add(_("GBC"), "GBC", currentColorization == "GBC");
		colorizations_choices->add(_("SGB"), "SGB", currentColorization == "SGB");
#endif
		colorizations_choices->add(_("Best Guess"), "Best Guess", currentColorization == "Best Guess");

		const char* all_gambate_gc_colors_modes[] = { "GB - DMG",
								 "GB - Light",
								 "GB - Pocket",
								 "GBC - Blue",
								 "GBC - Brown",
								 "GBC - Dark Blue",
								 "GBC - Dark Brown",
								 "GBC - Dark Green",
								 "GBC - Grayscale",
								 "GBC - Green",
								 "GBC - Inverted",
								 "GBC - Orange",
								 "GBC - Pastel Mix",
								 "GBC - Red",
								 "GBC - Yellow",
								 "SGB - 1A",
								 "SGB - 1B",
								 "SGB - 1C",
								 "SGB - 1D",
								 "SGB - 1E",
								 "SGB - 1F",
								 "SGB - 1G",
								 "SGB - 1H",
								 "SGB - 2A",
								 "SGB - 2B",
								 "SGB - 2C",
								 "SGB - 2D",
								 "SGB - 2E",
								 "SGB - 2F",
								 "SGB - 2G",
								 "SGB - 2H",
								 "SGB - 3A",
								 "SGB - 3B",
								 "SGB - 3C",
								 "SGB - 3D",
								 "SGB - 3E",
								 "SGB - 3F",
								 "SGB - 3G",
								 "SGB - 3H",
								 "SGB - 4A",
								 "SGB - 4B",
								 "SGB - 4C",
								 "SGB - 4D",
								 "SGB - 4E",
								 "SGB - 4F",
								 "SGB - 4G",
								 "SGB - 4H",
								 "Special 1",
								 "Special 2",
								 "Special 3",
								 "TWB01 - 756 Production",
								 "TWB02 - AKB48 Pink",
								 "TWB03 - Angry Volcano",
								 "TWB04 - Anime Expo",
								 "TWB05 - Aqours Blue",
								 "TWB06 - Aquatic Iro",
								 "TWB07 - Bandai Namco",
								 "TWB08 - Blossom Pink",
								 "TWB09 - Bubbles Blue",
								 "TWB10 - Builder Yellow",
								 "TWB11 - Buttercup Green",
								 "TWB12 - Camouflage",
								 "TWB13 - Cardcaptor Pink",
								 "TWB14 - Christmas",
								 "TWB15 - Crunchyroll Orange",
								 "TWB16 - Digivice",
								 "TWB17 - Do The Dew",
								 "TWB18 - Eevee Brown",
								 "TWB19 - Fruity Orange",
								 "TWB20 - Game.com",
								 "TWB21 - Game Grump Orange",
								 "TWB22 - GameKing",
								 "TWB23 - Game Master",
								 "TWB24 - Ghostly Aoi",
								 "TWB25 - Golden Wild",
								 "TWB26 - Green Banana",
								 "TWB27 - Greenscale",
								 "TWB28 - Halloween",
								 "TWB29 - Hero Yellow",
								 "TWB30 - Hokage Orange",
								 "TWB31 - Labo Fawn",
								 "TWB32 - Legendary Super Saiyan",
								 "TWB33 - Lemon Lime Green",
								 "TWB34 - Lime Midori",
								 "TWB35 - Mania Plus Green",
								 "TWB36 - Microvision",
								 "TWB37 - Million Live Gold",
								 "TWB38 - Miraitowa Blue",
								 "TWB39 - NASCAR",
								 "TWB40 - Neo Geo Pocket",
								 "TWB41 - Neon Blue",
								 "TWB42 - Neon Green",
								 "TWB43 - Neon Pink",
								 "TWB44 - Neon Red",
								 "TWB45 - Neon Yellow",
								 "TWB46 - Nick Orange",
								 "TWB47 - Nijigasaki Orange",
								 "TWB48 - Odyssey Gold",
								 "TWB49 - Patrick Star Pink",
								 "TWB50 - Pikachu Yellow",
								 "TWB51 - Pocket Tales",
								 "TWB52 - Pokemon mini",
								 "TWB53 - Pretty Guardian Gold",
								 "TWB54 - S.E.E.S. Blue",
								 "TWB55 - Saint Snow Red",
								 "TWB56 - Scooby-Doo Mystery",
								 "TWB57 - Shiny Sky Blue",
								 "TWB58 - Sidem Green",
								 "TWB59 - Slime Blue",
								 "TWB60 - Spongebob Yellow",
								 "TWB61 - Stone Orange",
								 "TWB62 - Straw Hat Red",
								 "TWB63 - Superball Ivory",
								 "TWB64 - Super Saiyan Blue",
								 "TWB65 - Super Saiyan Rose",
								 "TWB66 - Supervision",
								 "TWB67 - Survey Corps Brown",
								 "TWB68 - Tea Midori",
								 "TWB69 - TI-83",
								 "TWB70 - Tokyo Midtown",
								 "TWB71 - Travel Wood",
								 "TWB72 - Virtual Boy",
								 "TWB73 - VMU",
								 "TWB74 - Wisteria Murasaki",
								 "TWB75 - WonderSwan",
								 "TWB76 - Yellow Banana" };

		int n_all_gambate_gc_colors_modes = 126;
		for (int i = 0; i < n_all_gambate_gc_colors_modes; i++)
			colorizations_choices->add(all_gambate_gc_colors_modes[i], all_gambate_gc_colors_modes[i], currentColorization == std::string(all_gambate_gc_colors_modes[i]));
		
#ifdef _ENABLEEMUELEC
        if (CustomFeatures::FeaturesLoaded || (!CustomFeatures::FeaturesLoaded && (systemData->getName() == "gb" || systemData->getName() == "gbc" || systemData->getName() == "gb2players" || systemData->getName() == "gbc2players" || systemData->getName() == "gbh" || systemData->getName() == "gbch"))) // only for gb, gbc and gb2players gbh gbch
#else
		if (CustomFeatures::FeaturesLoaded || (!CustomFeatures::FeaturesLoaded && (systemData->getName() == "gb" || systemData->getName() == "gbc" || systemData->getName() == "gb2players" || systemData->getName() == "gbc2players")))  // only for gb, gbc and gb2players
#endif
		{
			systemConfiguration->addWithLabel(_("COLORIZATION"), colorizations_choices);
			systemConfiguration->addSaveFunc([colorizations_choices, configName] { SystemConf::getInstance()->set(configName + "-renderer.colorization", colorizations_choices->getSelected()); });
		}		
	}

#ifndef _ENABLEEMUELEC
	// ps2 full boot
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::fullboot))
	{
		if (CustomFeatures::FeaturesLoaded || (!CustomFeatures::FeaturesLoaded && systemData->getName() == "ps2")) // only for ps2			
		{
			auto fullboot_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("FULL BOOT"));
			fullboot_enabled->addRange({ { _("AUTO"), "auto" },{ _("ON") , "1" },{ _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".fullboot"));
			systemConfiguration->addWithLabel(_("FULL BOOT"), fullboot_enabled);
			systemConfiguration->addSaveFunc([fullboot_enabled, configName] { SystemConf::getInstance()->set(configName + ".fullboot", fullboot_enabled->getSelected()); });
		}
	}

	// wii emulated wiimotes
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::emulated_wiimotes))
	{
		if (CustomFeatures::FeaturesLoaded || (!CustomFeatures::FeaturesLoaded && systemData->getName() == "wii"))  // only for wii
		{
			auto emulatedwiimotes_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("EMULATED WIIMOTES"));
			emulatedwiimotes_enabled->addRange({ { _("AUTO"), "auto" },{ _("ON") , "1" },{ _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".emulatedwiimotes"));
			systemConfiguration->addWithLabel(_("EMULATED WIIMOTES"), emulatedwiimotes_enabled);
			systemConfiguration->addSaveFunc([emulatedwiimotes_enabled, configName] { SystemConf::getInstance()->set(configName + ".emulatedwiimotes", emulatedwiimotes_enabled->getSelected()); });
		}
	}

	// citra change screen layout
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::screen_layout))
	{
		if (CustomFeatures::FeaturesLoaded || (!CustomFeatures::FeaturesLoaded && systemData->getName() == "3ds"))  // only for 3ds
		{
			auto changescreen_layout = std::make_shared<OptionListComponent<std::string>>(mWindow, _("CHANGE SCREEN LAYOUT"));
			changescreen_layout->addRange({ { _("AUTO"), "auto" },{ _("LARGE SCREEN") , "2" },{ _("SIDE BY SIDE"), "3" } }, SystemConf::getInstance()->get(configName + ".layout_option"));
			systemConfiguration->addWithLabel(_("CHANGE SCREEN LAYOUT"), changescreen_layout);
			systemConfiguration->addSaveFunc([changescreen_layout, configName] { SystemConf::getInstance()->set(configName + ".layout_option", changescreen_layout->getSelected()); });
		}
	}

	// psp internal resolution
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::internal_resolution))
	{
		std::string curResol = SystemConf::getInstance()->get(configName + ".internalresolution");

		auto internalresolution = std::make_shared<OptionListComponent<std::string>>(mWindow, _("INTERNAL RESOLUTION"));
		internalresolution->add(_("AUTO"), "auto", curResol.empty() || curResol == "auto");
		internalresolution->add("1:1", "0", curResol == "0");
		internalresolution->add("x1", "1", curResol == "1");
		internalresolution->add("x2", "2", curResol == "2");
		internalresolution->add("x3", "3", curResol == "3");
		internalresolution->add("x4", "4", curResol == "4");
		internalresolution->add("x5", "5", curResol == "5");
		internalresolution->add("x8", "8", curResol == "8");
		internalresolution->add("x10", "10", curResol == "10");

		if (!internalresolution->hasSelection())
			internalresolution->selectFirstItem();

		if (CustomFeatures::FeaturesLoaded || (!CustomFeatures::FeaturesLoaded && (systemData->getName() == "psp" || systemData->getName() == "wii" || systemData->getName() == "gamecube"))) // only for psp, wii, gamecube
		{
			systemConfiguration->addWithLabel(_("INTERNAL RESOLUTION"), internalresolution);
			systemConfiguration->addSaveFunc([internalresolution, configName] { SystemConf::getInstance()->set(configName + ".internalresolution", internalresolution->getSelected()); });
		}
	}

#endif
	// Load per-game / per-emulator / per-system custom features
	addFeatures(customFeatures, mWindow, systemConfiguration, configName, systemData->getName(), currentEmulator.empty() ? systemData->getEmulator(true) : currentEmulator, currentCore.empty() ? systemData->getCore(true) : currentCore, _("SETTINGS"), true);

	// automatic controller configuration
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::autocontrollers))
	{
		auto autoControllers = std::make_shared<OptionListComponent<std::string>>(mWindow, _("AUTOCONFIGURE CONTROLLERS"));
		autoControllers->addRange({ { _("AUTO"), "" },{ _("ON"), "0" },{ _("OFF"), "1" } }, SystemConf::getInstance()->get(configName + ".disableautocontrollers"));
		systemConfiguration->addWithLabel(_("AUTOCONFIGURE CONTROLLERS"), autoControllers);
		systemConfiguration->addSaveFunc([configName, autoControllers] { SystemConf::getInstance()->set(configName + ".disableautocontrollers", autoControllers->getSelected()); });
	}

	if (fileData == nullptr && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::ScriptId::EVMAPY) && systemData->isCurrentFeatureSupported(EmulatorFeatures::Features::padTokeyboard))
	{
		if (systemData->hasKeyboardMapping())
			systemConfiguration->addEntry(_("EDIT PADTOKEY PROFILE"), true, [mWindow, systemData] { editKeyboardMappings(mWindow, systemData, true); });
		else
			systemConfiguration->addEntry(_("CREATE PADTOKEY PROFILE"), true, [mWindow, systemData] { editKeyboardMappings(mWindow, systemData, true); });
	}
	
#ifndef WIN32
	// Set as boot game 
	if (fileData != nullptr)
	{
		std::string gamePath = fileData->getFullPath();

		auto bootgame = std::make_shared<SwitchComponent>(mWindow);
		bootgame->setState(SystemConf::getInstance()->get("global.bootgame.path") == gamePath);
		systemConfiguration->addWithLabel(_("LAUNCH THIS GAME AT STARTUP"), bootgame);
		systemConfiguration->addSaveFunc([bootgame, fileData, gamePath]
		{ 
			if (bootgame->changed()) 
			{
				SystemConf::getInstance()->set("global.bootgame.path", bootgame->getState() ? gamePath : "");
				SystemConf::getInstance()->set("global.bootgame.cmd", bootgame->getState() ? fileData->getlaunchCommand(false) : "");
			}
		});
	}
#endif

#ifdef _ENABLEEMUELEC
	systemConfiguration->addEntry(_("INTERNAL VIDEO OPTIONS"), true, [=] {
		GuiSettings* videoOptions = new GuiSettings(mWindow, _("INTERNAL VIDEO OPTIONS").c_str());
		addFrameBufferOptions(mWindow, videoOptions, configName, "EMU ", systemData->getName());
		mWindow->pushGui(videoOptions);
	});
#endif

#ifdef _ENABLEEMUELEC
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::midi))
	{
		auto ra_midi_def = std::make_shared< OptionListComponent<std::string> >(mWindow, "RETROARCH MIDI", false);

		std::vector<std::string> midi_output;
		std::string def_midi;
		std::string midi_cmd = "emuelec-utils midi_output "+currentEmulator+" "+currentCore;
		for(std::stringstream ss(Utils::Platform::getShOutput(midi_cmd.c_str())); getline(ss, def_midi, ','); ) {
			if (!std::count(midi_output.begin(), midi_output.end(), def_midi)) {
				 midi_output.push_back(def_midi);
			}
		}
		//midi_output.push_back("timidity");
		//midi_output.push_back("mt32d");
		//midi_output.push_back("fluidsynth");
		std::string saved_midi = SystemConf::getInstance()->get(configName+".ra_midi_output");
		ra_midi_def->add("none", "none", saved_midi.empty());
		for (auto it = midi_output.cbegin(); it != midi_output.cend(); it++)
			ra_midi_def->add(*it, *it, saved_midi == *it);
		systemConfiguration->addWithLabel(_("RETROARCH MIDI"), ra_midi_def);
		systemConfiguration->addSaveFunc([ra_midi_def, configName] {
			if (ra_midi_def->changed()) {
				std::string selectedMidiOutput = ra_midi_def->getSelected();
				if (selectedMidiOutput == "none")
					selectedMidiOutput="";
				SystemConf::getInstance()->set(configName+".ra_midi_output", selectedMidiOutput);
				SystemConf::getInstance()->saveSystemConf();
			}
		});
	}
#endif

#ifdef _ENABLEEMUELEC
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::rotation))
	{
		auto ra_rotation_def = std::make_shared< OptionListComponent<std::string> >(mWindow, _("SCREEN ROTATION"), false);

		std::vector<std::string> rotation_output;
		std::string def_rotation;
		std::string rotation_cmd = "emuelec-utils rotation_output "+currentEmulator+" "+currentCore;
		for(std::stringstream ss(Utils::Platform::getShOutput(rotation_cmd.c_str())); getline(ss, def_rotation, ','); ) {
			if (!std::count(rotation_output.begin(), rotation_output.end(), def_rotation)) {
				 rotation_output.push_back(def_rotation);
			}
		}
		std::string saved_rotation = SystemConf::getInstance()->get(configName+"."+currentEmulator+".rotation_output");
		ra_rotation_def->add("none", "none", saved_rotation.empty());
		
		int rotate_index = 1;
		for (auto it = rotation_output.cbegin(); it != rotation_output.cend(); it++) {
			ra_rotation_def->add(*it, std::to_string(rotate_index), atoi(saved_rotation.c_str()) == rotate_index);
			rotate_index++;
		}
		systemConfiguration->addWithLabel(_("SCREEN ROTATION"), ra_rotation_def);
		systemConfiguration->addSaveFunc([ra_rotation_def, configName, currentEmulator] {
			if (ra_rotation_def->changed()) {
				std::string selectedRotationOutput = ra_rotation_def->getSelected();
				if (selectedRotationOutput == "none")
					selectedRotationOutput="";
				SystemConf::getInstance()->set(configName+"."+currentEmulator+".rotation_output", selectedRotationOutput);
				SystemConf::getInstance()->saveSystemConf();
			}
		});
	}
#endif

	mWindow->pushGui(systemConfiguration);
}

std::shared_ptr<OptionListComponent<std::string>> GuiMenu::createRatioOptionList(Window *window, std::string configname)
{
	auto ratio_choice = std::make_shared<OptionListComponent<std::string> >(window, _("GAME ASPECT RATIO"), false);
	std::string currentRatio = SystemConf::getInstance()->get(configname + ".ratio");
	if (currentRatio.empty())
		currentRatio = std::string("auto");

	std::map<std::string, std::string> *ratioMap = LibretroRatio::getInstance()->getRatio();
	for (auto ratio = ratioMap->begin(); ratio != ratioMap->end(); ratio++)
		ratio_choice->add(_(ratio->first.c_str()), ratio->second, currentRatio == ratio->second);	

	if (!ratio_choice->hasSelection())
		ratio_choice->selectFirstItem();
	
	return ratio_choice;
}

#ifdef _ENABLEEMUELEC

int getResWidth (std::string res)
{
	std::string tmp = "";
	std::size_t pos = res.find("x");

	if (pos != std::string::npos) {
		tmp = res.substr(0, pos);
		return atoi( tmp.c_str() );
	}
	pos = res.find("p");
	if (pos != std::string::npos) {
		tmp = res.substr(0, pos);
		int resv = atoi(tmp.c_str());
		return std::ceil(( (float)16 / 9 * resv));
	}
	pos = res.find("i");
	if (pos != std::string::npos) {
		tmp = res.substr(0, pos);
		int resv = atoi(tmp.c_str());
		return std::ceil(( (float)16 / 9 * resv));
	}
	return 0;
}

int getHzFromRes(std::string res)
{
	int tmp = atoi(res.substr(res.length()-4, 2).c_str());
	if (tmp > 0) return tmp;
	return 60;
}

bool sortResolutions (std::string a, std::string b) {
	int ia = getResWidth(a);
	int ib = getResWidth(b);
	
	if (ia == ib) return (getHzFromRes(a) < getHzFromRes(b));
	
	return (ia < ib);
}

std::shared_ptr<OptionListComponent<std::string>> GuiMenu::createNativeVideoResolutionModeOptionList(Window *window, std::string configname)
{
	auto emuelec_video_mode = std::make_shared< OptionListComponent<std::string> >(window, "NATIVE VIDEO", false);
	std::vector<std::string> videomode;
	videomode.push_back("640x480p60hz");
	videomode.push_back("480p60hz");
	videomode.push_back("576p50hz");
	videomode.push_back("576p60hz");
	videomode.push_back("720p50hz");	
	videomode.push_back("720p60hz");
	videomode.push_back("1080i50hz");
	videomode.push_back("1080p50hz");
	videomode.push_back("1080i60hz");
	videomode.push_back("1080p60hz");

	std::string def_video;
	for(std::stringstream ss(Utils::Platform::getShOutput(R"(/usr/bin/emuelec-utils resolutions)")); getline(ss, def_video, ','); ) {
		if (!std::count(videomode.begin(), videomode.end(), def_video)) {
			 videomode.push_back(def_video);
		}
	}

	std::sort(videomode.begin(), videomode.end(), sortResolutions);

	std::string index = SystemConf::getInstance()->get(configname + ".nativevideo");
	if (index.empty())
		index = "auto";

	emuelec_video_mode->add(_("AUTO"), "auto", index == "auto");
	for (auto it = videomode.cbegin(); it != videomode.cend(); it++) {
		emuelec_video_mode->add(*it, *it, index == *it);
	}

	return emuelec_video_mode;
}

#endif 


std::shared_ptr<OptionListComponent<std::string>> GuiMenu::createVideoResolutionModeOptionList(Window *window, std::string configname, std::string configoptname, const std::string output) 
{
	auto videoResolutionMode_choice = std::make_shared<OptionListComponent<std::string> >(window, _("VIDEO MODE"), false);

	std::string currentVideoMode = SystemConf::getInstance()->get(configname + "." + configoptname);
	if (currentVideoMode.empty())
		currentVideoMode = std::string("auto");
	
	std::vector<std::string> videoResolutionModeMap = ApiSystem::getInstance()->getVideoModes(output);
	videoResolutionMode_choice->add(_("AUTO"), "auto", currentVideoMode == "auto");
	for (auto videoMode = videoResolutionModeMap.begin(); videoMode != videoResolutionModeMap.end(); videoMode++)
	{
		std::vector<std::string> tokens = Utils::String::split(*videoMode, ':');

		// concatenat the ending words
		std::string vname;
		for (unsigned int i = 1; i < tokens.size(); i++) 
		{
			if (i > 1) 
				vname += ":";

			vname += tokens.at(i);
		}

		videoResolutionMode_choice->add(_(vname.c_str()), tokens.at(0), currentVideoMode == tokens.at(0));
	}

	if (!videoResolutionMode_choice->hasSelection())
		videoResolutionMode_choice->selectFirstItem();

	return videoResolutionMode_choice;
}

std::vector<DecorationSetInfo> GuiMenu::getDecorationsSets(SystemData* system)
{
	std::vector<DecorationSetInfo> sets;
	if (system == nullptr)
		return sets;

	static const size_t pathCount = 3;

	std::vector<std::string> paths = 
	{
#if WIN32
		Paths::getUserEmulationStationPath() + "/decorations", // for win32 testings
#endif
		Paths::getUserDecorationsPath(),
		Paths::getDecorationsPath()
	};
	
	Utils::FileSystem::stringList dirContent;
	std::string folder;

	for (auto path : paths)
	{
		if (!Utils::FileSystem::isDirectory(path))
			continue;

		dirContent = Utils::FileSystem::getDirContent(path);
		for (Utils::FileSystem::stringList::const_iterator it = dirContent.cbegin(); it != dirContent.cend(); ++it)
		{
			if (Utils::FileSystem::isDirectory(*it))
			{
				folder = *it;

				DecorationSetInfo info;
				info.name = folder.substr(path.size() + 1);
				info.path = folder;

				if (system != nullptr && Utils::String::startsWith(info.name, "default"))
				{
					std::string systemImg = path + "/"+ info.name +"/systems/" + system->getName() + ".png";
					if (Utils::FileSystem::exists(systemImg))
						info.imageUrl = systemImg;
				}

				if (info.imageUrl.empty())
				{
					std::string img = folder + "/default.png";
					if (Utils::FileSystem::exists(img))
						info.imageUrl = img;
				}

				sets.push_back(info);
			}
		}
	}

	struct { bool operator()(DecorationSetInfo& a, DecorationSetInfo& b) const { return a.name < b.name; } } compareByName;
	struct { bool operator()(DecorationSetInfo& a, DecorationSetInfo& b) const { return a.name == b.name; } } nameEquals;

	// sort and remove duplicates
	std::sort(sets.begin(), sets.end(), compareByName);
	sets.erase(std::unique(sets.begin(), sets.end(), nameEquals), sets.end());

	return sets;
}


void GuiMenu::openFormatDriveSettings()
{
	Window *window = mWindow;

	auto s = new GuiSettings(mWindow, _("FORMAT DEVICE").c_str());

	// Drive
	auto optionsStorage = std::make_shared<OptionListComponent<std::string> >(window, _("DEVICE TO FORMAT"), false);

	std::vector<std::string> disks = ApiSystem::getInstance()->getFormatDiskList();
	if (disks.size() == 0)
		optionsStorage->add(_("NONE"), "", false);
	else 
	{
		for (auto disk : disks)
		{
			auto idx = disk.find(" ");
			if (idx != std::string::npos)
				optionsStorage->add(disk.substr(idx + 1), disk.substr(0, idx), false);
		}
	}

	optionsStorage->selectFirstItem();
	s->addWithLabel(_("DEVICE TO FORMAT"), optionsStorage);

	// File system
	auto fileSystem = std::make_shared<OptionListComponent<std::string> >(window, _("FILE SYSTEM"), false);

	std::vector<std::string> fileSystems = ApiSystem::getInstance()->getFormatFileSystems();
	if (fileSystems.size() == 0)
		fileSystem->add(_("NONE"), "", false);
	else
	{
		for (auto fs : fileSystems)
			fileSystem->add(fs, fs, false);
	}

	fileSystem->selectFirstItem();
	s->addWithLabel(_("FILE SYSTEM"), fileSystem);

	s->addEntry(_("FORMAT NOW"), false, [s, optionsStorage, fileSystem, window]
		{
			std::string disk = optionsStorage->getSelected();
			std::string fs = fileSystem->getSelected();

			if (disk.empty() || fs.empty())
			{
				window->pushGui(new GuiMsgBox(window, _("SELECTED OPTIONS ARE INVALID")));
				return;
			}

			window->pushGui(new GuiMsgBox(window, _("ARE YOU SURE YOU WANT TO FORMAT THIS DRIVE?"), _("YES"), [s, window, disk, fs]
			{
				ThreadedFormatter::start(window, disk, fs);
				s->close();
			}, _("NO"), nullptr));
			
		});

	mWindow->pushGui(s);
}



void GuiMenu::saveSubsetSettings()
{
	auto currentSystem = ViewController::get()->getState().getSystem();
	if (currentSystem == nullptr || currentSystem->getTheme() == nullptr)
		return;

	std::string fileData;

	auto subsets = currentSystem->getTheme()->getSubSetNames();
	for (auto subset : subsets)
	{
		std::string name = subset;
		std::string value;

		if (name == "colorset")
			value = Settings::getInstance()->getString("ThemeColorSet");
		else if (name == "iconset")
			value = Settings::getInstance()->getString("ThemeIconSet");
		else if (name == "menu")
			value = Settings::getInstance()->getString("ThemeMenu");
		else if (name == "systemview")
			value = Settings::getInstance()->getString("ThemeSystemView");
		else if (name == "gamelistview")
			value = Settings::getInstance()->getString("ThemeGamelistView");
		else if (name == "region")
			value = Settings::getInstance()->getString("ThemeRegionName");
		else
		{
			value = Settings::getInstance()->getString("subset." + name);
			name = "subset." + name;
		}

		if (!value.empty())
			fileData += name + "=" + value + "\r";

		for (auto system : SystemData::sSystemVector)
		{
			value = Settings::getInstance()->getString("subset." + system->getThemeFolder() + "." + subset);
			if (!value.empty())
				fileData += "subset." + system->getThemeFolder() + "." + subset + "=" + value + "\r";
		}
	}

	if (!Settings::getInstance()->getString("GamelistViewStyle").empty() && Settings::getInstance()->getString("GamelistViewStyle") != "automatic")
		fileData += "GamelistViewStyle=" + Settings::getInstance()->getString("GamelistViewStyle") + "\r";

	if (!Settings::getInstance()->getString("DefaultGridSize").empty())
		fileData += "DefaultGridSize=" + Settings::getInstance()->getString("DefaultGridSize") + "\r";

	for (auto system : SystemData::sSystemVector)
	{
		auto defaultView = Settings::getInstance()->getString(system->getName() + ".defaultView");
		if (!defaultView.empty())
			fileData += system->getName() + ".defaultView=" + defaultView + "\r";

		auto gridSizeOverride = Settings::getInstance()->getString(system->getName() + ".gridSize");
		if (!gridSizeOverride.empty())
			fileData += system->getName() + ".gridSize=" + gridSizeOverride + "\r";
	}

	std::string path = Paths::getUserEmulationStationPath() + "/themesettings";
	if (!Utils::FileSystem::exists(path))
		Utils::FileSystem::createDirectory(path);

	std::string themeSet = Settings::getInstance()->getString("ThemeSet");
	std::string fileName = path + "/" + themeSet + ".cfg";

	if (fileData.empty())
	{
		if (Utils::FileSystem::exists(fileName))
			Utils::FileSystem::removeFile(fileName);
	}
	else
		Utils::FileSystem::writeAllText(fileName, fileData);

}

void GuiMenu::loadSubsetSettings(const std::string themeName)
{
	std::string path = Paths::getUserEmulationStationPath() + "/themesettings";
	if (!Utils::FileSystem::exists(path))
		Utils::FileSystem::createDirectory(path);

	std::string fileName = path + "/" + themeName + ".cfg";
	if (!Utils::FileSystem::exists(fileName))
		return;

	std::string line;
	std::ifstream systemConf(fileName);
	if (systemConf && systemConf.is_open())
	{
		while (std::getline(systemConf, line, '\r'))
		{
			int idx = line.find("=");
			if (idx == std::string::npos || line.find("#") == 0 || line.find(";") == 0)
				continue;

			std::string name = line.substr(0, idx);
			std::string value = line.substr(idx + 1);
			if (!name.empty() && !value.empty())
			{
				if (name == "colorset")
					Settings::getInstance()->setString("ThemeColorSet", value);
				else if (name == "iconset")
					Settings::getInstance()->setString("ThemeIconSet", value);
				else if (name == "menu")
					Settings::getInstance()->setString("ThemeMenu", value);
				else if (name == "systemview")
					Settings::getInstance()->setString("ThemeSystemView", value);
				else if (name == "gamelistview")
					Settings::getInstance()->setString("ThemeGamelistView", value);
				else if (name == "region")
					Settings::getInstance()->setString("ThemeRegionName", value);
				else if (name == "GamelistViewStyle")
					Settings::getInstance()->setString("GamelistViewStyle", value);
				else if (name == "DefaultGridSize")
					Settings::getInstance()->setString("DefaultGridSize", value);
				else if (name.find(".defaultView") != std::string::npos)
					Settings::getInstance()->setString(name, value);
				else if (name.find(".gridSize") != std::string::npos)
					Settings::getInstance()->setString(name, value);
				else if (Utils::String::startsWith(name, "subset."))
					Settings::getInstance()->setString(name, value);
			}
		}
		systemConf.close();

		for (auto system : SystemData::sSystemVector)
		{
			auto defaultView = Settings::getInstance()->getString(system->getName() + ".defaultView");
			auto gridSizeOverride = Vector2f::parseString(Settings::getInstance()->getString(system->getName() + ".gridSize"));
			system->setSystemViewMode(defaultView, gridSizeOverride, false);
		}
	}
	else
		LOG(LogError) << "Unable to open " << fileName;
}

void GuiMenu::editKeyboardMappings(Window *window, IKeyboardMapContainer* mapping, bool editable)
{
	window->pushGui(new GuiKeyMappingEditor(window, mapping, editable));
}

bool GuiMenu::hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult)
{
	if (pResult) pResult->push_back(this); // Always return this as it's a fake fullscreen, so we always have click events
	GuiComponent::hitTest(x, y, parentTransform, pResult);
	return true;
}

bool GuiMenu::onMouseClick(int button, bool pressed, int x, int y)
{
	if (pressed && button == 1 && !mMenu.isMouseOver())
	{
		delete this;
		return true;
	}

	return (button == 1);
}
