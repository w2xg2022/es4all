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
#include <cstdio>
#include <string>

static int getBindButton(SDL_GameController* gc, SDL_GameControllerButton b)
{
	if (gc == nullptr)
		return -1;
	SDL_GameControllerButtonBind bind = SDL_GameControllerGetBindForButton(gc, b);
	return (bind.bindType == SDL_CONTROLLER_BINDTYPE_BUTTON) ? bind.value.button : -1;
}

// es4all 診斷：直接寫檔(立即 flush)，不依賴 ES log 系統
static void dbgLog(const std::string& s)
{
	FILE* f = fopen("/storage/detectlayout.log", "a");
	if (f) { fputs(s.c_str(), f); fputc('\n', f); fclose(f); }
}

GuiDetectLayout::GuiDetectLayout(Window* window, InputConfig* target, const std::function<void()>& doneCallback)
	: GuiComponent(window), mTarget(target), mDoneCallback(doneCallback),
	  mGC(nullptr), mBtnA(-1), mBtnB(-1), mBtnX(-1), mBtnY(-1),
	  mPhase(0), mABInverted(false), mXYInverted(false),
	  mBackground(window, ":/frame.png"), mGrid(window, Vector2i(1, 4))
{
	if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 0)
		SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);

	int idx = (mTarget != nullptr) ? mTarget->getDeviceIndex() : -1;
	if (idx >= 0 && SDL_IsGameController(idx))
		mGC = SDL_GameControllerOpen(idx);

	mBtnA = getBindButton(mGC, SDL_CONTROLLER_BUTTON_A);
	mBtnB = getBindButton(mGC, SDL_CONTROLLER_BUTTON_B);
	mBtnX = getBindButton(mGC, SDL_CONTROLLER_BUTTON_X);
	mBtnY = getBindButton(mGC, SDL_CONTROLLER_BUTTON_Y);

	// es4all 診斷(LogError 確保寫入 log)：印出 SDL 反查的 binds
	LOG(LogError) << "[DetectLayout] gc=" << (mGC != nullptr ? SDL_GameControllerName(mGC) : "NULL")
		<< " idx=" << idx << " binds A=" << mBtnA << " B=" << mBtnB << " X=" << mBtnX << " Y=" << mBtnY;
	dbgLog("binds A=" + std::to_string(mBtnA) + " B=" + std::to_string(mBtnB) +
		" X=" + std::to_string(mBtnX) + " Y=" + std::to_string(mBtnY) +
		" gc=" + std::string(mGC != nullptr && SDL_GameControllerName(mGC) ? SDL_GameControllerName(mGC) : "NULL"));

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

	setPrompt(_("PRESS THE BUTTON LABELED \"A\" (by its printed letter, not position)."));
}

GuiDetectLayout::~GuiDetectLayout()
{
	if (mGC != nullptr)
	{
		SDL_GameControllerClose(mGC);
		mGC = nullptr;
	}
}

void GuiDetectLayout::setPrompt(const std::string& msg)
{
	if (mMsg != nullptr)
		mMsg->setText(msg);
}

void GuiDetectLayout::onSizeChanged()
{
	GuiComponent::onSizeChanged();
	mBackground.fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));
	mGrid.setSize(mSize);
	if (mTitle != nullptr)
		mGrid.setRowHeight(0, mTitle->getFont()->getHeight() * 1.1f);
}

bool GuiDetectLayout::input(InputConfig* config, Input input)
{
	// 鍵盤 ESC 或非目標裝置的取消：跳過偵測直接進手動設定
	if (input.device == DEVICE_KEYBOARD && input.type == TYPE_KEY && input.value && input.id == SDLK_ESCAPE)
	{
		finishSkip();
		return true;
	}

	// SDL 反查不到對應(非標準手把) → 無法自動偵測，跳過
	if (mBtnA < 0 || mBtnB < 0 || mBtnX < 0 || mBtnY < 0)
	{
		if (input.type == TYPE_BUTTON && input.value)
			finishSkip();
		return true;
	}

	if (config != mTarget || input.type != TYPE_BUTTON || input.value == 0)
		return true;

	int id = input.id;

	if (mPhase == 0)
	{
		LOG(LogError) << "[DetectLayout] A-press id=" << id << " (mBtnA=" << mBtnA << " mBtnB=" << mBtnB << ")";
		dbgLog("A-press id=" + std::to_string(id));
		if (id == mBtnA)      mABInverted = false; // 印刷A = SDL_A = 南 → Xbox 式
		else if (id == mBtnB) mABInverted = true;  // 印刷A = SDL_B = 東 → 任天堂式
		else return true;                          // 非 A/B 臉鍵，忽略
		mPhase = 1;
		setPrompt(_("NOW PRESS THE BUTTON LABELED \"X\"."));
	}
	else if (mPhase == 1)
	{
		LOG(LogError) << "[DetectLayout] X-press id=" << id << " (mBtnX=" << mBtnX << " mBtnY=" << mBtnY << ")";
		dbgLog("X-press id=" + std::to_string(id));
		if (id == mBtnX)      mXYInverted = false; // 印刷X = SDL_X = 西 → 正常
		else if (id == mBtnY) mXYInverted = true;  // 印刷X = SDL_Y = 北 → XY 反
		else return true;
		applyAndFinish();
	}
	return true;
}

void GuiDetectLayout::applyAndFinish()
{
	// 前端 AB：InvertButtons(A在東=任天堂式時反轉確認/取消)
	Settings::getInstance()->setBool("InvertButtons", mABInverted);
	// 遊戲內 AB(透傳到 RA per-core remap)：A在南(Xbox)預設對調、A在東(任天堂)不對調
	Settings::getInstance()->setBool("InvertGameButtons", !mABInverted);
	// XY 對調(前端顯示；遊戲內 XY remap 另做)
	Settings::getInstance()->setBool("InvertXYButtons", mXYInverted);
	Settings::getInstance()->saveFile();
	InputConfig::AssignActionButtons();

	LOG(LogError) << "[DetectLayout] RESULT AB inverted=" << mABInverted << ", XY inverted=" << mXYInverted;
	dbgLog("RESULT AB_inv=" + std::to_string(mABInverted ? 1 : 0) + " XY_inv=" + std::to_string(mXYInverted ? 1 : 0));

	auto cb = mDoneCallback;
	delete this;
	if (cb)
		cb();
}

void GuiDetectLayout::finishSkip()
{
	LOG(LogInfo) << "GuiDetectLayout: skipped (controller not recognized or user cancelled).";
	auto cb = mDoneCallback;
	delete this;
	if (cb)
		cb();
}
