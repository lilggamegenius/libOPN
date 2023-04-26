#pragma once

#include "miniaudio.h"
#include "stream.hpp"

#include <span>
#include <vector>

extern "C" uint32_t SampleRate;

class YM2612DataSource {
	ma_data_source_base base;
	std::vector<WAVE_16BS> sampleBuffer;

	static ma_data_source_vtable vtable;

public:
	ma_result read(void *pFramesOut, ma_uint64 &frameCount, const ma_uint64 &pFramesRead = 0);

	YM2612DataSource();
	~YM2612DataSource();
};

ma_result YM2612DataSource_read(ma_data_source *pDataSource, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead);

ma_result YM2612DataSource_seek([[maybe_unused]] ma_data_source *pDataSource, ma_uint64 frameIndex);

ma_result YM2612DataSource_get_data_format([[maybe_unused]] ma_data_source *pDataSource, ma_format *pFormat, ma_uint32 *pChannels, ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap);

ma_result YM2612DataSource_get_cursor([[maybe_unused]] ma_data_source *pDataSource, ma_uint64 *pCursor);

ma_result YM2612DataSource_get_length([[maybe_unused]] ma_data_source *pDataSource, ma_uint64 *pLength);