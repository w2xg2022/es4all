#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2024 ROCKNIX (https://github.com/ROCKNIX)

### setup is the same
. $(dirname $0)/es_settings

# es4all: ★不要加回 --no-splash★
#   该参数在 main.cpp 的命令列解析里直接 `setBool("SplashScreen", false)`，而且发生在
#   **载入 es_settings.cfg 之后** —— 等于每次开机都把使用者的设定覆写成「关」。
#   症状：平台设置 → 启动画面设置 里把「启用启动加载画面」切成开、按返回（存档确实成功，
#   es_log 有 `Settings::saveFile()` 纪录、档案也确实更新），但一重开机就变回「关」，怎么设都记不住。
#   EmuELEC/Armbian 的启动脚本没带这个参数，所以**只有 ROCKNIX 中招**。
#   曾误判为 ES 的存档 bug，查了好几轮（delete s 绕过 save、saveMap 跳过预设值、载入端解析…）
#   全部无辜 —— 真凶是这一行。实机 .179 验证：移除后选单立刻能控制加载/退出两个开关。
#   现在 SPLASH 已做成选单项交给使用者控制，就不该再由启动脚本写死。
emulationstation --log-path /var/log
