#!/bin/bash
# es4all ROCKNIX 持久化钩子 (autostart 在 ES 启动前执行)
# 1) es4all EmulationStation 本体
ES4ALL_BIN="/storage/es4all-emulationstation"
if [ -f "${ES4ALL_BIN}" ] && ! mountpoint -q /usr/bin/emulationstation; then
    mount --bind "${ES4ALL_BIN}" /usr/bin/emulationstation
fi
# 2) input_sense: 退出热键改 SELECT+START (2键)
ES4ALL_INSENSE="/storage/es4all-input_sense"
if [ -f "${ES4ALL_INSENSE}" ] && ! mountpoint -q /usr/bin/input_sense; then
    mount --bind "${ES4ALL_INSENSE}" /usr/bin/input_sense
    systemctl restart input.service 2>/dev/null
fi
