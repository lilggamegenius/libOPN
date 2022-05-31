// OPN_DLL.c - DLL for realtime OPN playback
// Written by Valley Bell, 2011, 2014

#include "stdbool.h"
#include <malloc.h>
#include <memory.h>
#include <windows.h>

#include "mamedef.h"

#include "Stream.h"
#include "OPN_DLL.h"

#include "2612intf.h"

typedef struct chip_audio_attributes
{
	UINT32 SmpRate;
	UINT16 Volume;
	UINT8 Resampler;	// Resampler Type: 00 - Old, 01 - Upsampling, 02 - Copy, 03 - Downsampling
	UINT32 SmpP;		// Current Sample (Playback Rate)
	UINT32 SmpLast;		// Sample Number Last
	UINT32 SmpNext;		// Sample Number Next
	WAVE_32BS LSmpl;	// Last Sample
	WAVE_32BS NSmpl;	// Next Sample
} CAUD_ATTR;

typedef struct
{
	UINT32 DataSize;
	const UINT8* Data;
	UINT32 Frequency;
	UINT16 Volume;
	
	UINT32 Delta;
	UINT32 SmplPos;
	UINT32 SmplFric;	// .16 Friction
} DAC_STATE;


BOOL APIENTRY DllMain(HANDLE hModule, DWORD fdwReason, LPVOID lpReserved);
//void OPNAPI SetOPNOptions(UINT32 OutSmplRate, UINT8 ResmplMode, UINT8 ChipSmplMode, UINT32 ChipSmplRate);
//UINT8 OPNAPI OpenOPNDriver(UINT8 Chips);
static void InitChips(UINT8 ChipCount);
static void DeinitChips(void);
//void OPNAPI CloseOPNDriver(void);
void CloseOPNDriver_Unload(void);
//void OPNAPI OPN_Write(UINT8 ChipID, UINT16 Register, UINT8 Data);
//void OPNAPI OPN_Mute(UINT8 ChipID, UINT8 MuteMask);

INLINE INT16 Limit2Short(INT32 Value);
static void GetChipStream(UINT8 ChipID, UINT8 ChipNum, INT32** Buffer, UINT32 BufSize);
static void ResampleChipStream(UINT8 ChipID, WAVE_32BS* RetSample, UINT32 Length);
void FillBuffer(WAVE_16BS* Buffer, UINT32 BufferSize);

static void UpdateDAC(UINT8 ChipID, UINT32 Samples);
//void OPNAPI PlayDACSample(UINT8 ChipID, UINT32 DataSize, const UINT8* Data, UINT32 SmplFreq);
//void OPNAPI SetDACFrequency(UINT8 ChipID, UINT32 SmplFreq);
//void OPNAPI SetDACVolume(UINT8 ChipID, UINT16 Volume);


#define YM2612_CLOCK	7670454
#define MAX_CHIPS		0x10

UINT32 SampleRate;	// Note: also used by some sound cores to determinate the chip sample rate

UINT8 ResampleMode;	// 00 - HQ both, 01 - LQ downsampling, 02 - LQ both
UINT8 CHIP_SAMPLING_MODE;	// 00 - native, 01 - highest (native/custom), 02 - custom (CHIP_SAMPLE_RATE)
INT32 CHIP_SAMPLE_RATE;


static UINT8 OPN_CHIPS;	// also indicates, if DLL is running

CAUD_ATTR ChipAudio[MAX_CHIPS];
DAC_STATE DACState[MAX_CHIPS];

#define SMPL_BUFSIZE	0x100
static INT32* StreamBufs[0x02];
stream_sample_t* DUMMYBUF[0x02] = {NULL, NULL};

static UINT32 NullSamples;

static CRITICAL_SECTION write_sect;

BOOL APIENTRY DllMain(HANDLE hModule, DWORD fdwReason, LPVOID lpReserved)
{
	switch(fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		// Initialize once for each new process.
		// Return FALSE to fail DLL load.
		OPN_CHIPS = 0x00;
		
		SampleRate = 44100;
		ResampleMode = OPT_RSMPL_HIGH;
		CHIP_SAMPLING_MODE = OPT_CSMPL_NATIVE;
		CHIP_SAMPLE_RATE = SampleRate;
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		// Perform any necessary cleanup.
		if (OPN_CHIPS)
		{
			// a special function is called, as waveOutClose hangs the process at this point
			CloseOPNDriver_Unload();
		}
		break;
	}
	
	return TRUE; 
}

void OPNAPI SetOPNOptions(UINT32 OutSmplRate, UINT8 ResmplMode, UINT8 ChipSmplMode, UINT32 ChipSmplRate)
{
	SampleRate = OutSmplRate;
	ResampleMode = ResmplMode;
	CHIP_SAMPLING_MODE = ChipSmplMode;
	CHIP_SAMPLE_RATE = ChipSmplRate;
	
	return;
}

UINT8 OPNAPI OpenOPNDriver(UINT8 Chips)
{
	if (OPN_CHIPS)
		return 0x80;	// already running
	if (Chips > 0x10)
		return 0xFF;	// too many chips
	
	InitChips(Chips);
	
	SoundLogging(false);
	if (StartStream(0x00))
	{
		//printf("Error openning Sound Device!\n");
		CloseOPNDriver();
		return 0xC0;
	}
	
	NullSamples = 0xFFFFFFFF;
	PauseStream(true);
	
	return 0x00;
}

static void InitChips(UINT8 ChipCount)
{
	UINT8 CurChip;
	CAUD_ATTR* CAA;
	
	for (CurChip = 0x00; CurChip < MAX_CHIPS; CurChip ++)
	{
		CAA = &ChipAudio[CurChip];
		CAA->SmpRate = 0x00;
		CAA->Volume = 0x00;
		
		DACState[CurChip].Data = NULL;
	}
	
	InitializeCriticalSection(&write_sect);
	for (CurChip = 0x00; CurChip < ChipCount; CurChip ++)
	{
		CAA = &ChipAudio[CurChip];
		CAA->SmpRate = device_start_ym2612(CurChip, YM2612_CLOCK);
		CAA->Volume = 0x100;
		device_reset_ym2612(CurChip);
	}
	
	for (CurChip = 0x00; CurChip < ChipCount; CurChip ++)
	{
		CAA = &ChipAudio[CurChip];
		if (! CAA->SmpRate)
			CAA->Resampler = 0xFF;
		else if (CAA->SmpRate < SampleRate)
			CAA->Resampler = 0x01;
		else if (CAA->SmpRate == SampleRate)
			CAA->Resampler = 0x02;
		else if (CAA->SmpRate > SampleRate)
			CAA->Resampler = 0x03;
		if ((ResampleMode == OPT_RSMPL_LQ_DOWN && CAA->Resampler == 0x03) ||
			ResampleMode == OPT_RSMPL_LOW)
			CAA->Resampler = 0x00;
		
		CAA->SmpP = 0x00;
		CAA->SmpLast = 0x00;
		CAA->SmpNext = 0x00;
		CAA->LSmpl.Left = 0x00;
		CAA->LSmpl.Right = 0x00;
		if (CAA->Resampler == 0x01)
		{
			// Pregenerate first Sample (the upsampler is always one too late)
			GetChipStream(0x00, CurChip, StreamBufs, 1);
			CAA->NSmpl.Left = StreamBufs[0x00][0x00];
			CAA->NSmpl.Right = StreamBufs[0x01][0x00];
		}
		else
		{
			CAA->NSmpl.Left = 0x00;
			CAA->NSmpl.Right = 0x00;
		}
		
		DACState[CurChip].Data = NULL;
		DACState[CurChip].Volume = 0x100;
		DACState[CurChip].Frequency = 16000;
	}
	
	StreamBufs[0x00] = (INT32*)malloc(SMPL_BUFSIZE * sizeof(INT32));
	StreamBufs[0x01] = (INT32*)malloc(SMPL_BUFSIZE * sizeof(INT32));
	
	OPN_CHIPS = ChipCount;
	return;
}

static void DeinitChips(void)
{
	UINT8 CurChip;
	
	free(StreamBufs[0x00]);
	free(StreamBufs[0x01]);
	
	for (CurChip = 0x00; CurChip < OPN_CHIPS; CurChip ++)
		device_stop_ym2612(CurChip);
	DeleteCriticalSection(&write_sect);
	
	OPN_CHIPS = 0x00;
	return;
}


void OPNAPI CloseOPNDriver(void)
{
	StopStream(false);
	
	DeinitChips();
	
	return;
}

void CloseOPNDriver_Unload(void)
{
	StopStream(true);
	
	DeinitChips();
	
	return;
}

void OPNAPI OPN_Write(UINT8 ChipID, UINT16 Register, UINT8 Data)
{
	UINT8 RegSet;
	
	if (ChipID >= OPN_CHIPS)
		return;
	
	EnterCriticalSection(&write_sect);
	if (Register == 0x28 && (Data & 0xF0))
	{
		// Note On - Resume Stream
		NullSamples = 0;
		PauseStream(false);
	}
	
	if (NullSamples == 0xFFFFFFFF)	// if chip is paused, do safe update
		GetChipStream(0x00, ChipID, StreamBufs, 1);
	
	RegSet = Register >> 8;
	ym2612_w(ChipID, 0x00 | (RegSet << 1), Register & 0xFF);
	ym2612_w(ChipID, 0x01 | (RegSet << 1), Data);
	LeaveCriticalSection(&write_sect);
	
	return;
}

void OPNAPI OPN_Mute(UINT8 ChipID, UINT8 MuteMask)
{
	if (ChipID >= OPN_CHIPS)
		return;
	
	EnterCriticalSection(&write_sect);
	ym2612_set_mute_mask(ChipID, MuteMask);
	LeaveCriticalSection(&write_sect);
	
	return;
}


INLINE INT16 Limit2Short(INT32 Value)
{
	INT32 NewValue;
	
	NewValue = Value;
	if (NewValue < -0x8000)
		NewValue = -0x8000;
	if (NewValue > 0x7FFF)
		NewValue = 0x7FFF;
	
	return (INT16)NewValue;
}

INLINE void GetChipStream(UINT8 ChipID, UINT8 ChipNum, INT32** Buffer, UINT32 BufSize)
{
	ym2612_stream_update(ChipNum, Buffer, BufSize);
	
	return;
}

// I recommend 11 bits as it's fast and accurate
#define FIXPNT_BITS		11
#define FIXPNT_FACT		(1 << FIXPNT_BITS)
#if (FIXPNT_BITS <= 11)
	typedef UINT32	SLINT;	// 32-bit is a lot faster
#else
	typedef UINT64	SLINT;
#endif
#define FIXPNT_MASK		(FIXPNT_FACT - 1)

#define getfriction(x)	((x) & FIXPNT_MASK)
#define getnfriction(x)	((FIXPNT_FACT - (x)) & FIXPNT_MASK)
#define fpi_floor(x)	((x) & ~FIXPNT_MASK)
#define fpi_ceil(x)		((x + FIXPNT_MASK) & ~FIXPNT_MASK)
#define fp2i_floor(x)	((x) / FIXPNT_FACT)
#define fp2i_ceil(x)	((x + FIXPNT_MASK) / FIXPNT_FACT)

static void ResampleChipStream(UINT8 ChipID, WAVE_32BS* RetSample, UINT32 Length)
{
	UINT8 ChipNum;
	UINT8 ChipIDP;	// ChipID with Paired flag
	CAUD_ATTR* CAA;
	INT32* CurBufL;
	INT32* CurBufR;
	INT32* StreamPnt[0x02];
	UINT32 InBase;
	UINT32 InPos;
	UINT32 InPosNext;
	UINT32 OutPos;
	UINT32 SmpFrc;	// Sample Friction
	UINT32 InPre;
	UINT32 InNow;
	SLINT InPosL;
	INT64 TempSmpL;
	INT64 TempSmpR;
	INT32 TempS32L;
	INT32 TempS32R;
	INT32 SmpCnt;	// must be signed, else I'm getting calculation errors
	INT32 CurSmpl;
	UINT64 ChipSmpRate;
	
	ChipIDP = 0x00;
	ChipNum = ChipID;
	CAA = (CAUD_ATTR*)&ChipAudio[ChipNum] + ChipIDP;
	CurBufL = StreamBufs[0x00];
	CurBufR = StreamBufs[0x01];
	
	// This Do-While-Loop gets and resamples the chip output of one or more chips.
	// It's a loop to support the AY8910 paired with the YM2203/YM2608/YM2610.
	//do
	//{
		switch(CAA->Resampler)
		{
		case 0x00:	// old, but very fast resampler
			CAA->SmpLast = CAA->SmpNext;
			CAA->SmpP += Length;
			CAA->SmpNext = (UINT32)((UINT64)CAA->SmpP * CAA->SmpRate / SampleRate);
			if (CAA->SmpLast >= CAA->SmpNext)
			{
				RetSample->Left += CAA->LSmpl.Left * CAA->Volume;
				RetSample->Right += CAA->LSmpl.Right * CAA->Volume;
			}
			else
			{
				SmpCnt = CAA->SmpNext - CAA->SmpLast;
				
				GetChipStream(ChipIDP, ChipNum, StreamBufs, SmpCnt);
				
				if (SmpCnt == 1)
				{
					RetSample->Left += CurBufL[0x00] * CAA->Volume;
					RetSample->Right += CurBufR[0x00] * CAA->Volume;
					CAA->LSmpl.Left = CurBufL[0x00];
					CAA->LSmpl.Right = CurBufR[0x00];
				}
				else if (SmpCnt == 2)
				{
					RetSample->Left += (CurBufL[0x00] + CurBufL[0x01]) * CAA->Volume >> 1;
					RetSample->Right += (CurBufR[0x00] + CurBufR[0x01]) * CAA->Volume >> 1;
					CAA->LSmpl.Left = CurBufL[0x01];
					CAA->LSmpl.Right = CurBufR[0x01];
				}
				else
				{
					// I'm using InPos
					TempS32L = CurBufL[0x00];
					TempS32R = CurBufR[0x00];
					for (CurSmpl = 0x01; CurSmpl < SmpCnt; CurSmpl ++)
					{
						TempS32L += CurBufL[CurSmpl];
						TempS32R += CurBufR[CurSmpl];
					}
					RetSample->Left += TempS32L * CAA->Volume / SmpCnt;
					RetSample->Right += TempS32R * CAA->Volume / SmpCnt;
					CAA->LSmpl.Left = CurBufL[SmpCnt - 1];
					CAA->LSmpl.Right = CurBufR[SmpCnt - 1];
				}
			}
			break;
		case 0x01:	// Upsampling
			ChipSmpRate = CAA->SmpRate;
			InPosL = (SLINT)(FIXPNT_FACT * CAA->SmpP * ChipSmpRate / SampleRate);
			InPre = (UINT32)fp2i_floor(InPosL);
			InNow = (UINT32)fp2i_ceil(InPosL);
			
			CurBufL[0x00] = CAA->LSmpl.Left;
			CurBufR[0x00] = CAA->LSmpl.Right;
			CurBufL[0x01] = CAA->NSmpl.Left;
			CurBufR[0x01] = CAA->NSmpl.Right;
			StreamPnt[0x00] = &CurBufL[0x02];
			StreamPnt[0x01] = &CurBufR[0x02];
			GetChipStream(ChipIDP, ChipNum, StreamPnt, InNow - CAA->SmpNext);
			
			InBase = FIXPNT_FACT + (UINT32)(InPosL - (SLINT)CAA->SmpNext * FIXPNT_FACT);
			SmpCnt = FIXPNT_FACT;
			CAA->SmpLast = InPre;
			CAA->SmpNext = InNow;
			for (OutPos = 0x00; OutPos < Length; OutPos ++)
			{
				InPos = InBase + (UINT32)(FIXPNT_FACT * OutPos * ChipSmpRate / SampleRate);
				
				InPre = fp2i_floor(InPos);
				InNow = fp2i_ceil(InPos);
				SmpFrc = getfriction(InPos);
				
				// Linear interpolation
				TempSmpL = ((INT64)CurBufL[InPre] * (FIXPNT_FACT - SmpFrc)) +
							((INT64)CurBufL[InNow] * SmpFrc);
				TempSmpR = ((INT64)CurBufR[InPre] * (FIXPNT_FACT - SmpFrc)) +
							((INT64)CurBufR[InNow] * SmpFrc);
				RetSample[OutPos].Left += (INT32)(TempSmpL * CAA->Volume / SmpCnt);
				RetSample[OutPos].Right += (INT32)(TempSmpR * CAA->Volume / SmpCnt);
			}
			CAA->LSmpl.Left = CurBufL[InPre];
			CAA->LSmpl.Right = CurBufR[InPre];
			CAA->NSmpl.Left = CurBufL[InNow];
			CAA->NSmpl.Right = CurBufR[InNow];
			CAA->SmpP += Length;
			break;
		case 0x02:	// Copying
			CAA->SmpNext = CAA->SmpP * CAA->SmpRate / SampleRate;
			GetChipStream(ChipIDP, ChipNum, StreamBufs, Length);
			
			for (OutPos = 0x00; OutPos < Length; OutPos ++)
			{
				RetSample[OutPos].Left += CurBufL[OutPos] * CAA->Volume;
				RetSample[OutPos].Right += CurBufR[OutPos] * CAA->Volume;
			}
			CAA->SmpP += Length;
			CAA->SmpLast = CAA->SmpNext;
			break;
		case 0x03:	// Downsampling
			ChipSmpRate = CAA->SmpRate;
			InPosL = (SLINT)(FIXPNT_FACT * (CAA->SmpP + Length) * ChipSmpRate / SampleRate);
			CAA->SmpNext = (UINT32)fp2i_ceil(InPosL);
			
			CurBufL[0x00] = CAA->LSmpl.Left;
			CurBufR[0x00] = CAA->LSmpl.Right;
			StreamPnt[0x00] = &CurBufL[0x01];
			StreamPnt[0x01] = &CurBufR[0x01];
			GetChipStream(ChipIDP, ChipNum, StreamPnt, CAA->SmpNext - CAA->SmpLast);
			
			InPosL = (SLINT)(FIXPNT_FACT * CAA->SmpP * ChipSmpRate / SampleRate);
			// I'm adding 1.0 to avoid negative indexes
			InBase = FIXPNT_FACT + (UINT32)(InPosL - (SLINT)CAA->SmpLast * FIXPNT_FACT);
			InPosNext = InBase;
			for (OutPos = 0x00; OutPos < Length; OutPos ++)
			{
				//InPos = InBase + (UINT32)(FIXPNT_FACT * OutPos * ChipSmpRate / SampleRate);
				InPos = InPosNext;
				InPosNext = InBase + (UINT32)(FIXPNT_FACT * (OutPos+1) * ChipSmpRate / SampleRate);
				
				// first frictional Sample
				SmpFrc = getnfriction(InPos);
				if (SmpFrc)
				{
					InPre = fp2i_floor(InPos);
					TempSmpL = (INT64)CurBufL[InPre] * SmpFrc;
					TempSmpR = (INT64)CurBufR[InPre] * SmpFrc;
				}
				else
				{
					TempSmpL = TempSmpR = 0x00;
				}
				SmpCnt = SmpFrc;
				
				// last frictional Sample
				SmpFrc = getfriction(InPosNext);
				InPre = fp2i_floor(InPosNext);
				if (SmpFrc)
				{
					TempSmpL += (INT64)CurBufL[InPre] * SmpFrc;
					TempSmpR += (INT64)CurBufR[InPre] * SmpFrc;
					SmpCnt += SmpFrc;
				}
				
				// whole Samples in between
				//InPre = fp2i_floor(InPosNext);
				InNow = fp2i_ceil(InPos);
				SmpCnt += (InPre - InNow) * FIXPNT_FACT;	// this is faster
				while(InNow < InPre)
				{
					TempSmpL += (INT64)CurBufL[InNow] * FIXPNT_FACT;
					TempSmpR += (INT64)CurBufR[InNow] * FIXPNT_FACT;
					//SmpCnt ++;
					InNow ++;
				}
				
				RetSample[OutPos].Left += (INT32)(TempSmpL * CAA->Volume / SmpCnt);
				RetSample[OutPos].Right += (INT32)(TempSmpR * CAA->Volume / SmpCnt);
			}
			
			CAA->LSmpl.Left = CurBufL[InPre];
			CAA->LSmpl.Right = CurBufR[InPre];
			CAA->SmpP += Length;
			CAA->SmpLast = CAA->SmpNext;
			break;
		default:
			return;	// do absolutely nothing
		}
		
		if (CAA->SmpLast >= CAA->SmpRate)
		{
			CAA->SmpLast -= CAA->SmpRate;
			CAA->SmpNext -= CAA->SmpRate;
			CAA->SmpP -= SampleRate;
		}
		
	//	CAA = CAA->Paired;
	//	ChipIDP |= 0x80;
	//} while(CAA != NULL);
	
	return;
}

void FillBuffer(WAVE_16BS* Buffer, UINT32 BufferSize)
{
	UINT32 CurSmpl;
	WAVE_32BS TempBuf;
	UINT8 CurChip;
	
	if (Buffer == NULL)
		return;
	
	EnterCriticalSection(&write_sect);
	for (CurSmpl = 0x00; CurSmpl < BufferSize; CurSmpl ++)
	{
		TempBuf.Left = 0x00;
		TempBuf.Right = 0x00;
		
		for (CurChip = 0x00; CurChip < OPN_CHIPS; CurChip ++)
			UpdateDAC(CurChip, 1);
		for (CurChip = 0x00; CurChip < OPN_CHIPS; CurChip ++)
			ResampleChipStream(CurChip, &TempBuf, 1);
		
		TempBuf.Left = TempBuf.Left >> 7;
		TempBuf.Right = TempBuf.Right >> 7;
		if (! TempBuf.Left && ! TempBuf.Right)
			NullSamples ++;
		Buffer[CurSmpl].Left = Limit2Short(TempBuf.Left);
		Buffer[CurSmpl].Right = Limit2Short(TempBuf.Right);
	}
	
	if (NullSamples >= SampleRate)
	{
		NullSamples = 0xFFFFFFFF;
		PauseStream(true);	// stop the stream if chip isn't used
	}
	LeaveCriticalSection(&write_sect);
	
	return;
}


static void UpdateDAC(UINT8 ChipID, UINT32 Samples)
{
	DAC_STATE* TempDAC;
	UINT32 RemDelta;
	INT32 SmplData;
	
	TempDAC = &DACState[ChipID];
	if (TempDAC->Data == NULL)
		return;
	
	RemDelta = TempDAC->Delta * Samples;
	TempDAC->SmplFric += TempDAC->Delta * Samples;
	if (TempDAC->SmplFric & 0xFFFF0000)
	{
		TempDAC->SmplPos += (TempDAC->SmplFric >> 16);
		TempDAC->SmplFric &= 0x0000FFFF;
		if (TempDAC->SmplPos >= TempDAC->DataSize)
		{
			TempDAC->Data = NULL;
			ym2612_w(ChipID, 0x00, 0x2A);
			ym2612_w(ChipID, 0x01, 0x80);
			return;
		}
		
		ym2612_w(ChipID, 0x00, 0x2A);
		if (TempDAC->Volume == 0x100)
		{
			ym2612_w(ChipID, 0x01, TempDAC->Data[TempDAC->SmplPos]);
		}
		else
		{
			SmplData = TempDAC->Data[TempDAC->SmplPos] - 0x80;	// 00..80..FF -> -80..00..+7F
			SmplData *= TempDAC->Volume;
			SmplData = (SmplData + 0x80) >> 8;	// +0x80 for proper rounding
			if (SmplData < -0x80)
				SmplData = -0x80;
			else if (SmplData > 0x7F)
				SmplData = 0x7F;
			ym2612_w(ChipID, 0x01, (UINT8)(SmplData + 0x80));	// YM2612 takes 00..FF
		}
		NullSamples = 0;	// keep everything running while the DAC is playing
	}
	
	return;
}

#define MulDivRoundU(Mul1, Mul2, Div)	(UINT32)( ((UINT64)Mul1 * Mul2 + Div / 2) / Div)
void OPNAPI PlayDACSample(UINT8 ChipID, UINT32 DataSize, const UINT8* Data, UINT32 SmplFreq)
{
	DAC_STATE* TempDAC;
	
	if (ChipID >= OPN_CHIPS)
		return;
	
	EnterCriticalSection(&write_sect);
	
	TempDAC = &DACState[ChipID];
	TempDAC->DataSize = DataSize;
	TempDAC->Data = Data;
	if (SmplFreq)
		TempDAC->Frequency = SmplFreq;
	TempDAC->Delta = MulDivRoundU(0x10000, TempDAC->Frequency, SampleRate);
	TempDAC->SmplPos = 0x00;
	
	// Resume Stream
	NullSamples = 0;
	PauseStream(false);
	LeaveCriticalSection(&write_sect);
	
	return;
}

void OPNAPI SetDACFrequency(UINT8 ChipID, UINT32 SmplFreq)
{
	DAC_STATE* TempDAC;
	
	if (ChipID >= OPN_CHIPS)
		return;
	
	TempDAC = &DACState[ChipID];
	TempDAC->Frequency = SmplFreq;
	TempDAC->Delta = MulDivRoundU(0x10000, TempDAC->Frequency, SampleRate);
	
	return;
}

void OPNAPI SetDACVolume(UINT8 ChipID, UINT16 Volume)
{
	DAC_STATE* TempDAC;
	
	if (ChipID >= OPN_CHIPS)
		return;
	
	TempDAC = &DACState[ChipID];
	TempDAC->Volume = Volume;
	
	return;
}
