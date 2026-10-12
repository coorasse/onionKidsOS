/* Runs next to an arcade game on the device (see onion/game.sh). Counts presses of the coin button
 * on the input device and, once the daily coins are spent, raises a flag that makes the preloaded
 * coinlock library hide the coin button from RetroArch: the game goes on with the credits it has and
 * SELECT does nothing until the next day.
 *
 * Usage: coinmon.elf <sdcard root>        watch SELECT
 *        coinmon.elf --press <keycode>     type one key into RetroArch's terminal and exit; RetroArch's
 *                                          SDL reads the console in medium-raw mode, which is how a
 *                                          button press is simulated over SSH when testing */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include "coins.h"

#define INPUT_DEVICE "/dev/input/event0"
#define COIN_KEY KEY_RIGHTCTRL /* SELECT on the Miyoo Mini */
#define LOCK_FLAG "/tmp/kidsos_no_coins"
#define DEBOUNCE_MS 300
#ifndef TIOCSTI
#define TIOCSTI 0x5412
#endif

/* the terminal RetroArch has open, /dev/tty2 on Onion; looked up from /proc in case that changes */
static void retroarch_tty(char *out, size_t n)
{
	snprintf(out, n, "/dev/tty2");
	DIR *proc = opendir("/proc");
	if (!proc) return;
	struct dirent *e;
	while ((e = readdir(proc))) {
		if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
		char path[300], cmd[256] = "";
		snprintf(path, sizeof(path), "/proc/%s/cmdline", e->d_name);
		FILE *f = fopen(path, "r");
		if (!f) continue;
		size_t len = fread(cmd, 1, sizeof(cmd) - 1, f);
		fclose(f);
		cmd[len] = 0;
		if (!strstr(cmd, "retroarch")) continue;
		snprintf(path, sizeof(path), "/proc/%s/fd", e->d_name);
		DIR *fds = opendir(path);
		if (!fds) continue;
		struct dirent *fe;
		while ((fe = readdir(fds))) {
			char link[600], target[64];
			snprintf(link, sizeof(link), "%s/%s", path, fe->d_name);
			ssize_t t = readlink(link, target, sizeof(target) - 1);
			if (t <= 0) continue;
			target[t] = 0;
			if (strncmp(target, "/dev/tty", 8) == 0 && target[8] >= '0' && target[8] <= '9') {
				snprintf(out, n, "%s", target);
				break;
			}
		}
		closedir(fds);
		break;
	}
	closedir(proc);
}

static int type_key(int keycode)
{
	char tty[64];
	retroarch_tty(tty, sizeof(tty));
	int fd = open(tty, O_RDWR | O_NOCTTY);
	if (fd < 0) {
		perror(tty);
		return -1;
	}
	unsigned char press = (unsigned char)keycode, release = (unsigned char)(keycode | 0x80);
	int rc = ioctl(fd, TIOCSTI, &press);
	usleep(60000);
	if (rc == 0) rc = ioctl(fd, TIOCSTI, &release);
	if (rc != 0) perror("TIOCSTI");
	close(fd);
	return rc;
}

static void set_lock(int on)
{
	if (!on) {
		unlink(LOCK_FLAG);
		return;
	}
	FILE *f = fopen(LOCK_FLAG, "w");
	if (f) fclose(f);
}

int main(int argc, char **argv)
{
	if (argc == 3 && strcmp(argv[1], "--press") == 0) return type_key(atoi(argv[2])) == 0 ? 0 : 1;

	coins_init(argc > 1 ? argv[1] : "/mnt/SDCARD");
	int fd = open(INPUT_DEVICE, O_RDONLY);
	if (fd < 0) {
		perror(INPUT_DEVICE);
		return 1;
	}
	set_lock(coins_left() <= 0);
	printf("coinmon: %d coins left of %d\n", coins_left(), coins_limit());
	fflush(stdout);

	struct input_event ev;
	long long last_ms = 0;
	int lock_on_release = 0;
	while (read(fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
		if (ev.type != EV_KEY || ev.code != COIN_KEY) continue;
		if (ev.value == 0) {
			/* the lock waits for the key to come up, so RetroArch never misses a release */
			if (lock_on_release) {
				set_lock(1);
				lock_on_release = 0;
				printf("coinmon: no coins left, SELECT is now ignored by the game\n");
				fflush(stdout);
			}
			continue;
		}
		if (ev.value != 1 || coins_left() <= 0) continue;
		long long now_ms = (long long)ev.time.tv_sec * 1000 + ev.time.tv_usec / 1000;
		if (now_ms - last_ms < DEBOUNCE_MS) continue;
		last_ms = now_ms;
		coins_spend();
		printf("coinmon: coin inserted, %d left\n", coins_left());
		fflush(stdout);
		if (coins_left() <= 0) lock_on_release = 1;
	}
	close(fd);
	return 0;
}
