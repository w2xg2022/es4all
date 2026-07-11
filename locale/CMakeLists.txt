add_custom_target (i18n ALL COMMENT "Building i18n messages.")
file (GLOB es_PO_FILES ${dir}/locale/lang/*)
set(es_POT ${dir}/locale/emulationstation2.pot)
# update the pot
MESSAGE("Building the pot file")
add_custom_command (TARGET i18n COMMAND find "${dir}/es-app" "${dir}/es-core" -name "*.cpp" -o -name "*.h" | ${XGETTEXT_EXECUTABLE} --from-code=UTF-8 --add-comments=TRANSLATION -f - -o ${es_POT} --no-location --keyword=_)
add_custom_command (TARGET i18n COMMAND sed -i '/^\"POT-Creation-Date:/d' ${es_POT})

foreach (es_PO_INPUT ${es_PO_FILES})
        get_filename_component (es_PO_LANG ${es_PO_INPUT} NAME_WE)
	MESSAGE("LANG = ${es_PO_LANG}")
	set (es_MO_OUTPUT ${es_PO_INPUT}/LC_MESSAGES/emulationstation2.mo)
	# NOTE(w2xg2022): 原本这里 msgmerge -U 拿 xgettext 重生的 .pot 更新 .po——但 xgettext
	# 只抓 _() 的字面 msgid，抓不到用 pgettext("joystick", 变量) 显示的手柄标签(BUTTON A /
	# SOUTH、HOTKEY ENABLE 等，msgid 是运行时变量)，会把这些手动维护的条目标成过时(#~)、
	# 从 .mo 剥掉，导致手柄配置界面翻译丢失。故停用 msgmerge，直接把仓库维护好的 .po 编成
	# .mo(下一行)。新增源码字符串时手动加进 .po 即可。
	# add_custom_command (TARGET i18n COMMAND ${MSGMERGE_EXECUTABLE} -U --no-fuzzy-matching ${es_PO_INPUT}/LC_MESSAGES/emulationstation2.po ${es_POT})
	# compile the po to mo
        add_custom_command (TARGET i18n COMMAND basename ${es_PO_INPUT} && ${MSGFMT_EXECUTABLE} -o ${es_MO_OUTPUT} ${es_PO_INPUT}/LC_MESSAGES/emulationstation2.po)
        install (FILES ${es_MO_OUTPUT} DESTINATION share/locale/${es_PO_LANG}/LC_MESSAGES RENAME emulationstation2.mo)
endforeach (es_PO_INPUT ${es_PO_FILES})

add_custom_command (TARGET i18n COMMAND echo)
foreach (es_PO_INPUT ${es_PO_FILES})
  add_custom_command (TARGET i18n COMMAND printf "%-5s " `basename ${es_PO_INPUT}` && LANG=C ${MSGFMT_EXECUTABLE} -o /dev/null ${es_PO_INPUT}/LC_MESSAGES/emulationstation2.po --statistics)
endforeach (es_PO_INPUT ${es_PO_FILES})
add_custom_command (TARGET i18n COMMAND echo)
