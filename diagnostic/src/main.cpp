// SPDX-License-Identifier: GPL-3.0-or-later
#include <SDL2/SDL.h>

#include <stdint.h>
#include <stdio.h>

namespace {

constexpr int kWidth = 1920;
constexpr int kHeight = 1080;
constexpr int kCrossButton = 0;
constexpr int kCircleButton = 1;
constexpr int kAudioFrames = 48000 / 3;

volatile int gToneFramesRemaining = kAudioFrames;
uint32_t gTonePhase = 0;
FILE *gLog = nullptr;

void logLine(const char *message) {
	printf("[scummvm-ps4-diag] %s\n", message);
	if (!gLog)
		return;
	fprintf(gLog, "%s\n", message);
	fflush(gLog);
}

void audioCallback(void *, Uint8 *stream, int length) {
	Sint16 *samples = reinterpret_cast<Sint16 *>(stream);
	const int sampleCount = length / static_cast<int>(sizeof(Sint16));

	for (int i = 0; i < sampleCount; i += 2) {
		Sint16 sample = 0;
		if (gToneFramesRemaining > 0) {
			// Integer square wave: deliberately simple and independent of libm.
			sample = (gTonePhase++ % 109) < 54 ? 5000 : -5000;
			--gToneFramesRemaining;
		}
		samples[i] = sample;
		if (i + 1 < sampleCount)
			samples[i + 1] = sample;
	}
}

void fill(SDL_Renderer *renderer, const SDL_Rect &rect, Uint8 r, Uint8 g, Uint8 b) {
	SDL_SetRenderDrawColor(renderer, r, g, b, SDL_ALPHA_OPAQUE);
	SDL_RenderFillRect(renderer, &rect);
}

} // namespace

int main(int, char **) {
	setvbuf(stdout, nullptr, _IONBF, 0);
	gLog = fopen("/data/scummvm-ps4-diagnostic.log", "w");
	logLine("entered main");

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) != 0) {
		logLine(SDL_GetError());
		return 1;
	}
	logLine("SDL initialized");

	SDL_DisplayMode display{};
	if (SDL_GetCurrentDisplayMode(0, &display) != 0 || display.w <= 0 || display.h <= 0) {
		logLine("could not determine display size");
		SDL_Quit();
		return 2;
	}
	// The PS4 software framebuffer clips using unsigned window coordinates.
	// An oversized, implicitly centered window can therefore render off-screen.
	SDL_Window *window = SDL_CreateWindow("ScummVM PS4 Diagnostic",
		0, 0, display.w, display.h, 0);
	if (!window) {
		logLine(SDL_GetError());
		SDL_Quit();
		return 2;
	}

	SDL_Surface *surface = SDL_GetWindowSurface(window);
	SDL_Renderer *renderer = surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
	if (!renderer) {
		logLine(SDL_GetError());
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 3;
	}
	logLine("video initialized");
	SDL_RenderSetLogicalSize(renderer, kWidth, kHeight);

	SDL_AudioSpec wanted{};
	wanted.freq = 48000;
	wanted.format = AUDIO_S16SYS;
	wanted.channels = 2;
	wanted.samples = 512;
	wanted.callback = audioCallback;
	SDL_AudioSpec obtained{};
	SDL_AudioDeviceID audioDevice = SDL_OpenAudioDevice(nullptr, 0, &wanted, &obtained, 0);
	const bool audioReady = audioDevice != 0;
	if (audioReady) {
		logLine("audio initialized");
		SDL_PauseAudioDevice(audioDevice, 0);
	} else {
		logLine(SDL_GetError());
	}

	SDL_Joystick *joystick = nullptr;
	if (SDL_NumJoysticks() > 0)
		joystick = SDL_JoystickOpen(0);
	const bool controllerReady = joystick != nullptr;
	logLine(controllerReady ? "controller initialized" : "controller unavailable");

	bool running = true;
	bool alternatePalette = false;
	while (running) {
		SDL_Event event{};
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT)
				running = false;
			if (event.type != SDL_JOYBUTTONDOWN)
				continue;

			if (event.jbutton.button == kCrossButton) {
				alternatePalette = !alternatePalette;
				if (audioReady) {
					SDL_LockAudioDevice(audioDevice);
					gToneFramesRemaining = kAudioFrames;
					SDL_UnlockAudioDevice(audioDevice);
				}
				logLine("cross pressed; tone restarted");
			} else if (event.jbutton.button == kCircleButton) {
				logLine("circle pressed; exiting");
				running = false;
			} else {
				logLine("controller button pressed");
			}
		}

		SDL_SetRenderDrawColor(renderer, alternatePalette ? 45 : 12,
			alternatePalette ? 20 : 36, alternatePalette ? 72 : 68, SDL_ALPHA_OPAQUE);
		SDL_RenderClear(renderer);

		const SDL_Rect videoBar{120, 130, 1680, 190};
		const SDL_Rect audioBar{120, 445, 1680, 190};
		const SDL_Rect controllerBar{120, 760, 1680, 190};
		fill(renderer, videoBar, 20, 210, 100);
		fill(renderer, audioBar, audioReady ? 20 : 210, audioReady ? 210 : 35, 35);
		fill(renderer, controllerBar, controllerReady ? 20 : 210,
			controllerReady ? 210 : 35, 35);

		SDL_RenderPresent(renderer);
		if (SDL_UpdateWindowSurface(window) != 0) {
			logLine(SDL_GetError());
			running = false;
		}
		SDL_Delay(16);
	}

	if (joystick)
		SDL_JoystickClose(joystick);
	if (audioDevice)
		SDL_CloseAudioDevice(audioDevice);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	logLine("clean shutdown");
	if (gLog)
		fclose(gLog);
	return 0;
}
