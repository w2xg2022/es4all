#pragma once
#ifndef ES_APP_EMULATION_STATION_H
#define ES_APP_EMULATION_STATION_H

// These numbers and strings need to be manually updated for a new version.
// Do this version number update as the very last commit for the new release version.
#define PROGRAM_VERSION_MAJOR        42
#define PROGRAM_VERSION_MINOR        0
#define PROGRAM_VERSION_MAINTENANCE  0
#define PROGRAM_VERSION_STRING		"1.1pre"

// es4all: ★这里刻意不用 __DATE__ / __TIME__★
//   自我更新改用「binary 的 md5」当构建指纹(见 Es4allUpdate)，前提是**相同源码要编出相同 binary**。
//   __DATE__/__TIME__ 会让每次编译的产物都不同 -> md5 永远不同 -> 每次建置三个 target 都被判定
//   「有新版」，即使该 target 的代码一个字都没改(实测: 调整 ROCKNIX 专属选单时 EmuELEC 也被强迫提示)。
//   已用两次云端建置(同 commit、不同 runner)验证: 移除时间戳与 ES4ALL_BUILD_SHA 后，
//   产物 md5 完全一致 -> 工具链本身是确定性的，此方案成立。
#define PROGRAM_BUILT_STRING PROGRAM_VERSION_STRING

#define RESOURCE_VERSION_STRING "42,0,0\0"
#define RESOURCE_VERSION PROGRAM_VERSION_MAJOR,PROGRAM_VERSION_MINOR,PROGRAM_VERSION_MAINTENANCE

#ifndef SCREENSCRAPER_SOFTNAME

#if BATOCERA
#define SCREENSCRAPER_SOFTNAME			"Batocera-Emulationstation"
#elif RETROBAT
#define SCREENSCRAPER_SOFTNAME			"Retrobat-Emulationstation"
#elif _ENABLEEMUELEC
#define SCREENSCRAPER_SOFTNAME			"EmuELEC-Emulationstation"
#else
#define SCREENSCRAPER_SOFTNAME			"Emulationstation"
#endif

#endif

#endif // ES_APP_EMULATION_STATION_H
