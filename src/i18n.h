#ifndef I18N_H
#define I18N_H

/* The few texts the kid sees, in capitals so early readers can follow them. The language follows the
 * host (Onion's language setting); KIDS_LANG=de overrides it for the desktop build and screenshots. */
enum {
	STR_PLAY,
	STR_GO,
	STR_BACK,
	STR_CONTINUE,
	STR_START_OVER,
	STR_HOLD_TO_LEAVE,
	STR_NO_GAMES,
	STR_NO_GAMES_MARK, /* takes the host name: "... IN %s" */
	STR_NO_GAMES_HOLD,
	STR_NO_COINS,
	STR_POWERING_OFF,
	STR_COUNT
};

void i18n_init(void);
const char *T(int id);
/* two-letter code of the language in use, for logs and screenshots */
const char *i18n_language(void);

#endif
