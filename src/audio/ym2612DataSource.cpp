//
// Created by ggonz on 4/11/2023.
//

#include "ym2612DataSource.hpp"

ma_data_source_vtable YM2612DataSource::vtable = {
        YM2612DataSource_read,
        YM2612DataSource_seek,
        YM2612DataSource_get_data_format,
        YM2612DataSource_get_cursor,
        YM2612DataSource_get_length,
};

ma_result YM2612DataSource::read(void *pFramesOut, ma_uint64 &frameCount, const ma_uint64 &pFramesRead) {
	// Read data here. Output in the same format returned by my_data_source_get_data_format().

}

YM2612DataSource::YM2612DataSource() {
	ma_data_source_config baseConfig = ma_data_source_config_init();
	baseConfig.vtable = &vtable;

	ma_result result = ma_data_source_init(&baseConfig, &base);
	if (result != MA_SUCCESS) {
		throw ma_result(result);
	}

	// ... do the initialization of your custom data source here ...
}

YM2612DataSource::~YM2612DataSource() {
	// ... do the uninitialization of your custom data source here ...

	// You must uninitialize the base data source.
	ma_data_source_uninit(&base);
}

ma_result YM2612DataSource_read(ma_data_source *pDataSource, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead){
	return reinterpret_cast<YM2612DataSource *>(pDataSource)->read(pFramesOut, frameCount, *pFramesRead);
}
ma_result YM2612DataSource_seek(ma_data_source *pDataSource, ma_uint64 frameIndex) {
	// Seek to a specific PCM frame here. Return MA_NOT_IMPLEMENTED if seeking is not supported.
	return MA_NOT_IMPLEMENTED;
}
ma_result YM2612DataSource_get_data_format(ma_data_source *pDataSource, ma_format *pFormat, ma_uint32 *pChannels, ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap) {
	std::span<ma_channel> channelMap = {pChannelMap, channelMapCap};
	// Return the format of the data here.
	*pFormat = ma_format_s16;
	*pChannels = 2;
	*pSampleRate = SampleRate;
	switch(channelMap.size()){
		default:
		case 2: pChannelMap[1] = MA_CHANNEL_RIGHT;
			[[fallthrough]];
		case 1: pChannelMap[0] = MA_CHANNEL_LEFT;
			[[fallthrough]];
		case 0:
			break;
	}
	return MA_SUCCESS;
}
ma_result YM2612DataSource_get_cursor(ma_data_source *pDataSource, ma_uint64 *pCursor) {
	// Retrieve the current position of the cursor here. Return MA_NOT_IMPLEMENTED and set *pCursor to 0 if there is no notion of a cursor.
	*pCursor = 0;
	return MA_NOT_IMPLEMENTED;
}
ma_result YM2612DataSource_get_length(ma_data_source *pDataSource, ma_uint64 *pLength) {
	// Retrieve the length in PCM frames here. Return MA_NOT_IMPLEMENTED and set *pLength to 0 if there is no notion of a length or if the length is unknown.
	*pLength = 0;
	return MA_NOT_IMPLEMENTED;
}
