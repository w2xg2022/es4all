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
