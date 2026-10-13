/* Miyoo Mini Plus running OnionOS. Onion's runtime.sh starts this launcher instead of MainUI
 * (see onion/install.sh). A game is started exactly like MainUI does it: the command goes to
 * /tmp/cmd_to_run.sh and the launcher exits, runtime.sh runs it and comes back to us afterwards.
 * Sleep, power off and the MENU button are handled by Onion's keymon, not here. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/time.h>
#include "platform.h"
#include "library.h"
#include "coins.h"

#define SDCARD "/mnt/SDCARD"
#define CMD_PATH "/tmp/cmd_to_run.sh"
#define EXIT_FLAG "/tmp/kidsos_exit"

static SDL_Surface *video;   /* the framebuffer, written only in PLAT_flip */
static SDL_Surface *screen;  /* what the UI draws into */
static uint32_t held;
/* the bits of <linux/input.h> that are needed; the header itself defines BTN_* names that clash with ours */
struct input_event {
	struct timeval time;
	uint16_t type, code;
	int32_t value;
};
#define EV_KEY 1
#define KEY_VOLUMEDOWN 114
#define KEY_VOLUMEUP 115

static int rotate;           /* the panel
 is mounted upside down; .kidsos/no-rotate turns the software fix off */
static int input_fd = -1;    /* the volume keys never reach SDL, they are read from the input device */
static int volume = 10, muted; /* mirrors keymon: one step per release or repeat, synced from system.json */
static uint32_t vol_pulse;     /* a press and its release read in the same poll still count as one press */
static uint32_t last_vol_key;  /* keymon saves system.json a while after the last key; until then the count wins */

static void put_file(const char *path, const char *value)
{
	FILE *f = fopen(path, "w");
	if (!f) return;
	fputs(value, f);
	fclose(f);
}

/* keymon keeps the volume in system.json and rewrites the whole file on every change; a read that lands in
 * the middle of a write is ignored, which is why the value is also tracked from the keys */
static int json_int(const char *buf, const char *key, int *out)
{
	const char *p = strstr(buf, key);
	if (!p || !(p = strchr(p, ':'))) return 0;
	p++;
	while (*p == ' ' || *p == '\t') p++;
	if (*p < '0' || *p > '9') return 0;
	*out = atoi(p);
	return 1;
}

static void read_volume(void)
{
	FILE *f = fopen(SDCARD "/system.json", "r");
	if (!f) return;
	char buf[2048];
	size_t n = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	buf[n] = 0;
	if (!strchr(buf, '}')) return;
	int v, m;
	int before = volume * 2 + muted;
	if (json_int(buf, "\"vol\"", &v) && v >= 0 && v <= 20) volume = v;
	if (json_int(buf, "\"mute\"", &m)) muted = m;
	if (volume * 2 + muted != before) fprintf(stderr, "volume from system.json: %d%s\n", volume, muted ? " (muted)" : "");
}

const char *PLAT_language(void)
{
	static char code[8];
	FILE *f = fopen(SDCARD "/system.json", "r");
	if (!f) return NULL;
	char buf[2048];
	size_t n = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	buf[n] = 0;
	const char *key = strstr(buf, "\"lang\"");
	if (!key) return NULL;
	const char *start = strchr(key + 6, '"');
	if (!start) return NULL;
	start++;
	size_t len = strcspn(start, ".\"");
	if (len == 0 || len >= sizeof(code)) return NULL;
	memcpy(code, start, len);
	code[len] = 0;
	return code;
}

SDL_Surface *PLAT_init(void)
{
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO);
	SDL_ShowCursor(0);
	/* Miyoo's SDL rotates the picture 180 degrees only in its hardware blit (MI_GFX); a software frame
	 * copied into the hardware surface lands unrotated, so PLAT_flip rotates it while copying. Double
	 * buffering makes SDL_Flip swap framebuffer pages, so the panel never scans a half-written frame. */
	video = SDL_SetVideoMode(SCREEN_W, SCREEN_H, 32, SDL_HWSURFACE | SDL_DOUBLEBUF);
	if (!video) {
		fprintf(stderr, "SDL_SetVideoMode: %s\n", SDL_GetError());
		exit(1);
	}
	screen = SDL_CreateRGBSurface(SDL_SWSURFACE, SCREEN_W, SCREEN_H, 32, 0, 0, 0, 0);
	rotate = access(SDCARD "/.kidsos/no-rotate", F_OK) != 0;
	input_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
	read_volume();
	return screen;
}

static void blit_rotated_180(SDL_Surface *src, SDL_Surface *dst)
{
	if (SDL_MUSTLOCK(dst)) SDL_LockSurface(dst);
	const uint32_t *s = src->pixels;
	uint32_t *d = dst->pixels;
	for (int y = 0; y < src->h; y++) {
		const uint32_t *row = (const uint32_t *)((const uint8_t *)s + y * src->pitch);
		uint32_t *out = (uint32_t *)((uint8_t *)d + (src->h - 1 - y) * dst->pitch) + src->w - 1;
		for (int x = 0; x < src->w; x += 4) {
			out[0] = row[x];
			out[-1] = row[x + 1];
			out[-2] = row[x + 2];
			out[-3] = row[x + 3];
			out -= 4;
		}
	}
	if (SDL_MUSTLOCK(dst)) SDL_UnlockSurface(dst);
}

void PLAT_quit(void)
{
	PLAT_audio_quit();
	SDL_FreeSurface(screen);
	SDL_Quit();
}

void PLAT_flip(SDL_Surface *s)
{
	if (rotate) blit_rotated_180(s, video);
	else SDL_BlitSurface(s, NULL, video, NULL);
	SDL_Flip(video);
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
		if (e.type == SDL_KEYDOWN) held |= key_to_button(e.key.keysym.sym);
		if (e.type == SDL_KEYUP) held &= ~key_to_button(e.key.keysym.sym);
	}
	struct input_event ev;
	while (input_fd >= 0 && read(input_fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
		if (ev.type != EV_KEY || (ev.code != KEY_VOLUMEUP && ev.code != KEY_VOLUMEDOWN)) continue;
		uint32_t bit = ev.code == KEY_VOLUMEUP ? BTN_VOL_UP : BTN_VOL_DOWN;
		last_vol_key = SDL_GetTicks();
		if (ev.value) held |= bit, vol_pulse |= bit;
		else held &= ~bit;
		if (ev.value != 1) { /* keymon steps the volume on release and on repeat */
			volume += ev.code == KEY_VOLUMEUP ? 1 : -1;
			if (volume < 0) volume = 0;
			if (volume > 20) volume = 20;
		}
	}
	pad_update(held | vol_pulse);
	vol_pulse = 0;
	/* keymon writes system.json a moment after the last key; once the keys have been quiet the file wins,
	 * which also picks up a mute toggled with both keys */
	static uint32_t last_read;
	uint32_t now = SDL_GetTicks();
	if (now - last_vol_key > 3000 && now - last_read > 1000) {
		last_read = now;
		read_volume();
	}
}

void PLAT_volume(int *level, int *max, int *mute)
{
	*level = volume;
	*max = 20;
	*mute = muted;
}

void PLAT_power_update(uint32_t now)
{
	(void)now;
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
	return "res/Fredoka-Bold.ttf";
}

int PLAT_save_screenshot(SDL_Surface *s, const char *path)
{
	return SDL_SaveBMP(s, path);
}

int PLAT_launch(const struct Game *game, int mode)
{
	const System *sys = &lib.systems[game->system];
	if (!sys->launcher[0] || access(sys->launcher, F_OK) != 0) {
		fprintf(stderr, "no launcher for %s\n", sys->tag);
		return LAUNCH_FAKE;
	}
	char cmd[4096];
	const char *wrapper = coins_is_arcade(game->system_tag) ? "/mnt/SDCARD/.kidsos/game.sh " : "";
	snprintf(cmd, sizeof(cmd), "LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so %s\"%s\" \"%s\"\n", wrapper, sys->launcher,
	         game->path);
	put_file(CMD_PATH, cmd);
	chmod(CMD_PATH, 0755);
	/* Onion's runtime.sh turns these flags into a RetroArch --appendconfig with savestate_auto_load on or off */
	unlink("/tmp/reset_game");
	unlink("/tmp/force_auto_load_state");
	if (mode == LAUNCH_FRESH) put_file("/tmp/reset_game", "1");
	if (mode == LAUNCH_RESUME) put_file("/tmp/force_auto_load_state", "1");
	sync();
	return LAUNCH_HANDOVER;
}

/* The stock firmware's menu music and selection click; Onion leaves them in place in /customer. The theme
 * music volume and mute switch are Onion's own (Tweaks > Appearance), so the parent keeps control. */
#define STOCK_BGM "/customer/app/sound/bgm.mp3"
#define STOCK_CLICK "/customer/app/sound/change.wav"
#define BGM_MUTE_FLAG SDCARD "/.tmp_update/config/.bgmMute"

typedef struct Mix_Music Mix_Music;
typedef struct Mix_Chunk Mix_Chunk;
static struct {
	void *lib;
	int (*open_audio)(int, uint16_t, int, int);
	Mix_Music *(*load_mus)(const char *);
	int (*play_music)(Mix_Music *, int);
	int (*volume_music)(int);
	void (*halt_music)(void);
	void (*free_music)(Mix_Music *);
	Mix_Chunk *(*load_wav_rw)(SDL_RWops *, int);
	int (*play_channel_timed)(int, Mix_Chunk *, int, int);
	int (*volume)(int, int);
	void (*free_chunk)(Mix_Chunk *);
	void (*close_audio)(void);
	Mix_Music *music;
	Mix_Chunk *click;
} mix;

static int bgm_volume(void) /* 0..20 like Onion's bgmvol */
{
	int v = 20;
	FILE *f = fopen(SDCARD "/system.json", "r");
	if (!f) return v;
	char buf[2048];
	size_t n = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	buf[n] = 0;
	int parsed;
	if (json_int(buf, "\"bgmvol\"", &parsed) && parsed >= 0 && parsed <= 20) v = parsed;
	return v;
}

#define LOAD(field, name) (*(void **)&mix.field = dlsym(mix.lib, name))

void PLAT_audio_init(void)
{
	if (access(BGM_MUTE_FLAG, F_OK) == 0) {
		fprintf(stderr, "audio: theme music muted in Onion\n");
		return;
	}
	mix.lib = dlopen("libSDL_mixer-1.2.so.0", RTLD_NOW);
	if (!mix.lib) {
		fprintf(stderr, "audio: %s\n", dlerror());
		return;
	}
	LOAD(open_audio, "Mix_OpenAudio");
	LOAD(load_mus, "Mix_LoadMUS");
	LOAD(play_music, "Mix_PlayMusic");
	LOAD(volume_music, "Mix_VolumeMusic");
	LOAD(halt_music, "Mix_HaltMusic");
	LOAD(free_music, "Mix_FreeMusic");
	LOAD(load_wav_rw, "Mix_LoadWAV_RW");
	LOAD(play_channel_timed, "Mix_PlayChannelTimed");
	LOAD(volume, "Mix_Volume");
	LOAD(free_chunk, "Mix_FreeChunk");
	LOAD(close_audio, "Mix_CloseAudio");
	if (!mix.open_audio || !mix.load_mus || !mix.play_music || !mix.close_audio || !mix.load_wav_rw ||
	    !mix.play_channel_timed) {
		fprintf(stderr, "audio: libSDL_mixer is missing symbols\n");
		dlclose(mix.lib);
		mix.lib = NULL;
		return;
	}
	if (mix.open_audio(48000, 0x8010 /* AUDIO_S16LSB */, 2, 2048) != 0) {
		fprintf(stderr, "audio: Mix_OpenAudio failed\n");
		dlclose(mix.lib);
		mix.lib = NULL;
		return;
	}
	int vol = bgm_volume() * 128 / 20;
	mix.music = mix.load_mus(STOCK_BGM);
	if (mix.music) {
		if (mix.volume_music) mix.volume_music(vol);
		mix.play_music(mix.music, -1);
	} else {
		fprintf(stderr, "audio: cannot load %s\n", STOCK_BGM);
	}
	mix.click = mix.load_wav_rw(SDL_RWFromFile(STOCK_CLICK, "rb"), 1);
	if (mix.click && mix.volume) mix.volume(-1, vol);
	fprintf(stderr, "audio: music %s, click %s, volume %d/20\n", mix.music ? "on" : "off", mix.click ? "on" : "off",
	        bgm_volume());
}

void PLAT_audio_click(void)
{
	if (mix.lib && mix.click) mix.play_channel_timed(-1, mix.click, 0, -1);
}

void PLAT_audio_quit(void)
{
	if (!mix.lib) return;
	if (mix.music) {
		if (mix.halt_music) mix.halt_music();
		if (mix.free_music) mix.free_music(mix.music);
	}
	if (mix.click && mix.free_chunk) mix.free_chunk(mix.click);
	mix.close_audio();
	dlclose(mix.lib);
	mix.lib = NULL;
}

void PLAT_exit_to_system(void)
{
	put_file(EXIT_FLAG, "1");
	sync();
}
