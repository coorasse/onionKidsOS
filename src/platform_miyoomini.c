/* Miyoo Mini / Mini Plus running MinUI: SDL 1.2, 640x480 16 bit, buttons arrive as keyboard keys.
 * Games are started the MinUI way: the command is written to /tmp/next and the launcher exits,
 * pak/launch.sh runs the command and restarts the launcher afterwards. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "platform.h"
#include "library.h"
#include "gfx.h"

#define SDCARD "/mnt/SDCARD"
#define SYSTEM_PAKS SDCARD "/.system/miyoomini/paks"
#define AUTOSLEEP_MS 60000
#define SLEEP_POWEROFF_MS 120000
#define POWER_HOLD_MS 1000

static SDL_Surface *screen;
static uint32_t held;
static uint32_t last_input_at, power_pressed_at;
static int power_down, sleep_requested;

static void put_file(const char *path, const char *value)
{
	FILE *f = fopen(path, "w");
	if (!f) return;
	fputs(value, f);
	fclose(f);
}

static int exists(const char *path)
{
	return access(path, F_OK) == 0;
}

SDL_Surface *PLAT_init(void)
{
	putenv("SDL_HIDE_BATTERY=1");
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
	SDL_ShowCursor(0);
	screen = SDL_SetVideoMode(SCREEN_W, SCREEN_H, 16, SDL_SWSURFACE);
	if (!screen) {
		fprintf(stderr, "SDL_SetVideoMode: %s\n", SDL_GetError());
		exit(1);
	}
	last_input_at = SDL_GetTicks();
	return screen;
}

void PLAT_quit(void)
{
	SDL_Quit();
}

void PLAT_flip(SDL_Surface *s)
{
	SDL_Flip(s);
}

static uint32_t key_to_button(SDLKey key)
{
	switch (key) {
	case SDLK_UP: return BTN_UP;
	case SDLK_DOWN: return BTN_DOWN;
	case SDLK_LEFT: return BTN_LEFT;
	case SDLK_RIGHT: return BTN_RIGHT;
	case SDLK_SPACE: return BTN_A;
	case SDLK_LCTRL: return BTN_B;
	case SDLK_LSHIFT: return BTN_X;
	case SDLK_LALT: return BTN_Y;
	case SDLK_e: return BTN_L1;
	case SDLK_t: return BTN_R1;
	case SDLK_RCTRL: return BTN_SELECT;
	case SDLK_RETURN: return BTN_START;
	case SDLK_ESCAPE: return BTN_MENU;
	default: return 0;
	}
}

void PLAT_poll_input(void)
{
	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_POWER) {
			power_down = 1;
			power_pressed_at = SDL_GetTicks();
		} else if (e.type == SDL_KEYUP && e.key.keysym.sym == SDLK_POWER) {
			if (power_down) sleep_requested = 1;
			power_down = 0;
		} else if (e.type == SDL_KEYDOWN) {
			held |= key_to_button(e.key.keysym.sym);
		} else if (e.type == SDL_KEYUP) {
			held &= ~key_to_button(e.key.keysym.sym);
		}
	}
	pad_update(held);
}

static void set_backlight(int on)
{
	if (on) {
		put_file("/sys/class/gpio/gpio4/value", "1");
		put_file("/sys/class/gpio/unexport", "4");
		put_file("/sys/class/pwm/pwmchip0/export", "0");
		put_file("/sys/class/pwm/pwmchip0/pwm0/enable", "0");
		put_file("/sys/class/pwm/pwmchip0/pwm0/enable", "1");
	} else {
		put_file("/sys/class/gpio/export", "4");
		put_file("/sys/class/gpio/gpio4/direction", "out");
		put_file("/sys/class/gpio/gpio4/value", "0");
	}
}

static void power_off(void)
{
	gfx_set_target(NULL);
	gfx_clear(0x000000);
	gfx_text(font_big, "Powering off", 0xFFFFFF, SCREEN_W / 2, SCREEN_H / 2 - 20, ALIGN_CENTER);
	PLAT_flip(screen);
	sync();
	sleep(1);
	set_backlight(0);
	system("shutdown");
	while (1) pause();
}

/* Screen off until the power button is released again; power off after a while. */
static void faux_sleep(void)
{
	set_backlight(0);
	system("killall -STOP keymon.elf");
	sync();
	uint32_t slept_at = SDL_GetTicks();
	for (;;) {
		SDL_Event e;
		int wake = 0;
		while (SDL_PollEvent(&e))
			if (e.type == SDL_KEYUP && e.key.keysym.sym == SDLK_POWER) wake = 1;
		if (wake) break;
		if (SDL_GetTicks() - slept_at >= SLEEP_POWEROFF_MS) power_off();
		SDL_Delay(200);
	}
	system("killall -CONT keymon.elf");
	set_backlight(1);
	held = 0;
	power_down = 0;
	pad_update(0);
}

void PLAT_power_update(uint32_t now)
{
	if (pad.held) last_input_at = now;
	if (power_down && now - power_pressed_at >= POWER_HOLD_MS) power_off();
	if (sleep_requested || now - last_input_at >= AUTOSLEEP_MS) {
		sleep_requested = 0;
		faux_sleep();
		last_input_at = SDL_GetTicks();
	}
}

uint32_t PLAT_ticks(void)
{
	return SDL_GetTicks();
}

void PLAT_delay(uint32_t ms)
{
	SDL_Delay(ms);
}

const char *PLAT_sdcard_root(void)
{
	const char *env = getenv("KIDS_ROOT");
	return env && *env ? env : SDCARD;
}

const char *PLAT_font_path(void)
{
	return "res/BPreplayBold-unhinted.otf";
}

int PLAT_save_screenshot(SDL_Surface *s, const char *path)
{
	return SDL_SaveBMP(s, path);
}

static void escape_single_quotes(const char *in, char *out, size_t n)
{
	size_t o = 0;
	for (; *in && o + 5 < n; in++) {
		if (*in == '\'') {
			memcpy(out + o, "'\\''", 4);
			o += 4;
		} else {
			out[o++] = *in;
		}
	}
	out[o] = 0;
}

int PLAT_launch(const struct Game *game, int mode)
{
	(void)mode; /* MinUI resumes on its own */
	char emu[1024];
	snprintf(emu, sizeof(emu), "%s/Emus/%s.pak/launch.sh", SYSTEM_PAKS, game->system_tag);
	if (!exists(emu)) snprintf(emu, sizeof(emu), "%s/Emus/miyoomini/%s.pak/launch.sh", SDCARD, game->system_tag);
	if (!exists(emu)) {
		fprintf(stderr, "no emulator pak for %s\n", game->system_tag);
		return LAUNCH_FAKE;
	}
	char emu_q[2048], rom_q[2048], cmd[4200];
	escape_single_quotes(emu, emu_q, sizeof(emu_q));
	escape_single_quotes(game->path, rom_q, sizeof(rom_q));
	snprintf(cmd, sizeof(cmd), "'%s' '%s'", emu_q, rom_q);
	put_file("/tmp/next", cmd);
	sync();
	return LAUNCH_HANDOVER;
}

void PLAT_audio_init(void)
{
}

void PLAT_audio_click(void)
{
}

void PLAT_audio_quit(void)
{
}

void PLAT_volume(int *level, int *max, int *mute)
{
	*level = 10;
	*max = 20;
	*mute = 0;
}

void PLAT_exit_to_system(void)
{
	put_file("/tmp/kids_exit", "1");
	sync();
}
