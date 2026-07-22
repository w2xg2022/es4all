# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)

# es4all: 源码改由统一仓库 es4all 提供（原 ROCKNIX/emulationstation-next）。
PKG_NAME="emulationstation"
PKG_VERSION="85f618b28adba6b3645067cb551158d3c3ad717f"
PKG_GIT_CLONE_BRANCH="v1.1-dev"
PKG_LICENSE="GPL"
PKG_SITE="https://github.com/w2xg2022/es4all"
PKG_URL="${PKG_SITE}.git"
PKG_DEPENDS_TARGET="boost toolchain SDL2 freetype curl freeimage bash rapidjson SDL2_mixer fping p7zip alsa vlc drm_tool pugixml"
PKG_NEED_UNPACK="busybox"
PKG_LONGDESC="Emulationstation emulator frontend"
PKG_BUILD_FLAGS="-gold"
GET_HANDLER_SUPPORT="git"
PKG_PATCH_DIRS+="${DEVICE}"

if [ ! "${OPENGL}" = "no" ]; then
  PKG_DEPENDS_TARGET+=" ${OPENGL} glu"
  PKG_CMAKE_OPTS_TARGET+=" -DGL=1"
fi

if [ ! "${OPENGLES_SUPPORT}" = no ]; then
  PKG_DEPENDS_TARGET+=" ${OPENGLES}"
  PKG_CMAKE_OPTS_TARGET+=" -DGLES2=1"
fi

# es4all: 必须带 -DENABLE_EMUELEC=1。
# 它并非「启用 EmuELEC」的意思，而是 es4all 的完整功能集开关(源自 EmuELEC 分支的一批功能)，
# 三个 target 都要开。少了它, CloudSaves.cpp 会因 EmulatorFeatures::cloudsave 与
# GuiSaveState::loadGridAndCenter 未定义而编译失败 —— 本档原先缺此旗标, 用它做 buildroot
# 编译会直接挂掉。CI(.github/workflows/build.yml) 三个 job 皆已带此旗标。
PKG_CMAKE_OPTS_TARGET+=" -DES4ALL_TARGET=rocknix \
                         -DENABLE_EMUELEC=1 \
                         -DROCKNIX=1 \
                         -DDISABLE_KODI=1 \
                         -DENABLE_FILEMANAGER=0 \
                         -DCEC=0 \
                         -DENABLE_PULSE=1 \
                         -DUSE_SYSTEM_PUGIXML=1"

pre_configure_target() {
  for key in SCREENSCRAPER_DEV_LOGIN \
        GAMESDB_APIKEY \
        CHEEVOS_DEV_LOGIN
  do
    if [ -z "${!key}" ]
    then
      echo "WARNING: ${key} not declared, will not build support."
    else
      echo "USING: ${key} = ${!key}"
    fi
  done

  export DEVICE=$(echo ${DEVICE^^} | sed "s#-#_##g")
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/config/locale
  cp -rf ${PKG_BUILD}/locale/lang/* ${INSTALL}/usr/config/locale/

  # Pre-generate default (en_US.UTF-8) locale for lower-end devices to speed up first boot
  # This saves a minute or two on RK3326 in a cost of about 1 MB of SYSTEM size
  # Copy-paste of a locale generating part of es_settings script
  I18NPATH=$(get_install_dir glibc)/usr/share/i18n/locales/ \
    localedef --force --verbose --inputfile=en_US --charmap=UTF-8 \
    ${INSTALL}/usr/config/locale/en_US.UTF-8 || true

  mkdir -p ${INSTALL}/usr/config/emulationstation/resources
  cp -rf ${PKG_BUILD}/resources/* ${INSTALL}/usr/config/emulationstation/resources/
  rm -rf ${INSTALL}/usr/config/emulationstation/resources/logo.png

  mkdir -p ${INSTALL}/usr/bin
  cp ${PKG_BUILD}/es_settings ${INSTALL}/usr/bin
  chmod 0755 ${INSTALL}/usr/bin/es_settings

  cp ${PKG_BUILD}/start_es.sh ${INSTALL}/usr/bin
  chmod 0755 ${INSTALL}/usr/bin/start_es.sh

  cp ${PKG_BUILD}/serial_number_check ${INSTALL}/usr/bin
  chmod 0755 ${INSTALL}/usr/bin/serial_number_check

  # es4all: 音源输出切换(ES 平台设置 → AUDIO OUTPUT 调用它)。ROCKNIX 走 PipeWire，
  # 不能照抄 EmuELEC 改 asound.conf 的做法，原因见脚本头部说明。
  # ⚠️ 本文件是参考副本；真正生效的是 w2xg2022/rocknix 的
  #    projects/ROCKNIX/packages/ui/emulationstation/package.mk —— 那边也要加同样这两行。
  cp ${PKG_BUILD}/dist/rocknix/sources/es4all-setauddev ${INSTALL}/usr/bin
  chmod 0755 ${INSTALL}/usr/bin/es4all-setauddev

  # es4all: 视频模式(分辨率/刷新率)切换(ES 平台设置 → VIDEO MODE 调用它，含 --list 枚举)。
  # ROCKNIX 走 Wayland(sway)，必须用 wlr-randr 设 output mode；EmuELEC 那套写
  # /flash/EE_VIDEO_MODE + emuelec-utils resolutions 是 Amlogic 专属、在此无效。
  # ⚠️ 同上，w2xg2022/rocknix 那份 package.mk 也要加这两行，否则选单枚举不到模式、整项不显示。
  cp ${PKG_BUILD}/dist/rocknix/sources/es4all-setvideomode ${INSTALL}/usr/bin
  chmod 0755 ${INSTALL}/usr/bin/es4all-setvideomode

  mkdir -p ${INSTALL}/usr/bin
  #ln -sf /storage/.config/emulationstation/resources ${INSTALL}/usr/bin/resources
  cp -rf ${PKG_BUILD}/emulationstation ${INSTALL}/usr/bin

  mkdir -p ${INSTALL}/etc/emulationstation/
  ln -sf /storage/.config/emulationstation/themes ${INSTALL}/etc/emulationstation/

  cp -rf ${PKG_DIR}/config/common/*.cfg ${INSTALL}/usr/config/emulationstation

  # If we're not an emulation device, ES may still be installed so we need a default config.
  if [ "${EMULATION_DEVICE}" = "no" ] || \
     [ "${BASE_ONLY}" = "true" ]
  then
    cat <<EOF >${INSTALL}/usr/config/emulationstation/es_systems.cfg
<?xml version="1.0" encoding="UTF-8"?>
<systemList>
        <system>
                <name>tools</name>
                <fullname>Tools</fullname>
                <manufacturer>ROCKNIX</manufacturer>
                <release>2024</release>
                <hardware>system</hardware>
                <path>/storage/.config/modules</path>
                <extension>.sh</extension>
                <command>%ROM%</command>
                <platform>tools</platform>
                <theme>tools</theme>
        </system>
</systemList>
EOF
  fi

  ln -sf ${INSTALL}/usr/config/emulationstation/es_systems.cfg ${INSTALL}/etc/emulationstation/es_systems.cfg

  ln -sf /storage/.cache/system_timezone ${INSTALL}/etc/timezone

  #Delete all vulkan options from es_features when vulkan is not present
  if [ ! "${VULKAN_SUPPORT}" = "yes" ]
    then
      sed -i '/vulkan/d' ${INSTALL}/usr/config/emulationstation/es_features.cfg
  fi
}


post_install() {
  mkdir -p ${INSTALL}/usr/share
  ln -sf /storage/.config/locale ${INSTALL}/usr/share/locale

  mkdir -p ${INSTALL}/usr/lib
  ln -sf /usr/share/locale ${INSTALL}/usr/lib/locale

  ln -sf /usr/share/locale  ${INSTALL}/usr/config/emulationstation/locale
}
