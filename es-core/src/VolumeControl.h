#pragma once
#ifndef ES_APP_VOLUME_CONTROL_H
#define ES_APP_VOLUME_CONTROL_H

#include <memory>

#if defined (__APPLE__)
    #error TODO: Not implemented for MacOS yet!!!
#elif defined(__linux__)
	#include <unistd.h>
	#include <fcntl.h>
	#include <alsa/asoundlib.h>
#elif defined(WIN32) || defined(_WIN32)
	#include <Windows.h>
	#include <endpointvolume.h>
	#include <mmeapi.h>
#endif

/*!
Singleton pattern. Call getInstance() to get an object.
*/
class VolumeControl
{
#if defined (__APPLE__)
    #error TODO: Not implemented for MacOS yet!!!
#elif defined(__linux__)
    static std::string mixerName;
    static std::string mixerCard;
    int mixerIndex;
    snd_mixer_t* mixerHandle;
    snd_mixer_elem_t* mixerElem;
    snd_mixer_selem_id_t* mixerSelemId;
#elif defined(WIN32) || defined(_WIN32)
	HMIXER mixerHandle;
	MIXERCONTROL mixerControl;
	IAudioEndpointVolume * endpointVolume;
#endif

// es4all: ★出厂预设音量, 三个 target(armbian/emuelec/rocknix)共用★
//   刻意写死在程式里、不走 profile: 这跟机型无关(是「第一次开机听到多大声」的
//   通用体验), 放进 profile 只会变成每台都要重复写一份的样板。
//   使用者调过音量之后 audio.volume 就有值了, 这个数字从此不再参与。
#define ES4ALL_DEFAULT_VOLUME 80

void applyInitialVolumeFromConfig();
	
	int internalVolume;

	static std::weak_ptr<VolumeControl> sInstance;

	VolumeControl();
	VolumeControl(const VolumeControl & right);
    VolumeControl & operator=(const VolumeControl & right);	

public:
	static std::shared_ptr<VolumeControl> & getInstance();

	void init();
	void deinit();

	bool isAvailable();

	int getVolume() const;
	void setVolume(int volume);

	~VolumeControl();
};

#endif // ES_APP_VOLUME_CONTROL_H
