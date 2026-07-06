#pragma once
#ifndef ES_CORE_GUIS_GUI_DETECT_LAYOUT_H
#define ES_CORE_GUIS_GUI_DETECT_LAYOUT_H

#include "components/ComponentGrid.h"
#include "components/NinePatchComponent.h"
#include "GuiComponent.h"

class TextComponent;
class InputConfig;

// es4all: 手柄布局偵測(evdev 版)。
// UI 維持「按印刷 A」「按印刷 X」——玩家按自己認得的印刷鍵；底層讀該裝置的
// /dev/input/eventN，用 kernel 標準化的 BTN_SOUTH/EAST/NORTH/WEST(真實物理位置)判定，
// 不依賴 SDL 的按鍵名(SDL 只照 pad 宣稱的 HID，遇到韌體把 X/Y 回報反的手把會判錯)。
// 結果寫入 Settings：InvertButtons(前端AB)、InvertGameButtons(遊戲內AB)、InvertXYButtons(XY)。
class GuiDetectLayout : public GuiComponent
{
public:
	GuiDetectLayout(Window* window, InputConfig* target, const std::function<void()>& doneCallback);
	~GuiDetectLayout();

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void onSizeChanged() override;

private:
	void setPrompt(const std::string& msg);
	void handlePhysBtn(int btnCode);   // btnCode = BTN_SOUTH/EAST/NORTH/WEST
	void applyAndFinish();
	void finishSkip();

	InputConfig* mTarget;
	std::function<void()> mDoneCallback;

	int  mEvFd;        // /dev/input/eventN 檔案描述子(-1=開失敗→跳過偵測)
	int  mPhase;       // 0=等按印刷A, 1=等按印刷X
	bool mABInverted;  // 印刷A在東(任天堂式)=true；在南(Xbox式)=false
	bool mXYInverted;  // 印刷X在北(任天堂式)=true；在西(Xbox式)=false
	bool mFinished;

	NinePatchComponent mBackground;
	ComponentGrid mGrid;
	std::shared_ptr<TextComponent> mTitle;
	std::shared_ptr<TextComponent> mMsg;
};

#endif // ES_CORE_GUIS_GUI_DETECT_LAYOUT_H
