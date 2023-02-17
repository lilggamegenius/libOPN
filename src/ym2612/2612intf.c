/***************************************************************************

  2612intf.c

  The YM2612 emulator supports up to 2 chips.
  Each chip has the following connections:
  - Status Read / Control Write A
  - Port Read / Data Write A
  - Control Write B
  - Data Write B

***************************************************************************/

#include "mamedef.h"
#include "fm.h"
#include "2612intf.h"

#define NULL    ((void *)0)

typedef struct ym2612_state ym2612_state;
struct ym2612_state{
	void *chip;
	const ym2612_interface *intf;
};

extern uint8_t CHIP_SAMPLING_MODE;
extern int32_t CHIP_SAMPLE_RATE;

#define MAX_CHIPS    0x10
static ym2612_state YM2612Data[MAX_CHIPS];

/* update request from fm.c */
void ym2612_update_request(void *param){
	ym2612_state *info = (ym2612_state *) param;
	ym2612_update_one(info->chip, DUMMYBUF, 0);
}

/***********************************************************/
/*    YM2612                                               */
/***********************************************************/

void ym2612_stream_update(uint8_t ChipID, stream_sample_t **outputs, int samples){
	ym2612_state *info = &YM2612Data[ChipID];
	ym2612_update_one(info->chip, outputs, samples);
}

int device_start_ym2612(uint8_t ChipID, int clock){
	static const ym2612_interface dummy = {0};
	ym2612_state *info;
	int rate;

	if(ChipID >= MAX_CHIPS){
		return 0;
	}

	info = &YM2612Data[ChipID];
	rate = clock / 144;
	if((CHIP_SAMPLING_MODE == 0x01 && rate < CHIP_SAMPLE_RATE) ||
	   CHIP_SAMPLING_MODE == 0x02){
		rate = CHIP_SAMPLE_RATE;
	}
	info->intf = &dummy;

	/**** initialize YM2612 ****/
	info->chip = ym2612_init(info, clock, rate, NULL, NULL);
	return rate;
}

void device_stop_ym2612(uint8_t ChipID){
	ym2612_state *info = &YM2612Data[ChipID];
	ym2612_shutdown(info->chip);
}

void device_reset_ym2612(uint8_t ChipID){
	ym2612_state *info = &YM2612Data[ChipID];
	ym2612_reset_chip(info->chip);
}

void ym2612_w(uint8_t ChipID, offs_t offset, uint8_t data){
	ym2612_state *info = &YM2612Data[ChipID];
	ym2612_write(info->chip, offset & 3, data);
}

void ym2612_set_mute_mask(uint8_t ChipID, uint32_t MuteMask){
	ym2612_state *info = &YM2612Data[ChipID];
	ym2612_set_mutemask(info->chip, MuteMask);
}
