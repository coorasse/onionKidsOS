#include "i18n.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct {
	const char *code;
	const char *strings[STR_COUNT];
} languages[] = {
	{"en", {"PLAY", "GO", "BACK", "CONTINUE", "START OVER",
	        "KEEP HOLDING TO LEAVE KIDS MODE",
	        "NO GAMES YET", "MARK GAMES AS FAVOURITES IN %s", "HOLD SELECT + START TO GET THERE",
	        "NO COINS LEFT, COME BACK TOMORROW", "POWERING OFF"}},
	{"de", {"SPIELEN", "GO", "ZURÜCK", "WEITER", "VON VORNE",
	        "HALTEN, UM DEN KINDERMODUS ZU VERLASSEN",
	        "NOCH KEINE SPIELE", "MARKIERE SPIELE IN %s ALS FAVORITEN", "HALTE SELECT + START, UM DORTHIN ZU KOMMEN",
	        "KEINE MÜNZEN MEHR, BIS MORGEN", "SCHALTET AUS"}},
	{"it", {"GIOCA", "GO", "INDIETRO", "CONTINUA", "DA CAPO",
	        "TIENI PREMUTO PER USCIRE DALLA MODALITÀ BAMBINI",
	        "ANCORA NESSUN GIOCO", "SEGNA I GIOCHI COME PREFERITI IN %s", "TIENI PREMUTO SELECT + START PER ARRIVARCI",
	        "MONETE FINITE, TORNA DOMANI", "SPEGNIMENTO"}},
	{"fr", {"JOUER", "GO", "RETOUR", "CONTINUER", "RECOMMENCER",
	        "MAINTIENS POUR QUITTER LE MODE ENFANTS",
	        "PAS ENCORE DE JEUX", "MARQUE DES JEUX COMME FAVORIS DANS %s", "MAINTIENS SELECT + START POUR Y ALLER",
	        "PLUS DE PIÈCES, REVIENS DEMAIN", "EXTINCTION"}},
	{"es", {"JUGAR", "GO", "ATRÁS", "CONTINUAR", "DE NUEVO",
	        "MANTÉN PULSADO PARA SALIR DEL MODO NIÑOS",
	        "TODAVÍA NO HAY JUEGOS", "MARCA JUEGOS COMO FAVORITOS EN %s", "MANTÉN SELECT + START PARA LLEGAR",
	        "SIN MONEDAS, VUELVE MAÑANA", "APAGANDO"}},
	{"nl", {"SPELEN", "GO", "TERUG", "VERDER", "OPNIEUW",
	        "BLIJF VASTHOUDEN OM DE KINDERMODUS TE VERLATEN",
	        "NOG GEEN SPELLEN", "MARKEER SPELLEN ALS FAVORIET IN %s", "HOUD SELECT + START VAST OM ER TE KOMEN",
	        "GEEN MUNTEN MEER, TOT MORGEN", "UITSCHAKELEN"}},
};
#define LANGUAGE_COUNT (sizeof(languages) / sizeof(languages[0]))

static int current;

void i18n_init(void)
{
	const char *code = getenv("KIDS_LANG");
	if (!code || !*code) code = PLAT_language();
	current = 0;
	if (!code) return;
	for (size_t i = 0; i < LANGUAGE_COUNT; i++)
		if (strncmp(code, languages[i].code, 2) == 0) current = (int)i;
	if (strncmp(code, languages[current].code, 2) != 0) fprintf(stderr, "language %s not available, using en\n", code);
}

const char *T(int id)
{
	return languages[current].strings[id];
}

const char *i18n_language(void)
{
	return languages[current].code;
}
