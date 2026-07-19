#pragma once
#ifndef ES_APP_ES4ALL_UPDATE_H
#define ES_APP_ES4ALL_UPDATE_H

// es4all 自我更新
// -----------------------------------------------------------------------------
// 通过 GitHub Releases 给已部署的 es4all EmulationStation 做 OTA 升级，替代发行版
// 自带的 batocera-upgrade / emuelec-upgrade(在本项目的设备上并不存在、故"更新功能无法使用")。
//
// 版本序:  1.0 < 1.1pre < 1.1 < 1.2pre < 1.2 ...   (pre = 预览版, 排在同号正式版之前)
// 构建指纹: 版本字串在 dev 期间固定为 "1.1pre" 不变, 光靠版本号无法区分同版本号的不同次构建。
//           故 CI 把 git commit SHA 编进 binary(ES4ALL_BUILD_SHA), 并写进 Release 说明。
//           同版本号 + SHA 不同 => 视为有更新(实现 1.1pre 的滚动升级)。
//
// 只在 ES4ALL_SELF_UPDATE 定义时编译(三个 es4all target 都开)。

#include <string>
#include <functional>

struct Es4allRelease
{
	std::string version;   // 去掉前导 v 的版本号, 如 "1.1pre"
	std::string tag;       // 原始 tag, 如 "v1.1pre"
	std::string sha;       // Release 说明里记录的构建指纹(git commit SHA), 可能为空
	std::string assetUrl;  // 本 target 对应 zip 的下载地址
	bool valid = false;
};

namespace Es4allUpdate
{
	// 当前安装的版本号 / 构建指纹(编译期烤进 binary)。
	std::string getInstalledVersion();
	std::string getInstalledSha();

	// 版本号比较: 依 1.0 < 1.1pre < 1.1 ... 的序。返回 <0 / 0 / >0。
	// 只比较版本号本身, 不涉及构建指纹。非法版本号按 (0,0,release) 处理。
	int compareVersion(const std::string& a, const std::string& b);

	// 候选 Release 相对当前安装是否"更新":版本号更高, 或同版本号(pre)但构建指纹不同。
	bool isNewer(const std::string& installedVer, const std::string& installedSha,
	             const std::string& candVer, const std::string& candSha);

	// 查询 GitHub Releases(含预览版), 挑出对本 target 可用且比当前更新的那个。
	// 有更新返回 true 并填 out; 无更新 / 网络失败返回 false。
	bool findLatestApplicable(Es4allRelease& out);

	// 下载并套用某个 Release。进度经 func 以 ">>> N%" 约定回报(供 GuiUpdate 解析)。
	// 返回 {讯息, 退出码}: 退出码 0 = 成功(重启后生效), 非 0 = 失败。
	std::pair<std::string, int> apply(const Es4allRelease& rel,
	                                  const std::function<void(const std::string)>& func = nullptr);
}

#endif // ES_APP_ES4ALL_UPDATE_H
