#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <SDL.h>

#define SCREEN_W 640
#define SCREEN_H 480

#if defined(PLATFORM_ONION)
#define HOST_SYSTEM_NAME "ONION"
#elif defined(PLATFORM_MIYOOMINI)
#define HOST_SYSTEM_NAME "MINUI"
#else
#define HOST_SYSTEM_NAME "DESKTOP"
#endif

enum {
	BTN_UP = 1 << 0,
	BTN_DOWN = 1 << 1,
	BTN_LEFT = 1 << 2,
	BTN_RIGHT = 1 << 3,
	BTN_A = 1 << 4,
	BTN_B = 1 << 5,
	BTN_X = 1 << 6,
	BTN_Y = 1 << 7,
	BTN_L1 = 1 << 8,
	BTN_R1 = 1 << 9,
	BTN_START = 1 << 10,
	BTN_SELECT = 1 << 11,
	BTN_MENU = 1 << 12,
	BTN_VOL_UP = 1 << 13,   /* the host changes the volume, the launcher only shows it */
	BTN_VOL_DOWN = 1 << 14,
	BTN_COUNT = 15,
};

typedef struct {
	uint32_t held;
	uint32_t pressed;
	uint32_t released;
	uint32_t repeat; /* pressed, plus auto-repeat while held */
	int quit;
} Input;

extern Input pad;

void pad_update(uint32_t held);
void input_update_repeat(uint32_t now);

struct Game;

SDL_Surface *PLAT_init(void);
void PLAT_quit(void);
void PLAT_flip(SDL_Surface *screen);
void PLAT_poll_input(void);
uint32_t PLAT_ticks(void);
void PLAT_delay(uint32_t ms);
const char *PLAT_sdcard_root(void);
const char *PLAT_font_path(void);
int PLAT_save_screenshot(SDL_Surface *screen, const char *path);

/* LAUNCH_HANDOVER: the game was handed over to the OS and the launcher must exit.
 * LAUNCH_RETURNED: the game ran to completion inside this call (desktop RetroArch).
 * LAUNCH_FAKE: this platform cannot launch anything and the caller should fake it. */
enum { LAUNCH_FAKE, LAUNCH_HANDOVER, LAUNCH_RETURNED };
/* mode: LAUNCH_RESUME loads the auto save state, LAUNCH_FRESH ignores it, LAUNCH_DEFAULT leaves it to the host. */
enum { LAUNCH_DEFAULT, LAUNCH_RESUME, LAUNCH_FRESH };
int PLAT_launch(const struct Game *game, int mode);

/* Sleep, autosleep and power button; a no-op on the desktop. */
void PLAT_power_update(uint32_t now);
/* Menu music and the selection click, taken from the host (the stock Miyoo sounds on Onion). Nothing
 * plays when the host has no sounds, the parent muted the theme music, or the launcher runs scripted. */
void PLAT_audio_init(void);
void PLAT_audio_click(void);
void PLAT_audio_quit(void);

/* Current volume as the host keeps it, for the on-screen indicator. */
void PLAT_volume(int *level, int *max, int *mute);
/* Two-letter code of the host's language setting ("de"), NULL when the host has none. */
const char *PLAT_language(void);
/* Ask the host launcher (MinUI) to take over once the main loop exits. */
void PLAT_exit_to_system(void);

#endif
