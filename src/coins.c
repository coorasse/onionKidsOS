#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/stat.h>
#include "coins.h"

static char root_dir[1024];
static int per_day = 10;
static char arcade_tags[256] = "ARCADE,CPS1,CPS2,CPS3,NEOGEO,FBN,FBNEO,MAME";

static void kids_path(const char *file, char *out, size_t n)
{
	snprintf(out, n, "%s/kids/%s", root_dir, file);
}

static void write_default_settings(const char *path)
{
	FILE *f = fopen(path, "w");
	if (!f) return;
	fprintf(f, "# Kids OS settings. Lines are key=value.\n");
	fprintf(f, "# Arcade coins the kid may insert per day (0 = unlimited).\n");
	fprintf(f, "arcade_coins_per_day=%d\n", per_day);
	fprintf(f, "# Systems whose games use coins, by folder tag.\n");
	fprintf(f, "arcade_systems=%s\n", arcade_tags);
	fprintf(f, "# SSH into the console over Wi-Fi (root, no password, like Onion). 0 turns it off.\n");
	fprintf(f, "ssh=1\n");
	fclose(f);
}

void coins_init(const char *root)
{
	snprintf(root_dir, sizeof(root_dir), "%s", root);
	char dir[1100], path[1100];
	snprintf(dir, sizeof(dir), "%s/kids", root);
	mkdir(dir, 0755);
	kids_path("settings.txt", path, sizeof(path));
	FILE *f = fopen(path, "r");
	if (!f) {
		write_default_settings(path);
		return;
	}
	char line[512];
	while (fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\r\n")] = 0;
		if (!*line || *line == '#') continue;
		char *eq = strchr(line, '=');
		if (!eq) continue;
		*eq = 0;
		const char *key = line, *value = eq + 1;
		if (strcmp(key, "arcade_coins_per_day") == 0) per_day = atoi(value);
		else if (strcmp(key, "arcade_systems") == 0) snprintf(arcade_tags, sizeof(arcade_tags), "%s", value);
	}
	fclose(f);
}

int coins_limit(void)
{
	return per_day;
}

int coins_is_arcade(const char *tag)
{
	if (!tag || !*tag || per_day <= 0) return 0;
	size_t len = strlen(tag);
	for (const char *p = arcade_tags; *p;) {
		const char *end = strchr(p, ',');
		size_t n = end ? (size_t)(end - p) : strlen(p);
		if (n == len && strncasecmp(p, tag, n) == 0) return 1;
		if (!end) break;
		p = end + 1;
	}
	return 0;
}

/* The day the coins belong to. The Miyoo has no battery-backed clock: unless Onion synced the time over
 * Wi-Fi the date is in 1970 and would never change, so then the allowance lasts until the next power on. */
static void today(char *buf, size_t n)
{
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	if (tm && tm->tm_year + 1900 >= 2024) {
		strftime(buf, n, "%Y-%m-%d", tm);
		return;
	}
	char id[48] = "";
	FILE *f = fopen("/proc/sys/kernel/random/boot_id", "r");
	if (f) {
		if (!fgets(id, sizeof(id), f)) id[0] = 0;
		fclose(f);
	}
	id[strcspn(id, "\r\n")] = 0;
	snprintf(buf, n, "boot-%s", id[0] ? id : "unknown");
}

int coins_used(void)
{
	char path[1100], date[64] = "", day[64];
	int used = 0;
	kids_path("coins.txt", path, sizeof(path));
	FILE *f = fopen(path, "r");
	if (!f) return 0;
	if (fscanf(f, "%63s %d", date, &used) != 2) used = 0;
	fclose(f);
	today(day, sizeof(day));
	return strcmp(date, day) == 0 ? used : 0;
}

int coins_left(void)
{
	int left = per_day - coins_used();
	return left < 0 ? 0 : left;
}

static void write_used(int used)
{
	char path[1100], day[64];
	kids_path("coins.txt", path, sizeof(path));
	today(day, sizeof(day));
	FILE *f = fopen(path, "w");
	if (!f) return;
	fprintf(f, "%s %d\n", day, used);
	fclose(f);
}

void coins_spend(void)
{
	write_used(coins_used() + 1);
}

