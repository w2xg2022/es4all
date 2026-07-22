#pragma once

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/BusyComponent.h"

#include <thread>

namespace GuiUpdateState
{
	enum State
	{
		NO_UPDATE,
		UPDATER_RUNNING,
		UPDATE_READY
	};
}

class GuiUpdate : public GuiComponent 
{
public:
	static GuiUpdateState::State state;

    GuiUpdate(Window *window);
    virtual ~GuiUpdate();

    void render(const Transform4x4f &parentTrans) override;
    bool input(InputConfig *config, Input input) override;
    std::vector<HelpPrompt> getHelpPrompts() override;
    void update(int deltaTime) override;

private:
    BusyComponent mBusyAnim;
    bool mLoading;
    int mState;
	std::string mUpdateVersion;
	std::string mUpdateBuildInfo;   // es4all: 目标版本的建置时间戳(来自 release 的 .build-info 资产)

    std::pair<std::string, int> mResult;

	std::thread* mPingHandle;

    void threadPing();
    void onUpdateAvailable();
    void onNoUpdateAvailable();
    void onPingError();
};
