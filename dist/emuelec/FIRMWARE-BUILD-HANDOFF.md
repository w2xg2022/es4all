# E900V22C EmuELEC 固件构建交接单（给编译 session）

> 目的：把 es4all 最新改动全部烤进 E900V22C 固件。本单列出**必须做的步骤**、**每步为什么**、
> **已知未修（不阻塞出固件）**。es4all 侧已全部提交并 CI 绿；下面是 EmuELEC 树侧要配合的。

**最后更新**：2026-07-19　**目标 es4all ref**：`1712b16e34c560589bbcc3d516324de0d05fdc14`（v1.1-dev HEAD，CI 三目标全绿）

---

## 一、必须做的步骤

### 1. emulationstation 包指向最新 es4all（✅ 现状已对，确认即可）
- 文件：`packages/sx05re/emuelec-emulationstation/package.mk`
- 应为：
  ```
  PKG_VERSION="1712b16e34c560589bbcc3d516324de0d05fdc14"
  PKG_GIT_CLONE_BRANCH="v1.1-dev"
  ```
- ⚠️ 若还停在旧的 `0cd366b`（只有崩溃修复），会缺：正版 splash+白底 / AUDIO OUTPUT 配置文件 /
  CPU governor 翻译 / **任天堂式手柄确认键(fe2be48)** / wifi 防冻 / USB→TF 文案。**必须是 1712b16**。
- 改了版本记得 `./scripts/clean emuelec-emulationstation`（带正确 PROJECT/DEVICE 环境）强制重编。

### 2. 把蓝牙垫片烤进镜像（新增，必做）
- 源文件：es4all 仓库 `dist/emuelec/sources/batocera-bluetooth`
- 作用：EmuELEC 自带的 `/usr/bin/batocera/batocera-bluetooth` 只有 list/trust/remove，**没有 live_devices**，
  ES 配对 GUI 用不了（手动配对无法使用）。本垫片用 bluetoothctl 实作 live_devices/trust `<mac>` 等，
  实机 .165 验证过：能扫描串流 + 配对连接（Nintendo Switch Pro Controller 实测通）。
- 部署：构建时用该垫片**覆盖** `/usr/bin/batocera/batocera-bluetooth`（chmod 0755）。做法参考 ROCKNIX 那套
  sources 部署（把 dist/emuelec/sources/batocera-bluetooth 装进 image 的 /usr/bin/batocera/）。
- 目前 .165 上是临时 bind-mount，重启失效；**烤进镜像才永久生效**。

### 3. distro 默认值（建议，改 emuelec.conf 模板）
- 文件：`packages/sx05re/emuelec/config/emuelec/configs/emuelec.conf`
- 改：
  - `ee_splash.enabled=1` → `0`　（RA 启动画面默认关；用户要求，ES 侧只反映值、改不了默认）
  - 补 `retroarchLogging=0`　（RA 日志实机真正默认关；emuelecRunEmu.sh 该键为空时默认开日志）

### 4. 构建 + 重刷
- 构建：`PROJECT=Amlogic-ce DEVICE=Amlogic-no SUBDEVICE=E900V22C ARCH=aarch64 DISTRO=EmuELEC make image`
  （别带 IMAGE_SUFFIX；SUBDEVICE 已会加一次）
- 产物在 `target/*E900V22C*.img.gz`，刷回 SD。
- ⚠️ 重刷后 SSH host key 会变（要把新 key 给 es4all session 才能继续实机验证）。

---

## 二、1712b16 相比 0cd366b 新增了什么（刷了就能看到）
- **splash**：换回正版 EmuELEC ES splash（从 4.8 Generic 镜像 SYSTEM 提取）+ 加白色背景（原深色副标在黑底看不见）
- **AUDIO OUTPUT**：平台设置按机型白名单列音源（E900V22C=HDMI/AV），映射改由 `resources/audio_outputs.cfg` 数据文件（加机型只改一行）
- **任天堂式手柄确认键**：确认恒为 A（印刷 A）、返回恒为 B，与布局无关（原任天堂式误绑成按 B 确认）
- **CPU governor**：名称中文化（按需/性能/省电…）
- **翻译**：ES 帧率描述、外部挂载、开机影片、EXTRACTING/INSTALLING、开发者 LOG LEVEL→ES 日志级别 等（zh_CN+zh_TW）
- **wifi**：扫描加 timeout 防 UI 冻死
- **USB/SD → USB/TF** 文案
- **自我更新**：修好崩溃(不再动运行中 live 目录)、locale 随版本更新、滚动更新自动整机重开机、提示改「请重开机以完成更新」

---

## 三、已知未修（不阻塞出固件）
- **E900V22C wifi 连不上**：`connmanctl scan wifi` 卡 30s+ 不返回（uwe5631/connman 侧），且 EmuELEC 的
  batocera-wifi 无 enable/disable。es4all 只加了防冻护栏（UI 不死）。**wifi 真能用需 EmuELEC/内核侧修**，
  或该机型用有线。
- **splash 副标**：现为白底可读；若想保持黑底风格则需改副标为浅色（未做，等定夺）。

---

## 四、es4all 侧无需再动
所有上述 ES 代码/资源改动已在 `w2xg2022/es4all` 的 `v1.1-dev @ 1712b16`，CI 三目标全绿。
只有蓝牙垫片(dist/emuelec/sources/batocera-bluetooth)是 distro glue，需 EmuELEC 树装进镜像。
