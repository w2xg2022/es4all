#include "guis/GuiDetectLayout.h"

#include "guis/GuiInputConfig.h"
#include "components/TextComponent.h"
#include "InputConfig.h"
#include "Settings.h"
#include "Window.h"
#include "LocaleES.h"
#include "Log.h"
#include "ThemeData.h"
#include "renderers/Renderer.h"
#include <cstring>
#include <string>

#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#endif

#ifdef __linux__
// 找該手把的 /dev/input/eventN：優先用 SDL 給的 devicePath；否則掃描 event* 依裝置名匹配
static int openEvdev(const std::string& devPath, const std::string& devName)
{
	// 1) SDL 給的路徑本身就是 event 節點
	if (devPath.rfind("/dev/input/event", 0) == 0)
	{
		int fd = open(devPath.c_str(), O_RDONLY | O_NONBLOCK);
		if (fd >= 0) return fd;
	}
	// 2) 掃描 /dev/input/event*，用 EVIOCGNAME 比對裝置名
	DIR* d = opendir("/dev/input");
	if (d == nullptr) return -1;
	int found = -1;
	struct dirent* e;
	while ((e = readdir(d)) != nullptr)
	{
		if (strncmp(e->d_name, "event", 5) != 0) continue;
		std::string p = std::string("/dev/input/") + e->d_name;
		int fd = open(p.c_str(), O_RDONLY | O_NONBLOCK);
		if (fd < 0) continue;
		char name[256] = {0};
		if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) >= 0 && !devName.empty() && devName == name)
		{
			found = fd;
			break;
		}
		close(fd);
	}
	closedir(d);
	return found;
}
#endif

GuiDetectLayout::GuiDetectLayout(Window* window, InputConfig* target, const std::function<void()>& doneCallback)
	: GuiComponent(window), mTarget(target), mDoneCallback(doneCallback),
	  mEvFd(-1), mPhase(0), mABInverted(false), mXYInverted(false), mFinished(false),
	  mBackground(window, ":/frame.png"), mGrid(window, Vector2i(1, 4))
{
	std::string devPath = (mTarget != nullptr) ? mTarget->getDevicePath() : "";
	std::string devName = (mTarget != nullptr) ? mTarget->getDeviceName() : "";
#ifdef __linux__
	mEvFd = openEvdev(devPath, devName);
#endif

	auto theme = ThemeData::getMenuTheme();
	mBackground.setImagePath(theme->Background.path);
	mBackground.setEdgeColor(theme->Background.color);
	mBackground.setCenterColor(theme->Background.centerColor);
	mBackground.setCornerSize(theme->Background.cornerSize);
	mBackground.setPostProcessShader(theme->Background.menuShader);

	addChild(&mBackground);
	addChild(&mGrid);

	mTitle = std::make_shared<TextComponent>(mWindow, _("DETECT CONTROLLER LAYOUT"), theme->Title.font, theme->Title.color, ALIGN_CENTER);
	mGrid.setEntry(mTitle, Vector2i(0, 0), false, true);
	mGrid.setEntry(std::make_shared<GuiComponent>(mWindow), Vector2i(0, 1), false);
	mMsg = std::make_shared<TextComponent>(mWindow, "", theme->TextSmall.font, theme->Text.color, ALIGN_CENTER);
	mGrid.setEntry(mMsg, Vector2i(0, 2), false, true);

	if (Renderer::ScreenSettings::fullScreenMenus())
		setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
	else
		setSize(Renderer::getScreenWidth() * 0.6f, Renderer::getScreenHeight() * 0.4f);
	setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, (Renderer::getScreenHeight() - mSize.y()) / 2);

	setPrompt(_("PRESS THE BUTTON LABELED \"A\" (by its printed letter)."));
}

GuiDetectLayout::~GuiDetectLayout()
{
#ifdef __linux__
	if (mEvFd >= 0) close(mEvFd);
#endif
	mEvFd = -1;
}

void GuiDetectLayout::setPrompt(const std::string& msg)
{
	if (mMsg != nullptr) mMsg->setText(msg);
}

void GuiDetectLayout::onSizeChanged()
{
	GuiComponent::onSizeChanged();
	mBackground.fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));
	mGrid.setSize(mSize);
	if (mTitle != nullptr)
		mGrid.setRowHeight(0, mTitle->getFont()->getHeight() * 1.1f);
}

void GuiDetectLayout::update(int deltaTime)
{
	GuiComponent::update(deltaTime);
	if (mFinished)
		return;

#ifdef __linux__
	if (mEvFd < 0)
		return;

	struct input_event ev;
	ssize_t n;
	while ((n = read(mEvFd, &ev, sizeof(ev))) == (ssize_t)sizeof(ev))
	{
		if (ev.type != EV_KEY || ev.value != 1)
			continue; // 只看按下(value==1)的按鍵
		if (ev.code == BTN_SOUTH || ev.code == BTN_EAST || ev.code == BTN_NORTH || ev.code == BTN_WEST)
			handlePhysBtn(ev.code);
		if (mFinished)
			break;
	}
#endif
}

void GuiDetectLayout::handlePhysBtn(int btnCode)
{
#ifdef __linux__
	if (mPhase == 0)
	{
		if (btnCode == BTN_SOUTH)      mABInverted = false; // 印刷A在南 → Xbox 式
		else if (btnCode == BTN_EAST)  mABInverted = true;  // 印刷A在東 → 任天堂式
		else return;                                        // 上/左不是 A 該在的位置，忽略等重按
		mPhase = 1;
		setPrompt(_("NOW PRESS THE BUTTON LABELED \"X\"."));
	}
	else if (mPhase == 1)
	{
		if (btnCode == BTN_WEST)       mXYInverted = false; // 印刷X在西 → Xbox 式
		else if (btnCode == BTN_NORTH) mXYInverted = true;  // 印刷X在北 → 任天堂式(XY反)
		else return;                                        // 南/東不是 X 該在的位置，忽略
		applyAndFinish();
	}
#endif
}

bool GuiDetectLayout::input(InputConfig* config, Input input)
{
	// 鍵盤 ESC 取消 → 跳過偵測
	if (input.device == DEVICE_KEYBOARD && input.type == TYPE_KEY && input.value && input.id == SDLK_ESCAPE)
	{
		finishSkip();
		return true;
	}
	// evdev 開不了(非 Linux / 無權限) → 按任意鍵跳過，走原本手動設定
	if (mEvFd < 0 && input.type == TYPE_BUTTON && input.value)
	{
		finishSkip();
		return true;
	}
	// 其餘吞掉：實體按鍵由 evdev 那條路處理，這裡不讓它去操作背後的選單
	return true;
}

void GuiDetectLayout::applyAndFinish()
{
	if (mFinished) return;
	mFinished = true;

	Settings::getInstance()->setBool("InvertButtons", mABInverted);
	Settings::getInstance()->setBool("InvertGameButtons", !mABInverted);
	Settings::getInstance()->setBool("InvertXYButtons", mXYInverted);
	Settings::getInstance()->saveFile();
	InputConfig::AssignActionButtons();

	LOG(LogInfo) << "GuiDetectLayout: AB inverted=" << mABInverted << ", XY inverted=" << mXYInverted;

	auto cb = mDoneCallback;
	delete this;
	if (cb) cb();
}

void GuiDetectLayout::finishSkip()
{
	if (mFinished) return;
	mFinished = true;
	auto cb = mDoneCallback;
	delete this;
	if (cb) cb();
}
