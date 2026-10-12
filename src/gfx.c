#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL_image.h>
#include "gfx.h"

SDL_Surface *gfx_screen;
TTF_Font *font_huge, *font_big, *font_med, *font_small;

static SDL_Surface *target;
static SDL_Surface *argb_reference;

static TTF_Font *open_font(const char *path, int size)
{
	TTF_Font *f = TTF_OpenFont(path, size);
	if (!f) fprintf(stderr, "font %s: %s\n", path, TTF_GetError());
	return f;
}

void gfx_init(SDL_Surface *screen, const char *font_path)
{
	gfx_screen = screen;
	target = screen;
#ifdef USE_SDL2
	IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
#endif
	TTF_Init();
	argb_reference = gfx_new_surface(1, 1);
	font_huge = open_font(font_path, 120);
	font_big = open_font(font_path, 36);
	font_med = open_font(font_path, 24);
	font_small = open_font(font_path, 17);
}

void gfx_quit(void)
{
	TTF_Quit();
#ifdef USE_SDL2
	IMG_Quit();
#endif
}

void gfx_set_target(SDL_Surface *t)
{
	target = t ? t : gfx_screen;
}

static uint32_t map_rgb(SDL_Surface *s, uint32_t rgb)
{
	return SDL_MapRGBA(s->format, (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, 255);
}

void gfx_clear(uint32_t rgb)
{
	SDL_FillRect(target, NULL, map_rgb(target, rgb));
}

void gfx_fill_rect(SDL_Rect r, uint32_t rgb)
{
	SDL_FillRect(target, &r, map_rgb(target, rgb));
}

static void fill_rounded_on(SDL_Surface *dst, SDL_Rect r, int rad, uint32_t pixel)
{
	if (rad * 2 > r.w) rad = r.w / 2;
	if (rad * 2 > r.h) rad = r.h / 2;
	if (rad <= 0) {
		SDL_FillRect(dst, &r, pixel);
		return;
	}
	SDL_Rect mid = {r.x, r.y + rad, r.w, r.h - 2 * rad};
	SDL_FillRect(dst, &mid, pixel);
	for (int i = 0; i < rad; i++) {
		float dy = rad - i - 0.5f;
		int inset = rad - (int)lroundf(sqrtf((float)rad * rad - dy * dy));
		SDL_Rect top = {r.x + inset, r.y + i, r.w - 2 * inset, 1};
		SDL_Rect bottom = {r.x + inset, r.y + r.h - 1 - i, r.w - 2 * inset, 1};
		SDL_FillRect(dst, &top, pixel);
		SDL_FillRect(dst, &bottom, pixel);
	}
}

void gfx_fill_rounded(SDL_Rect r, int radius, uint32_t rgb)
{
	fill_rounded_on(target, r, radius, map_rgb(target, rgb));
}

void gfx_fill_rounded_alpha(SDL_Rect r, int radius, uint32_t rgb, uint8_t alpha)
{
	SDL_Surface *tmp = gfx_new_surface(r.w, r.h);
	if (!tmp) return;
	SDL_Rect local = {0, 0, r.w, r.h};
	fill_rounded_on(tmp, local, radius, map_rgb(tmp, rgb));
	gfx_blit_alpha(tmp, r.x, r.y, alpha);
	SDL_FreeSurface(tmp);
}

static uint32_t lerp_rgb(uint32_t a, uint32_t b, float t)
{
	int r = (int)(((a >> 16) & 255) + (((int)((b >> 16) & 255) - (int)((a >> 16) & 255)) * t));
	int g = (int)(((a >> 8) & 255) + (((int)((b >> 8) & 255) - (int)((a >> 8) & 255)) * t));
	int bl = (int)((a & 255) + (((int)(b & 255) - (int)(a & 255)) * t));
	return (uint32_t)(r << 16 | g << 8 | bl);
}

void gfx_vgradient(SDL_Rect r, uint32_t top, uint32_t bottom)
{
	for (int y = 0; y < r.h; y++) {
		SDL_Rect row = {r.x, r.y + y, r.w, 1};
		float t = r.h > 1 ? (float)y / (float)(r.h - 1) : 0;
		SDL_FillRect(target, &row, map_rgb(target, lerp_rgb(top, bottom, t)));
	}
}

void gfx_fill_rounded_border(SDL_Rect r, int radius, uint32_t fill, uint32_t border_rgb, int border_px)
{
	fill_rounded_on(target, r, radius, map_rgb(target, border_rgb));
	SDL_Rect inner = {r.x + border_px, r.y + border_px, r.w - 2 * border_px, r.h - 2 * border_px};
	if (inner.w > 0 && inner.h > 0) fill_rounded_on(target, inner, radius - border_px, map_rgb(target, fill));
}

static int dot_inside(float x, float y, const float *shape)
{
	float dx = x - shape[0], dy = y - shape[0];
	return dx * dx + dy * dy <= shape[1] * shape[1];
}

static SDL_Surface *icon_from(int size, uint32_t rgb, int (*inside)(float, float, const float *), const float *shape);

void gfx_dots(uint32_t rgb, int step, float radius, uint8_t alpha)
{
	int size = (int)ceilf(radius * 2) + 1;
	float shape[2] = {(float)size / 2, radius};
	SDL_Surface *dot = icon_from(size, rgb, dot_inside, shape);
	if (!dot) return;
	for (int y = step / 2; y < target->h; y += step)
		for (int x = step / 2; x < target->w; x += step)
			gfx_blit_alpha(dot, x - size / 2, y - size / 2, alpha);
	SDL_FreeSurface(dot);
}

SDL_Surface *gfx_paper(void)
{
	static SDL_Surface *paper;
	if (paper) return paper;
	SDL_Surface *drawn = gfx_new_surface(gfx_screen->w, gfx_screen->h);
	SDL_Surface *previous = target;
	target = drawn;
	gfx_clear(CREAM);
	gfx_dots(0xE4DCCB, 22, 1.5f, 153);
	target = previous;
	/* in the screen's own format, without an alpha channel, the blit is a plain copy instead of a
	 * blend of every pixel: this is drawn under every frame */
	paper = SDL_ConvertSurface(drawn, gfx_screen->format, 0);
	SDL_FreeSurface(drawn);
	return paper;
}

SDL_Surface *gfx_new_surface(int w, int h)
{
	SDL_Surface *s = SDL_CreateRGBSurface(0, w, h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
	if (s) SDL_FillRect(s, NULL, 0);
	return s;
}

SDL_Surface *gfx_load_image(const char *path)
{
	SDL_Surface *img = IMG_Load(path);
	if (!img) return NULL;
	SDL_Surface *converted = SDL_ConvertSurface(img, argb_reference->format, 0);
	SDL_FreeSurface(img);
	return converted;
}

SDL_Surface *gfx_scale(SDL_Surface *src, int w, int h)
{
	if (w < 1) w = 1;
	if (h < 1) h = 1;
	SDL_Surface *dst = gfx_new_surface(w, h);
	if (!dst) return NULL;
	SDL_LockSurface(src);
	SDL_LockSurface(dst);
	const uint32_t *sp = src->pixels;
	uint32_t *dp = dst->pixels;
	int spitch = src->pitch / 4, dpitch = dst->pitch / 4;
	int sw = src->w, sh = src->h;
	for (int y = 0; y < h; y++) {
		float fy = ((float)y + 0.5f) * (float)sh / (float)h - 0.5f;
		if (fy < 0) fy = 0;
		int y0 = (int)fy;
		if (y0 > sh - 1) y0 = sh - 1;
		int y1 = y0 + 1 < sh ? y0 + 1 : y0;
		int wy = (int)((fy - (float)y0) * 256);
		for (int x = 0; x < w; x++) {
			float fx = ((float)x + 0.5f) * (float)sw / (float)w - 0.5f;
			if (fx < 0) fx = 0;
			int x0 = (int)fx;
			if (x0 > sw - 1) x0 = sw - 1;
			int x1 = x0 + 1 < sw ? x0 + 1 : x0;
			int wx = (int)((fx - (float)x0) * 256);
			uint32_t p00 = sp[y0 * spitch + x0], p01 = sp[y0 * spitch + x1];
			uint32_t p10 = sp[y1 * spitch + x0], p11 = sp[y1 * spitch + x1];
			uint32_t out = 0;
			for (int shift = 0; shift < 32; shift += 8) {
				int c00 = (p00 >> shift) & 255, c01 = (p01 >> shift) & 255;
				int c10 = (p10 >> shift) & 255, c11 = (p11 >> shift) & 255;
				int top = c00 * (256 - wx) + c01 * wx;
				int bot = c10 * (256 - wx) + c11 * wx;
				int c = (top * (256 - wy) + bot * wy) >> 16;
				out |= (uint32_t)c << shift;
			}
			dp[y * dpitch + x] = out;
		}
	}
	SDL_UnlockSurface(dst);
	SDL_UnlockSurface(src);
	return dst;
}

void gfx_round_corners(SDL_Surface *s, int rad)
{
	if (rad * 2 > s->w) rad = s->w / 2;
	if (rad * 2 > s->h) rad = s->h / 2;
	SDL_LockSurface(s);
	uint32_t *p = s->pixels;
	int pitch = s->pitch / 4;
	for (int i = 0; i < rad; i++) {
		float dy = rad - i - 0.5f;
		int inset = rad - (int)lroundf(sqrtf((float)rad * rad - dy * dy));
		for (int x = 0; x < inset; x++) {
			p[i * pitch + x] = 0;
			p[i * pitch + s->w - 1 - x] = 0;
			p[(s->h - 1 - i) * pitch + x] = 0;
			p[(s->h - 1 - i) * pitch + s->w - 1 - x] = 0;
		}
	}
	SDL_UnlockSurface(s);
}

SDL_Surface *gfx_rotate(SDL_Surface *src, float degrees)
{
	float a = degrees * (float)M_PI / 180.0f, c = cosf(a), sn = sinf(a);
	int w = (int)ceilf(fabsf((float)src->w * c) + fabsf((float)src->h * sn));
	int h = (int)ceilf(fabsf((float)src->w * sn) + fabsf((float)src->h * c));
	SDL_Surface *dst = gfx_new_surface(w, h);
	if (!dst) return NULL;
	SDL_LockSurface(src);
	SDL_LockSurface(dst);
	const uint32_t *sp = src->pixels;
	uint32_t *dp = dst->pixels;
	int spitch = src->pitch / 4, dpitch = dst->pitch / 4, sw = src->w, sh = src->h;
	float scx = (float)sw / 2, scy = (float)sh / 2, dcx = (float)w / 2, dcy = (float)h / 2;
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			float dx = (float)x + 0.5f - dcx, dy = (float)y + 0.5f - dcy;
			float fx = c * dx + sn * dy + scx - 0.5f, fy = -sn * dx + c * dy + scy - 0.5f;
			int x0 = (int)floorf(fx), y0 = (int)floorf(fy);
			if (x0 < -1 || y0 < -1 || x0 >= sw || y0 >= sh) {
				dp[y * dpitch + x] = 0;
				continue;
			}
			int wx = (int)((fx - (float)x0) * 256), wy = (int)((fy - (float)y0) * 256);
			/* bilinear on straight alpha: the colour of transparent neighbours must not bleed in,
			 * so the colours are weighted by alpha and divided out again */
			unsigned a_sum = 0, r_sum = 0, g_sum = 0, b_sum = 0;
			for (int j = 0; j < 2; j++) {
				int sy = y0 + j;
				if (sy < 0 || sy >= sh) continue;
				for (int i = 0; i < 2; i++) {
					int sx = x0 + i;
					if (sx < 0 || sx >= sw) continue;
					unsigned wgt = (unsigned)((i ? wx : 256 - wx) * (j ? wy : 256 - wy)); /* 0..65536 */
					uint32_t p = sp[sy * spitch + sx];
					unsigned wa = wgt * (p >> 24) >> 8;
					a_sum += wa;
					r_sum += wa * ((p >> 16) & 255);
					g_sum += wa * ((p >> 8) & 255);
					b_sum += wa * (p & 255);
				}
			}
			if (!a_sum) {
				dp[y * dpitch + x] = 0;
				continue;
			}
			unsigned alpha = a_sum >> 8;
			if (alpha > 255) alpha = 255;
			dp[y * dpitch + x] = alpha << 24 | (r_sum / a_sum) << 16 | (g_sum / a_sum) << 8 | (b_sum / a_sum);
		}
	}
	SDL_UnlockSurface(dst);
	SDL_UnlockSurface(src);
	return dst;
}

void gfx_blit(SDL_Surface *s, int x, int y)
{
	SDL_Rect dst = {x, y, 0, 0};
	SDL_BlitSurface(s, NULL, target, &dst);
}

/* exact x / 255 for x in 0..65025, without a division */
#define DIV255(x) (((x) + 1 + ((x) >> 8)) >> 8)

int *gfx_opaque_spans(SDL_Surface *s)
{
	int *spans = malloc(sizeof(int) * 2 * s->h);
	if (!spans) return NULL;
	SDL_LockSurface(s);
	for (int y = 0; y < s->h; y++) {
		const uint32_t *row = (const uint32_t *)((const uint8_t *)s->pixels + y * s->pitch);
		int l = 0, r = s->w - 1;
		while (l < s->w && (row[l] >> 24) != 255) l++;
		while (r > l && (row[r] >> 24) != 255) r--;
		spans[2 * y] = l;
		spans[2 * y + 1] = l < s->w ? r : -1;
	}
	SDL_UnlockSurface(s);
	return spans;
}

/* The pixel loops below are the hot path of the carousel on the device, where a branch per pixel
 * costs more than the pixel: the opaque run of a row goes through the two branch-free loops, only
 * its edges through the general one. */
static void blend_row32(const uint32_t *restrict src, const int *restrict xs, uint32_t *restrict dst, int n,
                        unsigned alpha, int rs, int gs, int bs)
{
	const uint32_t keep = ~((uint32_t)255 << rs | (uint32_t)255 << gs | (uint32_t)255 << bs);
	for (int i = 0; i < n; i++) {
		uint32_t p = src[xs[i]];
		unsigned a = p >> 24;
		if (!a) continue;
		if (alpha != 255) a = DIV255(a * alpha);
		unsigned sr = (p >> 16) & 255, sg = (p >> 8) & 255, sb = p & 255;
		if (a == 255) {
			dst[i] = (dst[i] & keep) | sr << rs | sg << gs | sb << bs;
			continue;
		}
		uint32_t d = dst[i];
		unsigned na = 255 - a;
		unsigned dr = DIV255(((d >> rs) & 255) * na + sr * a);
		unsigned dg = DIV255(((d >> gs) & 255) * na + sg * a);
		unsigned db = DIV255(((d >> bs) & 255) * na + sb * a);
		dst[i] = (d & keep) | dr << rs | dg << gs | db << bs;
	}
}

/* opaque source pixels mixed in with one constant alpha */
static void mix_row32(const uint32_t *restrict src, const int *restrict xs, uint32_t *restrict dst, int n,
                      unsigned a, int rs, int gs, int bs)
{
	const uint32_t keep = ~((uint32_t)255 << rs | (uint32_t)255 << gs | (uint32_t)255 << bs);
	const unsigned na = 255 - a;
	for (int i = 0; i < n; i++) {
		uint32_t p = src[xs[i]], d = dst[i];
		unsigned dr = DIV255(((d >> rs) & 255) * na + ((p >> 16) & 255) * a);
		unsigned dg = DIV255(((d >> gs) & 255) * na + ((p >> 8) & 255) * a);
		unsigned db = DIV255((((d >> bs) & 255) * na) + (p & 255) * a);
		dst[i] = (d & keep) | dr << rs | dg << gs | db << bs;
	}
}

static void copy_row32(const uint32_t *restrict src, const int *restrict xs, uint32_t *restrict dst, int n,
                       int same_layout)
{
	if (same_layout) {
		for (int i = 0; i < n; i++) dst[i] = src[xs[i]];
	} else {
		for (int i = 0; i < n; i++) {
			uint32_t p = src[xs[i]];
			dst[i] = (dst[i] & 0xFF000000) | (p & 0xFFFFFF); /* only the ARGB/XRGB layouts are swapped here */
		}
	}
}

static void blend_row16(const uint32_t *restrict src, const int *restrict xs, uint16_t *restrict dst, int n,
                        unsigned alpha)
{
	for (int i = 0; i < n; i++) {
		uint32_t p = src[xs[i]];
		unsigned a = p >> 24;
		if (!a) continue;
		if (alpha != 255) a = DIV255(a * alpha);
		unsigned sr = (p >> 19) & 31, sg = (p >> 10) & 63, sb = (p >> 3) & 31;
		if (a == 255) {
			dst[i] = (uint16_t)(sr << 11 | sg << 5 | sb);
			continue;
		}
		unsigned d = dst[i], na = 255 - a;
		unsigned dr = DIV255(((d >> 11) & 31) * na + sr * a);
		unsigned dg = DIV255(((d >> 5) & 63) * na + sg * a);
		unsigned db = DIV255((d & 31) * na + sb * a);
		dst[i] = (uint16_t)(dr << 11 | dg << 5 | db);
	}
}

static void copy_row16(const uint32_t *restrict src, const int *restrict xs, uint16_t *restrict dst, int n)
{
	for (int i = 0; i < n; i++) {
		uint32_t p = src[xs[i]];
		dst[i] = (uint16_t)(((p >> 19) & 31) << 11 | ((p >> 10) & 63) << 5 | ((p >> 3) & 31));
	}
}

/* Software blend of an ARGB surface with an extra global alpha, scaled on the way (nearest neighbour).
 * SDL 1.2 ignores per-surface alpha on surfaces that have per-pixel alpha, so this is done by hand for
 * both 32 bit (desktop, Onion) and 16 bit RGB565 (MinUI) targets. */
void gfx_blit_alpha_scaled(SDL_Surface *s, const int *spans, int x, int y, int w, int h, uint8_t alpha)
{
	if (w < 1 || h < 1) return;
	SDL_Rect clip;
	SDL_GetClipRect(target, &clip);
	int x0 = x > clip.x ? x : clip.x, y0 = y > clip.y ? y : clip.y;
	int x1 = x + w < clip.x + clip.w ? x + w : clip.x + clip.w;
	int y1 = y + h < clip.y + clip.h ? y + h : clip.y + clip.h;
	if (x0 >= x1 || y0 >= y1) return;
	int n = x1 - x0, xs[n];
	for (int i = 0; i < n; i++) xs[i] = (int)(((long)(x0 + i - x) * 2 + 1) * s->w / (2 * w));

	SDL_LockSurface(s);
	SDL_LockSurface(target);
	const SDL_PixelFormat *f = target->format;
	int bpp = f->BytesPerPixel, rs = f->Rshift, gs = f->Gshift, bs = f->Bshift;
	int same_layout = rs == 16 && gs == 8 && bs == 0 && f->Amask == 0;
	for (int ty = y0; ty < y1; ty++) {
		int sy = (int)(((long)(ty - y) * 2 + 1) * s->h / (2 * h));
		const uint32_t *srow = (const uint32_t *)((const uint8_t *)s->pixels + sy * s->pitch);
		uint8_t *drow = (uint8_t *)target->pixels + ty * target->pitch;
		/* the part of this row that maps onto the opaque run of the source row */
		int lo = n, hi = -1;
		if (spans && spans[2 * sy + 1] >= spans[2 * sy]) {
			int l = spans[2 * sy], r = spans[2 * sy + 1];
			lo = 0;
			while (lo < n && xs[lo] < l) lo++;
			hi = n - 1;
			while (hi >= lo && xs[hi] > r) hi--;
		}
		int run = hi - lo + 1;
		if (bpp == 4) {
			uint32_t *d = (uint32_t *)drow + x0;
			if (run <= 0) {
				blend_row32(srow, xs, d, n, alpha, rs, gs, bs);
				continue;
			}
			blend_row32(srow, xs, d, lo, alpha, rs, gs, bs);
			if (alpha == 255 && same_layout && w == s->w) memcpy(d + lo, srow + xs[lo], (size_t)run * 4);
			else if (alpha == 255) copy_row32(srow, xs + lo, d + lo, run, same_layout);
			else mix_row32(srow, xs + lo, d + lo, run, alpha, rs, gs, bs);
			blend_row32(srow, xs + hi + 1, d + hi + 1, n - hi - 1, alpha, rs, gs, bs);
		} else if (bpp == 2) {
			uint16_t *d = (uint16_t *)drow + x0;
			if (run <= 0) {
				blend_row16(srow, xs, d, n, alpha);
				continue;
			}
			blend_row16(srow, xs, d, lo, alpha);
			if (alpha == 255) copy_row16(srow, xs + lo, d + lo, run);
			else blend_row16(srow, xs + lo, d + lo, run, alpha);
			blend_row16(srow, xs + hi + 1, d + hi + 1, n - hi - 1, alpha);
		}
	}
	SDL_UnlockSurface(target);
	SDL_UnlockSurface(s);
}

void gfx_blit_alpha(SDL_Surface *s, int x, int y, uint8_t alpha)
{
	gfx_blit_alpha_scaled(s, NULL, x, y, s->w, s->h, alpha);
}

int gfx_text(TTF_Font *font, const char *text, uint32_t rgb, int x, int y, int align)
{
	if (!font || !text || !*text) return 0;
	SDL_Color color = {(rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, 255};
	SDL_Surface *s = TTF_RenderUTF8_Blended(font, text, color);
	if (!s) return 0;
	int dx = align == ALIGN_CENTER ? x - s->w / 2 : align == ALIGN_RIGHT ? x - s->w : x;
	gfx_blit(s, dx, y);
	int w = s->w;
	SDL_FreeSurface(s);
	return w;
}

int gfx_text_width(TTF_Font *font, const char *text)
{
	int w = 0, h = 0;
	if (font && text) TTF_SizeUTF8(font, text, &w, &h);
	return w;
}

int gfx_text_fit(TTF_Font *font, const char *text, uint32_t rgb, int x, int y, int align, int max_w)
{
	if (gfx_text_width(font, text) <= max_w) return gfx_text(font, text, rgb, x, y, align);
	char buf[256];
	size_t len = strlen(text);
	if (len >= sizeof(buf) - 4) len = sizeof(buf) - 4;
	while (len > 0) {
		len--;
		while (len > 0 && (text[len] & 0xC0) == 0x80) len--;
		memcpy(buf, text, len);
		strcpy(buf + len, "…");
		if (gfx_text_width(font, buf) <= max_w) break;
	}
	return gfx_text(font, buf, rgb, x, y, align);
}

uint32_t gfx_hsv(float h, float s, float v)
{
	float c = v * s, hp = fmodf(h, 360.0f) / 60.0f;
	float x = c * (1 - fabsf(fmodf(hp, 2) - 1)), m = v - c;
	float r = 0, g = 0, b = 0;
	if (hp < 1) r = c, g = x;
	else if (hp < 2) r = x, g = c;
	else if (hp < 3) g = c, b = x;
	else if (hp < 4) g = x, b = c;
	else if (hp < 5) r = x, b = c;
	else r = c, b = x;
	return (uint32_t)((r + m) * 255) << 16 | (uint32_t)((g + m) * 255) << 8 | (uint32_t)((b + m) * 255);
}

/* Icons are drawn once into ARGB surfaces with 3x3 supersampling, so the device never
 * pays for per-pixel maths every frame. */
#define SS 3

static int in_triangle(float px, float py, const float *t)
{
	float d1 = (px - t[2]) * (t[1] - t[3]) - (t[0] - t[2]) * (py - t[3]);
	float d2 = (px - t[4]) * (t[3] - t[5]) - (t[2] - t[4]) * (py - t[5]);
	float d3 = (px - t[0]) * (t[5] - t[1]) - (t[4] - t[0]) * (py - t[1]);
	int neg = d1 < 0 || d2 < 0 || d3 < 0, pos = d1 > 0 || d2 > 0 || d3 > 0;
	return !(neg && pos);
}

static SDL_Surface *icon_from(int size, uint32_t rgb, int (*inside)(float, float, const float *), const float *shape)
{
	SDL_Surface *s = gfx_new_surface(size, size);
	if (!s) return NULL;
	SDL_LockSurface(s);
	uint32_t *p = s->pixels;
	int pitch = s->pitch / 4;
	for (int y = 0; y < size; y++) {
		for (int x = 0; x < size; x++) {
			int hits = 0;
			for (int sy = 0; sy < SS; sy++)
				for (int sx = 0; sx < SS; sx++)
					hits += inside((float)x + ((float)sx + 0.5f) / SS, (float)y + ((float)sy + 0.5f) / SS, shape);
			uint32_t a = (uint32_t)(hits * 255 / (SS * SS));
			p[y * pitch + x] = a << 24 | (rgb & 0xFFFFFF);
		}
	}
	SDL_UnlockSurface(s);
	return s;
}

/* shape: three triangle points */
static int play_inside(float x, float y, const float *shape)
{
	return in_triangle(x, y, shape);
}

SDL_Surface *gfx_icon_play(int size, uint32_t rgb)
{
	float s = (float)size;
	float tri[6] = {s * 0.30f, s * 0.14f, s * 0.30f, s * 0.86f, s * 0.90f, s * 0.50f};
	return icon_from(size, rgb, play_inside, tri);
}

/* shape: cx, cy, outer radius, inner radius, then the arrowhead triangle. The ring has a gap at the
 * upper right into which the arrowhead points. */
static int restart_inside(float x, float y, const float *shape)
{
	float dx = x - shape[0], dy = y - shape[1], d = sqrtf(dx * dx + dy * dy);
	if (d <= shape[2] && d >= shape[3]) {
		float deg = atan2f(dy, dx) * 180.0f / (float)M_PI;
		if (!(deg > -82 && deg < -8)) return 1;
	}
	return in_triangle(x, y, shape + 4);
}

SDL_Surface *gfx_icon_restart(int size, uint32_t rgb)
{
	float s = (float)size, cx = s / 2, cy = s / 2, r = s * 0.34f, t = s * 0.13f;
	float end = -82.0f * (float)M_PI / 180.0f;
	float px = cx + r * cosf(end), py = cy + r * sinf(end);  /* end of the ring */
	float tx = -sinf(end), ty = cosf(end);                   /* direction of travel there */
	float nx = cosf(end), ny = sinf(end);                    /* outward normal */
	float head = t * 2.2f, half = t * 1.25f;
	float shape[10] = {cx, cy, r + t / 2, r - t / 2,
	                   px + tx * head, py + ty * head,
	                   px + nx * half, py + ny * half,
	                   px - nx * half, py - ny * half};
	return icon_from(size, rgb, restart_inside, shape);
}

/* shape: size, number of sound waves. A box, a cone and arcs to the right of it. */
static int speaker_inside(float x, float y, const float *shape)
{
	float s = shape[0], cx = s * 0.42f, cy = s * 0.5f;
	if (x >= s * 0.08f && x <= s * 0.26f && y >= s * 0.36f && y <= s * 0.64f) return 1;
	float cone[6] = {s * 0.22f, s * 0.36f, s * 0.46f, s * 0.12f, s * 0.46f, s * 0.88f};
	float cone2[6] = {s * 0.22f, s * 0.64f, s * 0.22f, s * 0.36f, s * 0.46f, s * 0.88f};
	if (in_triangle(x, y, cone) || in_triangle(x, y, cone2)) return 1;
	float dx = x - cx, dy = y - cy, d = sqrtf(dx * dx + dy * dy);
	if (dx <= 0 || fabsf(dy) > dx * 0.9f) return 0;
	for (int i = 0; i < (int)shape[1]; i++) {
		float r = s * (0.3f + 0.16f * (float)i), t = s * 0.07f;
		if (d >= r - t / 2 && d <= r + t / 2) return 1;
	}
	return 0;
}

SDL_Surface *gfx_icon_speaker(int size, uint32_t rgb, int waves)
{
	float shape[2] = {(float)size, waves ? 2.0f : 0.0f};
	return icon_from(size, rgb, speaker_inside, shape);
}

/* A Mario-style coin: an upright gold oval with a dark rim and an inner ring, grey once spent. */
static uint32_t coin_sample(float x, float y, float s, int grey)
{
	const uint32_t rim = grey ? 0x6E6A62 : INK, body = grey ? 0xCFC8BD : 0xFFC928;
	const uint32_t ring = grey ? 0xA8A196 : 0xD98E00, shine = grey ? 0xE6E1D8 : 0xFFF1A8;
	float dx = x - s / 2, dy = y - s / 2;
	float rx = s * 0.36f, ry = s * 0.47f, t = s * 0.06f;
	float outer = (dx * dx) / (rx * rx) + (dy * dy) / (ry * ry);
	if (outer > 1) return 0;
	float inner = (dx * dx) / ((rx - t) * (rx - t)) + (dy * dy) / ((ry - t) * (ry - t));
	if (inner > 1) return rim;
	float hx = dx + s * 0.11f, hy = dy + s * 0.2f;
	if ((hx * hx) / (s * s * 0.004f) + (hy * hy) / (s * s * 0.01f) <= 1) return shine;
	float ax = s * 0.17f, ay = s * 0.30f, w = s * 0.055f;
	float o = (dx * dx) / (ax * ax) + (dy * dy) / (ay * ay);
	float i = (dx * dx) / ((ax - w) * (ax - w)) + (dy * dy) / ((ay - w) * (ay - w));
	return o <= 1 && i > 1 ? ring : body;
}

SDL_Surface *gfx_icon_coin(int size, int grey)
{
	SDL_Surface *s = gfx_new_surface(size, size);
	if (!s) return NULL;
	SDL_LockSurface(s);
	uint32_t *p = s->pixels;
	int pitch = s->pitch / 4;
	for (int y = 0; y < size; y++) {
		for (int x = 0; x < size; x++) {
			unsigned hits = 0, r = 0, g = 0, b = 0;
			for (int sy = 0; sy < SS; sy++)
				for (int sx = 0; sx < SS; sx++) {
					uint32_t c = coin_sample((float)x + ((float)sx + 0.5f) / SS, (float)y + ((float)sy + 0.5f) / SS,
					                         (float)size, grey);
					if (!c) continue;
					hits++;
					r += c >> 16 & 0xFF;
					g += c >> 8 & 0xFF;
					b += c & 0xFF;
				}
			if (!hits) {
				p[y * pitch + x] = 0;
				continue;
			}
			uint32_t a = hits * 255 / (SS * SS);
			p[y * pitch + x] = a << 24 | (r / hits) << 16 | (g / hits) << 8 | (b / hits);
		}
	}
	SDL_UnlockSurface(s);
	return s;
}

void gfx_button_hint(int x, int y, const char *letter, uint32_t rgb, const char *action, int align)
{
	const int r = 15, gap = 8;
	int w = 2 * r + gap + gfx_text_width(font_small, action);
	int x0 = align == ALIGN_CENTER ? x - w / 2 : align == ALIGN_RIGHT ? x - w : x;
	SDL_Rect disc = {x0, y - r, 2 * r, 2 * r};
	gfx_fill_rounded_border(disc, r, rgb, INK, 3);
	gfx_text(font_small, letter, 0xFFFFFF, x0 + r, y - TTF_FontHeight(font_small) / 2, ALIGN_CENTER);
	gfx_text(font_small, action, INK, x0 + 2 * r + gap, y - TTF_FontHeight(font_small) / 2, ALIGN_LEFT);
}
