#include "osd.h"
#include "gfx.h"
#include "platform.h"

#define SHOW_MS 1600
#define FADE_MS 350
#define PILL_W 380
#define PILL_H 72
#define SHADOW 4

static uint32_t left_ms;
static int hiding; /* the pill has just gone: the screen under it must be drawn once more */
static int level, max_level, mute;
static SDL_Surface *pill, *icon_on, *icon_off;

void osd_update(uint32_t dt)
{
	if (pad.repeat & (BTN_VOL_UP | BTN_VOL_DOWN)) left_ms = SHOW_MS;
	if (!left_ms) return;
	left_ms = left_ms > dt ? left_ms - dt : 0;
	if (!left_ms) hiding = 1;
	PLAT_volume(&level, &max_level, &mute);
}

int osd_visible(void)
{
	return left_ms > 0 || hiding;
}

void osd_render(void)
{
	hiding = 0;
	if (!left_ms) return;
	if (!pill) {
		pill = gfx_new_surface(PILL_W + SHADOW, PILL_H + SHADOW);
		icon_on = gfx_icon_speaker(44, INK, 1);
		icon_off = gfx_icon_speaker(44, RED, 0);
	}
	/* shadow and body in one surface, so the fade blends the whole pill with the screen only once and
	 * the shadow never shows through the body */
	gfx_set_target(pill);
	SDL_FillRect(pill, NULL, 0);
	SDL_Rect shadow = {SHADOW, SHADOW, PILL_W, PILL_H};
	gfx_fill_rounded(shadow, PILL_H / 2, INK);
	SDL_Rect body = {0, 0, PILL_W, PILL_H};
	gfx_fill_rounded_border(body, PILL_H / 2, 0xFFFFFF, INK, 3);
	SDL_Surface *icon = mute || level <= 0 ? icon_off : icon_on;
	gfx_blit(icon, 24, (PILL_H - icon->h) / 2);

	SDL_Rect track = {86, PILL_H / 2 - 11, PILL_W - 86 - 28, 22};
	gfx_fill_rounded(track, 11, 0xECEAE4);
	int w = max_level > 0 ? track.w * (mute ? 0 : level) / max_level : 0;
	if (w > 0) {
		SDL_Rect fill = {track.x, track.y, w < 22 ? 22 : w, 22};
		gfx_fill_rounded(fill, 11, GREEN);
	}
	gfx_set_target(NULL);

	uint8_t alpha = left_ms >= FADE_MS ? 255 : (uint8_t)(255 * left_ms / FADE_MS);
	gfx_blit_alpha(pill, 320 - PILL_W / 2, 14, alpha);
}
