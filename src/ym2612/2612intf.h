#pragma once

#include "mamedef.h"

typedef struct ym2612_interface ym2612_interface;
struct ym2612_interface
{
	void (*handler)(int irq);
};

void ym2612_stream_update(uint8_t ChipID, stream_sample_t **outputs, int samples);
int device_start_ym2612(uint8_t ChipID, int clock);
void device_stop_ym2612(uint8_t ChipID);
void device_reset_ym2612(uint8_t ChipID);

void ym2612_w(uint8_t ChipID, offs_t offset, uint8_t data);
void ym2612_set_mute_mask(uint8_t ChipID, uint32_t MuteMask);
