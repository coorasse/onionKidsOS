#ifndef COINS_H
#define COINS_H

/* Daily arcade coins. One coin is one press of the coin button (SELECT) in an arcade game.
 * State lives in <root>/kids/coins.txt as "YYYY-MM-DD <used>", settings in <root>/kids/settings.txt. */
void coins_init(const char *root);
int coins_limit(void);
int coins_is_arcade(const char *system_tag);
int coins_used(void);
int coins_left(void);
void coins_spend(void);

#endif
