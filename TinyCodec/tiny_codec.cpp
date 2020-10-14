/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

#include <inttypes.h>
#include <stdlib.h>

#include "tiny_codec.h"

void av_init_packet(AVPacket* pkt)
{
	pkt->pos = 0;
	pkt->data = NULL;
	pkt->size = 0;
	pkt->allocated_size = 0;
	pkt->flags = 0;
	pkt->stream_index = 0;
}

int av_get_packet(FILE* pb, AVPacket* pkt, int size)
{
	int ret = 0;
	if (pkt->allocated_size < size)
	{
		pkt->allocated_size = size + 1024 - size % 1024;
		if (pkt->data)
		{
			free(pkt->data);
		}
		pkt->data = (uint8_t*)malloc(pkt->allocated_size);
	}
	pkt->size = size;
	pkt->pts = 0;
	pkt->duration = 0;
	pkt->flags = 0;
	ret = RWread(pb, pkt->data, 1, size);
	pkt->pos = RWtell(pb);
	return ret;
}

void av_packet_unref(AVPacket* pkt)
{
	if (pkt->data)
	{
		free(pkt->data);
	}
	pkt->data = NULL;
	pkt->allocated_size = 0;
	pkt->size = 0;
}

void codec_init(struct tiny_codec_s* s, FILE* rw)
{
	s->input = rw;
	s->private_context = NULL;
	s->free_context = NULL;
	s->fps_num = 24;
	s->fps_denum = 1;
	s->time_ns = 0;
	s->frame = 0;

	av_init_packet(&s->audio.pkt);
	s->audio.pkt.is_video = 0;
	s->audio.buff_allocated_size = 0;
	s->audio.buff_size = 0;
	s->audio.buff_offset = 0;
	s->audio.buff = NULL;
	s->audio.buff_p = NULL;
	s->audio.entry = NULL;
	s->audio.entry_size = 0;
	s->audio.entry_current = 0;
	s->audio.extradata_size = 0;
	s->audio.extradata = NULL;
	s->audio.priv_data = NULL;
	s->audio.free_data = NULL;
	s->audio.decode = NULL;
	s->audio.codec_tag = 0;
	s->audio.block_align = 0;

	av_init_packet(&s->video.pkt);
	s->video.pkt.is_video = 1;
	s->video.rgba = NULL;
	s->video.entry = NULL;
	s->video.entry_size = 0;
	s->video.entry_current = 0;
	s->video.private_data = NULL;
	s->video.free_data = NULL;
	s->video.decode = NULL;
	s->video.codec_tag = 0;
}

void codec_clear(struct tiny_codec_s* s)
{
	if (s->free_context)
	{
		s->free_context(s->private_context);
		s->free_context = NULL;
		s->private_context = NULL;
	}
	s->packet = NULL;
	av_packet_unref(&s->video.pkt);
	s->video.decode = NULL;
	s->video.codec_tag = 0;
	av_packet_unref(&s->audio.pkt);
	s->audio.decode = NULL;
	s->audio.codec_tag = 0;
	s->audio.buff_allocated_size = 0;
	s->audio.buff_size = 0;
	s->audio.buff_offset = 0;

	if (s->video.entry)
	{
		free(s->video.entry);
		s->video.entry = NULL;
		s->video.entry_size = 0;
		s->video.entry_current = 0;
	}
	if (s->video.free_data)
	{
		s->video.free_data(s->video.private_data);
		s->video.private_data = NULL;
		s->video.free_data = NULL;
	}
	if (s->video.rgba)
	{
		free(s->video.rgba);
		s->video.rgba = NULL;
	}

	if (s->audio.buff)
	{
		free(s->audio.buff);
		s->audio.buff = NULL;
	}
	if (s->audio.buff_p)
	{
		free(s->audio.buff_p);
		s->audio.buff_p = NULL;
	}
	if (s->audio.entry)
	{
		free(s->audio.entry);
		s->audio.entry = NULL;
		s->audio.entry_size = 0;
		s->audio.entry_current = 0;
	}
	if (s->audio.free_data)
	{
		s->audio.free_data(s->audio.priv_data);
		s->audio.priv_data = NULL;
		s->audio.free_data = NULL;
	}
	if (s->input)
	{
		RWclose(s->input);
		s->input = NULL;
	}
}

void codec_simplify_fps(struct tiny_codec_s* s)
{
	if (s->fps_denum && s->fps_num)
	{
		while ((s->fps_denum % 10 == 0) && (s->fps_num % 10 == 0))
		{
			s->fps_denum /= 10;
			s->fps_num /= 10;
		}
	}
}

uint64_t codec_inc_time(struct tiny_codec_s* s, uint64_t time_ns)
{
	s->time_ns += time_ns;
	s->frame = (s->time_ns * s->fps_num) / (s->fps_denum * 1e9);
	return s->frame;
}

uint32_t codec_resize_audio_buffer(struct tiny_codec_s* s, uint32_t sample_size, uint32_t samples)
{
	uint32_t bytes_per_channel = sample_size * samples;
	uint32_t ret = s->audio.channels * bytes_per_channel;
	if (s->audio.buff_allocated_size < ret)
	{
		s->audio.buff_allocated_size = ret + 1024 - ret % 1024;
		if (s->audio.buff)
		{
			free(s->audio.buff);
		}
		if (s->audio.buff_p)
		{
			free(s->audio.buff_p);
		}
		s->audio.buff = (uint8_t*)malloc(s->audio.buff_allocated_size);
		s->audio.buff_p = (uint8_t * *)malloc(s->audio.channels * sizeof(int16_t*));
	}

	for (uint16_t i = 0; i < s->audio.channels; ++i)
	{
		s->audio.buff_p[i] = s->audio.buff + bytes_per_channel * i;
	}

	return ret;
}

FILE* RWFromFile(const char* file, const char* mode)
{
	return fopen(file, mode);
}

int RWclose(FILE* context)
{
	return fclose(context);
}

long long RWtell(FILE* context)
{
	return ftell(context);
}

long long RWseek(FILE* context, long long offset, int whence)
{
	return _fseeki64(context, offset, whence);
}

int RWread(FILE* context, void* ptr, int size, int maxnum)
{
	return fread(ptr, size, maxnum, context);
}

unsigned long ReadLE32(FILE* src)
{
	unsigned long output = 0;
	fread(&output, sizeof(unsigned long), 1, src);
	return output;
}