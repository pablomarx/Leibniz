//
//  sdlnewton.c
//  Leibniz
//
//  Created by Steve White on 2/19/2017.
//  Copyright (c) 2017 Steve White. All rights reserved.
//

#include <stdint.h>
#include <unistd.h>

#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>

#include "newton.h"
#include "HammerConfigBits.h"
#include "silkscreen.xbm"

newton_t *gNewton = NULL;
SDL_Surface *gScreen = NULL;
bool gNeedsSilkScreen = true;

void newton_display_open(int width, int height) {
	gScreen = SDL_SetVideoMode(width, height + silkscreen_height, 8, SDL_HWSURFACE|SDL_DOUBLEBUF);
	if (!gScreen) {
		fprintf(stderr, "SDL_SetVideoMode returned NULL\n");
		return;
	}
	
	// Create a grayscale palette.  While Runt
	// devices were only 1bpp, the emulator does
	// use a gray color instead of black to indicate
	// the device is asleep.
	SDL_Color colors[256];
	for(int i=0;i<256;i++){
		colors[i].r=i;
		colors[i].g=i;
		colors[i].b=i;
	}
	SDL_SetColors(gScreen, colors, 0, 256);
	
	SDL_WM_SetCaption("Leibniz","Leibniz");
}


void newton_display_set_framebuffer(const uint8_t *src, int width, int height) {
	uint8_t *dest;
	if (gScreen == NULL) {
		return;
	}
	
	SDL_LockSurface(gScreen);
	
	dest = (uint8_t *)gScreen->pixels;
	int destidx = 0;
	for (int x=0; x<width; x++) {
		for (int y=height-1; y>=0; y--) {
			int srcidx = (y * width) + x;
			dest[destidx++] = src[srcidx];
		}
	}
	
	if (gNeedsSilkScreen == true) {
		// Meh. Couldn't get this to work inside newton_display_open().
		for (int srcidx=0; srcidx<(silkscreen_width * silkscreen_height)/8; srcidx++) {
			uint8_t pixels = silkscreen_bits[srcidx];
			for (int bit=0; bit<8; bit++) {
				dest[destidx++] = (((pixels >> bit) & 1) ? 0x00 : 0xff);
			}
		}
		gNeedsSilkScreen = false;
	}
	
	SDL_UnlockSurface(gScreen);
	SDL_Flip(gScreen);
}

static int cpu_startup_thread_entry(void *args)
{
	SDL_Event event;
	
	newton_emulate(gNewton, INT32_MAX);
	
	event.type = SDL_QUIT;
	SDL_PushEvent(&event);
	
	return 0;
}

int start_cpu(void)
{
	SDL_CreateThread(&cpu_startup_thread_entry, NULL);
	return 0;
}

static int init_sdl(void)
{
	atexit(SDL_Quit);
	
	return SDL_Init(SDL_INIT_TIMER);
}

static int init_display(void)
{
	return SDL_InitSubSystem(SDL_INIT_VIDEO);
}

void handle_mouse_event(SDL_Event event) {
	if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
		if (event.button.button == SDL_BUTTON_LEFT) {
			if (event.type == SDL_MOUSEBUTTONDOWN) {
				newton_touch_down(gNewton, event.button.y, 240-event.button.x);
			}
			else {
				newton_touch_up(gNewton);
			}
		}
	}
	else if (event.type == SDL_MOUSEMOTION) {
		if ((event.motion.state & SDL_BUTTON_LMASK) == SDL_BUTTON_LMASK) {
			newton_touch_down(gNewton, event.motion.y, 240-event.motion.x);
		}
	}
}

void handle_keyup_event(SDL_Event event) {
	char sym = event.key.keysym.sym;
	if (sym == ' ') {
		runt_switch_toggle(newton_get_runt(gNewton), RuntSwitchPower);
	}	
}

int system_message_loop(void) {
	SDL_Event event;
	int quit = 0;
	while (!quit) {
		SDL_WaitEvent(&event);
		
		switch (event.type) {
			case SDL_KEYUP:
				handle_keyup_event(event);
				break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEMOTION:
				handle_mouse_event(event);
				break;
			case SDL_QUIT:
				quit = 1;
				break;
		}
	}
	
	return 0;
}

int main(int argc, char **argv) {
	int result = 0;
	
	if (argc != 2) {
		fprintf(stderr, "usage: %s romfile\n", argv[0]);
		result = -1;
		goto out;
	}
	
	gNewton = newton_new();
	if (newton_load_rom(gNewton, argv[1]) == -1) {
		result = -1;
		goto out;
	}
	
	if (init_sdl() < 0) {
		fprintf(stderr, "init_sdl failed\n");
		result = -1;
		goto out;
	}
	
	if (init_display() < 0) {
		fprintf(stderr, "init_display failed\n");
		result = -1;
		goto out;
	}
	
	if (start_cpu() < 0) {
		fprintf(stderr, "start_cpu failed\n");
		result = -1;
		goto out;
	}
	
	system_message_loop();
	
out:
	return result;
}
