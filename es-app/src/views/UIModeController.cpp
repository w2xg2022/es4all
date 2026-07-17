#include "UIModeController.h"

#include "utils/StringUtil.h"
#include "views/ViewController.h"
#include "Log.h"
#include "Window.h"

#define fake_gettext_full _("Full")
#define fake_gettext_kiosk _("Kiosk")
#define fake_gettext_kid _("Kid")
#define fake_gettext_basic _("Basic")

UIModeController *  UIModeController::sInstance = NULL;

UIModeController * UIModeController::getInstance()
{
	if (sInstance == NULL)
		sInstance = new UIModeController();

	return sInstance;
}

UIModeController::UIModeController()
	: mPassKeyCounter(0), mUIModeChanged(false)
{
	mPassKeySequence = Settings::getInstance()->getString("UIMode_passkey");
	mCurrentUIMode = Settings::getInstance()->getString("UIMode");

	Settings::settingChanged += this;
}

void UIModeController::onSettingChanged(const std::string& name)
{
	if (name == "UIMode")
	{
		mUIModeChanged = true;
		monitorUIMode();
	}
}

void UIModeController::monitorUIMode()
{
	if (!mUIModeChanged)
		return;

	mUIModeChanged = false;
	std::string uimode = Settings::getInstance()->getString("UIMode");
	if (uimode != mCurrentUIMode) // UIMODE HAS CHANGED
	{
		mCurrentUIMode = uimode;
		ViewController::get()->ReloadAndGoToStart();
	}
}

bool UIModeController::listen(InputConfig * config, Input input)
{
	// Reads the current input to listen for the passkey
	// sequence to unlock the UI mode. The progress is saved in mPassKeyCounter
	if (Settings::getInstance()->getBool("Debug"))
	{
		logInput(config, input);
	}

	if ((Settings::getInstance()->getString("UIMode") == "Full") || !isValidInput(config, input))
	{
		return false; // Already unlocked, or invalid input, nothing to do here.
	}


	if (!inputIsMatch(config, input))
	{
		mPassKeyCounter = 0; // current input is incorrect, reset counter
	}

	if (mPassKeyCounter == (int)mPassKeySequence.length())
	{
		unlockUIMode();
		return true;
	}
	return false;
}

bool UIModeController::inputIsMatch(InputConfig * config, Input input)
{
	for (auto valstring : mInputVals)
	{
		if (config->isMappedLike(valstring, input) &&
			(mPassKeySequence[mPassKeyCounter] == valstring[0]))
		{
			mPassKeyCounter++;
			return true;
		}
	}
	return false;
}

// When we have reached the end of the list, trigger UI_mode unlock
void UIModeController::unlockUIMode()
{
	LOG(LogDebug) << " UIModeController::listen(): Passkey sequence completed, switching UIMode to full";

	Settings::getInstance()->setString("UIMode", "Full");
	Settings::getInstance()->saveFile();
	mPassKeyCounter = 0;
}

bool UIModeController::LoadEmptySystems()
{
	return getInstance()->isUIModeFull() && Settings::LoadEmptySystems();
}

// es4all: Kid / Kiosk / Basic 受限界面模式已移除，界面恒为「完整」。
//
// 取舍说明：不去逐一改写 32 处调用点(isUIModeFull 13 处 / isUIModeKid 15 处 /
// isUIModeKiosk 4 处，散落 18 个档案)，而是把这四个判断式定为常数 ——
// 所有调用点自然走「完整模式」那条路，零遗漏；若逐处手改，一个 ! 方向弄反就会
// 静默改变菜单，而 CI 只验编译不验行为，抓不到。被绕过的分支成为不可达代码，
// 后续可安全清理。
//
// 附带效果(已确认接受)：
//   - 主菜单的「信息」「解锁用户界面模式」只在 !isFullUI 时出现 → 不再出现
//   - 「退出」原本在 Kid 模式隐藏 → 恒显示
//   - 命令行 --force-kid / --force-kiosk (ForceKid/ForceKiosk 设定) 不再生效
//   - 既有用户 es_settings.cfg 里若残留 UIMode="Kid"/"Kiosk"，会自动回到完整模式
//
// isUIModeBasic() 本就是死码：mUIModes 里 "Basic" 被注释掉(选不到)，且全专案零调用。

bool UIModeController::isUIModeFull()
{
	return true;
}

bool UIModeController::isUIModeBasic()
{
	return false;
}

bool UIModeController::isUIModeKid()
{
	return false;
}

bool UIModeController::isUIModeKiosk()
{
	return false;
}

std::string UIModeController::getFormattedPassKeyStr()
{
	// supported sequence-inputs: u (up), d (down), l (left), r (right), a, b, x, y

	std::string out = "";
	for (auto c : mPassKeySequence)
	{
		out += (out == "") ? "" : ", ";  // add a comma after the first entry

		switch (c)
		{
		case 'u':
			out += Utils::String::unicode2Chars(0x2191); // arrow pointing up
			break;
		case 'd':
			out += Utils::String::unicode2Chars(0x2193); // arrow pointing down
			break;
		case 'l':
			out += Utils::String::unicode2Chars(0x2190); // arrow pointing left
			break;
		case 'r':
			out += Utils::String::unicode2Chars(0x2192); // arrow pointing right
			break;
		case 'a':
			out += "A";
			break;
		case 'b':
			out += "B";
			break;
		case 'x':
			out += "X";
			break;
		case 'y':
			out += "Y";
			break;
		}
	}
	return out;
}


void UIModeController::logInput(InputConfig * config, Input input)
{
	std::string mapname = "";
	std::vector<std::string> maps = config->getMappedTo(input);
	for( auto mn : maps)
	{
		mapname += mn;
		mapname += ", ";
	}
	LOG(LogDebug) << "UIModeController::logInput( " << config->getDeviceName() <<" ):" << input.string() << ", isMappedTo= " << mapname << ", value=" << input.value;
}

bool UIModeController::isValidInput(InputConfig * config, Input input)
{
	if((config->getMappedTo(input).size() == 0)  || // not a mapped input, so ignore.
		(input.type == TYPE_HAT) ||  // ignore all HAT inputs
		(!input.value))	// not a key-down event
	{
		return false;
	}
	else
	{
		return true;
	}
}