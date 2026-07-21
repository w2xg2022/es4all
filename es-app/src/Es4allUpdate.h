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
//           **指纹 = 该 target binary 的 md5**: 本机算 /proc/self/exe 的 md5, 与 release 里
//           `<zip名>.md5` 资产的内容比对; 同版本号 + md5 不同 => 有更新(1.1pre 滚动升级)。
//
//           为何不再用 commit SHA: CI 一次编三个 target、共用同一个 github.sha, 任何一次 push
//           都让三个指纹一起变 —— 即使改动只在某个 target 的守卫内、对另两个的产物毫无影响,
//           它们也会被提示「有新版」。改用产物内容的 md5 后, 只有真的变了才提示。
//           前提「相同源码编出相同 binary」已移除 __DATE__/__TIME__ 与 -DES4ALL_BUILD_SHA,
//           并用两次云端建置(同 commit、不同 runner)验证 md5 完全一致。
//
// 只在 ES4ALL_SELF_UPDATE 定义时编译(三个 es4all target 都开)。

#include <string>
#include <functional>

struct Es4allRelease
{
	std::string version;   // 去掉前导 v 的版本号, 如 "1.1pre"
	std::string tag;       // 原始 tag, 如 "v1.1pre"
	std::string sha;       // 构建指纹: 优先取 .md5 资产内容(本 target binary 的 md5);
	                       // 旧版 release 无 .md5 时退回 body 里的 commit SHA。可能为空。
	std::string assetUrl;  // 本 target 对应 zip 的下载地址
	std::string md5Url;    // 本 target 的 <zip>.md5 下载地址, 可能为空
	bool valid = false;
};

namespace Es4allUpdate
{
	// 当前安装的版本号 / 构建指纹(指纹为执行期计算的 /proc/self/exe md5)。
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

	// 上一次 apply() 成功后, 是否需要"整机重开机"才能生效(而非仅重启 ES 进程)。
	// 唯读 rootfs 平台(ROCKNIX/EMUELEC)靠 bind-mount 覆盖 binary: mount --bind 绑定的是当时的
	// inode, 若安装路径此前已被绑定过(即"滚动更新"——同版本号 1.1pre 又出新 build), 新文件写入
	// /storage 后旧挂载仍指向旧 inode, 只重启 ES 进程不会换成新档, 必须整机重开机(重新走一次
	// 挂载钩子/服务, 才会绑定到新 inode)。首次安装(此前未挂载)或 ARMBIAN(就地覆盖)则不需要。
	bool needsFullReboot();
}

#endif // ES_APP_ES4ALL_UPDATE_H
