#include "stream.hpp"
#include "miniaudio.h"
#include "ym2612DataSource.hpp"

#include <memory>

static ma_device device;
static std::unique_ptr<YM2612DataSource> chipSource;

uint8_t SoundLogging(bool Mode){

}

void data_callback([[maybe_unused]] ma_device *pDevice, void *pOutput, [[maybe_unused]] const void *pInput, ma_uint32 frameCount){
	// In playback mode copy data to pOutput. In capture mode read data from pInput. In full-duplex mode, both
	// pOutput and pInput will be valid, and you can move data from pInput into pOutput. Never process more than
	// frameCount frames.
	chipSource->read(pOutput, reinterpret_cast<ma_uint64 &>(frameCount));
}

uint8_t StartStream(uint8_t DeviceID){
	ma_device_config config = ma_device_config_init(ma_device_type_playback);
	config.playback.format = ma_format_s16;   // Set to ma_format_unknown to use the device's native format.
	config.playback.channels = 2;               // Set to 0 to use the device's native channel count.
	config.sampleRate = 0;           // Set to 0 to use the device's native sample rate.
	config.dataCallback = data_callback;   // This function will be called when miniaudio needs more data.
	//config.pUserData         = pMyCustomData;   // Can be accessed from the device object (device.pUserData).

	auto result = ma_device_init(nullptr, &config, &device);

	if(result != MA_SUCCESS){
		return result;  // Failed to initialize the device.
	}

	try {
		auto* dataSource = new YM2612DataSource;
		chipSource.reset(dataSource);
	} catch(ma_result maResult) {
		return maResult;
	}

	result = ma_device_start(&device); // The device is sleeping by default so you'll need to start it manually.
	if(result != MA_SUCCESS){
		return result;  // Failed to start the device.
	}
}

uint8_t StopStream(bool SkipWOClose){
	ma_device_uninit(&device);
}

void PauseStream(bool PauseOn){

}