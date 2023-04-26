// OPN_DLL.c - DLL for realtime OPN playback
// Written by Valley Bell, 2011, 2014

#include "OPN_DLL.hpp"

#include "audio/stream.hpp"
#include "src/ym2612/fm2612.hpp"

#include <mutex>

constexpr uint32_t YM2612_CLOCK = 7670454;

extern "C" {
uint32_t SampleRate = 0;    // Note: also used by some sound cores to determinate the chip sample rate
}

static uint8_t OPN_CHIPS = 0x00;    // also indicates, if DLL is running

std::array<ChipAudioAttributes, MAX_CHIPS> ChipAudio;
// Todo: Give better name or make not global
std::array<DACState, MAX_CHIPS> DACStates;

constexpr uint32_t SMPL_BUFSIZE = 0x100;
static int32_t *StreamBufs[0x02];
stream_sample_t *DUMMYBUF[0x02] = {nullptr, nullptr};

static uint32_t NullSamples;

static std::mutex writeGuard;

static void DeinitChips(){
	uint8_t CurChip;

	delete StreamBufs[0x00];
	delete StreamBufs[0x01];

	for(CurChip = 0x00; CurChip < OPN_CHIPS; CurChip++){
		device_stop_ym2612(CurChip);
	}

	OPN_CHIPS = 0x00;
}

#ifndef _MSC_VER
__attribute__((destructor))
#endif
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

void SetOPNOptions(uint32_t SmplRate){
	SampleRate = SmplRate;
}

INLINE void GetChipStream(uint8_t ChipID, int32_t **Buffer, size_t BufSize){
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

	StreamBufs[0x00] = new int32_t[SMPL_BUFSIZE];
	StreamBufs[0x01] = new int32_t[SMPL_BUFSIZE];

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

	const std::lock_guard lock(writeGuard);
	if(Register == 0x28 && static_cast<bool>(Data & 0xF0)){
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
}

void OPN_Mute(uint8_t ChipID, uint8_t MuteMask){
	if(ChipID >= OPN_CHIPS){
		return;
	}

	const std::lock_guard lock(writeGuard);
	ym2612_set_mute_mask(ChipID, MuteMask);
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
using SLINT = uint32_t;    // 32-bit is a lot faster
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

static void UpdateDAC(uint8_t ChipID, uint32_t Samples){
	DACState *TempDAC = &DACStates[ChipID];
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
			int32_t SmplData = TempDAC->Data[TempDAC->SmplPos] - 0x80;    // 00..80..FF -> -80..00..+7F
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
	uint8_t CurChip;

	if(Buffer == nullptr){
		return;
	}

	const std::lock_guard lock(writeGuard);
	//EnterCriticalSection(&write_sect);
	for(uint32_t CurSmpl = 0x00; CurSmpl < BufferSize; CurSmpl++){
		WAVE_32BS TempBuf{};

		for(CurChip = 0x00; CurChip < OPN_CHIPS; CurChip++){
			UpdateDAC(CurChip, 1);
		}
		//for(CurChip = 0x00; CurChip < OPN_CHIPS; CurChip++){
		//	ResampleChipStream(CurChip, &TempBuf, 1);
		//}

		TempBuf.Left >>= 7;
		TempBuf.Right >>= 7;
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

void PlayDACSample(uint8_t ChipID, size_t DataSize, const uint8_t *Data, uint32_t SmplFreq){
	PlayDACSample(ChipID, {Data, DataSize}, SmplFreq);
}

void PlayDACSample(uint8_t ChipID, std::span<uint8_t const> Data, uint32_t SmplFreq){
	DACState *TempDAC;

	if(ChipID >= OPN_CHIPS){
		return;
	}

	const std::lock_guard lock(writeGuard);
	//EnterCriticalSection(&write_sect);

	TempDAC = &DACStates[ChipID];
	TempDAC->DataSize = Data.size();
	TempDAC->Data = Data.data();
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

size_t GetMaxChipsSupported(){
	return MAX_CHIPS;
}