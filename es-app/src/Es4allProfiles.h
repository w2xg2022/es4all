#pragma once
#ifndef ES_APP_ES4ALL_PROFILES_H
#define ES_APP_ES4ALL_PROFILES_H

// es4all 机型专属配置下发
// -----------------------------------------------------------------------------
// 从 w2xg2022/es4all-profiles 拉取本机型适用的纯数据/脚本配置(音源输出映射表、
// installtoemmc 板子表、PSP controls.ini 之类), 让「改一行数据」不必发一包 ES、
// 更不必云编译。⚠️ dts/dtb/内核 config 及编进 binary 的东西不在此列, 只能重编。
//
// 仓库目录约定: <scope>/<dest-root>/<相对路径>
//   scope 解析顺序 common -> <T>/_common -> <T>/<机型>, 后者覆盖前者(T = A/E/R)。
//   机型键 = /proc/device-tree/model 的子串, 与 audio_outputs.cfg 同一套写法。
//   dest-root 是落点 token: es-resources / storage-config(见 kDestRoots)。
//
// ★为什么落在 ES 的【用户】resources 目录而不是系统那份★
//   ResourceManager 的搜索顺序是用户目录优先于系统目录, 于是:
//     - E/R 的只读 squashfs 完全不必动;
//     - ES 自我更新(OTA)不会洗掉下发的配置 —— OTA 覆盖的是系统那份。
//
// 安全性: 仓库地址写死在此, **不开放使用者自订 URL** —— 内容含可执行脚本,
//         可设定 URL 等同把机器交出去。
//
// 全有或全无: 任一档 md5 校验不过就整批放弃, 不做部分套用(半套的配置比旧的更糟)。
// 套用前把现有落点备份到 <storeBase>/.profiles.bak, 供出事时还原。
//
// 只在 ES4ALL_SELF_UPDATE 定义时编译(三个 es4all target 都开), 与自我更新同一条件:
// 两者都依赖 HttpReq + 可写的使用者目录。

#include <string>

namespace Es4allProfiles
{
	// 选单开关 system.profiles.enabled(**预设开启**)。首次启动若键不存在则写入预设值 ——
	// GuiSettings::addSwitch 读的是 SystemConf::getBool(默认 false), 不种下预设值
	// 开关会显示成「关」, 与实际行为不符。
	void ensureDefaults();
	bool isEnabled();

	// 已套用的内容版本(读旁档), 未套用过为空。
	std::string installedVersion();

	// 已下发的机型专属脚本(落在 bin/)的绝对路径; 不存在或不可执行则回空字串。
	// ★选单可见性的判断依据★: 「这台机器有没有这个功能」不写死在 ES 里, 而是看
	// profile 有没有给对应的脚本 —— 加一台机器不必动 ES。
	std::string scriptPath(const std::string& name);

	// 查某个已下发的档案在不在。参数是仓库里【落点根之后】的那段, 例如
	//   hasFile("storage-config/es4all/emmc-layout.conf")
	// ★用途: 判断「这台机器支不支持某个功能」时, 要看的是【资料】不是【机制】★。
	//   机制(_common/bin 的脚本)每台都有, 拿它当依据会让选单一律显示;
	//   资料(<机型>/ 的配方)才代表这台真的做过、验证过。
	bool hasFile(const std::string& destRelPath);

	// 拉取并套用。返回 true = 确实套用了新内容(需重启 ES 才完全生效)。
	// outMsg 填人类可读的结果说明(成功/无更新/失败原因), 可为 nullptr。
	// 阻塞式, 请在背景线程调用。
	bool sync(std::string* outMsg = nullptr);
}

#endif // ES_APP_ES4ALL_PROFILES_H
