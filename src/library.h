#ifndef LIBRARY_H
#define LIBRARY_H

#include <SDL.h>

#define MAX_SYSTEMS 32

typedef struct Game {
	char path[1024];      /* absolute path to the rom (or .m3u) */
	char rel[512];        /* path relative to Roms/, the id stored in enabled.txt */
	char name[128];       /* cleaned display name */
	char system_tag[16];
	int system;
	int enabled;
	int order;            /* position in the favourites list */
	SDL_Surface *cover;
	int cover_tried;
	SDL_Surface *scaled;  /* render cache */
	SDL_Surface *resume_screen; /* RetroArch save state thumbnail */
	int resume_tried;
	int scaled_w, scaled_h;
} Game;

typedef struct {
	char name[96];
	char tag[32];
	char dir[1024];
	char img_dir[1024];   /* Onion: Roms/<TAG>/Imgs */
	char launcher[1024];  /* Onion: Emu/<TAG>/launch.sh */
	char extlist[256];    /* Onion: allowed extensions, "gba|zip|7z" */
	int count;
	int enabled_count;
} System;

enum { LAYOUT_MINUI, LAYOUT_ONION };

typedef struct {
	int layout;
	System systems[MAX_SYSTEMS];
	int system_count;
	Game *games;
	int game_count;
	int game_cap;
} Library;

extern Library lib;

void library_scan(const char *root);
/* Onion: the games marked as favourites in Onion (Roms/favourite.json); MinUI: kids/enabled.txt */
void library_load_enabled(const char *root);
void library_recount(void);
int library_enabled(Game **out, int max);
SDL_Surface *game_cover(Game *g);
/* Covers downloaded in the background since the last call (Onion: covers.sh) are picked up. */
int library_covers_refresh(void);
/* An auto save state exists for the game (Onion only), its thumbnail if RetroArch wrote one. */
int game_resume_available(Game *g);
SDL_Surface *game_resume_screen(Game *g);
void game_resume_forget(Game *g);

#endif
