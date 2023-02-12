#pragma once

#define OPNAPI  APIENTRY
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
EXPORTED void OPNAPI SetOPNOptions(UINT32 OutSmplRate, UINT8 ResmplMode, UINT8 ChipSmplMode, UINT32 ChipSmplRate);
EXPORTED UINT8 OPNAPI OpenOPNDriver(UINT8 Chips);
EXPORTED void OPNAPI CloseOPNDriver();

EXPORTED void OPNAPI OPN_Write(UINT8 ChipID, UINT16 Register, UINT8 Data);
EXPORTED void OPNAPI OPN_Mute(UINT8 ChipID, UINT8 MuteMask);

EXPORTED void OPNAPI PlayDACSample(UINT8 ChipID, UINT32 DataSize, const UINT8 *Data, UINT32 SmplFreq);
EXPORTED void OPNAPI SetDACFrequency(UINT8 ChipID, UINT32 SmplFreq);
EXPORTED void OPNAPI SetDACVolume(UINT8 ChipID, UINT16 Volume);    // 0x100 = 100%
}

struct CAUD_ATTR{
	UINT32 SmpRate;
	UINT16 Volume;
	UINT8 Resampler;        // Resampler Type: 00 - Old, 01 - Upsampling, 02 - Copy, 03 - Downsampling
	UINT32 SmpP;            // Current Sample (Playback Rate)
	UINT32 SmpLast;         // Sample Number Last
	UINT32 SmpNext;         // Sample Number Next
	WAVE_32BS LSmpl;        // Last Sample
	WAVE_32BS NSmpl;        // Next Sample
};

struct DAC_STATE{
	UINT32 DataSize;
	const UINT8 *Data;
	UINT32 Frequency;
	UINT16 Volume;

	UINT32 Delta;
	UINT32 SmplPos;
	UINT32 SmplFric;    // .16 Friction
};

// default Sample Rate: 44100 Hz

// Resampling Modes
enum OPT_RSMPL{
	OPT_RSMPL_HIGH, // high quality linear resampling [default]
	OPT_RSMPL_LQ_DOWN,  // low quality downsampling, high quality upsampling
	OPT_RSMPL_LOW   // low quality resampling
};

// Chip Sample Rate Modes
enum OPT_CSMPL{
	OPT_CSMPL_NATIVE,   // native chip sample rate [default]
	OPT_CSMPL_HIGHEST,  // highest sample rate (native or custom)
	OPT_CSMPL_CUSTOM    // custom sample rate
};
