# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2019-present Shanti Gilbert (https://github.com/shantigilbert)

PKG_NAME="es-theme-alekfull-EmueELEC"
PKG_VERSION="08315fbf1e317ec67ff1f35896e6a6f6cf4f0989"
PKG_SHA256="97731e8ec99ecee87888a07cebd882363d2326765546b69f26069c3dce4197ff"
PKG_REV="1"
PKG_ARCH="any"
PKG_LICENSE="GPL"
PKG_SITE="https://github.com/EmuELEC/es-theme-alekfull-EmueELEC"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_SECTION="emuelec"
PKG_SHORTDESC="The EmulationStation theme Alekfull for EmuELEC"
PKG_IS_ADDON="no"
PKG_AUTORECONF="no"
PKG_TOOLCHAIN="manual"

make_target() {
  : not
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/config/emulationstation/themes/es-theme-alekfull-EmueELEC
    cp -r * ${INSTALL}/usr/config/emulationstation/themes/es-theme-alekfull-EmueELEC

# NOTE(w2xg2022): 這是實際生效的預設主題(不是Crystal，Crystal只是依賴包但沒被選用)，
# 主菜單"EMUELEC設置"圖示一直空白是因為這個主題的menuicons.xml根本沒定義iconEmuelec，
# 不是圖檔問題。Emuelec.png(從Crystal主題的彩色logo用ImageMagick轉成白色單色剪影，
# 保留原本alpha透明度)是這個package.mk目錄裡的本地檔案，跟其他純白圖示風格一致，
# 套用colorShift才不會變成黑白反色。
  cp ${PKG_DIR}/Emuelec.png ${INSTALL}/usr/config/emulationstation/themes/es-theme-alekfull-EmueELEC/_inc/icons/Emuelec.png
  sed -i 's|<iconAdvanced>./Advanced.png</iconAdvanced>|<iconAdvanced>./Advanced.png</iconAdvanced>\n            <iconEmuelec>./Emuelec.png</iconEmuelec>|' \
    ${INSTALL}/usr/config/emulationstation/themes/es-theme-alekfull-EmueELEC/_inc/icons/menuicons.xml
}
