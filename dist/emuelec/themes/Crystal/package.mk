# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2020-present Shanti Gilbert (https://github.com/shantigilbert)

PKG_NAME="Crystal"
PKG_VERSION="0e3c9e15768517cb80926850345a90fb24bd20f2"
PKG_REV="1"
PKG_ARCH="any"
PKG_LICENSE="GPL"
PKG_SITE="https://github.com/dm2912/Crystal"
PKG_URL="${PKG_SITE}.git"
PKG_DEPENDS_TARGET="toolchain Crystal-Collections"
PKG_SECTION="emuelec"
PKG_SHORTDESC="Crystal theme for EMUELEC by Dim (dm2912)"
PKG_TOOLCHAIN="manual"
GET_HANDLER_SUPPORT="git"

make_target() {
  : not
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/config/emulationstation/themes/Crystal
    cp -r * ${INSTALL}/usr/config/emulationstation/themes/Crystal
    rm -rf ${INSTALL}/usr/config/emulationstation/themes/Crystal/screens.png

# NOTE(w2xg2022): 主選單"EMUELEC設置"圖示(Emuelec.png)在實機上一直顯示空白，
# 其他項目(系統設置等用Advanced.png)都正常，跟ES4A遇到同個問題一樣，
# ES4A的解法是直接換掉圖示，這裡比照辦理改用已驗證沒問題的扳手圖示(Advanced.png)。
  sed -i 's|menu_icons/\${cmenuicons}/Emuelec.png|menu_icons/${cmenuicons}/Advanced.png|' \
    ${INSTALL}/usr/config/emulationstation/themes/Crystal/theme.xml
}
