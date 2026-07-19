#pragma once

#include <string>
#include "components/ImageComponent.h"
#include "components/TextComponent.h"

class Window;
class TextureResource;

// es4all: 三个 target 都带 -DENABLE_EMUELEC=1(即 _ENABLEEMUELEC), 原来的 `#elifdef _ENABLEEMUELEC`
// (①C23 写法, 应为 #elif defined; ②对三个 target 都成立)导致 armbian/rocknix 也用 splash_emuelec.svg。
// 而 es4all 的 splash_emuelec.svg 内容是 Armbian 品牌 d-pad 图。改为按 target 分:
//   EMUELEC → splash_emuelec.png(真正的 EmuELEC 原图, 上游 EmuELEC/EmuELEC dev 的 splash-1080.png)
//   armbian/rocknix → splash_emuelec.svg(现有 Armbian d-pad 图, 用户确认此图 OK)
#if WIN32
#define DEFAULT_SPLASH_IMAGE ":/splash.svg"
#define OLD_SPLASH_LAYOUT true
#elif defined(ES4ALL_TARGET_EMUELEC)
#define DEFAULT_SPLASH_IMAGE ":/splash_emuelec.png"
#define OLD_SPLASH_LAYOUT true
#elif defined(_ENABLEEMUELEC)
#define DEFAULT_SPLASH_IMAGE ":/splash_emuelec.svg"
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
