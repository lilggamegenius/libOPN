#include "src/OPN_DLL.hpp"

int main(int argc, char** argv){
	if(argc == 0){ // Only here to hide unused warnings for exported functions
		OpenOPNDriver(MAX_CHIPS);
		SetOPNOptions(0, 0, 0, 0);
		for(auto i = 0U; i < MAX_CHIPS; i++){
			OPN_Write(i, 0, 0);
			OPN_Mute(i, 0);
			PlayDACSample(i, 0, nullptr, 0);
			SetDACFrequency(i, 0);
			SetDACVolume(i, 0);
		}
		CloseOPNDriver();
	}
}