#include "src/OPN_DLL.hpp"

int main(int argc, char** argv){
	if(argc == 0){ // Only here to hide unused warnings for exported functions
		OpenOPNDriver(1);
		SetOPNOptions(0, 0, 0, 0);
		OPN_Write(0, 0, 0);
		OPN_Mute(0, 0);
		PlayDACSample(0, 0, nullptr, 0);
		SetDACFrequency(0, 0);
		SetDACVolume(0, 0);
		CloseOPNDriver();
	}
}