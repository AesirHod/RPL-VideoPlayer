#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include "TinyCodec/tiny_codec.h"

#define SDL_AUDIO_BUFFER_SIZE 1024

using namespace std;

tiny_codec_t engine_video;
uint8_t* audio_buff = nullptr;

void audio_callback1(void* userdata, Uint8* stream, int len)
{
	while (len > 0)
	{
		if (audio_buff == nullptr || audio_buff >= (engine_video.audio.buff + engine_video.audio.buff_size))
		{
			codec_decode_audio(&engine_video);
			audio_buff = engine_video.audio.buff;
		}

		*stream = *audio_buff;
		stream++;
		audio_buff++;
		len--;
	}
}

int main(int argc, char** argv)
{
	if (argc <= 1 || argv[1] == "")
	{
		return -1;
	}

	//if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0)
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
	{
		//fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
		printf("Could not initialize SDL - %s\n", SDL_GetError());
		return -1;
	}

	SDL_Window* window = SDL_CreateWindow(
		"Video Player",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		320, 240, SDL_WINDOW_SHOWN
	);

	if (window == nullptr)
	{
		SDL_Log("Window could not be created: %s\n", SDL_GetError());
		return -1;
	}

	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
	if (renderer == nullptr)
	{
		SDL_Log("Renderer could not be created: %s\n", SDL_GetError());
		SDL_DestroyWindow(window);
		SDL_Quit();
		return -1;
	}

	SDL_Texture* texTarget = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, 320, 240);

	uint64_t frequency = SDL_GetPerformanceFrequency();
	uint64_t oldtime = SDL_GetPerformanceCounter();
	uint64_t time_ns = 0;

	codec_init(&engine_video, nullptr);

	bool canPlaySound = false;
	SDL_AudioDeviceID audioDevice = 0;
	SDL_AudioSpec audioSpec;
	SDL_zero(audioSpec);

	codec_init(&engine_video, RWFromFile(argv[1], "rb"));
	if (engine_video.input)
	{
		if (0 == codec_open_rpl(&engine_video))
		{
			codec_decode_video(&engine_video);

			SDL_AudioSpec desiredAudioSpec;
			SDL_zero(desiredAudioSpec);
			desiredAudioSpec.freq = engine_video.audio.sample_rate;
			desiredAudioSpec.format = AUDIO_S16SYS; // AUDIO_S8; // AUDIO_S16SYS;
			desiredAudioSpec.channels = engine_video.audio.channels;
			desiredAudioSpec.silence = 0;
			desiredAudioSpec.samples = SDL_AUDIO_BUFFER_SIZE;
			desiredAudioSpec.callback = audio_callback1;

			audioDevice = SDL_OpenAudioDevice(NULL, 0, &desiredAudioSpec, &audioSpec, SDL_AUDIO_ALLOW_ANY_CHANGE & ~SDL_AUDIO_ALLOW_FORMAT_CHANGE);
			SDL_PauseAudioDevice(audioDevice, 0);
		}
		else
		{
			codec_clear(&engine_video);
		}
	}

	while (true)
	{
		SDL_Event event;
		if (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
			{
				break;
			}
		}

		uint64_t newtime = SDL_GetPerformanceCounter();
		time_ns = newtime - oldtime;
		time_ns *= 1e9;
		time_ns /= frequency;
		oldtime = newtime;

		if (engine_video.input)
		{
			bool isVideoPlaying = false;
			uint64_t frame = engine_video.frame;
			codec_inc_time(&engine_video, time_ns);
			while (frame < engine_video.frame)
			{
				frame++;
				isVideoPlaying = (codec_decode_video(&engine_video)) ? true : false;
			}

			//if (isVideoPlaying)
			//{
				if (engine_video.video.rgba)
				{
					SDL_Rect rect;
					rect.x = 0;
					rect.y = 0;
					rect.w = engine_video.video.width;
					rect.h = engine_video.video.height;
					SDL_UpdateTexture(texTarget, &rect, engine_video.video.rgba, 1280);
					//SDL_SetTextureScaleMode(texTarget, SDL_ScaleModeBest);
				}
			//}
			//else
			//{
			//	codec_clear(&engine_video);
			//}
		}

		// SDL_SetRenderTarget(renderer, NULL);
		SDL_RenderClear(renderer);
		SDL_RenderCopyEx(renderer, texTarget, NULL, NULL, 0, NULL, SDL_FLIP_NONE);
		SDL_RenderPresent(renderer);
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}