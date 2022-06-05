
#pragma once

#define OPNAPI  APIENTRY

extern "C" {
void OPNAPI SetOPNOptions(UINT32 OutSmplRate, UINT8 ResmplMode, UINT8 ChipSmplMode, UINT32 ChipSmplRate);
UINT8 OPNAPI OpenOPNDriver(UINT8 Chips);
void OPNAPI CloseOPNDriver();

void OPNAPI OPN_Write(UINT8 ChipID, UINT16 Register, UINT8 Data);
void OPNAPI OPN_Mute(UINT8 ChipID, UINT8 MuteMask);

void OPNAPI PlayDACSample(UINT8 ChipID, UINT32 DataSize, const UINT8 *Data, UINT32 SmplFreq);
void OPNAPI SetDACFrequency(UINT8 ChipID, UINT32 SmplFreq);
void OPNAPI SetDACVolume(UINT8 ChipID, UINT16 Volume);    // 0x100 = 100%
}


// default Sample Rate: 44100 Hz

// Resampling Modes
enum OPT_RSMPL {
	OPT_RSMPL_HIGH, // high quality linear resampling [default]
	OPT_RSMPL_LQ_DOWN,  // low quality downsampling, high quality upsampling
	OPT_RSMPL_LOW   // low quality resampling
};

// Chip Sample Rate Modes
enum OPT_CSMPL {
	OPT_CSMPL_NATIVE,   // native chip sample rate [default]
	OPT_CSMPL_HIGHEST,  // highest sample rate (native or custom)
	OPT_CSMPL_CUSTOM    // custom sample rate
};
