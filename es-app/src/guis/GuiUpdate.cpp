#include "guis/GuiUpdate.h"
#include "guis/GuiMsgBox.h"
#include "Window.h"
#include <string>
#include "Log.h"
#include "Settings.h"
#include "ApiSystem.h"
#include "utils/Platform.h"
#include "LocaleES.h"
#include "components/AsyncNotificationComponent.h"
#ifdef ES4ALL_SELF_UPDATE
#include "Es4allUpdate.h"   // getInstalledBuildInfo / needsFullReboot
#endif

GuiUpdateState::State GuiUpdate::state = GuiUpdateState::State::NO_UPDATE;

class ThreadedUpdater
{
public:
	ThreadedUpdater(Window* window) : mWindow(window)
	{
		GuiUpdate::state = GuiUpdateState::State::UPDATER_RUNNING;

		mWndNotification = mWindow->createAsyncNotificationComponent();

// es4all: all three targets build with -DENABLE_EMUELEC=1, so the old code always showed
// "UPDATING EMUELEC". But self-update updates the ES4All frontend, not the EmuELEC system,
// so that name is misleading. Prefer ES4ALL_SELF_UPDATE to show "UPDATING ES4ALL".
#ifdef ES4ALL_SELF_UPDATE
		mWndNotification->updateTitle(_U("\uF019 ") + _("UPDATING ES4All"));
#elif defined(_ENABLEEMUELEC)
		mWndNotification->updateTitle(_U("\uF019 ") + _("UPDATING EMUELEC"));
#else
		auto label = Utils::String::format(_("UPDATING %s").c_str(), ApiSystem::getInstance()->getApplicationName().c_str());
		mWndNotification->updateTitle(_U("\uF019 ") + label);

#endif
		mHandle = new std::thread(&ThreadedUpdater::threadUpdate, this);
	}

	~ThreadedUpdater()
	{
		mWndNotification->close();
		mWndNotification = nullptr;
	}

	void threadUpdate()
	{
		std::pair<std::string, int> updateStatus = ApiSystem::getInstance()->updateSystem([this](const std::string info)
		{
			auto pos = info.find(">>>");
			if (pos != std::string::npos)
			{
				std::string percent(info.substr(pos));		
				percent = Utils::String::replace(percent, ">", "");
				percent = Utils::String::replace(percent, "%", "");
				percent = Utils::String::replace(percent, " ", "");

				int value = atoi(percent.c_str());

				std::string text(info.substr(0, pos));
				text = Utils::String::trim(text);

				mWndNotification->updatePercent(value);
				mWndNotification->updateText(text);
			}
			else
			{
				mWndNotification->updatePercent(-1);
				mWndNotification->updateText(info);
			}
		});

		if (updateStatus.second == 0)
		{
			GuiUpdate::state = GuiUpdateState::State::UPDATE_READY;

			mWndNotification->updateTitle(_U("\uF019 ") + _("UPDATE IS READY"));
#ifdef ES4ALL_SELF_UPDATE
			// es4all: \u81EA\u6211\u66F4\u65B0\u8D70\u6574\u673A\u91CD\u5F00\u673A\u624D\u751F\u6548(\u5F00\u673A\u670D\u52A1/\u94A9\u5B50 bind-mount \u65B0\u6863), \u975E\u4EC5\u91CD\u542F ES,
			// \u6545\u7528\u660E\u786E\u63AA\u8F9E, \u907F\u514D\u7528\u6237\u8BEF\u4EE5\u4E3A\u53EA\u662F\u91CD\u542F\u524D\u7AEF\u3002
			mWndNotification->updateText(_("Please reboot to finish updating."));

			// es4all: \u4E0B\u8F7D\u5B89\u88C5\u5B8C\u6210\u540E, \u76F4\u63A5\u5F39\u5BF9\u8BDD\u6846\u8BF7\u7528\u6237\u300C\u7ACB\u5373\u91CD\u542F\u5957\u7528\u300D\u2014\u2014\u4E0D\u5FC5\u8BA9\u7528\u6237\u81EA\u5DF1\u56DE\u9009\u5355\u627E
			// \u300C\u5957\u7528\u66F4\u65B0\u300D\u518D\u624B\u52A8\u91CD\u542F(\u65E7\u6D41\u7A0B\u53EA\u628A\u89D2\u843D\u901A\u77E5\u6539\u6210"\u8BF7\u91CD\u5F00\u673A"\u5C31\u5E72\u7B49, \u5F88\u5BB9\u6613\u88AB\u5FFD\u7565/\u56F0\u60D1)\u3002
			//   \u662F \u2192 \u7ACB\u523B\u91CD\u542F\u5957\u7528(\u552F\u8BFB\u5E73\u53F0\u9700\u6574\u673A\u91CD\u5F00\u673A, \u7531 needsFullReboot \u51B3\u5B9A REBOOT/RESTART);
			//   \u5426 \u2192 \u4FDD\u7559 UPDATE_READY \u72B6\u6001\u4E0E\u89D2\u843D\u901A\u77E5, \u4E4B\u540E\u4ECD\u53EF\u4ECE\u9009\u5355\u5957\u7528\u3002
			{
				Window* win = mWindow;
				win->postToUiThread([win]()
				{
					win->pushGui(new GuiMsgBox(win,
						_("UPDATE DOWNLOADED. RESTART NOW TO APPLY?"),
						_("YES"), [win]()
						{
							if (Es4allUpdate::needsFullReboot())
								Utils::Platform::quitES(Utils::Platform::QuitMode::REBOOT);
							else
								Utils::Platform::quitES(Utils::Platform::QuitMode::RESTART);
						},
						_("NO"), nullptr));
				});
			}
#else
			mWndNotification->updateText(_("REBOOT TO APPLY"));
#endif

			std::this_thread::yield();
			std::this_thread::sleep_for(std::chrono::hours(12));
		}
		else
		{
			GuiUpdate::state = GuiUpdateState::State::NO_UPDATE;

			std::string error = _("AN ERROR OCCURRED") + std::string(": ") + updateStatus.first;
			mWindow->displayNotificationMessage(error);
		}

		delete this;
	}

private:
	std::thread*				mHandle;
	AsyncNotificationComponent* mWndNotification;
	Window*						mWindow;
};


GuiUpdate::GuiUpdate(Window* window) : GuiComponent(window), mBusyAnim(window)
{
	LOG(LogInfo) << "Starting GuiUpdate";

	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	mState = 0;	
    mLoading = true;
    mPingHandle = new std::thread(&GuiUpdate::threadPing, this);
    mBusyAnim.setSize(mSize);
}

GuiUpdate::~GuiUpdate()
{	
	mPingHandle->join();
	delete mPingHandle;
}

void GuiUpdate::threadPing()
{	
	if (ApiSystem::getInstance()->ping())
	{
		std::vector<std::string> msgtbl;
		if (ApiSystem::getInstance()->canUpdate(msgtbl))
		{
			// es4all: canUpdate 回填 [0]=版本号, [1]=目标建置时间戳(可选)。
			if (msgtbl.size() >= 1)
				mUpdateVersion = msgtbl[0];
			if (msgtbl.size() >= 2)
				mUpdateBuildInfo = msgtbl[1];

			onUpdateAvailable();
		}
		else
			onNoUpdateAvailable();
	}
	else
		onPingError();
}

void GuiUpdate::onUpdateAvailable()
{
	mLoading = false;
	LOG(LogInfo) << "GuiUpdate : Update available" << "\n";
	mState = 1;
}

void GuiUpdate::onNoUpdateAvailable()
{
	mLoading = false;
	LOG(LogInfo) << "GuiUpdate : No update available" << "\n";
	mState = 6;
}

void GuiUpdate::onPingError()
{
	LOG(LogError) << "GuiUpdate : Ping failed" << "\n";

	mLoading = false;
	mState = 3;
}

void GuiUpdate::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	if (mLoading)
		mBusyAnim.update(deltaTime);

	Window* window = mWindow;

	switch (mState)
	{
		case 1:
		{
			mState = 0;

			std::string message = _("REALLY UPDATE?");

			if (!mUpdateVersion.empty())
			{
				// es4all: 开发期版本号固定为 1.1pre, 光靠版本号分不出新旧, 故把建置时间戳一并显示,
				// 格式与页脚一致(如 1.1pre_20260722031544)。目标建置时间戳来自 release 的 .build-info 资产,
				// 当前建置的来自本机 resources/build-info.txt; 任一为空则自动退回只显示版本号。
				std::string curDisp = ApiSystem::getInstance()->getVersion();
				std::string newDisp = mUpdateVersion;
#ifdef ES4ALL_SELF_UPDATE
				std::string curBi = Es4allUpdate::getInstalledBuildInfo();
				if (!curBi.empty()) curDisp += "_" + curBi;
				if (!mUpdateBuildInfo.empty()) newDisp += "_" + mUpdateBuildInfo;
#endif
				std::string versionExtra = ApiSystem::getInstance()->getVersion(true);
				if (versionExtra == "none")
					message = Utils::String::format(_("YOU ARE CURRENTLY USING VERSION %s\nDO YOU WANT TO UPDATE TO VERSION %s?").c_str(), curDisp.c_str(), newDisp.c_str());
				else
					message = Utils::String::format(_("UNOFFICIAL SYSTEM MODIFICATIONS DETECTED.\nUPGRADING COULD BREAK YOUR SYSTEM.\nDO YOU WANT TO UPDATE TO VERSION %s?").c_str(), newDisp.c_str());

				window->pushGui(new GuiMsgBox(window, message, _("YES"), [this]
				{
					mState = 2;
					mLoading = true;

					mState = -1;
					new ThreadedUpdater(mWindow);

				}, _("NO"), [this] { mState = -1; }));
			}
		}		
		break;

		case 3:
		
			mState = 0;
			window->pushGui(new GuiMsgBox(window, _("NETWORK CONNECTION NEEDED"), _("OK"), [this] 
			{
				mState = -1;
			}));			
		
			break;

		case 6:

			mState = 0;
			window->pushGui(new GuiMsgBox(window, _("NO UPDATE AVAILABLE"), _("OK"), [this] 
			{
				mState = -1;
			}));

			break;

		case -1:
			delete this;
			break;
	}
}

void GuiUpdate::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = parentTrans * getTransform();

	renderChildren(trans);

	Renderer::setMatrix(trans);
	Renderer::drawRect(0.f, 0.f, mSize.x(), mSize.y(), 0x00000011);

	if (mLoading)
		mBusyAnim.render(trans);
}

bool GuiUpdate::input(InputConfig* config, Input input)
{
	return false;
}

std::vector<HelpPrompt> GuiUpdate::getHelpPrompts()
{
	return std::vector<HelpPrompt>();
}

