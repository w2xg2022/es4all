# es4all — EmulationStation for Armbian / ROCKNIX / EmuELEC

统一维护的 EmulationStation 前端源码。同一份代码、多个 build profile，用
`ES4ALL_TARGET` 切换平台差异，覆盖三个 target：

| target    | 系统            | libc  | init    | 编译方式                         |
|-----------|-----------------|-------|---------|----------------------------------|
| `armbian` | Debian trixie   | glibc | systemd | 直接 CMake（VM bookworm-arm64 chroot） |
| `rocknix` | ROCKNIX         | glibc | systemd | 直接 CMake 出成品，交给发行版直接取用 |
| `emuelec` | EmuELEC         | glibc | systemd | 直接 CMake 出成品，交给发行版直接取用 |

源码基底来自 `es4armbian`（EmuELEC/emuelec-emulationstation 的分化 fork）。
原 es4armbian 的详细改动笔记见 [`README_es4armbian.md`](README_es4armbian.md)。

## 架构：独立编译 + 发行版直接取用成品

es4all 要求**能独立编译**（在 VM bookworm-arm64 chroot 直接用 CMake 编出成品）。
EmuELEC / ROCKNIX 做 userspace 时**不在自己的 buildroot 编源码**，直接取用我们
编好的二进制。三边都是 glibc + aarch64，主要相容性变量是动态库（SDL2/VLC 等）。

## 分歧开关：target → 能力标志

代码**不**直接用 target 名做 `#if`，而是先由 CMake 把 target 翻成一组「能力标志」，
代码只认能力、保留合并机会（例：三边皆 systemd，重启逻辑共用一份）：

```
-DES4ALL_TARGET=armbian|rocknix|emuelec
   → -DES4ALL_TARGET_<NAME>=1
   → -DES4ALL_INIT_SYSTEMD=1        # 三边皆开
   → -DES4ALL_PATHS_<NAME>=1        # 唯一真正三分：配置/ROM/retroarch.cfg 路径
   → -DES4ALL_BUILD_BUILDROOT=1     # rocknix / emuelec
```

三边的实质差异几乎只剩「路径」一个维度（init、libc、渲染 GLES2 皆一致）。

## 目录

- `es-core/` `es-app/` `locale/` … ES 源码本体（跨 target 共用）
- `external/` … 内置依赖（含 vendored pugixml，独立/云编译不依赖系统库或 submodule）
- `dist/armbian/` — 直接 CMake 编，含启动脚本
- `dist/rocknix/` — ROCKNIX 打包胶水（`package.mk` 等）
- `dist/emuelec/` — EmuELEC 打包胶水（`package.mk` 等）

## 编译（armbian，独立编译）

在 VM `bookworm-arm64` chroot：

```bash
cmake -DES4ALL_TARGET=armbian -DGLES=OFF -DGLES2=ON -DENABLE_EMUELEC=1 -DCEC=OFF .
make -j4
```

ROCKNIX / EmuELEC 版同上，改 `-DES4ALL_TARGET=rocknix`（或 `emuelec`）即可。

## 分支与发布

| 分支 | 用途 |
|---|---|
| `v1.1-stable` | **当前开发线**（default）。1.1 已定版发布，后续修正也先进这里 |
| `v1.0-stable` | 1.0 维护线 |

Release 由版本字串（`es-app/src/EmulationStation.h` 的 `PROGRAM_VERSION_STRING`）自动推导：
tag 为 `v<版本>`，含 `pre` 发预览版、不含则发正式版（Latest）。
维护分支带 `-stable` 后缀是因为 `v1.0` / `v1.1` 已被 tag 占用，同名会让 git 报 refname ambiguous。

> 开发分支改名时，**必须同步四处 `PKG_GIT_CLONE_BRANCH`**（本仓库 `dist/{rocknix,emuelec}/package.mk`
> 两份参考副本，以及 `w2xg2022/rocknix` 与 EmuELEC 树里的两份真源），否则固件树 clone 不到分支、
> 编译直接失败。

## 待办（v1.2）

1.1 收敛时刻意砍掉或藏起来的功能，都是**因为后端做不到、留着只会误导使用者**。
这里记下现况与要补的东西：

### 1. 外部存储：三个 target 统一做成「内外部盘聚合」

| target | 现况 |
|---|---|
| `armbian` | **入口已藏**。整组功能是空壳：碟的枚举写死 `find /var/media/`（该目录在 Armbian 上不存在）、挂载后端 `eemount`/`mount_romfs.sh` 也不存在、ROM 根是 `/home/game/ROMs` 而非 `/storage/roms`。更底层的是该机**没有任何自动挂载机制**，插上的 USB 碟连挂都不会挂 |
| `rocknix` | **入口已藏**。ES 侧四个键都对得上 `automount`，但上游韧体有两个 bug：①`rocknix-automount` 跑得比 USB 枚举早（实测系统碟 +3.9s、外接碟 **+65.4s**），永远扫不到；②之后 udevil 把碟挂到 `/var/media`，而 `find_games` 把「已挂载」当成「不可用」而跳过。另外「合并储存」在 FAT/exFAT/NTFS 碟上必定无效（overlayfs 要求 upperdir 支援 xattr，脚本只认 ext4/btrfs），UI 却显示已开启 |
| `emuelec` | 可用，但语义是「二选一」（内部 / 外接碟择一），不是聚合 |

目标：三边统一成 ROCKNIX 那种 overlay 叠加（内部 ROM 与外接碟 ROM 合并呈现）。
韧体侧那两个 bug 归 `w2xg2022/rocknix`；ARMBIAN 的挂载层归 `es4all-1key`。

### 2. 视频模式（分辨率切换）

1.1 三个 target 一律移除选单，**后端保留**、接回来即可：

- `rocknix` — glue 脚本 `es4all-setvideomode`（wlr-randr，Wayland）
- `emuelec` — `ApiSystem::applyEmuelecVideoMode()`（解 `/sys/class/display/debug` 锁 + 写完还原 PHY）
- `armbian` — 尚未实作

> ⚠️ 难点在**验证手段**：采集卡本身不重新协商输入时序，切模式后会黑屏/角落/撕裂，
> **看到异常不一定是程式的错**（1.1 期间为此误判过一次）。要嘛接真电视验，
> 要嘛先做好防呆（试用确认对话框那套机制已写过）。

### 3. ROCKNIX：把 zh_CN 预编进映像

R 的 locale 是**执行时现编**的（JELOS 继承的设计，为省映像空间）：`es_settings` 开机检查
`zh_CN.UTF-8/LC_NAME` 不在就跑 `localedef`，在 RK3566 上要 **约 28 秒**，期间画面全黑、
没有任何提示。上游只预编了 `en_US.UTF-8`（`dist/rocknix/package.mk` 的注解自己写着
「在 RK3326 上省下一两分钟、代价约 1MB」），而我们预设语言是 zh_CN。
→ 比照办理，把 zh_CN 也预编进去，连全新安装第一次开机的 28 秒也省掉。

（**A 与 E 没有这个问题**：A 靠 Debian 发行版自带、E 映像里就备妥。）

### 4. 自我更新加「接收测试版」开关

`Es4allUpdate::findLatestApplicable()` 抓 releases 清单挑**版本号最高的**，**完全不过滤 prerelease**。
开发分支持续发 `vX.Ypre` 预览版，于是**所有开着自我更新的设备——包括跑稳定版的——都会看到「有新版」**
并被推送开发中的版本。

→ 加一个「接收测试版」开关，**预设关闭**；关闭时跳过 `prerelease=true` 的 release
（GitHub API 的 release 物件本来就有这个栏位，不必自己解析版本字串）。
