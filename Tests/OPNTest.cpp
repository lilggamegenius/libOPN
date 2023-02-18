#include "src/OPN_DLL.hpp"

#include <filesystem>
#include <vector>
#include <fstream>
#include <iterator>
#include <array>
#include <thread>
#include <iostream>

using DRUM_SOUND = std::vector<uint8_t>;

constexpr uint8_t DRUM_COUNT = 2;

static std::atomic<bool> flag;

namespace fs = std::filesystem;
void LoadDrumSound(const fs::path &FileName, DRUM_SOUND &DrumSnd){
	std::error_code err{};
	if(!fs::exists(FileName, err)) throw fs::filesystem_error("Sound file doesn't exist", FileName, err);
	std::ifstream drumStream(FileName, std::ios::binary);
	DrumSnd.insert(DrumSnd.begin(), std::istream_iterator<uint8_t>(drumStream), std::istream_iterator<uint8_t>());
}

void Timer(const std::array<DRUM_SOUND, DRUM_COUNT> &DrumLib){
	uint8_t NextDrum = 0;
	uint8_t playCount = 20;
	while(!flag.load(std::memory_order::relaxed)){
		PlayDACSample(0, DrumLib[NextDrum++], 0);
		NextDrum %= 2;
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		if(--playCount == 0) break;
	}
}

int main(int argc, char**){
	if(argc == 0){ // Only here to hide unused warnings for exported functions
		OpenOPNDriver(MAX_CHIPS);
		SetOPNOptions();
		for(uint8_t i = 0; i < MAX_CHIPS; i++){
			OPN_Write(i, 0, 0);
			OPN_Mute(i, 0);
			PlayDACSample(i, 0, nullptr, 0);
			SetDACFrequency(i, 0);
			SetDACVolume(i, 0);
		}
		CloseOPNDriver();
		return 0;
	} // Now for the actual test code
	std::array<DRUM_SOUND, DRUM_COUNT> DrumLib;

	auto RetVal = OpenOPNDriver(1);
	if(RetVal != DriverReturnCode::Success){
		return static_cast<int>(RetVal);
	}

	LoadDrumSound("00_BassDrum.raw", DrumLib[0]);
	LoadDrumSound("01_Snare.raw", DrumLib[1]);

	OPN_Write(0, 0x2B, 0x80);
	flag.store(false, std::memory_order::relaxed);
	std::thread dac(Timer, DrumLib);
	std::cin.ignore();
	flag.store(true, std::memory_order::relaxed);
	dac.join();
}