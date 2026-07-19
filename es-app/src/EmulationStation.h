#pragma once
#ifndef ES_APP_EMULATION_STATION_H
#define ES_APP_EMULATION_STATION_H

// These numbers and strings need to be manually updated for a new version.
// Do this version number update as the very last commit for the new release version.
#define PROGRAM_VERSION_MAJOR        42
#define PROGRAM_VERSION_MINOR        0
#define PROGRAM_VERSION_MAINTENANCE  0
#define PROGRAM_VERSION_STRING		"1.1pre"

#define PROGRAM_BUILT_STRING __DATE__ " - " __TIME__

// es4all: 构建指纹(CI 注入的 git commit SHA)。见 CMakeLists.txt。
// 本地手动编译或未定义时为空字串；自我更新据此退化为纯版本号比较。
#ifndef ES4ALL_BUILD_SHA
#define ES4ALL_BUILD_SHA ""
#endif

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
