#pragma once
// Define EXPORTED for any platform
#ifdef BUILD_STATIC_LIB
# define EXPORTED
#else
# if (defined __WIN32__) || (defined _WIN32) || defined(__CYGWIN__)
#  ifdef __GNUC__
#   define EXPORTED   __attribute__((dllexport))
#  else
#   define EXPORTED   __declspec(dllexport)
#  endif
# else
#  define EXPORTED    __attribute__((visibility ("default")))
# endif
#endif

#include <cstdint>

// default Sample Rate: 44100 Hz
#ifdef __cplusplus
// Return codes for OpenOPNDriver
enum class DriverReturnCode : uint8_t {
	Success = 0,
	ZeroChipCount = 0x80,
	TooManyChips = 0xFF,
	SoundDeviceError = 0xC0,

};
// Resampling Modes
enum class ResampleMode : uint8_t{
	HIGH, // high quality linear resampling [default]
	LQ_DOWN,  // low quality downsampling, high quality upsampling
	LOW   // low quality resampling
};

// Chip Sample Rate Modes
enum class ChipSampleMode : uint8_t{
	NATIVE,   // native chip sample rate [default]
	HIGHEST,  // highest sample rate (native or custom)
	CUSTOM    // custom sample rate
};
#else
// Return codes for OpenOPNDriver
enum DriverReturnCode : uint8_t {
	DriverReturnCode_Success = 0,
	DriverReturnCode_ZeroChipCount = 0x80,
	DriverReturnCode_TooManyChips = 0xFF,
	DriverReturnCode_SoundDeviceError = 0xC0,

};
// Resampling Modes
enum ResampleMode : uint8_t{
	ResampleMode_HIGH, // high quality linear resampling [default]
	ResampleMode_LQ_DOWN,  // low quality downsampling, high quality upsampling
	ResampleMode_LOW   // low quality resampling
};

// Chip Sample Rate Modes
enum ChipSampleMode : uint8_t{
	ChipSampleMode_NATIVE,   // native chip sample rate [default]
	ChipSampleMode_HIGHEST,  // highest sample rate (native or custom)
	ChipSampleMode_CUSTOM    // custom sample rate
};
#endif

extern "C" {
#include "src/audio/Stream.h"

EXPORTED void SetOPNOptions(uint32_t OutSmplRate, uint8_t ResmplMode, uint8_t ChipSmplMode, uint32_t ChipSmplRate);
EXPORTED DriverReturnCode OpenOPNDriver(uint8_t Chips);
EXPORTED void CloseOPNDriver();

EXPORTED void OPN_Write(uint8_t ChipID, uint16_t Register, uint8_t Data);
EXPORTED void OPN_Mute(uint8_t ChipID, uint8_t MuteMask);

EXPORTED void PlayDACSample(uint8_t ChipID, uint32_t DataSize, const uint8_t *Data, uint32_t SmplFreq);
EXPORTED void SetDACFrequency(uint8_t ChipID, uint32_t SmplFreq);
EXPORTED void SetDACVolume(uint8_t ChipID, uint16_t Volume);    // 0x100 = 100%
}

struct ChipAudioAttributes{
	uint32_t SmpRate;
	uint16_t Volume;
	uint8_t Resampler;        // Resampler Type: 00 - Old, 01 - Upsampling, 02 - Copy, 03 - Downsampling
	uint32_t SmpP;            // Current Sample (Playback Rate)
	uint32_t SmpLast;         // Sample Number Last
	uint32_t SmpNext;         // Sample Number Next
	WAVE_32BS LSmpl;        // Last Sample
	WAVE_32BS NSmpl;        // Next Sample
};

struct DACState{
	uint32_t DataSize;
	const uint8_t *Data;
	uint32_t Frequency;
	uint16_t Volume;

	uint32_t Delta;
	uint32_t SmplPos;
	uint32_t SmplFric;    // .16 Friction
};