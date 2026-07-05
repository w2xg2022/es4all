#pragma once
#ifndef ES_CORE_GUIS_GUI_DETECT_LAYOUT_H
#define ES_CORE_GUIS_GUI_DETECT_LAYOUT_H

#include "components/ComponentGrid.h"
#include "components/NinePatchComponent.h"
#include "GuiComponent.h"
#include <SDL.h>

class TextComponent;
class InputConfig;

// es4all: 手柄布局偵測。
// 進 GuiInputConfig 前先跑：提示玩家按「印刷 A」定 AB 佈局、按「印刷 X」定 XY 佈局，
// 用 SDL GameController 反查按下的物理位置(南/東/西/北)，校準「印刷標籤 vs 系統實際回報」。
// 可同時處理：任天堂/Xbox 佈局差異、以及韌體把 X/Y 回報反的手把(實測 MD1000 手把即此類)。
// 結果寫入 Settings：InvertButtons(前端AB)、InvertGameButtons(遊戲內AB)、InvertXYButtons(XY)。
class GuiDetectLayout : public GuiComponent
{
public:
	GuiDetectLayout(Window* window, InputConfig* target, const std::function<void()>& doneCallback);
	~GuiDetectLayout();

	bool input(InputConfig* config, Input input) override;
	void onSizeChanged() override;

private:
	void setPrompt(const std::string& msg);
	void applyAndFinish();
	void finishSkip();

	InputConfig* mTarget;
	std::function<void()> mDoneCallback;

	SDL_GameController* mGC;
	// SDL_A/B/X/Y 對應的原始 joystick button index（-1 表示取不到）
	int mBtnA, mBtnB, mBtnX, mBtnY;

	int  mPhase;       // 0=等按印刷A, 1=等按印刷X
	bool mABInverted;  // 印刷A在東(任天堂式)=true；在南(Xbox式)=false
	bool mXYInverted;  // 印刷X在北(回報SDL_Y)=true；在西(回報SDL_X)=false

	NinePatchComponent mBackground;
	ComponentGrid mGrid;
	std::shared_ptr<TextComponent> mTitle;
	std::shared_ptr<TextComponent> mMsg;
};

#endif // ES_CORE_GUIS_GUI_DETECT_LAYOUT_H
