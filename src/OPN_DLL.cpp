// OPN_DLL.c - DLL for realtime OPN playback
// Written by Valley Bell, 2011, 2014

#include "OPN_DLL.hpp"

extern "C" {
#include "src/ym2612/2612intf.h"
}

#include <mutex>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

constexpr uint32_t YM2612_CLOCK = 7670454;

extern "C" {
uint32_t SampleRate = 44100;    // Note: also used by some sound cores to determinate the chip sample rate

ChipSampleMode CHIP_SAMPLING_MODE = ChipSampleMode::NATIVE;    // 00 - native, 01 - highest (native/custom), 02 - custom (CHIP_SAMPLE_RATE)
int32_t CHIP_SAMPLE_RATE = SampleRate;
}

// Todo: Give better name or make not global
ResampleMode ResampleMode_g = ResampleMode::HIGH;    // 00 - HQ both, 01 - LQ downsampling, 02 - LQ both

static uint8_t OPN_CHIPS = 0x00;    // also indicates, if DLL is running

ChipAudioAttributes ChipAudio[MAX_CHIPS];
// Todo: Give better name or make not global
DACState DACStates[MAX_CHIPS];

constexpr uint32_t SMPL_BUFSIZE = 0x100;
static int32_t *StreamBufs[0x02];
stream_sample_t *DUMMYBUF[0x02] = {nullptr, nullptr};

static uint32_t NullSamples;

//static CRITICAL_SECTION write_sect;
static std::mutex writeGuard;

static void DeinitChips(){
	uint8_t CurChip;

	delete StreamBufs[0x00]; //free(StreamBufs[0x00]);
	delete StreamBufs[0x01]; //free(StreamBufs[0x01]);

	for(CurChip = 0x00; CurChip < OPN_CHIPS; CurChip++){
		device_stop_ym2612(CurChip);
	}
	//DeleteCriticalSection(&write_sect);


	OPN_CHIPS = 0x00;
}

#ifdef _MSC_VER
#define destructor
#else
#define destructor __attribute__((destructor))
#endif

destructor
void CloseOPNDriver_Unload(){
	StopStream(true);

	DeinitChips();
}

#ifdef _MSC_VER
typedef BOOL WIN_BOOL;
WIN_BOOL APIENTRY DllMain(HANDLE, DWORD fdwReason, LPVOID){
	if(fdwReason == DLL_PROCESS_DETACH){
		// Perform any necessary cleanup.
		if(OPN_CHIPS){
			// a special function is called, as waveOutClose hangs the process at this point
			CloseOPNDriver_Unload();
		}
	}

	return TRUE;
}
#endif

void SetOPNOptions(uint32_t OutSmplRate, ResampleMode ResmplMode, ChipSampleMode ChipSmplMode, uint32_t ChipSmplRate){
	SampleRate = OutSmplRate;
	ResampleMode_g = ResmplMode;
	CHIP_SAMPLING_MODE = ChipSmplMode;
	CHIP_SAMPLE_RATE = static_cast<int32_t>(ChipSmplRate);
}

INLINE void GetChipStream(uint8_t ChipID, int32_t **Buffer, uint32_t BufSize){
	ym2612_stream_update(ChipID, Buffer, BufSize);
}

static void InitChips(uint8_t ChipCount){
	uint8_t CurChip;
	ChipAudioAttributes *CAA;

	for(CurChip = 0x00; CurChip < MAX_CHIPS; CurChip++){
		CAA = &ChipAudio[CurChip];
		CAA->SmpRate = 0x00;
		CAA->Volume = 0x00;

		DACStates[CurChip].Data = nullptr;
	}

	//InitializeCriticalSection(&write_sect);
	for(CurChip = 0x00; CurChip < ChipCount; CurChip++){
		CAA = &ChipAudio[CurChip];
		CAA->SmpRate = device_start_ym2612(CurChip, YM2612_CLOCK);
		CAA->Volume = 0x100;
		device_reset_ym2612(CurChip);
	}

	for(CurChip = 0x00; CurChip < ChipCount; CurChip++){
		CAA = &ChipAudio[CurChip];
		if(!CAA->SmpRate){
			CAA->Resampler = 0xFF;
		}else if(CAA->SmpRate < SampleRate){
			CAA->Resampler = 0x01;
		}else if(CAA->SmpRate == SampleRate){
			CAA->Resampler = 0x02;
		}else if(CAA->SmpRate > SampleRate){
			CAA->Resampler = 0x03;
		}
		if((ResampleMode_g == ResampleMode::LQ_DOWN && CAA->Resampler == 0x03) ||
		   ResampleMode_g == ResampleMode::LOW){
			CAA->Resampler = 0x00;
		}

		CAA->SmpP = 0x00;
		CAA->SmpLast = 0x00;
		CAA->SmpNext = 0x00;
		CAA->LSmpl.Left = 0x00;
		CAA->LSmpl.Right = 0x00;
		if(CAA->Resampler == 0x01){
			// Pregenerate first Sample (the upsampler is always one too late)
			GetChipStream(CurChip, StreamBufs, 1);
			CAA->NSmpl.Left = StreamBufs[0x00][0x00];
			CAA->NSmpl.Right = StreamBufs[0x01][0x00];
		}else{
			CAA->NSmpl.Left = 0x00;
			CAA->NSmpl.Right = 0x00;
		}

		DACStates[CurChip].Data = nullptr;
		DACStates[CurChip].Volume = 0x100;
		DACStates[CurChip].Frequency = 16000;
	}

	StreamBufs[0x00] = new int32_t[SMPL_BUFSIZE]; //(int32_t *) malloc(SMPL_BUFSIZE * sizeof(int32_t));
	StreamBufs[0x01] = new int32_t[SMPL_BUFSIZE]; //(int32_t *) malloc(SMPL_BUFSIZE * sizeof(int32_t));

	OPN_CHIPS = ChipCount;
}

DriverReturnCode OpenOPNDriver(uint8_t Chips){
	using enum DriverReturnCode;
	if(OPN_CHIPS){
		return DriverAlreadyInitalized;
	}    // already running
	if(Chips > 0x10){
		return TooManyChips;    // too many chips
	}

	InitChips(Chips);

	SoundLogging(false);
	if(StartStream(0x00)){
		//printf("Error opening Sound Device!\n");
		CloseOPNDriver();

		return SoundDeviceError;
	}

	NullSamples = 0xFFFFFFFF;
	PauseStream(true);

	return Success;
}

void CloseOPNDriver(){
	StopStream(false);

	DeinitChips();
}

void OPN_Write(uint8_t ChipID, uint16_t Register, uint8_t Data){
	if(ChipID >= OPN_CHIPS){
		return;
	}

	const std::lock_guard<std::mutex> lock(writeGuard);
	//EnterCriticalSection(&write_sect);
	if(Register == 0x28 && (Data & 0xF0)){
		// Note On - Resume Stream
		NullSamples = 0;
		PauseStream(false);
	}

	if(NullSamples == 0xFFFFFFFF){    // if chip is paused, do safe update
		GetChipStream(ChipID, StreamBufs, 1);
	}

	uint8_t RegSet = Register >> 8;
	ym2612_w(ChipID, 0x00 | (RegSet << 1), Register & 0xFF);
	ym2612_w(ChipID, 0x01 | (RegSet << 1), Data);
	//LeaveCriticalSection(&write_sect);
}

void OPN_Mute(uint8_t ChipID, uint8_t MuteMask){
	if(ChipID >= OPN_CHIPS){
		return;
	}

	const std::lock_guard<std::mutex> lock(writeGuard);
	//EnterCriticalSection(&write_sect);
	ym2612_set_mute_mask(ChipID, MuteMask);
	//LeaveCriticalSection(&write_sect);
}

INLINE int16_t Limit2Short(int32_t Value){
	if(Value < -0x8000){
		Value = -0x8000;
	} else if(Value > 0x7FFF){
		Value = 0x7FFF;
	}

	return static_cast<int16_t>(Value);
}

// I recommend 11 bits as it's fast and accurate
const uint32_t FIXPNT_BITS = 11;
const uint32_t FIXPNT_FACT = 1 << FIXPNT_BITS;
#if (FIXPNT_BITS <= 11)
typedef uint32_t SLINT;    // 32-bit is a lot faster
#else
typedef uint64_t	SLINT;
#endif
const uint32_t FIXPNT_MASK = FIXPNT_FACT - 1;

INLINE uint32_t getfriction(uint32_t x){
	return x & FIXPNT_MASK;
}

INLINE uint32_t getnfriction(uint32_t x){
	return (FIXPNT_FACT - x) & FIXPNT_MASK;
}

INLINE uint32_t fpi_floor(uint32_t x){
	return (x) & ~FIXPNT_MASK;
}

INLINE uint32_t fpi_ceil(uint32_t x){
	return (x + FIXPNT_MASK) & ~FIXPNT_MASK;
}

INLINE uint32_t fp2i_floor(uint32_t x){
	return (x) / FIXPNT_FACT;
}

INLINE uint32_t fp2i_ceil(uint32_t x){
	return (x + FIXPNT_MASK) / FIXPNT_FACT;
}

static void ResampleChipStream(uint8_t ChipNum, WAVE_32BS *RetSample, uint32_t Length){
	uint32_t InBase;
	uint32_t InPos;
	uint32_t InPosNext;
	uint32_t OutPos;
	uint32_t SmpFrc;    // Sample Friction
	uint32_t InPre;
	uint32_t InNow;
	SLINT InPosL;
	int64_t TempSmpL;
	int64_t TempSmpR;
	int32_t TempS32L;
	int32_t TempS32R;
	int32_t SmpCnt;    // must be signed, else I'm getting calculation errors
	int32_t CurSmpl;
	uint64_t ChipSmpRate;

	uint8_t ChipIDP = 0x00; // ChipID with Paired flag
	ChipAudioAttributes *CAA = (ChipAudioAttributes *) &ChipAudio[ChipNum] + ChipIDP;
	int32_t *CurBufL = StreamBufs[0x00];
	int32_t *CurBufR = StreamBufs[0x01];

	int32_t *StreamPnt[0x02];
	// This Do-While-Loop gets and resamples the chip output of one or more chips.
	// It's a loop to support the AY8910 paired with the YM2203/YM2608/YM2610.
	//do
	//{
	switch(CAA->Resampler){
		case 0x00:    // old, but very fast resampler
			CAA->SmpLast = CAA->SmpNext;
			CAA->SmpP += Length;
			CAA->SmpNext = (uint32_t) ((uint64_t) CAA->SmpP * CAA->SmpRate / SampleRate);
			if(CAA->SmpLast >= CAA->SmpNext){
				RetSample->Left += CAA->LSmpl.Left * CAA->Volume;
				RetSample->Right += CAA->LSmpl.Right * CAA->Volume;
			}else{
				SmpCnt = CAA->SmpNext - CAA->SmpLast;

				GetChipStream(ChipNum, StreamBufs, SmpCnt);

				if(SmpCnt == 1){
					RetSample->Left += CurBufL[0x00] * CAA->Volume;
					RetSample->Right += CurBufR[0x00] * CAA->Volume;
					CAA->LSmpl.Left = CurBufL[0x00];
					CAA->LSmpl.Right = CurBufR[0x00];
				}else if(SmpCnt == 2){
					RetSample->Left += (CurBufL[0x00] + CurBufL[0x01]) * CAA->Volume >> 1;
					RetSample->Right += (CurBufR[0x00] + CurBufR[0x01]) * CAA->Volume >> 1;
					CAA->LSmpl.Left = CurBufL[0x01];
					CAA->LSmpl.Right = CurBufR[0x01];
				}else{
					// I'm using InPos
					TempS32L = CurBufL[0x00];
					TempS32R = CurBufR[0x00];
					for(CurSmpl = 0x01; CurSmpl < SmpCnt; CurSmpl++){
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
		case 0x01:    // Upsampling
			ChipSmpRate = CAA->SmpRate;
			InPosL = (SLINT) (FIXPNT_FACT * CAA->SmpP * ChipSmpRate / SampleRate);
			InPre = (uint32_t) fp2i_floor(InPosL);
			InNow = (uint32_t) fp2i_ceil(InPosL);

			CurBufL[0x00] = CAA->LSmpl.Left;
			CurBufR[0x00] = CAA->LSmpl.Right;
			CurBufL[0x01] = CAA->NSmpl.Left;
			CurBufR[0x01] = CAA->NSmpl.Right;
			StreamPnt[0x00] = &CurBufL[0x02];
			StreamPnt[0x01] = &CurBufR[0x02];
			GetChipStream(ChipNum, StreamPnt, InNow - CAA->SmpNext);

			InBase = FIXPNT_FACT + (uint32_t) (InPosL - (SLINT) CAA->SmpNext * FIXPNT_FACT);
			SmpCnt = FIXPNT_FACT;
			CAA->SmpLast = InPre;
			CAA->SmpNext = InNow;
			for(OutPos = 0x00; OutPos < Length; OutPos++){
				InPos = InBase + (uint32_t) (FIXPNT_FACT * OutPos * ChipSmpRate / SampleRate);

				InPre = fp2i_floor(InPos);
				InNow = fp2i_ceil(InPos);
				SmpFrc = getfriction(InPos);

				// Linear interpolation
				TempSmpL = ((int64_t) CurBufL[InPre] * (FIXPNT_FACT - SmpFrc)) +
				           ((int64_t) CurBufL[InNow] * SmpFrc);
				TempSmpR = ((int64_t) CurBufR[InPre] * (FIXPNT_FACT - SmpFrc)) +
				           ((int64_t) CurBufR[InNow] * SmpFrc);
				RetSample[OutPos].Left += (int32_t) (TempSmpL * CAA->Volume / SmpCnt);
				RetSample[OutPos].Right += (int32_t) (TempSmpR * CAA->Volume / SmpCnt);
			}
			CAA->LSmpl.Left = CurBufL[InPre];
			CAA->LSmpl.Right = CurBufR[InPre];
			CAA->NSmpl.Left = CurBufL[InNow];
			CAA->NSmpl.Right = CurBufR[InNow];
			CAA->SmpP += Length;
			break;
		case 0x02:    // Copying
			CAA->SmpNext = CAA->SmpP * CAA->SmpRate / SampleRate;
			GetChipStream(ChipNum, StreamBufs, Length);

			for(OutPos = 0x00; OutPos < Length; OutPos++){
				RetSample[OutPos].Left += CurBufL[OutPos] * CAA->Volume;
				RetSample[OutPos].Right += CurBufR[OutPos] * CAA->Volume;
			}
			CAA->SmpP += Length;
			CAA->SmpLast = CAA->SmpNext;
			break;
		case 0x03:    // Downsampling
			ChipSmpRate = CAA->SmpRate;
			InPosL = (SLINT) (FIXPNT_FACT * (CAA->SmpP + Length) * ChipSmpRate / SampleRate);
			CAA->SmpNext = (uint32_t) fp2i_ceil(InPosL);

			CurBufL[0x00] = CAA->LSmpl.Left;
			CurBufR[0x00] = CAA->LSmpl.Right;
			StreamPnt[0x00] = &CurBufL[0x01];
			StreamPnt[0x01] = &CurBufR[0x01];
			GetChipStream(ChipNum, StreamPnt, CAA->SmpNext - CAA->SmpLast);

			InPosL = (SLINT) (FIXPNT_FACT * CAA->SmpP * ChipSmpRate / SampleRate);
			// I'm adding 1.0 to avoid negative indexes
			InBase = FIXPNT_FACT + (uint32_t) (InPosL - (SLINT) CAA->SmpLast * FIXPNT_FACT);
			InPosNext = InBase;
			InPre = 0x00; // Make sure this variable is initialized to something first, just in case
			for(OutPos = 0x00; OutPos < Length; OutPos++){
				//InPos = InBase + (uint32_t)(FIXPNT_FACT * OutPos * ChipSmpRate / SampleRate);
				InPos = InPosNext;
				InPosNext = InBase + (uint32_t) (FIXPNT_FACT * (OutPos + 1) * ChipSmpRate / SampleRate);

				// first frictional Sample
				SmpFrc = getnfriction(InPos);
				if(SmpFrc){
					InPre = fp2i_floor(InPos);
					TempSmpL = (int64_t) CurBufL[InPre] * SmpFrc;
					TempSmpR = (int64_t) CurBufR[InPre] * SmpFrc;
				}else{
					TempSmpL = TempSmpR = 0x00;
				}
				SmpCnt = SmpFrc;

				// last frictional Sample
				SmpFrc = getfriction(InPosNext);
				InPre = fp2i_floor(InPosNext);
				if(SmpFrc){
					TempSmpL += (int64_t) CurBufL[InPre] * SmpFrc;
					TempSmpR += (int64_t) CurBufR[InPre] * SmpFrc;
					SmpCnt += SmpFrc;
				}

				// whole Samples in between
				//InPre = fp2i_floor(InPosNext);
				InNow = fp2i_ceil(InPos);
				SmpCnt += (InPre - InNow) * FIXPNT_FACT;    // this is faster
				while(InNow < InPre){
					TempSmpL += (int64_t) CurBufL[InNow] * FIXPNT_FACT;
					TempSmpR += (int64_t) CurBufR[InNow] * FIXPNT_FACT;
					//SmpCnt ++;
					InNow++;
				}

				RetSample[OutPos].Left += (int32_t) (TempSmpL * CAA->Volume / SmpCnt);
				RetSample[OutPos].Right += (int32_t) (TempSmpR * CAA->Volume / SmpCnt);
			}

			CAA->LSmpl.Left = CurBufL[InPre];
			CAA->LSmpl.Right = CurBufR[InPre];
			CAA->SmpP += Length;
			CAA->SmpLast = CAA->SmpNext;
			break;
		default: return;    // do absolutely nothing
	}

	if(CAA->SmpLast >= CAA->SmpRate){
		CAA->SmpLast -= CAA->SmpRate;
		CAA->SmpNext -= CAA->SmpRate;
		CAA->SmpP -= SampleRate;
	}
}

static void UpdateDAC(uint8_t ChipID, uint32_t Samples){
	DACState *TempDAC;
	//uint32_t RemDelta;
	int32_t SmplData;

	TempDAC = &DACStates[ChipID];
	if(TempDAC->Data == nullptr){
		return;
	}

	//RemDelta = TempDAC->Delta * Samples;
	TempDAC->SmplFric += TempDAC->Delta * Samples;
	if(TempDAC->SmplFric & 0xFFFF0000){
		TempDAC->SmplPos += (TempDAC->SmplFric >> 16);
		TempDAC->SmplFric &= 0x0000FFFF;
		if(TempDAC->SmplPos >= TempDAC->DataSize){
			TempDAC->Data = nullptr;
			ym2612_w(ChipID, 0x00, 0x2A);
			ym2612_w(ChipID, 0x01, 0x80);
			return;
		}

		ym2612_w(ChipID, 0x00, 0x2A);
		if(TempDAC->Volume == 0x100){
			ym2612_w(ChipID, 0x01, TempDAC->Data[TempDAC->SmplPos]);
		}else{
			SmplData = TempDAC->Data[TempDAC->SmplPos] - 0x80;    // 00..80..FF -> -80..00..+7F
			SmplData *= TempDAC->Volume;
			SmplData = (SmplData + 0x80) >> 8;    // +0x80 for proper rounding
			if(SmplData < -0x80){
				SmplData = -0x80;
			}else if(SmplData > 0x7F){
				SmplData = 0x7F;
			}
			ym2612_w(ChipID, 0x01, (uint8_t) (SmplData + 0x80));    // YM2612 takes 00..FF
		}
		NullSamples = 0;    // keep everything running while the DAC is playing
	}
}

void FillBuffer(WAVE_16BS *Buffer, uint32_t BufferSize){
	uint32_t CurSmpl;
	WAVE_32BS TempBuf;
	uint8_t CurChip;

	if(Buffer == nullptr){
		return;
	}

	const std::lock_guard<std::mutex> lock(writeGuard);
	//EnterCriticalSection(&write_sect);
	for(CurSmpl = 0x00; CurSmpl < BufferSize; CurSmpl++){
		TempBuf.Left = 0x00;
		TempBuf.Right = 0x00;

		for(CurChip = 0x00; CurChip < OPN_CHIPS; CurChip++){
			UpdateDAC(CurChip, 1);
		}
		for(CurChip = 0x00; CurChip < OPN_CHIPS; CurChip++){
			ResampleChipStream(CurChip, &TempBuf, 1);
		}

		TempBuf.Left = TempBuf.Left >> 7;
		TempBuf.Right = TempBuf.Right >> 7;
		if(!TempBuf.Left && !TempBuf.Right){
			NullSamples++;
		}
		Buffer[CurSmpl].Left = Limit2Short(TempBuf.Left);
		Buffer[CurSmpl].Right = Limit2Short(TempBuf.Right);
	}

	if(NullSamples >= SampleRate){
		NullSamples = 0xFFFFFFFF;
		PauseStream(true);    // stop the stream if chip isn't used
	}
	//LeaveCriticalSection(&write_sect);
}

INLINE uint32_t MulDivRoundU(uint64_t Mul1, uint64_t Mul2, uint64_t Div){
	return static_cast<uint32_t>((Mul1 * Mul2 + Div / 2) / Div);
}

void PlayDACSample(uint8_t ChipID, uint32_t DataSize, const uint8_t *Data, uint32_t SmplFreq){
	DACState *TempDAC;

	if(ChipID >= OPN_CHIPS){
		return;
	}

	const std::lock_guard<std::mutex> lock(writeGuard);
	//EnterCriticalSection(&write_sect);

	TempDAC = &DACStates[ChipID];
	TempDAC->DataSize = DataSize;
	TempDAC->Data = Data;
	if(SmplFreq){
		TempDAC->Frequency = SmplFreq;
	}
	TempDAC->Delta = MulDivRoundU(0x10000, TempDAC->Frequency, SampleRate);
	TempDAC->SmplPos = 0x00;

	// Resume Stream
	NullSamples = 0;
	PauseStream(false);
	//LeaveCriticalSection(&write_sect);
}

void SetDACFrequency(uint8_t ChipID, uint32_t SmplFreq){
	DACState *TempDAC;

	if(ChipID >= OPN_CHIPS){
		return;
	}

	TempDAC = &DACStates[ChipID];
	TempDAC->Frequency = SmplFreq;
	TempDAC->Delta = MulDivRoundU(0x10000, TempDAC->Frequency, SampleRate);
}

void SetDACVolume(uint8_t ChipID, uint16_t Volume){
	DACState *TempDAC;

	if(ChipID >= OPN_CHIPS){
		return;
	}

	TempDAC = &DACStates[ChipID];
	TempDAC->Volume = Volume;
}
