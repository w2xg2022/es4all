#pragma once

#include <string>
#include "components/ImageComponent.h"
#include "components/TextComponent.h"

class Window;
class TextureResource;

// es4all: 三个 target 都带 -DENABLE_EMUELEC=1(即 _ENABLEEMUELEC), 原来的 `#elifdef _ENABLEEMUELEC`
// (①C23 写法, 应为 #elif defined; ②对三个 target 都成立)导致 armbian/rocknix 也用了 splash_emuelec.svg,
// 而该文件内容其实是 Armbian 品牌 d-pad 图。改为按 target 分, 并把文件名理正:
//   splash_emuelec.svg = 原厂 EmuELEC ES splash(从 EmuELEC 4.8 Generic 镜像 SYSTEM 里取出的正版)
//   splash_armbian.svg = Armbian 品牌 d-pad 图(原 splash_emuelec.svg 的内容)
//   EMUELEC → splash_emuelec.svg; armbian/rocknix → splash_armbian.svg。
#if WIN32
#define DEFAULT_SPLASH_IMAGE ":/splash.svg"
#define OLD_SPLASH_LAYOUT true
#elif defined(ES4ALL_TARGET_EMUELEC)
#define DEFAULT_SPLASH_IMAGE ":/splash_emuelec.svg"
// es4all: 曾试过改 false 做真全屏铺满，但会让内容贴满屏幕边缘，跟 78% 高度的
// "正在加载主题" 文字撞在一起；改用画布留白撑开又跟 splash_armbian.svg 观感不一致。
// 最终决定跟 armbian/rocknix 走同一套 old layout(80%x60% 黑边卡片、自动安全边界)，
// 三个 target 观感统一，也不用额外维护 splash SVG 的留白。
#define OLD_SPLASH_LAYOUT true
#elif defined(ES4ALL_TARGET_ROCKNIX)
// es4all: ROCKNIX 也带 -DENABLE_EMUELEC=1，本来会掉进下面的 _ENABLEEMUELEC 分支吃到
// splash_armbian.svg(Armbian 品牌图)。改用自家的 splash_rocknix.svg：沿用 EmuELEC 那张的
// 手柄图形与 EMULATIONSTATION 副标(直接取自 splash_emuelec.svg 的对应 group)，只把中间的
// 大字样换成同风格的像素字「ROCKNIX」(同一个位置 x241~1097/y408~509、同一组配色
// #A8A3D0 前三字 + #514688 后四字 + #231F20 立体阴影)。配 Splash.cpp 的白底处理。
#define DEFAULT_SPLASH_IMAGE ":/splash_rocknix.svg"
#define OLD_SPLASH_LAYOUT true
#elif defined(_ENABLEEMUELEC)
#define DEFAULT_SPLASH_IMAGE ":/splash_armbian.svg"
#define OLD_SPLASH_LAYOUT true
#else
#define DEFAULT_SPLASH_IMAGE ":/logo.png"
#define OLD_SPLASH_LAYOUT false
#endif

class Splash
{
public:
	Splash(Window* window, const std::string image, bool fullScreenBackGround = true, IBindable* bindable = nullptr);
	~Splash();

	void update(std::string text, float percent = -1);
	void render(float opacity, bool swapBuffers = true);

private:
	ImageComponent  mBackground;
	TextComponent   mText;
	float			mPercent;

	ImageComponent  mInactiveProgressbar;
	ImageComponent  mActiveProgressbar;

	unsigned int	mBackgroundColor;
	float			mRoundCorners;

	std::shared_ptr<TextureResource> mTexture;

	std::vector<GuiComponent*> mExtras;
} ;
