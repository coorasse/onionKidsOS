#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "kid_ui.h"
#include "i18n.h"
#include "gfx.h"
#include "platform.h"
#include "coins.h"

#define MAX_KID_GAMES 64
#define HOLD_MS 3000
#define EXIT_COMBO (BTN_SELECT | BTN_START) /* held for HOLD_MS: back to the host UI until the next reboot */

static Game *games[MAX_KID_GAMES];
static int count, sel;
static float scroll;
static uint32_t hold_ms;
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

/* The two looks of a card: the chosen one, big and leaning left, and a neighbour, smaller and leaning
 * right. Both are white with an ink border and a hard ink shadow, the cover inside, like blocks put down
 * by a child. */
enum { TILE_SMALL, TILE_BIG };
static const int tile_size[2] = {195, 300}, tile_shadow[2] = {6, 9}, tile_border[2] = {4, 5};
static const float tile_tilt[2] = {2.5f, -2.0f};

/* A neighbour sits back at 60%: the fade is baked into its colours over the cream of the paper, so the
 * card stays opaque and a frame of the slide copies it instead of blending every pixel. */
static void fade_into_paper(SDL_Surface *s, int amount)
{
	const unsigned cr = (CREAM >> 16) & 255, cg = (CREAM >> 8) & 255, cb = CREAM & 255;
	SDL_LockSurface(s);
	uint32_t *p = s->pixels;
	int pitch = s->pitch / 4;
	for (int y = 0; y < s->h; y++)
		for (int x = 0; x < s->w; x++) {
			uint32_t c = p[y * pitch + x];
			if (!(c >> 24)) continue;
			unsigned r = ((c >> 16 & 255) * (255 - amount) + cr * amount) / 255;
			unsigned g = ((c >> 8 & 255) * (255 - amount) + cg * amount) / 255;
			unsigned b = ((c & 255) * (255 - amount) + cb * amount) / 255;
			p[y * pitch + x] = (c & 0xFF000000) | r << 16 | g << 8 | b;
		}
	SDL_UnlockSurface(s);
}

static SDL_Surface *compose_tile(Game *g, int which)
{
	int size = tile_size[which], shadow = tile_shadow[which], radius = size / 15;
	SDL_Surface *comp = gfx_new_surface(size + shadow, size + shadow);
	if (!comp) return NULL;
	gfx_set_target(comp);
	SDL_Rect drop = {shadow, shadow, size, size};
	gfx_fill_rounded(drop, radius, INK);
	SDL_Rect card = {0, 0, size, size};
	gfx_fill_rounded_border(card, radius, 0xFFFFFF, INK, tile_border[which]);

	int inner = size - 2 * (size / 14), cx = size / 2, cy = size / 2;
	SDL_Surface *cover = game_cover(g);
	if (cover) {
		float s = fminf((float)inner / (float)cover->w, (float)inner / (float)cover->h);
		int w = (int)((float)cover->w * s), h = (int)((float)cover->h * s);
		SDL_Surface *scaled = gfx_scale(cover, w, h);
		if (scaled) {
			gfx_round_corners(scaled, radius / 2);
			gfx_blit(scaled, cx - w / 2, cy - h / 2);
			SDL_FreeSurface(scaled);
		}
	} else {
		draw_fallback(g, cx, cy, inner, radius / 2);
	}
	gfx_set_target(NULL);

	SDL_Surface *tilted = gfx_rotate(comp, tile_tilt[which]);
	SDL_FreeSurface(comp);
	if (tilted && which == TILE_SMALL) fade_into_paper(tilted, 102);
	return tilted;
}

static void release_tiles(Game *g)
{
	for (int i = 0; i < 2; i++) {
		if (g->tiles[i]) SDL_FreeSurface(g->tiles[i]);
		free(g->tile_spans[i]);
		g->tiles[i] = NULL;
		g->tile_spans[i] = NULL;
	}
}

static SDL_Surface *tile(Game *g, int which)
{
	if (g->tile_cover != g->cover) release_tiles(g);
	if (!g->tiles[which]) {
		g->tiles[which] = compose_tile(g, which);
		if (g->tiles[which]) g->tile_spans[which] = gfx_opaque_spans(g->tiles[which]);
		g->tile_cover = g->cover;
	}
	return g->tiles[which];
}

/* One look of the card at the size the carousel wants right now. The pre-rendered cards are only
 * scaled, never redrawn, so a frame of the animation stays cheap on the device. */
static void blit_tile(Game *g, int which, int cx, int cy, int size, uint8_t alpha)
{
	SDL_Surface *src = tile(g, which);
	if (!src) return;
	float k = (float)size / (float)tile_size[which];
	int w = (int)((float)src->w * k), h = (int)((float)src->h * k);
	/* the shadow makes the surface bigger than the card: centre the card, not the surface */
	int off = (int)((float)tile_shadow[which] * k / 2);
	gfx_blit_alpha_scaled(src, g->tile_spans[which], cx + off - w / 2, cy + off - h / 2, w, h, alpha);
}

/* d: distance from the chosen slot. Around the half-way point the two looks are cross-faded, so the
 * change of tilt happens while the card is moving fast. */
static void draw_tile(Game *g, int cx, int cy, int size, float d, uint8_t alpha)
{
	float away = fabsf(d);
	float big = away <= 0.4f ? 1 : away >= 0.6f ? 0 : (0.6f - away) / 0.2f;
	if (big < 1) blit_tile(g, TILE_SMALL, cx, cy, size, alpha);
	if (big > 0) blit_tile(g, TILE_BIG, cx, cy, size, (uint8_t)((float)alpha * big));
}

/* The name under the row, rendered once per game: rasterising it every frame costs more than the tiles. */
static void draw_title(const char *name)
{
	static SDL_Surface *title;
	static char shown[128];
	if (!title || strcmp(shown, name) != 0) {
		if (title) SDL_FreeSurface(title);
		title = gfx_new_surface(560, TTF_FontHeight(font_big) + 4);
		if (!title) return;
		gfx_set_target(title);
		gfx_text_fit(font_big, name, INK, 280, 0, ALIGN_CENTER, 560);
		gfx_set_target(NULL);
		snprintf(shown, sizeof(shown), "%s", name);
	}
	gfx_blit(title, 320 - 280, 388);
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
	gfx_text(font_big, buf, grey ? MUTED : INK, x + size + 6, y + size / 2 - TTF_FontHeight(font_big) / 2,
	         ALIGN_LEFT);
}

static void draw_hold_progress(void)
{
	if (hold_ms < 400) return;
	SDL_Rect track = {170, 12, 300, 14};
	gfx_fill_rounded_border(track, 7, 0xFFFFFF, INK, 2);
	SDL_Rect fill = {172, 14, (int)(296 * hold_ms / HOLD_MS), 10};
	if (fill.w > 10) gfx_fill_rounded(fill, 5, GREEN);
	gfx_text_fit(font_small, T(STR_HOLD_TO_LEAVE), INK, 320, 30, ALIGN_CENTER, 600);
}

/* The page dots are little blocks, each put down at its own angle; white, the current one red. */
static SDL_Surface *dot_block(int red, int slot)
{
	static SDL_Surface *blocks[2][4];
	static const float tilts[4] = {-6, 4, -3, 5};
	SDL_Surface **block = &blocks[red][slot];
	if (*block) return *block;
	SDL_Surface *square = gfx_new_surface(14, 14);
	if (!square) return NULL;
	gfx_set_target(square);
	SDL_Rect r = {0, 0, 14, 14};
	gfx_fill_rounded_border(r, 3, red ? RED : 0xFFFFFF, INK, 2);
	gfx_set_target(NULL);
	*block = gfx_rotate(square, tilts[slot]);
	SDL_FreeSurface(square);
	return *block;
}

static void draw_dots(void)
{
	if (count <= 1) return;
	if (count > 14) {
		char buf[32];
		snprintf(buf, sizeof(buf), "%d / %d", sel + 1, count);
		gfx_text(font_small, buf, INK, 320, 446, ALIGN_CENTER);
		return;
	}
	int spacing = 22, x0 = 320 - (count - 1) * spacing / 2;
	for (int i = 0; i < count; i++) {
		SDL_Surface *block = dot_block(i == sel, i % 4);
		if (block) gfx_blit(block, x0 + i * spacing - block->w / 2, 452 - block->h / 2);
	}
}

void kid_ui_render(void)
{
	gfx_blit(gfx_paper(), 0, 0);

	if (!count) {
		char mark[160];
		snprintf(mark, sizeof(mark), T(STR_NO_GAMES_MARK), HOST_SYSTEM_NAME);
		gfx_text_fit(font_big, T(STR_NO_GAMES), INK, 320, 190, ALIGN_CENTER, 600);
		gfx_text_fit(font_med, mark, 0x55555F, 320, 240, ALIGN_CENTER, 600);
		gfx_text_fit(font_med, T(STR_NO_GAMES_HOLD), 0x55555F, 320, 275, ALIGN_CENTER, 600);
		draw_hold_progress();
		return;
	}

	const int spacing = 300, base = 300, cy = 222;
	int first = sel - 2 < 0 ? 0 : sel - 2;
	int last = sel + 2 >= count ? count - 1 : sel + 2;
	for (int i = 0; i < count; i++)
		if (i < first || i > last) release_tiles(games[i]);
	for (int pass = 0; pass < 2; pass++) {
		for (int i = first; i <= last; i++) {
			float d = (float)i - scroll;
			int selected = fabsf(d) < 0.5f;
			if ((pass == 1) != selected) continue;
			float scale = 1.0f - fabsf(d) * 0.35f;
			if (scale < 0.5f) scale = 0.5f;
			uint8_t alpha = game_blocked(games[i]) ? 140 : 255;
			int size = (int)(base * scale), tx = 320 + (int)(d * spacing);
			if (tx + size / 2 + 20 < 0 || tx - size / 2 - 20 > SCREEN_W) {
				release_tiles(games[i]);
				continue;
			}
			draw_tile(games[i], tx, cy, size, d, alpha);
		}
	}

	draw_title(games[sel]->name);
	draw_dots();
	draw_coin_counter();
	gfx_button_hint(622, 456, "A", GREEN, T(STR_PLAY), ALIGN_RIGHT);
	draw_hold_progress();

	if (notice_ms > 0) {
		const char *msg = T(STR_NO_COINS);
		const int max_w = 482, cx = 365; /* centred in the space right of the coin counter */
		TTF_Font *font = gfx_text_width(font_med, msg) <= max_w ? font_med : font_small;
		int w = gfx_text_width(font, msg) + 48;
		if (w > max_w + 48) w = max_w + 48;
		SDL_Rect pill = {cx - w / 2, 10, w, 42};
		gfx_fill_rounded(pill, 21, INK);
		gfx_text_fit(font, msg, 0xFFFFFF, cx, 31 - TTF_FontHeight(font) / 2, ALIGN_CENTER, max_w);
	}
}
