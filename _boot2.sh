#!/bin/sh
echo "===== 磁碟被认出的时间 ====="
dmesg | grep -E "Attached SCSI|\[sd[ab]\] .*blocks"
echo ""
echo "===== USB 装置列举(含 hub) ====="
dmesg | grep -E "usb [0-9]-[0-9.]+: New USB device found|usb [0-9]-[0-9.]+: new .* USB device|hub .*: USB hub found" | head -20
echo ""
echo "===== 核心最后 15 行(看开机末尾卡在哪) ====="
dmesg | tail -15
echo ""
echo "===== rocknix-autostart 做了什么(耗时 7.2s) ====="
systemctl cat rocknix-autostart.service 2>/dev/null | head -20
echo "--- autostart 目录 ---"
ls /storage/.config/autostart/ 2>/dev/null
echo ""
echo "===== autostart 这次的日志(含时间) ====="
journalctl -u rocknix-autostart --no-pager -b -o short-monotonic 2>/dev/null | tail -20
echo ""
echo "===== 有没有 udevadm settle / 等待装置 ====="
grep -rn "udevadm\|settle\|sleep" /storage/.config/autostart/* 2>/dev/null | head -10
