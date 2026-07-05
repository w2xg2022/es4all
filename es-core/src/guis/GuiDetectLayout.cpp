#include "guis/GuiDetectLayout.h"

#include "guis/GuiInputConfig.h"
#include "components/TextComponent.h"
#include "InputConfig.h"
#include "Settings.h"
#include "Window.h"
#include "LocaleES.h"
#include "Log.h"
#include "ThemeData.h"
#include "Renderer.h"

static int getBindButton(SDL_GameController* gc, SDL_GameControllerButton b)
{
	if (gc == nullptr)
		return -1;
	SDL_GameControllerButtonBind bind = SDL_GameControllerGetBindForButton(gc, b);
	return (bind.bindType == SDL_CONTROLLER_BINDTYPE_BUTTON) ? bind.value.button : -1;
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
		if (id == mBtnA)      mABInverted = false; // 印刷A = SDL_A = 南 → Xbox 式
		else if (id == mBtnB) mABInverted = true;  // 印刷A = SDL_B = 東 → 任天堂式
		else return true;                          // 非 A/B 臉鍵，忽略
		mPhase = 1;
		setPrompt(_("NOW PRESS THE BUTTON LABELED \"X\"."));
	}
	else if (mPhase == 1)
	{
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

	LOG(LogInfo) << "GuiDetectLayout: detected AB inverted=" << mABInverted << ", XY inverted=" << mXYInverted;

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
