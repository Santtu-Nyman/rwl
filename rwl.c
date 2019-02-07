/*
	Raw Wave Library version 1.0.2 2019-02-07 by Santtu Nyman.
	git repository https://github.com/Santtu-Nyman/rwl
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "rwl.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>

typedef struct rwl_riff_chunk
{
	char identifier[4];
	size_t size;
	const void* data;
	void* parent;
	size_t sub_chunk_count;
	void* sub_chunks;
} rwl_riff_chunk;

static int rwl_load_file(const char* file_name, size_t* file_size, void** file_data);

static int rwl_store_file(const char* file_name, size_t file_size, const void* file_data);

static int rwl_create_riff_tree(size_t size, const void* data, rwl_riff_chunk** root);

static rwl_riff_chunk* rwl_get_riff_chunk(rwl_riff_chunk* chunk, const char* chunk_path);

static int rwl_get_audio_format(rwl_riff_chunk* chunk, int* sample_type, size_t* sample_size, size_t* channel_count, uint32_t* channel_mask, size_t* sample_rate, size_t* sample_count);

static float rwl_get_signal_absolute_peak(size_t sample_count, const float* signal);

static void rwl_scale_signal(size_t sample_count, float* signal, float multiplier);

static int rwl_load_file(const char* file_name, size_t* file_size, void** file_data)
{
	int error;
	FILE* file = fopen(file_name, "rb");
	if (!file)
	{
		error = errno;
		return error;
	}
	if (fseek(file, 0, SEEK_END))
	{
		error = errno;
		fclose(file);
		return error;
	}
	long end = ftell(file);
	if (end == EOF)
	{
		error = errno;
		fclose(file);
		return error;
	}
	if (fseek(file, 0, SEEK_SET))
	{
		error = errno;
		fclose(file);
		return error;
	}
	if ((sizeof(size_t) < sizeof(long)) && (end > (long)((size_t)~0)))
	{
		error = EFBIG;
		fclose(file);
		return error;
	}
	size_t size = (size_t)end;
	void* data = malloc(size);
	if (!data)
	{
		error = ENOMEM;
		fclose(file);
		return error;
	}
	for (size_t read = 0, result; read != size; read += result)
	{
		result = fread((void*)((uintptr_t)data + read), 1, size - read, file);
		if (!result)
		{
			error = ferror(file);
			free(data);
			fclose(file);
			return error;
		}
	}
	fclose(file);
	*file_size = size;
	*file_data = data;
	return 0;
}

static int rwl_store_file(const char* file_name, size_t file_size, const void* file_data)
{
	int error;
	size_t file_name_length = strlen(file_name);
	size_t temporal_file_name_length = file_name_length + 28;
	char* temporal_file_name = (char*)malloc(temporal_file_name_length * sizeof(char));
	if (!temporal_file_name)
	{
		error = ENOMEM;
		return error;
	}
	time_t current_time;
	struct tm* current_date = (time(&current_time) != -1) ? localtime(&current_time) : 0;
	if (current_date)
	{
		memcpy(temporal_file_name, file_name, file_name_length);
		temporal_file_name[file_name_length] = '.';
		for (int i = 0, v = 99; i != 2; ++i, v /= 10)
			temporal_file_name[file_name_length + 1 + 1 - i] = '0' + (char)(v % 10);
		temporal_file_name[file_name_length + 3] = '-';
		for (int i = 0, v = 1900 + current_date->tm_year; i != 4; ++i, v /= 10)
			temporal_file_name[file_name_length + 4 + 3 - i] = '0' + (char)(v % 10);
		temporal_file_name[file_name_length + 8] = '-';
		for (int i = 0, v = 1 + current_date->tm_mon; i != 2; ++i, v /= 10)
			temporal_file_name[file_name_length + 9 + 1 - i] = '0' + (char)(v % 10);
		temporal_file_name[file_name_length + 11] = '-';
		for (int i = 0, v = current_date->tm_mday; i != 2; ++i, v /= 10)
			temporal_file_name[file_name_length + 12 + 1 - i] = '0' + (char)(v % 10);
		temporal_file_name[file_name_length + 14] = '-';
		for (int i = 0, v = current_date->tm_hour; i != 2; ++i, v /= 10)
			temporal_file_name[file_name_length + 15 + 1 - i] = '0' + (char)(v % 10);
		temporal_file_name[file_name_length + 17] = '-';
		for (int i = 0, v = current_date->tm_min; i != 2; ++i, v /= 10)
			temporal_file_name[file_name_length + 18 + 1 - i] = '0' + (char)(v % 10);
		temporal_file_name[file_name_length + 20] = '-';
		for (int i = 0, v = current_date->tm_sec; i != 2; ++i, v /= 10)
			temporal_file_name[file_name_length + 21 + 1 - i] = '0' + (char)(v % 10);
		memcpy(temporal_file_name + file_name_length + 23, ".tmp", 5);
	}
	else
		memcpy(temporal_file_name + file_name_length, ".99-XXXX-XX-XX-XX-XX-XX.tmp", 28);
	FILE* file = 0;
	for (int count = 99; !file;)
	{
		file = fopen(temporal_file_name, "rb");
		if (!file)
		{
			file = fopen(temporal_file_name, "wb");
			if (!file)
			{
				error = errno;
				free(temporal_file_name);
				return error;
			}
		}
		else
		{
			fclose(file);
			file = 0;
			if (!count)
			{
				error = EEXIST;
				free(temporal_file_name);
				return error;
			}
			for (int i = 0, v = --count; i != 2; ++i, v /= 10)
				temporal_file_name[file_name_length + 1 + 1 - i] = '0' + (char)(v % 10);
		}
	}
	for (size_t written = 0, write_result; written != file_size; written += write_result)
	{
		write_result = fwrite((const void*)((uintptr_t)file_data + written), 1, file_size - written, file);
		if (!write_result)
		{
			error = ferror(file);
			fclose(file);
			remove(temporal_file_name);
			free(temporal_file_name);
			return error;
		}
	}
	if (fflush(file))
	{
		error = ferror(file);
		fclose(file);
		remove(temporal_file_name);
		free(temporal_file_name);
		return error;
	}
	fclose(file);
	if (rename(temporal_file_name, file_name))
	{
		if (remove(file_name))
		{
			error = errno;
			remove(temporal_file_name);
			free(temporal_file_name);
			return error;
		}
		if (rename(temporal_file_name, file_name))
		{
			error = errno;
			remove(temporal_file_name);
			free(temporal_file_name);
			return error;
		}
	}
	free(temporal_file_name);
	return 0;
}

static int rwl_create_riff_tree(size_t size, const void* data, rwl_riff_chunk** root)
{
	if (size < 8)
		return ENOBUFS;
	rwl_riff_chunk* root_chunk = (rwl_riff_chunk*)malloc(sizeof(rwl_riff_chunk));
	if (!root_chunk)
		return ENOMEM;
	memcpy(root_chunk->identifier, data, 4);
	root_chunk->size = (size_t)*(const uint8_t*)((uintptr_t)data + 4) | ((size_t)*(const uint8_t*)((uintptr_t)data + 5) << 8) | ((size_t)*(const uint8_t*)((uintptr_t)data + 6) << 16) | ((size_t)*(const uint8_t*)((uintptr_t)data + 7) << 24);
	root_chunk->data = (const void*)((uintptr_t)data + 8);
	if ((size_t)((uintptr_t)root_chunk->data - (uintptr_t)data) > size || (size_t)((uintptr_t)root_chunk->data - (uintptr_t)data) + root_chunk->size > (size_t)((uintptr_t)data + (uintptr_t)size))
	{
		free(root_chunk);
		return EILSEQ;
	}
	root_chunk->parent = 0;
	rwl_riff_chunk* chunk = root_chunk;
	size_t chunk_count = 1;
	for (size_t chunk_index = 0; chunk_index != chunk_count; ++chunk_index)
	{
		chunk->sub_chunk_count = 0;
		chunk->sub_chunks = 0;
		if ((chunk->identifier[0] == 'R' && chunk->identifier[1] == 'I' && chunk->identifier[2] == 'F' && chunk->identifier[3] == 'F') || (chunk->identifier[0] == 'L' && chunk->identifier[1] == 'I' && chunk->identifier[2] == 'S' && chunk->identifier[3] == 'T'))
		{
			if (chunk->size < 4)
			{
				free(root_chunk);
				return EILSEQ;
			}
			for (size_t chunk_pointer = 4; chunk->size - chunk_pointer >= 8;)
			{
				size_t sub_chunk_size = (size_t)*(const uint8_t*)((uintptr_t)chunk->data + chunk_pointer + 4) | ((size_t)*(const uint8_t*)((uintptr_t)chunk->data + chunk_pointer + 5) << 8) | ((size_t)*(const uint8_t*)((uintptr_t)chunk->data + chunk_pointer + 6) << 16) | ((size_t)*(const uint8_t*)((uintptr_t)chunk->data + chunk_pointer + 7) << 24);
				if (chunk_pointer + sub_chunk_size > chunk->size)
				{
					free(root_chunk);
					return EILSEQ;
				}
				chunk_pointer += 8 + sub_chunk_size;
				chunk->sub_chunk_count++;
			}
			if (chunk->sub_chunk_count)
			{
				chunk_count += chunk->sub_chunk_count;
				chunk = (rwl_riff_chunk*)((uintptr_t)chunk - (uintptr_t)root_chunk);
				rwl_riff_chunk* new_root_chunk = (rwl_riff_chunk*)realloc(root_chunk, chunk_count * sizeof(rwl_riff_chunk));
				if (!new_root_chunk)
				{
					free(root_chunk);
					return ENOMEM;
				}
				root_chunk = new_root_chunk;
				chunk = (rwl_riff_chunk*)((uintptr_t)chunk + (uintptr_t)root_chunk);
				chunk->sub_chunks = (void*)((uintptr_t)root_chunk + ((chunk_count - chunk->sub_chunk_count) * sizeof(rwl_riff_chunk)));
				for (size_t chunk_pointer = 4, sub_chunk_index = 0; sub_chunk_index != chunk->sub_chunk_count; ++sub_chunk_index)
				{
					memcpy(((rwl_riff_chunk*)chunk->sub_chunks)[sub_chunk_index].identifier, (const void*)((uintptr_t)chunk->data + chunk_pointer), 4);
					((rwl_riff_chunk*)chunk->sub_chunks)[sub_chunk_index].size = (size_t)*(const uint8_t*)((uintptr_t)chunk->data + chunk_pointer + 4) | ((size_t)*(const uint8_t*)((uintptr_t)chunk->data + chunk_pointer + 5) << 8) | ((size_t)*(const uint8_t*)((uintptr_t)chunk->data + chunk_pointer + 6) << 16) | ((size_t)*(const uint8_t*)((uintptr_t)chunk->data + chunk_pointer + 7) << 24);
					((rwl_riff_chunk*)chunk->sub_chunks)[sub_chunk_index].data = (const void*)((uintptr_t)chunk->data + chunk_pointer + 8);
					chunk_pointer += 8 + ((rwl_riff_chunk*)chunk->sub_chunks)[sub_chunk_index].size;
				}
				chunk->sub_chunks = (void*)((uintptr_t)chunk->sub_chunks - (uintptr_t)root_chunk);
			}
		}
		chunk = (rwl_riff_chunk*)((uintptr_t)chunk + sizeof(rwl_riff_chunk));
	}
	chunk = root_chunk;
	for (size_t chunk_index = 0; chunk_index != chunk_count; ++chunk_index)
	{
		if (chunk->sub_chunks)
		{
			chunk->sub_chunks = (void*)((uintptr_t)chunk->sub_chunks + (uintptr_t)root_chunk);
			for (size_t sub_chunk_index = 0; sub_chunk_index != chunk->sub_chunk_count; ++sub_chunk_index)
				((rwl_riff_chunk*)chunk->sub_chunks)[sub_chunk_index].parent = chunk;
		}
		chunk = (rwl_riff_chunk*)((uintptr_t)chunk + sizeof(rwl_riff_chunk));
	}
	*root = root_chunk;
	return 0;
}

static rwl_riff_chunk* rwl_get_riff_chunk(rwl_riff_chunk* chunk, const char* chunk_path)
{
	if (!*chunk_path)
		return 0;
	size_t chunk_count = 1;
	rwl_riff_chunk* chunks = chunk;
	for (;;)
	{
		size_t i = 0;
		while (i != chunk_count && !(chunks[i].identifier[0] == chunk_path[0] && chunks[i].identifier[1] == chunk_path[1] && chunks[i].identifier[2] == chunk_path[2] && chunks[i].identifier[3] == chunk_path[3]))
			++i;
		if (i == chunk_count)
			return 0;
		chunk_path += 4;
		if (!*chunk_path)
			return chunks + i;
		chunk_count = chunks[i].sub_chunk_count;
		chunks = chunks[i].sub_chunks;
	}
}

static int rwl_get_audio_format(rwl_riff_chunk* chunk, int* sample_type, size_t* sample_size, size_t* channel_count, uint32_t* channel_mask, size_t* sample_rate, size_t* sample_count)
{
	chunk = rwl_get_riff_chunk(chunk, "RIFF");
	if (!chunk || *(const char*)((uintptr_t)chunk->data) != 'W' || *(const char*)((uintptr_t)chunk->data + 1) != 'A' || *(const char*)((uintptr_t)chunk->data + 2) != 'V' || *(const char*)((uintptr_t)chunk->data + 3) != 'E')
		return ENOENT;
	rwl_riff_chunk* fmt = rwl_get_riff_chunk(chunk, "RIFFfmt ");
	if (!chunk)
		return ENOENT;
	if (fmt->size < 16)
		return ENOENT;
	uint16_t fmt_audio_format = (uint16_t)*(const uint8_t*)((uintptr_t)fmt->data) | ((uint16_t)*(const uint8_t*)((uintptr_t)fmt->data + 1) << 8);
	uint16_t fmt_channel_count = (uint16_t)*(const uint8_t*)((uintptr_t)fmt->data + 2) | ((uint16_t)*(const uint8_t*)((uintptr_t)fmt->data + 3) << 8);
	uint32_t fmt_sample_rate = (uint32_t)*(const uint8_t*)((uintptr_t)fmt->data + 4) | ((uint32_t)*(const uint8_t*)((uintptr_t)fmt->data + 5) << 8) | ((uint32_t)*(const uint8_t*)((uintptr_t)fmt->data + 6) << 16) | ((uint32_t)*(const uint8_t*)((uintptr_t)fmt->data + 7) << 24);
	uint32_t fmt_byte_rate = (uint32_t)*(const uint8_t*)((uintptr_t)fmt->data + 8) | ((uint32_t)*(const uint8_t*)((uintptr_t)fmt->data + 9) << 8) | ((uint32_t)*(const uint8_t*)((uintptr_t)fmt->data + 10) << 16) | ((uint32_t)*(const uint8_t*)((uintptr_t)fmt->data + 11) << 24);
	uint16_t fmt_frame_size = (uint16_t)*(const uint8_t*)((uintptr_t)fmt->data + 12) | ((uint16_t)*(const uint8_t*)((uintptr_t)fmt->data + 13) << 8);
	uint16_t fmt_bits_per_sample = (uint16_t)*(const uint8_t*)((uintptr_t)fmt->data + 14) | ((uint16_t)*(const uint8_t*)((uintptr_t)fmt->data + 15) << 8);
	size_t fmt_extension_size = 0;
	uint16_t fmt_valid_bits_per_sample = 0;
	uint32_t fmt_channel_mask = 0;
	uint16_t fmt_sub_format = 0;
	if (fmt_audio_format == 0xFFFE)
	{
		if (fmt->size > 17)
		{
			fmt_extension_size = (size_t)*(const uint8_t*)((uintptr_t)fmt->data + 16) | ((size_t)*(const uint8_t*)((uintptr_t)fmt->data + 17) << 8);
			if (fmt_extension_size == 22 && fmt->size > 39)
			{
				fmt_valid_bits_per_sample = (uint16_t)*(const uint8_t*)((uintptr_t)fmt->data + 18) | ((uint16_t)*(const uint8_t*)((uintptr_t)fmt->data + 19) << 8);
				if (fmt_valid_bits_per_sample > fmt_bits_per_sample)
					return EILSEQ;
				fmt_channel_mask = (uint32_t)*(const uint8_t*)((uintptr_t)fmt->data + 20) | ((uint32_t)*(const uint8_t*)((uintptr_t)fmt->data + 21) << 8) | ((uint32_t)*(const uint8_t*)((uintptr_t)fmt->data + 22) << 16) | ((uint32_t)*(const uint8_t*)((uintptr_t)fmt->data + 23) << 24);
				if (fmt_channel_mask & 0xFFFC0000)
					return EILSEQ;
				const uint8_t sub_furmat_guinds[6][16] = {
					{ 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 },
					{ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 },
					{ 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 },
					{ 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 },
					{ 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 },
					{ 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
				for (size_t i = 0; i != sizeof(sub_furmat_guinds) / 16 && !fmt_sub_format; ++i)
					if (!memcmp((const void*)((uintptr_t)fmt->data + 24), sub_furmat_guinds[i], 16))
						fmt_sub_format = (uint16_t)*(const uint8_t*)((uintptr_t)fmt->data + 24) | ((uint16_t)*(const uint8_t*)((uintptr_t)fmt->data + 25) << 8);
				if (!fmt_sub_format)
					return ENOTSUP;
			}
			else
				return EILSEQ;
		}
		else
			return EILSEQ;
	}
	else
	{
		fmt_valid_bits_per_sample = fmt_bits_per_sample;
		fmt_sub_format = fmt_audio_format;
		if (fmt_channel_count == 1)
			fmt_channel_mask = 0x00000004;
		else if (fmt_channel_count == 2)
			fmt_channel_mask = 0x00000600;
	}
	if ((fmt_byte_rate != (fmt_sample_rate * (fmt_channel_count * (fmt_bits_per_sample / 8)))) || (fmt_frame_size != (fmt_channel_count * (fmt_bits_per_sample / 8))))
		return EILSEQ;
	rwl_riff_chunk* data = rwl_get_riff_chunk(chunk, "RIFFdata");
	if (!data)
		return EILSEQ;
	*sample_type = (int)fmt_sub_format;
	*sample_size = (size_t)fmt_bits_per_sample;
	*channel_count = (size_t)fmt_channel_count;
	*channel_mask = fmt_channel_mask;
	*sample_rate = (size_t)fmt_sample_rate;
	*sample_count = data->size / ((size_t)fmt_channel_count * ((size_t)fmt_bits_per_sample / 8));
	return 0;
}

static float rwl_get_signal_absolute_peak(size_t sample_count, const float* signal)
{
	float peak = 0.0f;
	for (const float* signal_end = signal + sample_count; signal != signal_end; ++signal)
	{
		float sample = *signal;
		*(uint32_t*)&sample &= 0x7FFFFFFF;
		if (sample > peak)
			peak = sample;
	}
	return peak;
}

static void rwl_scale_signal(size_t sample_count, float* signal, float multiplier)
{
	for (float* signal_end = signal + sample_count; signal != signal_end; ++signal)
		*signal *= multiplier;
}

int rwl_load_wave_file(const char* file_name, size_t* sample_rate, size_t* sample_count, float* left_channel, float* rigth_channel)
{
	size_t file_size;
	void* file_data;
	int error = rwl_load_file(file_name, &file_size, &file_data);
	if (error)
		return error;
	rwl_riff_chunk* file_riff;
	error = rwl_create_riff_tree(file_size, file_data, &file_riff);
	if (error)
	{
		free(file_data);
		return error;
	}
	int file_sample_type;
	size_t file_sample_size;
	size_t file_channel_count;
	uint32_t file_channel_mask;
	size_t file_sample_rate;
	size_t file_sample_count;
	error = rwl_get_audio_format(file_riff, &file_sample_type, &file_sample_size, &file_channel_count, &file_channel_mask, &file_sample_rate, &file_sample_count);
	if (error)
	{
		free(file_data);
		return error;
	}
	if (!((file_sample_type == 1) && (file_sample_size == 8 || file_sample_size == 16 || file_sample_size == 24 || file_sample_size == 32)) && !((file_sample_type == 3) && (file_sample_size == 32)))
	{
		free(file_data);
		error = ENOTSUP;
		return error;
	}
	size_t channel_count = (left_channel ? (size_t)1 : (size_t)0) + (rigth_channel ? (size_t)1 : (size_t)0);
	if (!channel_count)
	{
		free(file_data);
		*sample_rate = file_sample_rate;
		*sample_count = file_sample_count;
		return 0;
	}
	if (channel_count == 2)
	{
		uint32_t file_channel_mask_channel_count = 0;
		for (uint32_t i = 0; i != 32; ++i)
			if (file_channel_mask & (1 << i))
				++file_channel_mask_channel_count;
		if (file_channel_mask_channel_count != file_channel_count)
		{
			free(file_data);
			error = ENOTSUP;
			return error;
		}
	}
	if (*sample_count < file_sample_count)
	{
		free(file_data);
		*sample_rate = file_sample_rate;
		*sample_count = file_sample_count;
		return ENOBUFS;
	}
	rwl_riff_chunk* wave_data = rwl_get_riff_chunk(file_riff, "RIFFdata");
	if (!wave_data)
	{
		free(file_data);
		error = ENOTSUP;
		return error;
	}
	const uint8_t* file_sample_data = (const uint8_t*)wave_data->data;
	if (channel_count == 1)
	{
		float* samples = left_channel ? left_channel : rigth_channel;
		if (file_sample_type == 1)
		{
			if (file_sample_size == 8)
			{
				for (size_t i = 0; i != file_sample_count; ++i)
				{
					float sample = 0.0f;
					for (size_t j = 0; j != file_channel_count; ++j)
						sample += ((float)file_sample_data[i * file_channel_count + j] - 127.5f) / 127.5f;
					samples[i] = sample;
				}
			}
			else if (file_sample_size == 16)
			{
				int16_t channel_raw_sample;
				for (size_t i = 0; i != file_sample_count; ++i)
				{
					float sample = 0.0f;
					for (size_t j = 0; j != file_channel_count; ++j)
					{
						*((uint8_t*)&channel_raw_sample) = file_sample_data[(i * file_channel_count + j) * 2];
						*((uint8_t*)&channel_raw_sample + 1) = file_sample_data[(i * file_channel_count + j) * 2 + 1];
						sample += (float)channel_raw_sample / 32768.0f;
					}
					samples[i] = sample;
				}
			}
			else if (file_sample_size == 24)
			{
				int32_t channel_raw_sample;
				*((uint8_t*)&channel_raw_sample + 3) = 0;
				for (size_t i = 0; i != file_sample_count; ++i)
				{
					float sample = 0.0f;
					for (size_t j = 0; j != file_channel_count; ++j)
					{
						*((uint8_t*)&channel_raw_sample) = file_sample_data[(i * file_channel_count + j) * 3];
						*((uint8_t*)&channel_raw_sample + 1) = file_sample_data[(i * file_channel_count + j) * 3 + 1];
						*((uint8_t*)&channel_raw_sample + 2) = file_sample_data[(i * file_channel_count + j) * 3 + 2];
						sample += (channel_raw_sample & 0x800000) ? ((float)(channel_raw_sample - 16777216) / 8388608.0f) : (((float)channel_raw_sample) / 8388608.0f);
					}
					samples[i] = sample;
				}
			}
			else if (file_sample_size == 32)
			{
				int32_t channel_raw_sample;
				for (size_t i = 0; i != file_sample_count; ++i)
				{
					float sample = 0.0f;
					for (size_t j = 0; j != file_channel_count; ++j)
					{
						*((uint8_t*)&channel_raw_sample) = file_sample_data[(i * file_channel_count + j) * 4];
						*((uint8_t*)&channel_raw_sample + 1) = file_sample_data[(i * file_channel_count + j) * 4 + 1];
						*((uint8_t*)&channel_raw_sample + 2) = file_sample_data[(i * file_channel_count + j) * 4 + 2];
						*((uint8_t*)&channel_raw_sample + 3) = file_sample_data[(i * file_channel_count + j) * 4 + 3];
						sample += (float)channel_raw_sample / 2147483648.0f;
					}
					samples[i] = sample;
				}
			}
			else
			{
				free(file_data);
				error = ENOSYS;
				return error;
			}
		}
		else if (file_sample_type == 3)
		{
			float channel_sample;
			for (size_t i = 0; i != file_sample_count; ++i)
			{
				float sample = 0.0f;
				for (size_t j = 0; j != file_channel_count; ++j)
				{
					*((uint8_t*)&channel_sample) = file_sample_data[(i * file_channel_count + j) * 4];
					*((uint8_t*)&channel_sample + 1) = file_sample_data[(i * file_channel_count + j) * 4 + 1];
					*((uint8_t*)&channel_sample + 2) = file_sample_data[(i * file_channel_count + j) * 4 + 2];
					*((uint8_t*)&channel_sample + 3) = file_sample_data[(i * file_channel_count + j) * 4 + 3];
					sample += channel_sample;
				}
				samples[i] = sample;
			}
		}
		else
		{
			free(file_data);
			error = ENOSYS;
			return error;
		}
		float signal_peak = rwl_get_signal_absolute_peak(file_sample_count, samples);
		if (signal_peak > 0.0009765625f)
			rwl_scale_signal(file_sample_count, samples, 1.0f / signal_peak);
	}
	else if (channel_count == 2)
	{
		const float channel_multipliers[18][2] = {
			{ 0.75f, 0.25f },// front left
			{ 0.25f, 0.75f },// front right
			{ 0.5f, 0.5f },// front center
			{ 0.5f, 0.5f },// low frequency
			{ 0.75f, 0.25f },// back left
			{ 0.25f, 0.75f },// back right
			{ 0.75f, 0.25f },// front left of center
			{ 0.25f, 0.75f },// front right of center
			{ 0.5f, 0.5f },// back center
			{ 1.0f, 0.0f },// left
			{ 0.0f, 1.0f },// right
			{ 0.5f, 0.5f },// top center
			{ 0.75f, 0.25f },// top front left
			{ 0.5f, 0.5f },// top front center
			{ 0.25f, 0.75f },// top front right
			{ 0.75f, 0.25f },// top back left
			{ 0.5f, 0.5f },// top back center
			{ 0.25f, 0.75f } };// top back right
		float channels[18][2];
		for (size_t channel_index = 0, bit_index = 0; channel_index != file_channel_count; ++bit_index, ++channel_index)
		{
			while (!(file_channel_mask & (1 << (uint32_t)bit_index)))
				++bit_index;
			channels[channel_index][0] = channel_multipliers[bit_index][0];
			channels[channel_index][1] = channel_multipliers[bit_index][1];
		}
		if (file_sample_type == 1)
		{
			if (file_sample_size == 8)
			{
				for (size_t i = 0; i != file_sample_count; ++i)
				{
					float left_sample = 0.0f;
					float rigth_sample = 0.0f;
					for (size_t j = 0; j != file_channel_count; ++j)
					{
						float channel_sample = (((float)file_sample_data[i * file_channel_count + j] - 127.5f) / 127.5f);
						left_sample += channels[j][0] * channel_sample;
						rigth_sample += channels[j][1] * channel_sample;
					}
					left_channel[i] = left_sample;
					rigth_channel[i] = rigth_sample;
				}
			}
			else if (file_sample_size == 16)
			{
				int16_t channel_raw_sample;
				for (size_t i = 0; i != file_sample_count; ++i)
				{
					float left_sample = 0.0f;
					float rigth_sample = 0.0f;
					for (size_t j = 0; j != file_channel_count; ++j)
					{
						*((uint8_t*)&channel_raw_sample) = file_sample_data[(i * file_channel_count + j) * 2];
						*((uint8_t*)&channel_raw_sample + 1) = file_sample_data[(i * file_channel_count + j) * 2 + 1];
						float channel_sample = ((float)channel_raw_sample / 32768.0f);
						left_sample += channels[j][0] * channel_sample;
						rigth_sample += channels[j][1] * channel_sample;
					}
					left_channel[i] = left_sample;
					rigth_channel[i] = rigth_sample;
				}
			}
			else if (file_sample_size == 24)
			{
				int32_t channel_raw_sample;
				*((uint8_t*)&channel_raw_sample + 3) = 0;
				for (size_t i = 0; i != file_sample_count; ++i)
				{
					float left_sample = 0.0f;
					float rigth_sample = 0.0f;
					for (size_t j = 0; j != file_channel_count; ++j)
					{
						*((uint8_t*)&channel_raw_sample) = file_sample_data[(i * file_channel_count + j) * 3];
						*((uint8_t*)&channel_raw_sample + 1) = file_sample_data[(i * file_channel_count + j) * 3 + 1];
						*((uint8_t*)&channel_raw_sample + 2) = file_sample_data[(i * file_channel_count + j) * 3 + 2];
						float channel_sample = ((channel_raw_sample & 0x800000) ? ((float)(channel_raw_sample - 16777216) / 8388608.0f) : (((float)channel_raw_sample) / 8388608.0f));
						left_sample += channels[j][0] * channel_sample;
						rigth_sample += channels[j][1] * channel_sample;
					}
					left_channel[i] = left_sample;
					rigth_channel[i] = rigth_sample;
				}
			}
			else if (file_sample_size == 32)
			{
				int32_t channel_raw_sample;
				for (size_t i = 0; i != file_sample_count; ++i)
				{
					float left_sample = 0.0f;
					float rigth_sample = 0.0f;
					for (size_t j = 0; j != file_channel_count; ++j)
					{
						*((uint8_t*)&channel_raw_sample) = file_sample_data[(i * file_channel_count + j) * 4];
						*((uint8_t*)&channel_raw_sample + 1) = file_sample_data[(i * file_channel_count + j) * 4 + 1];
						*((uint8_t*)&channel_raw_sample + 2) = file_sample_data[(i * file_channel_count + j) * 4 + 2];
						*((uint8_t*)&channel_raw_sample + 3) = file_sample_data[(i * file_channel_count + j) * 4 + 3];
						float channel_sample = ((float)channel_raw_sample / 2147483648.0f);
						left_sample += channels[j][0] * channel_sample;
						rigth_sample += channels[j][1] * channel_sample;
					}
					left_channel[i] = left_sample;
					rigth_channel[i] = rigth_sample;
				}
			}
			else
			{
				free(file_data);
				error = ENOSYS;
				return error;
			}
		}
		else if (file_sample_type == 3)
		{
			float channel_raw_sample;
			for (size_t i = 0; i != file_sample_count; ++i)
			{
				float left_sample = 0.0f;
				float rigth_sample = 0.0f;
				for (size_t j = 0; j != file_channel_count; ++j)
				{
					*((uint8_t*)&channel_raw_sample) = file_sample_data[(i * file_channel_count + j) * 4];
					*((uint8_t*)&channel_raw_sample + 1) = file_sample_data[(i * file_channel_count + j) * 4 + 1];
					*((uint8_t*)&channel_raw_sample + 2) = file_sample_data[(i * file_channel_count + j) * 4 + 2];
					*((uint8_t*)&channel_raw_sample + 3) = file_sample_data[(i * file_channel_count + j) * 4 + 3];
					left_sample += channels[j][0] * channel_raw_sample;
					rigth_sample += channels[j][1] * channel_raw_sample;
				}
				left_channel[i] = left_sample;
				rigth_channel[i] = rigth_sample;
			}
		}
		else
		{
			free(file_data);
			error = ENOSYS;
			return error;
		}
		float left_signal_peak = rwl_get_signal_absolute_peak(file_sample_count, left_channel);
		float right_signal_peak = rwl_get_signal_absolute_peak(file_sample_count, rigth_channel);
		float signal_peak = left_signal_peak < right_signal_peak ? right_signal_peak : left_signal_peak;
		if (signal_peak > 0.0009765625f)
		{
			rwl_scale_signal(file_sample_count, left_channel, 1.0f / signal_peak);
			rwl_scale_signal(file_sample_count, rigth_channel, 1.0f / signal_peak);
		}
	}
	else
	{
		free(file_data);
		error = ENOSYS;
		return error;
	}
	free(file_data);
	return 0;
}

int rwl_store_wave_file(const char* file_name, size_t sample_rate, size_t sample_count, const float* left_channel, const float* rigth_channel)
{
	if (!left_channel && !rigth_channel)
		return EINVAL;
	size_t channel_count = (left_channel && rigth_channel) ? 2 : 1;
	uintptr_t wav = (uintptr_t)malloc(44 + (channel_count * sample_count * 4));
	if (!wav)
		return ENOMEM;
	*(uint8_t*)(wav) = (uint8_t)'R';
	*(uint8_t*)(wav + 1) = (uint8_t)'I';
	*(uint8_t*)(wav + 2) = (uint8_t)'F';
	*(uint8_t*)(wav + 3) = (uint8_t)'F';
	*(uint32_t*)(wav + 4) = (uint32_t)(36 + (channel_count * sample_count * 4));
	*(uint8_t*)(wav + 8) = (uint8_t)'W';
	*(uint8_t*)(wav + 9) = (uint8_t)'A';
	*(uint8_t*)(wav + 10) = (uint8_t)'V';
	*(uint8_t*)(wav + 11) = (uint8_t)'E';
	*(uint8_t*)(wav + 12) = (uint8_t)'f';
	*(uint8_t*)(wav + 13) = (uint8_t)'m';
	*(uint8_t*)(wav + 14) = (uint8_t)'t';
	*(uint8_t*)(wav + 15) = (uint8_t)' ';
	*(uint32_t*)(wav + 16) = 16;
	*(uint16_t*)(wav + 20) = 3;
	*(uint16_t*)(wav + 22) = (uint16_t)channel_count;
	*(uint32_t*)(wav + 24) = (uint32_t)sample_rate;
	*(uint32_t*)(wav + 28) = (uint32_t)(channel_count * sample_rate * 4);
	*(uint16_t*)(wav + 32) = (uint16_t)(channel_count * 4);
	*(uint16_t*)(wav + 34) = 32;
	*(uint8_t*)(wav + 36) = (uint8_t)'d';
	*(uint8_t*)(wav + 37) = (uint8_t)'a';
	*(uint8_t*)(wav + 38) = (uint8_t)'t';
	*(uint8_t*)(wav + 39) = (uint8_t)'a';
	*(uint32_t*)(wav + 40) = (uint32_t)(channel_count * sample_count * 4);
	if (channel_count == 1)
		memcpy((void*)(wav + 44), left_channel ? left_channel : rigth_channel, sample_count * 4);
	else
		for (float* i = (float*)(wav + 44), * e = i + 2 * sample_count; i != e; ++left_channel, ++rigth_channel, i += 2)
		{
			i[0] = *left_channel;
			i[1] = *rigth_channel;
		}
	float signal_peak = rwl_get_signal_absolute_peak(channel_count * sample_count, (const float*)(wav + 44));
	if (signal_peak > 0.0009765625f)
		rwl_scale_signal(channel_count * sample_count, (float*)(wav + 44), 1.0f / signal_peak);
	int error = rwl_store_file(file_name, 44 + (channel_count * sample_count * 4), (const void*)wav);
	free((void*)wav);
	return error;
}

#ifdef __cplusplus
}
#endif