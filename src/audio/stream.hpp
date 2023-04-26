// Stream.h: Header File for constants and structures related to Sound Output
#pragma once

#include <cstdint>
template<typename SampleT>
struct WAVE_Sample{
	SampleT Left;
	SampleT Right;
};

using WAVE_16BS = WAVE_Sample<int16_t>;

constexpr auto SAMPLESIZE = sizeof(WAVE_16BS);
constexpr auto BUFSIZE_MAX = 0x1000;    // Maximum Buffer Size in Bytes
constexpr auto AUDIOBUFFERS = 200;      // Maximum Buffer Count
//	Windows:	BUFFERSIZE = SampleRate / 100 * SAMPLESIZE (44100 / 100 * 4 = 1764)
//				1 Audio-Buffer = 10 msec, Min: 5
//				Win95- / WinVista-safe: 500 msec

uint8_t SoundLogging(bool Mode);

uint8_t StartStream(uint8_t DeviceID);

uint8_t StopStream(bool SkipWOClose);

void PauseStream(bool PauseOn);

void FillBuffer(WAVE_16BS *Buffer, uint32_t BufferSize);
