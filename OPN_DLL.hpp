#pragma once

#include <cstdint>
// Define EXPORTED for any platform
#ifdef _WIN32
# ifdef WIN_EXPORT
#   define EXPORTED  __declspec( dllexport )
# else
#   define EXPORTED  __declspec( dllimport )
# endif
#else
# define EXPORTED
#endif

extern "C" {
EXPORTED void SetOPNOptions(uint32_t OutSmplRate, uint8_t ResmplMode, uint8_t ChipSmplMode, uint32_t ChipSmplRate);
EXPORTED uint8_t OpenOPNDriver(uint8_t Chips);
EXPORTED void CloseOPNDriver();

EXPORTED void OPN_Write(uint8_t ChipID, uint16_t Register, uint8_t Data);
EXPORTED void OPN_Mute(uint8_t ChipID, uint8_t MuteMask);

EXPORTED void PlayDACSample(uint8_t ChipID, uint32_t DataSize, const uint8_t *Data, uint32_t SmplFreq);
EXPORTED void SetDACFrequency(uint8_t ChipID, uint32_t SmplFreq);
EXPORTED void SetDACVolume(uint8_t ChipID, uint16_t Volume);    // 0x100 = 100%
}

struct CAUD_ATTR{
	uint32_t SmpRate;
	uint16_t Volume;
	uint8_t Resampler;        // Resampler Type: 00 - Old, 01 - Upsampling, 02 - Copy, 03 - Downsampling
	uint32_t SmpP;            // Current Sample (Playback Rate)
	uint32_t SmpLast;         // Sample Number Last
	uint32_t SmpNext;         // Sample Number Next
	WAVE_32BS LSmpl;        // Last Sample
	WAVE_32BS NSmpl;        // Next Sample
};

struct DAC_STATE{
	uint32_t DataSize;
	const uint8_t *Data;
	uint32_t Frequency;
	uint16_t Volume;

	uint32_t Delta;
	uint32_t SmplPos;
	uint32_t SmplFric;    // .16 Friction
};

// default Sample Rate: 44100 Hz

// Resampling Modes
enum class OPT_RSMPL : uint8_t{
	HIGH, // high quality linear resampling [default]
	LQ_DOWN,  // low quality downsampling, high quality upsampling
	LOW   // low quality resampling
};

// Chip Sample Rate Modes
enum class OPT_CSMPL : uint8_t{
	NATIVE,   // native chip sample rate [default]
	HIGHEST,  // highest sample rate (native or custom)
	CUSTOM    // custom sample rate
};
