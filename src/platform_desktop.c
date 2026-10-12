#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <SDL_image.h>
#include "platform.h"
#include "library.h"

static SDL_Window *window;
static SDL_Surface *screen;
static uint32_t held;

SDL_Surface *PLAT_init(void)
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) != 0) {
		fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		exit(1);
	}
	window = SDL_CreateWindow("Kids OS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W, SCREEN_H, 0);
	if (!window) {
		fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
		exit(1);
	}
	screen = SDL_GetWindowSurface(window);
	return screen;
}

void PLAT_quit(void)
{
	SDL_DestroyWindow(window);
	SDL_Quit();
}

void PLAT_flip(SDL_Surface *s)
{
	(void)s;
	SDL_UpdateWindowSurface(window);
}

static uint32_t key_to_button(SDL_Keycode key)
{
	switch (key) {
	case SDLK_UP: return BTN_UP;
	case SDLK_DOWN: return BTN_DOWN;
	case SDLK_LEFT: return BTN_LEFT;
	case SDLK_RIGHT: return BTN_RIGHT;
	case SDLK_x: return BTN_A;
	case SDLK_z: return BTN_B;
	case SDLK_s: return BTN_X;
	case SDLK_a: return BTN_Y;
	case SDLK_q: return BTN_L1;
	case SDLK_w: return BTN_R1;
	case SDLK_RETURN: return BTN_START;
	case SDLK_RSHIFT:
	case SDLK_TAB: return BTN_SELECT;
	case SDLK_ESCAPE: return BTN_MENU;
	case SDLK_EQUALS:
	case SDLK_PLUS: return BTN_VOL_UP;
	case SDLK_MINUS: return BTN_VOL_DOWN;
	default: return 0;
	}
}

void PLAT_poll_input(void)
{
	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		if (e.type == SDL_QUIT) pad.quit = 1;
		if (e.type == SDL_KEYDOWN && !e.key.repeat) held |= key_to_button(e.key.keysym.sym);
		if (e.type == SDL_KEYUP) held &= ~key_to_button(e.key.keysym.sym);
	}
	pad_update(held);
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
	return env && *env ? env : "sdcard";
}

const char *PLAT_font_path(void)
{
	static const char *candidates[] = {
		"assets/Fredoka-Bold.ttf",
		"/System/Library/Fonts/Supplemental/Arial Rounded Bold.ttf",
		"/System/Library/Fonts/Supplemental/Verdana Bold.ttf",
		"/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
		NULL,
	};
	const char *env = getenv("KIDS_FONT");
	if (env && *env) return env;
	for (int i = 0; candidates[i]; i++)
		if (access(candidates[i], R_OK) == 0) return candidates[i];
	return candidates[0];
}

int PLAT_save_screenshot(SDL_Surface *s, const char *path)
{
	return IMG_SavePNG(s, path);
}

/* Runs the rom in RetroArch for macOS with the libretro core that Onion's Emu/<TAG>/launch.sh names on
 * the device, so what is played here is what the Miyoo would play. */
#define RETROARCH_APP "/Applications/RetroArch.app/Contents/MacOS/RetroArch"

/* cores without a macOS build, with what RetroArch for macOS offers instead */
static const char *const CORE_FALLBACKS[][2] = {
	{"mednafen_supafaust", "snes9x"},
	{"picodrive", "genesis_plus_gx"},
	{"pcsx_rearmed", "swanstation"},
	{"mame2003_plus", "fbneo"},
	{"fbalpha2012_neogeo", "fbneo"},
	{NULL, NULL},
};

/* "cores/gambatte_libretro.so" inside the device launch script -> "gambatte" */
static int core_from_launcher(const char *launcher, char *out, size_t n)
{
	FILE *f = fopen(launcher, "r");
	if (!f) return 0;
	char line[2048];
	int found = 0;
	while (!found && fgets(line, sizeof(line), f)) {
		char *p = strstr(line, "cores/");
		char *e = p ? strstr(p, "_libretro") : NULL;
		if (!p || !e) continue;
		p += strlen("cores/");
		snprintf(out, n, "%.*s", (int)(e - p), p);
		found = 1;
	}
	fclose(f);
	return found;
}

static int core_path(const char *core, char *out, size_t n)
{
	const char *home = getenv("HOME");
	snprintf(out, n, "%s/Library/Application Support/RetroArch/cores/%s_libretro.dylib", home ? home : "", core);
	return access(out, R_OK) == 0;
}

int PLAT_launch(const struct Game *game, int mode)
{
	const System *sys = &lib.systems[game->system];
	char core[64] = "", dylib[1024];
	if (!core_from_launcher(sys->launcher, core, sizeof(core))) {
		printf("[desktop] no libretro core in %s, pretending to play %s\n", sys->launcher, game->name);
		return LAUNCH_FAKE;
	}
	if (!core_path(core, dylib, sizeof(dylib))) {
		const char *alt = NULL;
		for (int i = 0; CORE_FALLBACKS[i][0]; i++)
			if (strcmp(CORE_FALLBACKS[i][0], core) == 0) alt = CORE_FALLBACKS[i][1];
		if (!alt || !core_path(alt, dylib, sizeof(dylib))) {
			printf("[desktop] core %s%s%s not installed in RetroArch, pretending to play %s\n",
			       core, alt ? " nor " : "", alt ? alt : "", game->name);
			return LAUNCH_FAKE;
		}
	}
	const char *app = getenv("KIDS_RETROARCH");
	if (!app || !*app) app = RETROARCH_APP;
	if (access(app, X_OK) != 0) {
		printf("[desktop] %s not found, pretending to play %s\n", app, game->name);
		return LAUNCH_FAKE;
	}

	/* same knobs Onion sets through its append config, plus the thumbnail the resume screen shows */
	const char *cfg = "/tmp/kidsos_retroarch.cfg";
	FILE *f = fopen(cfg, "w");
	if (f) {
		fprintf(f, "savestate_auto_save = \"true\"\nsavestate_thumbnail_enable = \"true\"\n");
		if (mode != LAUNCH_DEFAULT) fprintf(f, "savestate_auto_load = \"%s\"\n", mode == LAUNCH_RESUME ? "true" : "false");
		fclose(f);
	}
	printf("[desktop] launching %s with %s (%s)\n", game->path, dylib,
	       mode == LAUNCH_RESUME ? "resume" : mode == LAUNCH_FRESH ? "fresh start" : "default");
	pid_t pid = fork();
	if (pid == 0) {
		execl(app, app, "--appendconfig", cfg, "-L", dylib, game->path, (char *)NULL);
		_exit(127);
	}
	int status = 0;
	if (pid > 0) waitpid(pid, &status, 0);
	printf("[desktop] RetroArch exited with %d\n", WIFEXITED(status) ? WEXITSTATUS(status) : -1);
	/* keys released while RetroArch had the focus never reached us */
	SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
	held = 0;
	return LAUNCH_RETURNED;
}

void PLAT_power_update(uint32_t now)
{
	(void)now;
}

#ifdef HAVE_MIXER
#include <SDL_mixer.h>
static Mix_Music *music;
static Mix_Chunk *click;
#endif

/* the stock sounds are not part of the repository: copy them from the device into build/sound/ */
void PLAT_audio_init(void)
{
#ifdef HAVE_MIXER
	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
		printf("[desktop] no audio: %s\n", Mix_GetError());
		return;
	}
	music = Mix_LoadMUS("build/sound/bgm.mp3");
	click = Mix_LoadWAV("build/sound/change.wav");
	if (music) {
		Mix_VolumeMusic(MIX_MAX_VOLUME / 2);
		Mix_PlayMusic(music, -1);
	}
	printf("[desktop] audio: music %s, click %s\n", music ? "on" : "off (no build/sound/bgm.mp3)", click ? "on" : "off");
#endif
}

void PLAT_audio_click(void)
{
#ifdef HAVE_MIXER
	if (click) Mix_PlayChannel(-1, click, 0);
#endif
}

void PLAT_audio_quit(void)
{
#ifdef HAVE_MIXER
	if (music) Mix_FreeMusic(music);
	if (click) Mix_FreeChunk(click);
	music = NULL;
	click = NULL;
	Mix_CloseAudio();
#endif
}

void PLAT_volume(int *level, int *max, int *mute)
{
	static int current = 9;
	if (pad.pressed & BTN_VOL_UP && current < 20) current++;
	if (pad.pressed & BTN_VOL_DOWN && current > 0) current--;
	*level = current;
	*max = 20;
	*mute = 0;
}

void PLAT_exit_to_system(void)
{
	printf("[desktop] exit to system launcher\n");
}
