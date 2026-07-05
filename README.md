# es4all — EmulationStation for Armbian / ROCKNIX / EmuELEC

統一維護的 EmulationStation 前端源碼。同一份程式碼、多個 build profile，用
`ES4ALL_TARGET` 切換平台差異，涵蓋三個 target：

| target    | 系統            | libc  | init    | 編譯方式                         |
|-----------|-----------------|-------|---------|----------------------------------|
| `armbian` | Debian trixie   | glibc | systemd | 直接 CMake（VM bookworm-arm64 chroot） |
| `rocknix` | ROCKNIX         | glibc | systemd | buildroot 交叉工具鏈（`dist/rocknix`） |
| `emuelec` | EmuELEC         | glibc | systemd | buildroot 交叉工具鏈（`dist/emuelec`） |

源碼基底來自 `es4armbian`（EmuELEC/emuelec-emulationstation 的分化 fork）。
原 es4armbian 的詳細改動筆記見 [`README_es4armbian.md`](README_es4armbian.md)。

## 分歧開關：target → 能力旗標

程式碼**不**直接用 target 名做 `#if`，而是先由 CMake 把 target 翻成一組「能力旗標」，
程式只認能力、保留合併機會（例：三邊皆 systemd，重開機邏輯共用一份）：

```
-DES4ALL_TARGET=armbian|rocknix|emuelec
   → -DES4ALL_TARGET_<NAME>=1
   → -DES4ALL_INIT_SYSTEMD=1        # 三邊皆開
   → -DES4ALL_PATHS_<NAME>=1        # 唯一真正三分：設定檔/ROM/retroarch.cfg 路徑
   → -DES4ALL_BUILD_BUILDROOT=1     # rocknix / emuelec
```

三邊的實質差異幾乎只剩「路徑」一個維度（init、libc、渲染 GLES2 皆一致）。

## 目錄

- `es-core/` `es-app/` `locale/` … ES 源碼本體（跨 target 共用）
- `dist/armbian/` — 直接 CMake 編，含啟動腳本
- `dist/rocknix/` — ROCKNIX buildroot 打包食譜（`package.mk` 等膠水）
- `dist/emuelec/` — EmuELEC buildroot 打包食譜（`package.mk` 等膠水）

`dist/*/package.mk` 的 `PKG_URL` 已指向本倉庫；首次 push 後需把 `PKG_VERSION`
更新為 es4all `main` 的 HEAD commit。

## 編譯（armbian）

在 VM `bookworm-arm64` chroot：

```bash
cmake -DES4ALL_TARGET=armbian -DGLES=OFF -DGLES2=ON -DENABLE_EMUELEC=1 -DCEC=OFF .
make -j4
```
