#define _DEFAULT_SOURCE
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include "library.h"
#include "gfx.h"

Library lib;

static const char *skipped_exts[] = {"bin", "img", "sub", "ccd", "sav", "srm", "txt", "cfg", "png", "jpg", "jpeg",
                                     "dat", "state", "bak", "db", "xml", "nfo", "mcd", "json", "md5", "sha1", NULL};

static const char *ext_of(const char *name)
{
	const char *dot = strrchr(name, '.');
	return dot && dot != name ? dot + 1 : "";
}

static int is_skipped(const char *name)
{
	const char *ext = ext_of(name);
	for (int i = 0; skipped_exts[i]; i++)
		if (strcasecmp(ext, skipped_exts[i]) == 0) return 1;
	return 0;
}

static void trim_right(char *s)
{
	size_t n = strlen(s);
	while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t')) s[--n] = 0;
}

static void clean_name(const char *file, char *out, size_t n)
{
	snprintf(out, n, "%s", file);
	char *dot = strrchr(out, '.');
	if (dot && dot != out) *dot = 0;
	for (;;) {
		trim_right(out);
		size_t len = strlen(out);
		if (len == 0) break;
		char close = out[len - 1], open = close == ')' ? '(' : close == ']' ? '[' : 0;
		if (!open) break;
		char *start = strrchr(out, open);
		if (!start || start == out) break;
		*start = 0;
	}
	trim_right(out);
	if (!*out) snprintf(out, n, "%s", file);
}

static void parse_system_dir(const char *dirname, System *s)
{
	snprintf(s->name, sizeof(s->name), "%s", dirname);
	s->tag[0] = 0;
	char *open = strrchr(s->name, '(');
	char *close = strrchr(s->name, ')');
	if (open && close && close > open) {
		snprintf(s->tag, sizeof(s->tag), "%.*s", (int)(close - open - 1), open + 1);
		*open = 0;
	}
	trim_right(s->name);
}

static Game *add_game(void)
{
	if (lib.game_count == lib.game_cap) {
		lib.game_cap = lib.game_cap ? lib.game_cap * 2 : 64;
		lib.games = realloc(lib.games, (size_t)lib.game_cap * sizeof(Game));
	}
	Game *g = &lib.games[lib.game_count++];
	memset(g, 0, sizeof(*g));
	return g;
}

static int compare_names(const void *a, const void *b)
{
	return strcasecmp(*(const char *const *)a, *(const char *const *)b);
}

static char **list_dir(const char *path, int *count)
{
	*count = 0;
	DIR *d = opendir(path);
	if (!d) return NULL;
	int cap = 32;
	char **names = malloc((size_t)cap * sizeof(char *));
	struct dirent *e;
	while ((e = readdir(d))) {
		if (e->d_name[0] == '.') continue;
		if (*count == cap) names = realloc(names, (size_t)(cap *= 2) * sizeof(char *));
		names[(*count)++] = strdup(e->d_name);
	}
	closedir(d);
	qsort(names, (size_t)*count, sizeof(char *), compare_names);
	return names;
}

static void free_list(char **names, int count)
{
	for (int i = 0; i < count; i++) free(names[i]);
	free(names);
}

static int is_dir(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static char roms_prefix[1024]; /* "<root>/Roms/", stripped from ids in enabled.txt */
static char sd_root[1024];

static void register_game(int sys_idx, const char *abs, const char *file)
{
	Game *g = add_game();
	snprintf(g->path, sizeof(g->path), "%s", abs);
	size_t n = strlen(roms_prefix);
	snprintf(g->rel, sizeof(g->rel), "%s", strncmp(abs, roms_prefix, n) == 0 ? abs + n : abs);
	clean_name(file, g->name, sizeof(g->name));
	snprintf(g->system_tag, sizeof(g->system_tag), "%s", lib.systems[sys_idx].tag);
	g->system = sys_idx;
	lib.systems[sys_idx].count++;
}

static const char *find_m3u(const char *dir)
{
	static char found[256];
	int n;
	char **names = list_dir(dir, &n);
	found[0] = 0;
	for (int i = 0; i < n && !found[0]; i++)
		if (strcasecmp(ext_of(names[i]), "m3u") == 0) snprintf(found, sizeof(found), "%s", names[i]);
	free_list(names, n);
	return found[0] ? found : NULL;
}

static int ext_allowed(const System *sys, const char *name)
{
	if (!sys->extlist[0]) return !is_skipped(name);
	const char *ext = ext_of(name);
	size_t elen = strlen(ext);
	if (!elen) return 0;
	for (const char *p = sys->extlist; *p;) {
		const char *end = strchr(p, '|');
		size_t len = end ? (size_t)(end - p) : strlen(p);
		if (len == elen && strncasecmp(p, ext, len) == 0) return 1;
		if (!end) break;
		p = end + 1;
	}
	return 0;
}

/* A .bin/.img next to a .cue of the same name is disc data, the .cue is the game. */
static int is_disc_data(const char *abs)
{
	const char *ext = ext_of(abs);
	if (strcasecmp(ext, "bin") != 0 && strcasecmp(ext, "img") != 0) return 0;
	char cue[1024];
	snprintf(cue, sizeof(cue), "%.*s.cue", (int)(ext - abs - 1), abs);
	struct stat st;
	return stat(cue, &st) == 0;
}

static void scan_dir(int sys_idx, const char *abs_dir, int depth)
{
	System *sys = &lib.systems[sys_idx];
	int n;
	char **names = list_dir(abs_dir, &n);
	for (int i = 0; i < n; i++) {
		char abs[1024];
		snprintf(abs, sizeof(abs), "%s/%s", abs_dir, names[i]);
		if (is_dir(abs)) {
			if (depth >= 1 || strcasecmp(names[i], "Imgs") == 0) continue;
			const char *m3u = find_m3u(abs);
			if (m3u) {
				char mabs[1024];
				snprintf(mabs, sizeof(mabs), "%s/%s", abs, m3u);
				register_game(sys_idx, mabs, names[i]);
			} else {
				scan_dir(sys_idx, abs, depth + 1);
			}
		} else if (ext_allowed(sys, names[i]) && !is_disc_data(abs)) {
			register_game(sys_idx, abs, names[i]);
		}
	}
	free_list(names, n);
}

/* Minimal JSON string lookup, enough for Onion's flat Emu/<TAG>/config.json. */
static int json_string(const char *json, const char *key, char *out, size_t n)
{
	char quoted[64];
	snprintf(quoted, sizeof(quoted), "\"%s\"", key);
	const char *p = strstr(json, quoted);
	if (!p) return 0;
	p = strchr(p + strlen(quoted), ':');
	if (!p) return 0;
	p = strchr(p, '"');
	if (!p) return 0;
	p++;
	const char *end = strchr(p, '"');
	if (!end) return 0;
	snprintf(out, n, "%.*s", (int)(end - p), p);
	return 1;
}

/* Joins base and a relative path, collapsing "." and ".." segments. Keeps a relative base relative. */
static void resolve_path(const char *base, const char *rel, char *out, size_t n)
{
	char joined[2048];
	if (rel[0] == '/') snprintf(joined, sizeof(joined), "%s", rel);
	else snprintf(joined, sizeof(joined), "%s/%s", base, rel);
	int absolute = joined[0] == '/';

	char *parts[256];
	int count = 0;
	for (char *tok = strtok(joined, "/"); tok && count < 256; tok = strtok(NULL, "/")) {
		if (strcmp(tok, ".") == 0) continue;
		if (strcmp(tok, "..") == 0 && count > 0 && strcmp(parts[count - 1], "..") != 0) {
			count--;
			continue;
		}
		parts[count++] = tok;
	}
	size_t o = 0;
	out[0] = 0;
	for (int i = 0; i < count && o + 1 < n; i++)
		o += (size_t)snprintf(out + o, n - o, "%s%s", i == 0 && !absolute ? "" : "/", parts[i]);
	if (!out[0]) snprintf(out, n, "%s", absolute ? "/" : ".");
}

static int read_small_file(const char *path, char *buf, size_t n)
{
	FILE *f = fopen(path, "r");
	if (!f) return 0;
	size_t got = fread(buf, 1, n - 1, f);
	fclose(f);
	buf[got] = 0;
	return 1;
}

static void scan_onion(const char *root)
{
	char emus[1024];
	snprintf(emus, sizeof(emus), "%s/Emu", root);
	int n;
	char **names = list_dir(emus, &n);
	for (int i = 0; i < n && lib.system_count < MAX_SYSTEMS; i++) {
		char emu_dir[1024], cfg_path[1100], json[8192];
		snprintf(emu_dir, sizeof(emu_dir), "%s/%s", emus, names[i]);
		snprintf(cfg_path, sizeof(cfg_path), "%s/config.json", emu_dir);
		if (!read_small_file(cfg_path, json, sizeof(json))) continue;

		int idx = lib.system_count;
		System *s = &lib.systems[idx];
		memset(s, 0, sizeof(*s));
		snprintf(s->tag, sizeof(s->tag), "%s", names[i]);
		if (!json_string(json, "label", s->name, sizeof(s->name))) snprintf(s->name, sizeof(s->name), "%s", names[i]);

		char rel[512] = "", launch[256] = "launch.sh";
		if (!json_string(json, "rompath", rel, sizeof(rel))) snprintf(rel, sizeof(rel), "../../Roms/%s", names[i]);
		resolve_path(emu_dir, rel, s->dir, sizeof(s->dir));
		if (json_string(json, "imgpath", rel, sizeof(rel))) resolve_path(emu_dir, rel, s->img_dir, sizeof(s->img_dir));
		else snprintf(s->img_dir, sizeof(s->img_dir), "%s/Imgs", s->dir);
		json_string(json, "launch", launch, sizeof(launch));
		resolve_path(emu_dir, launch, s->launcher, sizeof(s->launcher));
		json_string(json, "extlist", s->extlist, sizeof(s->extlist));

		int before = lib.game_count;
		scan_dir(idx, s->dir, 0);
		if (lib.game_count > before) lib.system_count++;
	}
	free_list(names, n);
}

static void scan_minui(const char *root)
{
	char roms[1024];
	snprintf(roms, sizeof(roms), "%s/Roms", root);
	int n;
	char **names = list_dir(roms, &n);
	if (!names) {
		fprintf(stderr, "no Roms folder in %s\n", root);
		return;
	}
	for (int i = 0; i < n && lib.system_count < MAX_SYSTEMS; i++) {
		char abs[1024];
		snprintf(abs, sizeof(abs), "%s/%s", roms, names[i]);
		if (!is_dir(abs)) continue;
		int idx = lib.system_count;
		System *s = &lib.systems[idx];
		memset(s, 0, sizeof(*s));
		parse_system_dir(names[i], s);
		snprintf(s->dir, sizeof(s->dir), "%s", abs);
		int before = lib.game_count;
		scan_dir(idx, abs, 0);
		if (lib.game_count > before) lib.system_count++;
	}
	free_list(names, n);
}

void library_scan(const char *root)
{
	char emus[1024];
	snprintf(emus, sizeof(emus), "%s/Emu", root);
	snprintf(roms_prefix, sizeof(roms_prefix), "%s/Roms/", root);
	snprintf(sd_root, sizeof(sd_root), "%s", root);
	lib.layout = is_dir(emus) ? LAYOUT_ONION : LAYOUT_MINUI;
	if (lib.layout == LAYOUT_ONION) scan_onion(root);
	else scan_minui(root);
}

static void enabled_path(const char *root, char *out, size_t n)
{
	snprintf(out, n, "%s/kids/enabled.txt", root);
}

/* Onion keeps one JSON object per line, with "rompath" as the launcher wrote it, usually through
 * Emu/<TAG>/../../Roms/...; the path is normalised the same way as the scanned ones. The card is
 * /mnt/SDCARD on the device, on the desktop that prefix is swapped for the fixture root. */
static int json_string_field(const char *line, const char *key, char *out, size_t n)
{
	const char *p = strstr(line, key);
	if (!p || !(p = strchr(p + strlen(key), '"'))) return 0;
	p++;
	const char *end = strchr(p, '"');
	if (!end) return 0;
	snprintf(out, n, "%.*s", (int)(end - p), p);
	return 1;
}

static int favourite_rompath(const char *line, const char *root, char *out, size_t n)
{
	char raw[1024];
	if (!json_string_field(line, "\"rompath\"", raw, sizeof(raw))) return 0;
	const char *device_root = "/mnt/SDCARD";
	char mapped[1200];
	if (strncmp(raw, device_root, strlen(device_root)) == 0 && strcmp(root, device_root) != 0)
		snprintf(mapped, sizeof(mapped), "%s%s", root, raw + strlen(device_root));
	else
		snprintf(mapped, sizeof(mapped), "%s", raw);
	resolve_path(".", mapped, out, n);
	return 1;
}

static void load_favourites(const char *root, FILE *f)
{
	char line[4096], path[1200];
	int order = 0;
	while (fgets(line, sizeof(line), f)) {
		if (!favourite_rompath(line, root, path, sizeof(path))) continue;
		for (int i = 0; i < lib.game_count; i++) {
			if (lib.games[i].enabled || strcmp(lib.games[i].path, path) != 0) continue;
			lib.games[i].enabled = 1;
			lib.games[i].order = order++;
			/* Onion's label is nicer than the file name for arcade roms ("mslug") */
			char label[128];
			if (json_string_field(line, "\"label\"", label, sizeof(label)) && label[0])
				snprintf(lib.games[i].name, sizeof(lib.games[i].name), "%s", label);
		}
	}
}

static void load_enabled_txt(FILE *f)
{
	char line[600];
	int order = 0;
	while (fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\r\n")] = 0;
		trim_right(line);
		if (!*line || *line == '#') continue;
		for (int i = 0; i < lib.game_count; i++) {
			if (lib.games[i].enabled || strcmp(lib.games[i].rel, line) != 0) continue;
			lib.games[i].enabled = 1;
			lib.games[i].order = order++;
		}
	}
}

void library_load_enabled(const char *root)
{
	char path[1024];
	if (lib.layout == LAYOUT_ONION) snprintf(path, sizeof(path), "%s/Roms/favourite.json", root);
	else enabled_path(root, path, sizeof(path));
	FILE *f = fopen(path, "r");
	if (!f) return;
	if (lib.layout == LAYOUT_ONION) load_favourites(root, f);
	else load_enabled_txt(f);
	fclose(f);
	library_recount();
}

void library_recount(void)
{
	for (int s = 0; s < lib.system_count; s++) lib.systems[s].enabled_count = 0;
	for (int i = 0; i < lib.game_count; i++)
		if (lib.games[i].enabled) lib.systems[lib.games[i].system].enabled_count++;
}

static int by_order(const void *a, const void *b)
{
	return (*(Game *const *)a)->order - (*(Game *const *)b)->order;
}

int library_enabled(Game **out, int max)
{
	int n = 0;
	for (int i = 0; i < lib.game_count && n < max; i++)
		if (lib.games[i].enabled) out[n++] = &lib.games[i];
	qsort(out, (size_t)n, sizeof(Game *), by_order);
	return n;
}

SDL_Surface *game_cover(Game *g)
{
	if (g->cover_tried) return g->cover;
	g->cover_tried = 1;

	char dir[1024], file[256];
	snprintf(dir, sizeof(dir), "%s", g->path);
	char *slash = strrchr(dir, '/');
	snprintf(file, sizeof(file), "%s", slash ? slash + 1 : dir);
	if (slash) *slash = 0;

	char candidates[5][1200];
	int n = 0;
	snprintf(candidates[n++], sizeof(candidates[0]), "%s/.res/%s.png", dir, file);
	char *dot = strrchr(file, '.');
	if (dot) *dot = 0;
	const System *sys = &lib.systems[g->system];
	if (sys->img_dir[0]) snprintf(candidates[n++], sizeof(candidates[0]), "%s/%s.png", sys->img_dir, file);
	snprintf(candidates[n++], sizeof(candidates[0]), "%s/.res/%s.png", dir, file);
	char *parent_slash = strrchr(dir, '/');
	if (parent_slash) {
		const char *folder = parent_slash + 1;
		char parent[1024];
		snprintf(parent, sizeof(parent), "%.*s", (int)(parent_slash - dir), dir);
		snprintf(candidates[n++], sizeof(candidates[0]), "%s/.res/%s.png", parent, folder);
	}
	for (int i = 0; i < n && !g->cover; i++) g->cover = gfx_load_image(candidates[i]);
	return g->cover;
}

/* covers.sh raises a flag after every cover it downloads: the misses are forgotten so the next frame loads
 * the new pictures. Returns 1 when a tile may have changed. */
int library_covers_refresh(void)
{
	char flag[1100];
	if (lib.layout == LAYOUT_ONION) snprintf(flag, sizeof(flag), "/tmp/kidsos_covers_changed");
	else snprintf(flag, sizeof(flag), "%s/kids/covers_changed", sd_root);
	if (unlink(flag) != 0) return 0;
	int changed = 0;
	for (int i = 0; i < lib.game_count; i++) {
		if (!lib.games[i].cover_tried || lib.games[i].cover) continue;
		lib.games[i].cover_tried = 0;
		changed = 1;
	}
	return changed;
}

/* RetroArch keeps the auto save state in Saves/CurrentProfile/states/<core name>/<rom>.state.auto on Onion;
 * the core name is not known here, so every core folder is tried. */
static int resume_state_path(Game *g, char *out, size_t n)
{
	if (lib.layout != LAYOUT_ONION) return 0;
	char base[256];
	const char *slash = strrchr(g->path, '/');
	snprintf(base, sizeof(base), "%s", slash ? slash + 1 : g->path);
	char *dot = strrchr(base, '.');
	if (dot) *dot = 0;

	char states[1200];
	snprintf(states, sizeof(states), "%s/Saves/CurrentProfile/states", sd_root);
	DIR *d = opendir(states);
	if (!d) return 0;
	int found = 0;
	struct dirent *e;
	while (!found && (e = readdir(d))) {
		if (e->d_name[0] == '.') continue;
		snprintf(out, n, "%s/%s/%s.state.auto", states, e->d_name, base);
		struct stat st;
		found = stat(out, &st) == 0 && S_ISREG(st.st_mode);
	}
	closedir(d);
	return found;
}

int game_resume_available(Game *g)
{
	char path[1500];
	return resume_state_path(g, path, sizeof(path));
}

SDL_Surface *game_resume_screen(Game *g)
{
	if (g->resume_tried) return g->resume_screen;
	g->resume_tried = 1;
	char path[1500];
	if (!resume_state_path(g, path, sizeof(path))) return NULL;
	strncat(path, ".png", sizeof(path) - strlen(path) - 1);
	g->resume_screen = gfx_load_image(path);
	return g->resume_screen;
}

void game_resume_forget(Game *g)
{
	if (g->resume_screen) SDL_FreeSurface(g->resume_screen);
	g->resume_screen = NULL;
	g->resume_tried = 0;
}
