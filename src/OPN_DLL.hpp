#pragma once
// Define EXPORTED for any platform
#ifdef BUILD_STATIC_LIB
	#define EXPORTED
#else
	#if(defined __WIN32__) || (defined _WIN32) || defined(__CYGWIN__)
		#ifdef __GNUC__
			#define EXPORTED __attribute__((dllexport))
		#else
			#define EXPORTED __declspec(dllexport)
		#endif
	#else
		#define EXPORTED __attribute__((visibility("default")))
	#endif
#endif

#ifndef MAX_CHIPS
	#pragma message("Using default value of '0x10' for MAX_CHIPS.\nConsider manually defining a value")
	#define MAX_CHIPS 0x10
#endif

// default Sample Rate: 48000 Hz
#ifdef __cplusplus
static_assert(MAX_CHIPS > 1, "MAX_CHIPS must be able to support at least 1 chip");

#define DEFAULT_ARGS(...) = __VA_ARGS__
#include <cstdint>
#include <span>
// Return codes for OpenOPNDriver
enum class DriverReturnCode : uint8_t {
	Success = 0,
	DriverAlreadyInitalized = 0x80,
	TooManyChips = 0xFF,
	SoundDeviceError = 0xC0,

};

#else
#define DEFAULT_ARGS(...)
#include <stdint.h>
// Return codes for OpenOPNDriver
enum DriverReturnCode : uint8_t {
	DriverReturnCode_Success = 0,
	DriverReturnCode_ZeroChipCount = 0x80,
	DriverReturnCode_TooManyChips = 0xFF,
	DriverReturnCode_SoundDeviceError = 0xC0,

};
#endif

extern "C" {
EXPORTED void SetOPNOptions(uint32_t SmplRate DEFAULT_ARGS(0));
EXPORTED DriverReturnCode OpenOPNDriver(uint8_t Chips DEFAULT_ARGS(MAX_CHIPS));
EXPORTED void CloseOPNDriver();

EXPORTED void OPN_Write(uint8_t ChipID, uint16_t Register, uint8_t Data);
EXPORTED void OPN_Mute(uint8_t ChipID, uint8_t MuteMask);

EXPORTED void PlayDACSample(uint8_t ChipID, size_t DataSize, const uint8_t *Data, uint32_t SmplFreq);
EXPORTED void SetDACFrequency(uint8_t ChipID, uint32_t SmplFreq);
EXPORTED void SetDACVolume(uint8_t ChipID, uint16_t Volume);// 0x100 = 100%

EXPORTED size_t GetMaxChipsSupported();
}

#ifdef __cplusplus
EXPORTED void PlayDACSample(uint8_t ChipID, std::span<uint8_t const> Data, uint32_t SmplFreq);
#endif

struct WAVE_32BS {
	int32_t Left;
	int32_t Right;
};

struct ChipAudioAttributes {
	uint32_t SmpRate;
	uint16_t Volume;
	uint8_t Resampler;// Resampler Type: 00 - Old, 01 - Upsampling, 02 - Copy, 03 - Downsampling
	uint32_t SmpP;    // Current Sample (Playback Rate)
	uint32_t SmpLast; // Sample Number Last
	uint32_t SmpNext; // Sample Number Next
	WAVE_32BS LSmpl;  // Last Sample
	WAVE_32BS NSmpl;  // Next Sample
};

struct DACState {
	uint32_t DataSize;
	const uint8_t *Data;
	uint32_t Frequency;
	uint16_t Volume;

	uint32_t Delta;
	uint32_t SmplPos;
	uint32_t SmplFric;// .16 Friction
};