#include <math.h>
#include <string.h>
#include "kid_ui.h"
#include "gfx.h"
#include "platform.h"
#include "coins.h"

#define MAX_KID_GAMES 64
#define HOLD_MS 3000
#define BUTTON_A_COLOR 0x3DBE5B
#define EXIT_COMBO (BTN_SELECT | BTN_START) /* held for HOLD_MS: back to the host UI until the next reboot */

static Game *games[MAX_KID_GAMES];
static int count, sel;
static float scroll;
static uint32_t hold_ms;
static SDL_Surface *background;
static uint32_t notice_ms; /* "no coins left" bubble, counts down */
static int coins_left_cache, coins_limit_cache, arcade_present;
static SDL_Surface *coin_icon[2]; /* gold, grey */

void kid_ui_enter(void)
{
	Game *previous = count ? games[sel] : NULL;
	count = library_enabled(games, MAX_KID_GAMES);
	sel = 0;
	for (int i = 0; i < count; i++)
		if (games[i] == previous) sel = i;
	scroll = (float)sel;
	hold_ms = 0;
	coins_left_cache = coins_left();
	coins_limit_cache = coins_limit();
	arcade_present = 0;
	for (int i = 0; i < count; i++)
		if (coins_is_arcade(games[i]->system_tag)) arcade_present = 1;
}

void kid_ui_notice_no_coins(void)
{
	notice_ms = 2500;
	coins_left_cache = coins_left();
}

static int game_blocked(const Game *g)
{
	return coins_is_arcade(g->system_tag) && coins_left_cache <= 0;
}

int kid_ui_animating(void)
{
	return scroll != (float)sel || hold_ms > 0 || notice_ms > 0;
}

Game *kid_ui_selected(void)
{
	return count ? games[sel] : NULL;
}

int kid_ui_update(uint32_t dt)
{
	if ((pad.held & EXIT_COMBO) == EXIT_COMBO) {
		hold_ms += dt;
		if (hold_ms >= HOLD_MS) {
			hold_ms = 0;
			return KID_EXIT_SYSTEM;
		}
	} else {
		hold_ms = 0;
	}

	if (count) {
		int before = sel;
		if ((pad.repeat & BTN_LEFT) && sel > 0) sel--;
		if ((pad.repeat & BTN_RIGHT) && sel < count - 1) sel++;
		if (sel != before) PLAT_audio_click();
		if (pad.pressed & BTN_A) {
			if (!game_blocked(games[sel])) return KID_LAUNCH;
			kid_ui_notice_no_coins();
		}
	}
	notice_ms = notice_ms > dt ? notice_ms - dt : 0;

	float k = (float)dt * 0.012f;
	if (k > 1) k = 1;
	scroll += ((float)sel - scroll) * k;
	if (fabsf((float)sel - scroll) < 0.005f) scroll = (float)sel;
	return KID_NONE;
}

static unsigned name_hash(const char *s)
{
	unsigned h = 5381;
	while (*s) h = h * 33 + (unsigned char)*s++;
	return h;
}

static void draw_fallback(Game *g, int cx, int cy, int inner, int radius)
{
	uint32_t color = gfx_hsv((float)(name_hash(g->name) % 360), 0.55f, 0.92f);
	SDL_Rect r = {cx - inner / 2, cy - inner / 2, inner, inner};
	gfx_fill_rounded(r, radius, color);

	char initial[8] = {0};
	size_t n = 1;
	while ((g->name[n] & 0xC0) == 0x80) n++;
	memcpy(initial, g->name, n);
	if (initial[0] >= 'a' && initial[0] <= 'z') initial[0] -= 32;

	TTF_Font *font = inner >= 200 ? font_huge : font_big;
	gfx_text(font, initial, 0xFFFFFF, cx, cy - TTF_FontHeight(font) / 2, ALIGN_CENTER);
}

static void draw_tile(Game *g, int cx, int cy, int size, uint8_t alpha)
{
	int radius = size / 12;
	SDL_Rect shadow = {cx - size / 2, cy - size / 2 + size / 30, size, size};
	gfx_fill_rounded_alpha(shadow, radius, 0x3A2E1A, (uint8_t)(alpha / 6));

	SDL_Rect card = {cx - size / 2, cy - size / 2, size, size};
	if (alpha == 255) gfx_fill_rounded(card, radius, 0xFFFFFF);
	else gfx_fill_rounded_alpha(card, radius, 0xFFFFFF, alpha);

	int inner = size - 2 * (size / 14);
	SDL_Surface *cover = game_cover(g);
	if (!cover) {
		draw_fallback(g, cx, cy, inner, radius / 2);
		return;
	}
	float s = fminf((float)inner / (float)cover->w, (float)inner / (float)cover->h);
	int w = (int)((float)cover->w * s), h = (int)((float)cover->h * s);
	if (!g->scaled || g->scaled_w != w || g->scaled_h != h) {
		if (g->scaled) SDL_FreeSurface(g->scaled);
		g->scaled = gfx_scale(cover, w, h);
		gfx_round_corners(g->scaled, radius / 2);
		g->scaled_w = w;
		g->scaled_h = h;
	}
	gfx_blit_alpha(g->scaled, cx - w / 2, cy - h / 2, alpha);
}

/* Daily coin counter in the top left corner, shown while an arcade game is in the list. */
static void draw_coin_counter(void)
{
	if (!arcade_present) return;
	const int size = 44, x = 14, y = 8;
	int grey = coins_left_cache <= 0;
	if (!coin_icon[grey]) coin_icon[grey] = gfx_icon_coin(size, grey);
	gfx_blit(coin_icon[grey], x, y);
	char buf[8];
	snprintf(buf, sizeof(buf), "%d", coins_left_cache);
	gfx_text(font_big, buf, grey ? 0x8A8580 : 0x7A4A00, x + size + 6, y + size / 2 - TTF_FontHeight(font_big) / 2,
	         ALIGN_LEFT);
}

static void draw_hold_progress(void)
{
	if (hold_ms < 400) return;
	SDL_Rect track = {170, 14, 300, 10};
	gfx_fill_rounded(track, 5, 0xFFFFFF);
	SDL_Rect fill = {170, 14, (int)(300 * hold_ms / HOLD_MS), 10};
	if (fill.w > 10) gfx_fill_rounded(fill, 5, 0x4CAF50);
	gfx_text(font_small, "Keep holding to leave kids mode", 0x5A5A5A, 320, 28, ALIGN_CENTER);
}

static void draw_dots(void)
{
	if (count <= 1) return;
	if (count > 14) {
		char buf[32];
		snprintf(buf, sizeof(buf), "%d / %d", sel + 1, count);
		gfx_text(font_small, buf, 0x6A6A6A, 320, 446, ALIGN_CENTER);
		return;
	}
	int spacing = 18, x0 = 320 - (count - 1) * spacing / 2;
	for (int i = 0; i < count; i++) {
		int r = i == sel ? 6 : 4;
		SDL_Rect d = {x0 + i * spacing - r, 452 - r, 2 * r, 2 * r};
		gfx_fill_rounded(d, r, i == sel ? 0x4A4A4A : 0xB8B0A4);
	}
}

void kid_ui_render(void)
{
	if (!background) {
		background = gfx_new_surface(SCREEN_W, SCREEN_H);
		gfx_set_target(background);
		SDL_Rect full = {0, 0, SCREEN_W, SCREEN_H};
		gfx_vgradient(full, 0xBFE6FF, 0xFFF3D6);
		gfx_set_target(NULL);
	}
	gfx_blit(background, 0, 0);

	if (!count) {
		gfx_text(font_big, "No games yet", 0x4A4A4A, 320, 190, ALIGN_CENTER);
		gfx_text(font_med, "Mark games as favourites in " HOST_SYSTEM_NAME, 0x7A7A7A, 320, 240, ALIGN_CENTER);
		gfx_text(font_med, "Hold SELECT + START to get there", 0x7A7A7A, 320, 275, ALIGN_CENTER);
		draw_hold_progress();
		return;
	}

	const int spacing = 300, base = 300, cy = 222;
	int first = sel - 2 < 0 ? 0 : sel - 2;
	int last = sel + 2 >= count ? count - 1 : sel + 2;
	for (int pass = 0; pass < 2; pass++) {
		for (int i = first; i <= last; i++) {
			float d = (float)i - scroll;
			int selected = fabsf(d) < 0.5f;
			if ((pass == 1) != selected) continue;
			float scale = 1.0f - fabsf(d) * 0.35f;
			if (scale < 0.5f) scale = 0.5f;
			float dim = fabsf(d) * 120.0f;
			uint8_t alpha = (uint8_t)(255 - (dim > 100 ? 100 : dim));
			if (game_blocked(games[i])) alpha = (uint8_t)(alpha * 0.55f);
			int size = (int)(base * scale), tx = 320 + (int)(d * spacing);
			draw_tile(games[i], tx, cy, size, alpha);
		}
	}

	gfx_text_fit(font_big, games[sel]->name, 0x333333, 320, 388, ALIGN_CENTER, 560);
	draw_dots();
	draw_coin_counter();
	gfx_button_hint(622, 456, "A", BUTTON_A_COLOR, "Play", ALIGN_RIGHT);
	draw_hold_progress();

	if (notice_ms > 0) {
		const char *msg = "No coins left today, come back tomorrow";
		int w = gfx_text_width(font_med, msg) + 48, cx = 370; /* centred in the space right of the coin counter */
		SDL_Rect pill = {cx - w / 2, 10, w, 42};
		gfx_fill_rounded_alpha(pill, 21, 0x3A2E1A, 200);
		gfx_text(font_med, msg, 0xFFFFFF, cx, 18, ALIGN_CENTER);
	}
}
