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
