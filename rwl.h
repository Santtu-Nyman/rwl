/*
	Raw Wave Library version 1.0.0 2018-09-04 by Santtu Nyman.
	git repository https://github.com/Santtu-Nyman/rwl
	
	Description
		Cross-Platform library for reading and writing raw wave files.
		Usage documentation is written to the rwl header after function declarations.
		Implementation code is not commented. I may add comments some day.
		
	Version history
		version 1.0.0 2018-09-04
			First publicly available version.
*/

#ifndef RAW_WAVE_LIB_H
#define RAW_WAVE_LIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

int rwl_load_wave_file(const char* file_name, size_t* sample_rate, size_t* sample_count, float* left_channel, float* rigth_channel);
/*
	Description
		Function load wave from raw(not compressed) wave(.wav) file to one or two channels of some signal.
		If both channel pointer are null function reads sample rate and sample count and does
		not write anything to left or right channel buffers. If only one channel pointer is not null all channels from
		the file are mixed to one resulting signal that is written to the channel's buffer that has not null pointer.
		If both channel pointer are not null left channel is written to left channel buffer and right channel is written to right channel buffer.
	Parameters
		file_name
			Pointer to name of the wave file.
		sample_rate
			Pointer variable that receives file's sample rate.
		sample_count
			Pointer to variable that specifies length in samples of channel buffers.
			Function overwrites value of this variable with file's per channel sample count.
		left_channel
			Pointer to left channel's buffer.
		rigth_channel
			Pointer to rigth channel's buffer.
	Return
		If the function succeeds, the return value is zero and non zero on failure.
*/

int rwl_store_wave_file(const char* file_name, size_t sample_rate, size_t sample_count, const float* left_channel, const float* rigth_channel);
/*
	Description
		Function stores wave to raw(not compressed) wave(.wav) file from one or two channels of some signal.
		If only one channel pointer is not null the function writes single channel wave file and
		signal of this one channel is read from the channel's buffer that has not null pointer.
	Parameters
		file_name
			Pointer to name of the wave file.
		sample_rate
			Wave file's sample rate.
		sample_count
			Number of samples in all channel buffers per channel that have non zero pointer.
		left_channel
			Pointer to left channel's buffer.
		rigth_channel
			Pointer to rigth channel's buffer.
	Return
		If the function succeeds, the return value is zero and non zero on failure.
*/

#ifdef __cplusplus
}
#endif

#endif
