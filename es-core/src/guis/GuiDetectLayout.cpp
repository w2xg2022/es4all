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
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <string>

#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <poll.h>
#endif

#ifdef __linux__
// 這個 fd 真的是 evdev 節點嗎。
//
// ★非有不可★(2026-08-04 實機 MD1000/Armbian 整個 ES 凍住才發現)
//   曾經發生 mEvFd == 0 —— 也就是 **stdin(/dev/tty1)**。本服務是
//   `StandardInput=tty` + `TTYPath=/dev/tty1`, 那個 fd 是【阻塞】的,
//   於是 update() 裡的 read() 一進去就再也不回來: 主迴圈停擺、畫面凍在最後一張畫格、
//   手把按什麼都沒反應。★而且完全看不出是當機★ —— 進程還在、CPU 0%、
//   log 也乾乾淨淨(程式自認開節點成功)。gdb 抓堆疊才看到
//   GuiDetectLayout::update -> read。
//   EVIOCGVERSION 只有真的 evdev 節點才會回成功, 拿它一票否決最省事。
static bool evdevIsValid(int fd)
{
	int ver = 0;
	return ioctl(fd, EVIOCGVERSION, &ver) >= 0;
}

// 開一個 evdev 節點。★保證回傳的 fd 一定 >= 3 且確實是 evdev★
static int openEvdevNode(const std::string& path)
{
	int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
	if (fd < 0)
		return -1;

	// ★別讓 fd 落在 0/1/2★: 若 stdin/stdout 在此之前被誰關掉, open 會把最小的空號給我們,
	// 拿到 0 就等於把「標準輸入」當成手把在讀 —— 上面那個凍住就是這麼來的。
	if (fd <= 2)
	{
		int hi = fcntl(fd, F_DUPFD_CLOEXEC, 3);
		close(fd);
		if (hi < 0)
			return -1;
		fd = hi;
	}

	if (!evdevIsValid(fd)) { close(fd); return -1; }
	return fd;
}

// 這個裝置是不是「有面鍵的手把」——用 BTN_SOUTH 的能力位判斷。
// 用途有二：把紅外線接收器/電源鍵那種 event 裝置排掉；以及最後保底時只挑真手把。
static bool evdevIsGamepad(int fd)
{
	const size_t kBitsPerLong = 8 * sizeof(unsigned long);
	unsigned long bits[(KEY_MAX / kBitsPerLong) + 1];
	memset(bits, 0, sizeof(bits));
	if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(bits)), bits) < 0)
		return false;
	return (bits[BTN_SOUTH / kBitsPerLong] >> (BTN_SOUTH % kBitsPerLong)) & 1UL;
}

// 從 SDL GUID 取出 vendor / product。
// SDL2 的 GUID 佈局(每 2 字元一個位元組、16 位元值是小端)：
//   [0..3]=bus  [4..7]=CRC16  [8..11]=vendor  [12..15]=0  [16..19]=product
// 例: 030000005e0400008e02000072050000 -> bus 0003, vendor 045e, product 028e
static bool guidVidPid(const std::string& guid, unsigned short& vid, unsigned short& pid)
{
	if (guid.size() < 20) return false;
	auto le16 = [&guid](size_t off) -> unsigned short {
		unsigned b0 = (unsigned) strtoul(guid.substr(off, 2).c_str(), nullptr, 16);
		unsigned b1 = (unsigned) strtoul(guid.substr(off + 2, 2).c_str(), nullptr, 16);
		return (unsigned short) ((b1 << 8) | b0);
	};
	vid = le16(8);
	pid = le16(16);
	return vid != 0 || pid != 0;
}

// 找該手把的 /dev/input/eventN。
//
// ★別拿 SDL 的裝置名去比 evdev 的名字★(2026-08-04 實機 MD1000/Armbian 查證)
//   SDL 會把它認得的手把換成自家 DB 的**友善名**，兩邊根本不是同一個字串：
//       SDL  : "Xbox 360 Controller"        (es_input.cfg 記的也是這個)
//       evdev: "Microsoft X-Box 360 pad"    (核心 xpad 驅動給的)
//   原本用全等比對 -> 永遠不成立 -> 開不到節點 -> ★佈局偵測從頭到尾沒在偵測★，
//   而且完全靜默：畫面照樣問「請按 A」，按下去卻只會走「跳過」那條路。
//
//   所以改成按【身分】而不是按名字找，依可靠度排序：
//     ① SDL 給的路徑本身就是 event 節點 —— 最準，直接開
//     ② SDL 給的是 jsN —— 用 sysfs 把它對到同一個實體裝置的 eventN(不是猜, 是核心的對應)
//     ③ 比對 GUID 裡的 vendor/product 與 EVIOCGID —— 名字可以被改寫，VID/PID 不會
//     ④ 名字寬鬆比對(忽略大小寫的子串)當退路
//     ⑤ 全機只有一顆手把時就用它
//   ②③⑤ 都會先用 BTN_SOUTH 的能力位確認「這是手把」，免得開到紅外線接收器。
static int openEvdev(const std::string& devPath, const std::string& devName, const std::string& devGuid)
{
	// ① SDL 給的路徑本身就是 event 節點
	if (devPath.rfind("/dev/input/event", 0) == 0)
	{
		int fd = openEvdevNode(devPath);
		if (fd >= 0) return fd;
	}

	// ② SDL 給的是 /dev/input/jsN -> 查 /sys/class/input/jsN/device/eventM
	if (devPath.rfind("/dev/input/js", 0) == 0)
	{
		std::string jsName = devPath.substr(std::string("/dev/input/").size());
		std::string sysDir = "/sys/class/input/" + jsName + "/device";
		DIR* sd = opendir(sysDir.c_str());
		if (sd != nullptr)
		{
			struct dirent* se;
			std::string evName;
			while ((se = readdir(sd)) != nullptr)
			{
				if (strncmp(se->d_name, "event", 5) == 0) { evName = se->d_name; break; }
			}
			closedir(sd);
			if (!evName.empty())
			{
				int fd = openEvdevNode("/dev/input/" + evName);
				if (fd >= 0) return fd;
			}
		}
	}

	// ③④⑤ 掃描 /dev/input/event*
	unsigned short wantVid = 0, wantPid = 0;
	bool haveIds = guidVidPid(devGuid, wantVid, wantPid);

	std::string lowerWanted;
	for (char c : devName) lowerWanted += (char) tolower((unsigned char) c);

	DIR* d = opendir("/dev/input");
	if (d == nullptr) return -1;

	int byName = -1, onlyPad = -1;
	int padCount = 0;
	struct dirent* e;
	while ((e = readdir(d)) != nullptr)
	{
		if (strncmp(e->d_name, "event", 5) != 0) continue;
		std::string p = std::string("/dev/input/") + e->d_name;
		int fd = openEvdevNode(p);
		if (fd < 0) continue;
		if (!evdevIsGamepad(fd)) { close(fd); continue; }

		// ③ VID/PID
		if (haveIds)
		{
			struct input_id id;
			memset(&id, 0, sizeof(id));
			if (ioctl(fd, EVIOCGID, &id) >= 0 && id.vendor == wantVid && id.product == wantPid)
			{
				if (byName >= 0) close(byName);
				if (onlyPad >= 0) close(onlyPad);
				closedir(d);
				return fd;
			}
		}

		// ④ 名字寬鬆比對(兩邊互為子串即可, 忽略大小寫)
		char name[256] = {0};
		if (byName < 0 && !lowerWanted.empty() &&
			ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) >= 0)
		{
			std::string lowerHave;
			for (char* c = name; *c; c++) lowerHave += (char) tolower((unsigned char) *c);
			if (lowerHave.find(lowerWanted) != std::string::npos ||
				lowerWanted.find(lowerHave) != std::string::npos)
			{
				byName = fd;
				continue;   // 先留著, 說不定後面還有 VID/PID 完全命中的
			}
		}

		// ⑤ 保底候選
		padCount++;
		if (onlyPad < 0) { onlyPad = fd; continue; }
		close(fd);
	}
	closedir(d);

	if (byName >= 0)
	{
		if (onlyPad >= 0) close(onlyPad);
		return byName;
	}
	if (padCount == 1 && onlyPad >= 0)
		return onlyPad;
	if (onlyPad >= 0) close(onlyPad);
	return -1;
}
#endif

GuiDetectLayout::GuiDetectLayout(Window* window, InputConfig* target, const std::function<void()>& doneCallback)
	: GuiComponent(window), mTarget(target), mDoneCallback(doneCallback),
	  mEvFd(-1), mReopenAccum(0), mPhase(0), mABInverted(false), mXYInverted(false), mFinished(false),
	  mBackground(window, ":/frame.png"), mGrid(window, Vector2i(1, 4))
{
	std::string devPath = (mTarget != nullptr) ? mTarget->getDevicePath() : "";
	std::string devName = (mTarget != nullptr) ? mTarget->getDeviceName() : "";
	std::string devGuid = (mTarget != nullptr) ? mTarget->getDeviceGUIDString() : "";
#ifdef __linux__
	mEvFd = openEvdev(devPath, devName, devGuid);
	if (mEvFd < 0)
		LOG(LogWarning) << "GuiDetectLayout: 找不到手把的 evdev 節點(path='" << devPath
		                << "' name='" << devName << "' guid='" << devGuid
		                << "') -> 佈局偵測跳過, 按任意鍵繼續";
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
	// ★手把中途重新列舉時要能自癒★(2026-08-04 實機 MD1000/Armbian 卡死才發現)
	//   本畫面不走 SDL、自己 open 手把的 /dev/input/eventN 直接讀 evdev。
	//   使用者若在這期間拔插手把(或 USB 重新列舉)，核心會把舊節點【刪掉】再建一個新的，
	//   而我們手上那個 fd 指向的是已刪除的節點 —— 永遠不會再有事件進來。
	//   ★最糟的是它會把自己鎖死★: 底下 input() 只有在 mEvFd < 0 時才允許「按任意鍵跳過」,
	//   而此時 fd 是有效的(只是指向死節點), 於是所有手把按鍵都被吞掉 ——
	//   畫面停在「請按 A」, 按什麼都沒反應也退不出去, 只剩接鍵盤按 ESC 一條路。
	//   實機證據: /proc/<pid>/fd/25 -> /dev/input/event2 (deleted)。
	//
	//   處置分兩層: ①裝置消失就把 fd 收掉 -> 立刻恢復「按任意鍵跳過」的活路;
	//               ②之後定期用同一組 path/name 重開 -> 插回來就自己接上, 不必重啟 ES。
	if (mEvFd < 0)
	{
		mReopenAccum += deltaTime;
		if (mReopenAccum >= 500)
		{
			mReopenAccum = 0;
			std::string devPath = (mTarget != nullptr) ? mTarget->getDevicePath() : "";
			std::string devName = (mTarget != nullptr) ? mTarget->getDeviceName() : "";
			std::string devGuid = (mTarget != nullptr) ? mTarget->getDeviceGUIDString() : "";
			mEvFd = openEvdev(devPath, devName, devGuid);
			if (mEvFd >= 0)
				LOG(LogInfo) << "GuiDetectLayout: evdev 已重新開啟(手把重新列舉後自癒)";
		}
		return;
	}

	// ★先 poll 再 read，一格都不能阻塞★
	//   這支是在【主迴圈的 update()】裡跑的 —— 只要 read 阻塞一次, 整個 ES 就停擺:
	//   畫面凍在最後一張畫格、手把按什麼都沒反應, 而且看起來不像當機(進程還在、CPU 0%、
	//   log 乾乾淨淨)。實機就這樣凍過, 最後是 gdb 抓堆疊才看到卡在這一行。
	//   O_NONBLOCK 照理已經保證不會阻塞, 但那是「fd 真的是我們開的那個」才成立 ——
	//   萬一它是別的東西(例如被誤當成手把的 tty), O_NONBLOCK 就不在了。
	//   poll(timeout=0) 是與那個前提無關的保險: 沒資料就直接回, 永遠不會卡住畫面。
	struct pollfd pfd;
	pfd.fd = mEvFd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	int pr = poll(&pfd, 1, 0);
	if (pr == 0)
		return;                       // 這一格沒資料, 下一格再看
	if (pr < 0 && errno != EINTR)
	{
		LOG(LogWarning) << "GuiDetectLayout: poll 失敗(errno=" << errno << "), 收掉 fd";
		close(mEvFd);
		mEvFd = -1;
		mReopenAccum = 0;
		return;
	}
	// 裝置被拔掉時 poll 會回 POLLERR/POLLHUP, 不會有 POLLIN —— 這是最直接的「裝置沒了」訊號。
	if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
	{
		LOG(LogWarning) << "GuiDetectLayout: 裝置已消失(revents=" << pfd.revents << "), 收掉 fd 並開始重試";
		close(mEvFd);
		mEvFd = -1;
		mReopenAccum = 0;
		return;
	}

	struct input_event ev;
	ssize_t n;
	errno = 0;   // 下面要靠 errno 判斷「沒資料」還是「裝置沒了」, 先清乾淨免得讀到上一次的殘值
	while ((n = read(mEvFd, &ev, sizeof(ev))) == (ssize_t)sizeof(ev))
	{
		if (ev.type != EV_KEY || ev.value != 1)
			continue; // 只看按下(value==1)的按鍵
		if (ev.code == BTN_SOUTH || ev.code == BTN_EAST || ev.code == BTN_NORTH || ev.code == BTN_WEST)
			handlePhysBtn(ev.code);
		if (mFinished)
			break;
	}

	// n < 0 且 errno 不是 EAGAIN/EWOULDBLOCK(那只是「這輪沒資料」的正常情形) => 裝置沒了。
	// n == 0 同樣不正常(evdev 不會回 EOF)，一併當作裝置消失處理。
	if (!mFinished && n <= 0 && errno != EAGAIN && errno != EWOULDBLOCK)
	{
		LOG(LogWarning) << "GuiDetectLayout: evdev 讀取失敗(errno=" << errno << "), 裝置可能已被移除; "
		                << "收掉 fd 以恢復「按任意鍵跳過」, 並開始定期重試";
		close(mEvFd);
		mEvFd = -1;
		mReopenAccum = 0;
	}
#endif
}

void GuiDetectLayout::handlePhysBtn(int btnCode)
{
#ifdef __linux__
	// es4all（2026-07 手柄三层架构定案）：只需问「按 A」这一步。
	// A 在南=Xbox 式、A 在东=任天堂式，就足以确定第一层（界面 A=确认/B=返回跟印刷走）。
	// 原本还问「按 X」是为了第三层的 X/Y 透传——第三层已改成写死位置对齐、不透传，
	// 故 X 那一步连同 InvertGameButtons/InvertXYButtons 一并废除，精灵只按一次 A 即完成。
	if (btnCode == BTN_SOUTH)      mABInverted = false; // 印刷A在南 → Xbox 式
	else if (btnCode == BTN_EAST)  mABInverted = true;  // 印刷A在东 → 任天堂式
	else return;                                        // 上/左不是 A 该在的位置，忽略等重按
	applyAndFinish();
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
	// evdev 開不了(非 Linux / 無權限 / ★裝置中途被移除★) → 按任意鍵跳過，走原本手動設定。
	// ★這是唯一的活路，別把條件收緊★: fd 有效時所有手把按鍵都被下面那個 return true 吞掉,
	// 一旦 fd 指向死節點又不收掉, 使用者就會被鎖在這個畫面(見 update() 的說明)。
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

	// 只写 InvertButtons（第一层：界面 A=确认/B=返回跟印刷走）。
	// InvertGameButtons/InvertXYButtons（第三层透传用）已废除，不再写入。
	Settings::getInstance()->setBool("InvertButtons", mABInverted);
	Settings::getInstance()->saveFile();
	InputConfig::AssignActionButtons();

	LOG(LogInfo) << "GuiDetectLayout: AB inverted=" << mABInverted << " (仅第一层，X/Y已废除)";

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
