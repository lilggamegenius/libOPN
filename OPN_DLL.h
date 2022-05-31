// Header file

#ifndef __OPN_DLL_H__
#define __OPN_DLL_H__

#define OPNAPI	APIENTRY _stdcall

void OPNAPI SetOPNOptions(UINT32 OutSmplRate, UINT8 ResmplMode, UINT8 ChipSmplMode, UINT32 ChipSmplRate);
UINT8 OPNAPI OpenOPNDriver(UINT8 Chips);
void OPNAPI CloseOPNDriver(void);

void OPNAPI OPN_Write(UINT8 ChipID, UINT16 Register, UINT8 Data);
void OPNAPI OPN_Mute(UINT8 ChipID, UINT8 MuteMask);

void OPNAPI PlayDACSample(UINT8 ChipID, UINT32 DataSize, const UINT8* Data, UINT32 SmplFreq);
void OPNAPI SetDACFrequency(UINT8 ChipID, UINT32 SmplFreq);
void OPNAPI SetDACVolume(UINT8 ChipID, UINT16 Volume);	// 0x100 = 100%


// default Sample Rate: 44100 Hz

// Resampling Modes
#define OPT_RSMPL_HIGH		0x00	// high quality linear resampling [default]
#define OPT_RSMPL_LQ_DOWN	0x01	// low quality downsampling, high quality upsampling
#define OPT_RSMPL_LOW		0x02	// low quality resampling

// Chip Sample Rate Modes
#define OPT_CSMPL_NATIVE	0x00	// native chip sample rate [default]
#define OPT_CSMPL_HIGHEST	0x01	// highest sample rate (native or custom)
#define OPT_CSMPL_CUSTOM	0x02	// custom sample rate

#endif	// __OPN_DLL_H__
