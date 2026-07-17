#pragma once

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "ApiSystem.h"

template<typename T>
class OptionListComponent;

#ifdef _ENABLEEMUELEC
#include <memory>
class ImageComponent;
class TextureResource;
class BusyComponent;
#include <vector>
#endif

class GuiFileBrowser : public GuiComponent
{
public:
	enum FileTypes
	{
		IMAGES = 1,
		MANUALS = 2,
		VIDEO = 4,
		DIRECTORY = 8,
		FILES = 16,
		// es4all: 定义无条件。44bcfac 把使用端(GuiMenu.cpp 的 CUSTOM MENU SCROLL
		// SOUND)移出了 _ENABLEEMUELEC 守卫，却漏了这里的定义端，于是 ROCKNIX 上
		// `GuiFileBrowser::AUDIO` 变成引用一个未定义的枚举值，编译直接失败。这批
		// 改动此前从没在 ROCKNIX 上编过(pin 一直停在旧版)，所以一直没暴露。AUDIO
		// 只是个 filetype 位标志，三边都该有它。
		AUDIO = 32,

		ALL = 255
	};

	GuiFileBrowser(Window* window, const std::string startPath, const std::string selectedFile, FileTypes types = FileTypes::IMAGES, const std::function<void(const std::string&)>& okCallback = nullptr, const std::string& title = "");

#ifdef _ENABLEEMUELEC        
		~GuiFileBrowser() override;
	void update(int deltaTime) override;
#endif
	bool input(InputConfig* config, Input input) override;
	virtual std::vector<HelpPrompt> getHelpPrompts() override;

private:
	void onOk(const std::string& path);
	void navigateTo(const std::string path);
	void centerWindow();

#ifdef _ENABLEEMUELEC
       void generateVideoPreview(const std::string& path);
       void clearVideoPreview();
       std::shared_ptr<ImageComponent> mPreview;
       std::shared_ptr<BusyComponent> mLoading;
       std::shared_ptr<ImageComponent> mLoadingBg;
       std::vector<std::string> mVideoFrames;
       std::vector<std::shared_ptr<TextureResource>> mFrameTextures;
       int mCurrentFrame;
       int mFrameTime;
       std::string mTempPreviewDir;
       bool mGeneratingPreview;
       int mExpectedFrames;
       int mLastFrameCount;
       int mNoFrameTime;
#endif

	MenuComponent mMenu;	

	std::string mCurrentPath;
	std::string mSelectedFile;
	FileTypes   mTypes;

	std::function<void(const std::string&)> mOkCallback;
};
