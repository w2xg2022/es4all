# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2019-present Shanti Gilbert (https://github.com/shantigilbert)

# es4all: 源碼改由統一倉庫 es4all 提供（原 EmuELEC/emuelec-emulationstation）。
# TODO: es4all 首次 push 後，把 PKG_VERSION 更新為 es4all main 的 HEAD commit。
PKG_NAME="emuelec-emulationstation"
PKG_VERSION="HEAD"
PKG_GIT_CLONE_BRANCH="main"
PKG_REV="1"
PKG_ARCH="any"
PKG_LICENSE="GPL"
PKG_SITE="https://github.com/w2xg2022/es4all"
PKG_URL="${PKG_SITE}.git"
PKG_DEPENDS_TARGET="toolchain SDL2 freetype freeimage vlc rapidjson ${OPENGLES} SDL2_mixer fping p7zip espeak"
PKG_SECTION="emuelec"
PKG_SHORTDESC="Emulationstation emulator frontend"
PKG_BUILD_FLAGS="-gold"
GET_HANDLER_SUPPORT="git"


if [[ ${DEVICE} == "OdroidGoAdvance"  ]] || [[ ${DEVICE} == "GameForce"  ]]; then 
	PKG_PATCH_DIRS="Rockchip/HH"
fi

if [[ ${DEVICE} == "OdroidM1"  ]] || [[ ${DEVICE} == "RK356x"  ]]; then 
	PKG_PATCH_DIRS="Rockchip"
fi

# themes for Emulationstation
PKG_DEPENDS_TARGET="${PKG_DEPENDS_TARGET} Crystal es-theme-alekfull-EmueELEC"

pre_configure_target() {

# build directly in ${PKG_BUILD} to avoid Python3 errors
  cd ${PKG_BUILD}
  rm -rf .${TARGET_NAME}

# 用w2xg2022/es4armbian已校正的繁中/簡中翻譯覆蓋預設(較不準確)翻譯，
# 編譯時CMake會用msgfmt把.po重新編成.mo
  curl -sL -o locale/lang/zh_CN/LC_MESSAGES/emulationstation2.po \
    https://raw.githubusercontent.com/w2xg2022/es4armbian/main/locale/lang/zh_CN/LC_MESSAGES/emulationstation2.po
  curl -sL -o locale/lang/zh_TW/LC_MESSAGES/emulationstation2.po \
    https://raw.githubusercontent.com/w2xg2022/es4armbian/main/locale/lang/zh_TW/LC_MESSAGES/emulationstation2.po

# NOTE(w2xg2022): 語言選單裡zh_TW的顯示名稱是寫死在GuiMenu.cpp的C++字串常數，
# 不是.po翻譯檔的內容(跟上面.po下載是兩回事)，"正體中文"應改為"繁體中文"。
  sed -i 's/"正體中文"/"繁體中文"/' es-app/src/guis/GuiMenu.cpp

# NOTE(w2xg2022): 手把設定介面(GuiInputConfig.cpp的_ENABLEEMUELEC分支，這是
# 實際編譯生效的那組，不是#else那組)原本a/b/x/y對應的方位標籤是任天堂式
# (A=右/B=下/X=上/Y=左)，跟es4armbian踩過的坑一樣，跟用戶實際Xbox式印刷布局
# 手把(物理A=下/B=右/X=左/Y=上，用/dev/input/js0原始位元組實測確認)相反，
# 設定手把時對不上。改成跟es4armbian一致的Xbox式標籤：a=SOUTH/b=EAST/
# x=WEST/y=NORTH。用行首按鍵名稱(a/b/x/y各自唯一)當比對依據，避免replace
# 順序互相污染。
  sed -i \
    -e 's|{ "a",                false, "BUTTON A / EAST",    ":/help/buttons_east.svg" }|{ "a",                false, "BUTTON A / SOUTH",   ":/help/buttons_south.svg" }|' \
    -e 's|{ "b",                true,  "BUTTON B / SOUTH",   ":/help/buttons_south.svg" }|{ "b",                true,  "BUTTON B / EAST",    ":/help/buttons_east.svg" }|' \
    -e 's|{ "x",                true,  "BUTTON X / NORTH",   ":/help/buttons_north.svg" }|{ "x",                true,  "BUTTON X / WEST",    ":/help/buttons_west.svg" }|' \
    -e 's|{ "y",                true,  "BUTTON Y / WEST",    ":/help/buttons_west.svg" }|{ "y",                true,  "BUTTON Y / NORTH",   ":/help/buttons_north.svg" }|' \
    es-core/src/guis/GuiInputConfig.cpp

# NOTE(w2xg2022): 新增ES設定畫面開關"InvertGameButtons"(跟既有InvertButtons
# 同一個畫面)，讓使用者可以動態切換遊戲內A/B、X/Y對調(位置對齊，PS/PSP等
# 幾何符號系統手感正確)，而不是寫死。這個bool設定由setsettings.sh在每次
# 啟動遊戲前讀取，動態產生/移除對應core的remap檔案(見emuelec/bin/setsettings.sh
# 的InvertGameButtons區塊)。
# 用python3做精確多行區塊插入(比對整段唯一字串，比sed逐行比對更不怕排版差異，
# 且assert比對失敗時會讓build直接報錯，不會像sed比對不到時悄悄不生效)。
  python3 - <<'PYEOF'
anchor = '''	auto invertJoy = std::make_shared<SwitchComponent>(mWindow);
	invertJoy->setState(Settings::getInstance()->getBool("InvertButtons"));
	s->addWithDescription(_("SWITCH CONFIRM & CANCEL BUTTONS IN EMULATIONSTATION"), _("Switches the South and East buttons' functionality"), invertJoy);
	s->addSaveFunc([this, s, invertJoy]
	{
		if (Settings::getInstance()->setBool("InvertButtons", invertJoy->getState()))
		{
			InputConfig::AssignActionButtons();
			s->setVariable("reloadAll", true);
		}
	});
'''

addition = '''
	auto invertGameButtons = std::make_shared<SwitchComponent>(mWindow);
	invertGameButtons->setState(Settings::getInstance()->getBool("InvertGameButtons"));
	s->addWithDescription(_("SWITCH A/B, X/Y BUTTONS IN GAMES"), _("Swaps A/B and X/Y RetroPad bindings during gameplay (position-aligned for PlayStation/PSP style symbol layouts)"), invertGameButtons);
	s->addSaveFunc([invertGameButtons]
	{
		Settings::getInstance()->setBool("InvertGameButtons", invertGameButtons->getState());
	});
'''

path = "es-app/src/guis/GuiMenu.cpp"
with open(path, "r", encoding="utf-8") as f:
    content = f.read()
assert content.count(anchor) == 1, "InvertButtons anchor block not found or not unique in GuiMenu.cpp"
content = content.replace(anchor, anchor + addition)
with open(path, "w", encoding="utf-8") as f:
    f.write(content)

path2 = "es-core/src/Settings.cpp"
with open(path2, "r", encoding="utf-8") as f:
    content2 = f.read()
old = '\tmBoolMap["InvertButtons"] = false;\n'
assert content2.count(old) == 1, "InvertButtons default line not found or not unique in Settings.cpp"
content2 = content2.replace(old, old + '\tmBoolMap["InvertGameButtons"] = true;\n')
with open(path2, "w", encoding="utf-8") as f:
    f.write(content2)
PYEOF

PKG_CMAKE_OPTS_TARGET=" -DES4ALL_TARGET=emuelec -DENABLE_EMUELEC=1 -DDISABLE_KODI=1 -DENABLE_FILEMANAGER=1 -DGLES2=1 -DENABLE_TTS=1"

# Read api_keys.txt if it exist to add the required keys for cheevos, thegamesdb and screenscrapper. You need to get your own API keys. 
# File should be in this format
# -DSCREENSCRAPER_DEV_LOGIN=devid=<devusername>&devpassword=<devpassword> 
# -DGAMESDB_APIKEY=<gamesdbapikey>
# -DCHEEVOS_DEV_LOGIN=z=<yourusername>&y=<yourapikey>
# and it should be placed next to this file

if [ -f ${PKG_DIR}/api_keys.txt ]; then
while IFS="" read -r p || [ -n "${p}" ]
do
  PKG_CMAKE_OPTS_TARGET+=" ${p}"
done < ${PKG_DIR}/api_keys.txt
fi

if [[ ${DEVICE} == "GameForce" ]]; then
PKG_CMAKE_OPTS_TARGET+=" -DENABLE_GAMEFORCE=1"
fi

if [[ ${DEVICE} == "OdroidGoAdvance"  ]]; then
PKG_CMAKE_OPTS_TARGET+=" -DODROIDGOA=1"
fi

}

makeinstall_target() {

	mkdir -p ${INSTALL}/usr/config/emuelec/configs/locale/i18n/charmaps
	cp -rf ${PKG_BUILD}/locale/lang/* ${INSTALL}/usr/config/emuelec/configs/locale/
	cp -PR "$(get_build_dir glibc)/localedata/charmaps/UTF-8" ${INSTALL}/usr/config/emuelec/configs/locale/i18n/charmaps/UTF-8
	
	mkdir -p ${INSTALL}/usr/lib
	ln -sf /storage/.config/emuelec/configs/locale ${INSTALL}/usr/lib/locale
	
	mkdir -p ${INSTALL}/usr/config/emulationstation/resources
    cp -rf ${PKG_BUILD}/resources/* ${INSTALL}/usr/config/emulationstation/resources/
    
    mkdir -p ${INSTALL}/usr/bin
    ln -sf /storage/.config/emulationstation/resources ${INSTALL}/usr/bin/resources
    cp -rf ${PKG_BUILD}/emulationstation ${INSTALL}/usr/bin
    cp -PR "$(get_build_dir glibc)/.${TARGET_NAME}/locale/localedef" ${INSTALL}/usr/bin

	mkdir -p ${INSTALL}/etc/emulationstation/
	ln -sf /storage/.config/emulationstation/themes ${INSTALL}/etc/emulationstation/
   
	mkdir -p ${INSTALL}/usr/config/emulationstation
	cp -rf ${PKG_DIR}/config/scripts ${INSTALL}/usr/config/emulationstation
	cp -rf ${PKG_DIR}/config/*.cfg ${INSTALL}/usr/config/emulationstation
	cp -rf ${PKG_DIR}/config/resources ${INSTALL}/usr/config/emulationstation/

	# Generate es_systems.cfg from json
	python3 ${PKG_DIR}/generate_es_systems.py -i ${PKG_DIR}/config/es_systems.json -o ${INSTALL}/usr/config/emulationstation/es_systems.cfg -b manufacturer

	chmod +x ${INSTALL}/usr/config/emulationstation/scripts/*
	chmod +x ${INSTALL}/usr/config/emulationstation/scripts/configscripts/*
	find ${INSTALL}/usr/config/emulationstation/scripts/ -type f -exec chmod o+x {} \; 
	
	# Vertical Games are only supported in the OdroidGoAdvance
    if [[ ${DEVICE} != "OdroidGoAdvance" ]]; then
        sed -i "s|, vertical||g" "${INSTALL}/usr/config/emulationstation/es_features.cfg"
    fi
	
	# Amlogic project has an issue with mixed audio
    if [[ "${DEVICE}" == "Amlogic-old" ]]; then
        sed -i "s|</config>|	<bool name=\"StopMusicOnScreenSaver\" value=\"false\" />\n</config>|g" "${INSTALL}/usr/config/emulationstation/es_settings.cfg"
    fi

    if [[ "${DEVICE}" == "OdroidGoAdvance" ]] || [[ "${DEVICE}" == "GameForce" ]]; then
        sed -i "s|<\/config>|	<string name=\"GamelistViewStyle\" value=\"Small Screen\" />\n<\/config>|g" "${INSTALL}/usr/config/emulationstation/es_settings.cfg"
        sed -i "s|value=\"panel\" />|value=\"small panel\" />|g" "${INSTALL}/usr/config/emulationstation/es_settings.cfg"
    fi
    
    if  [[ "${DEVICE}" == "GameForce" ]]; then
    	mkdir -p ${INSTALL}/usr/config/emulationstation/themesettings
        sed -i "s|<\/config>|	<string name=\"subset.ratio\" value=\"43\" />\n<\/config>|g" "${INSTALL}/usr/config/emulationstation/es_settings.cfg"
        echo "subset.ratio=43" > ${INSTALL}/usr/config/emulationstation/themesettings/Crystal.cfg
    fi

# Remove unused cores
CORESFILE="${INSTALL}/usr/config/emulationstation/es_systems.cfg"

if [[ "${DEVICE}" != "Amlogic-ng" || "${DEVICE}" != "Amlogic-ne" || "${DEVICE}" != "Amlogic-no" ]]; then
    if [[ ${DEVICE} == "OdroidGoAdvance" || "${DEVICE}" == "GameForce" ]]; then
        remove_cores="mesen-s quicknes mame2016 mesen"
    elif [ "${DEVICE}" == "Amlogic-old" ]; then
        remove_cores="mesen-s quicknes mame2016 mesen yabasanshiroSA yabasanshiro"
        xmlstarlet ed -L -P -d "/systemList/system[name='saturn']" ${CORESFILE}
        xmlstarlet ed -L -P -d "/systemList/system[name='philips-cdi']" ${CORESFILE}
        xmlstarlet ed -L -P -d "/systemList/system/emulators/emulator[@name='Duckstation']" ${CORESFILE}
    fi
    
    for discore in ${remove_cores}; do
        sed -i "s|<core>${discore}</core>||g" ${CORESFILE}
        sed -i '/^[[:space:]]*$/d' ${CORESFILE}
    done
fi

# Remove Retrorun For unsupported devices
if [[ ${DEVICE} != "OdroidGoAdvance" ]] && [[ "${DEVICE}" != "GameForce" ]]; then
	xmlstarlet ed -L -P -d "/systemList/system/emulators/emulator[@name='retrorun']" ${CORESFILE}
else
	# remove duckstation for the OGA/GF
	xmlstarlet ed -L -P -d "/systemList/system/emulators/emulator[@name='Duckstation']" ${CORESFILE}

	# set parallel_n64_32b as default for handhelds
	sed -i "s|<core default=\"true\">mupen64plus_next</core>|<core>mupen64plus_next</core>|g" ${CORESFILE}
	sed -i "s|<core>parallel_n64_32b</core>|<core default=\"true\">parallel_n64_32b</core>|g" ${CORESFILE}
fi

}

post_install() {  
	enable_service emustation.service
	mkdir -p ${INSTALL}/usr/share
	ln -sf /storage/.config/emuelec/configs/locale ${INSTALL}/usr/share/locale
	mkdir -p ${INSTALL}/usr/bin/batocera/
	ln -sf /usr/bin/7zr ${INSTALL}/usr/bin/batocera/7zr
}
