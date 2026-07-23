#!/bin/sh
echo "########## 有 U 盘的开机数据 ##########"
echo "===== 总耗时 ====="
systemd-analyze 2>/dev/null
echo "===== 最慢 12 个单元 ====="
systemd-analyze blame 2>/dev/null | head -12
echo "===== 关键链 ====="
systemd-analyze critical-chain 2>/dev/null | head -12
echo ""
echo "===== 磁碟被认出的时间(★重点★) ====="
dmesg | grep -E "Attached SCSI|\[sd[ab]\].*blocks"
echo ""
echo "===== USB 错误/重试/超时 ====="
dmesg | grep -iE "usb.*(error|timeout|reset|retry|fail|not responding|device descriptor|unable to enumerate)" | head -20
echo "(空 = 没有 USB 层面的错误)"
echo ""
echo "===== autostart / automount / udev 的时间线 ====="
journalctl -b -o short-monotonic --no-pager 2>/dev/null | grep -E "rocknix-autostart|rocknix-automount|systemd-udev-settle|sway|essway|udevil" | head -25
echo ""
echo "===== 开机末段(rocknix.target 前后) ====="
journalctl -b -o short-monotonic --no-pager 2>/dev/null | tail -25
