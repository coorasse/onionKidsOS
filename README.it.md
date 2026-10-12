# KidsOS per il Miyoo Mini Plus

*[English version](README.md)*

Una console per bambini piccoli: si accende, si sceglie un gioco, si gioca. Non si può fare altro.

![La schermata dei bambini: una grande immagine di gioco alla volta](docs/images/kid-ui.png)

Il bambino vede una cosa sola: una fila di grandi immagini dei giochi. Sinistra e destra per scegliere,
**A** per giocare, **MENU** per smettere. Un gioco già giocato prima fa una sola domanda per immagini:
continuare da dove si era rimasti, o ricominciare. Non serve saper leggere, non ci sono menu né impostazioni.

Il genitore decide quali giochi stanno nella fila e può raggiungere il sistema completo con una
combinazione segreta di tasti.

KidsOS è [Onion](https://onionui.github.io/) (il firmware alternativo più diffuso per il Miyoo Mini) con
un'interfaccia diversa, a misura di bambino. Tutto il resto è Onion: emulatori, salvataggi, Wi-Fi, copertine.

> **Versione iniziale.** Il launcher è stato usato su un vero Miyoo Mini Plus, ma lo zip di installazione
> qui sotto è stato provato solo con un avvio emulato. Se qualcosa va storto, la sezione
> [problemi](#problemi) spiega come tornare a una console funzionante: non viene scritto niente nella
> console, solo sulla scheda SD.

## Cosa serve

- Un **Miyoo Mini Plus**. Il Miyoo Mini originale dovrebbe funzionare ma non è stato provato.
- Una **microSD** da 32 GB o più di una marca affidabile (SanDisk, Samsung). Quella in dotazione con la
  console è lenta e inaffidabile.
- Un computer con lettore di schede.
- I file dei giochi (ROM) per i sistemi che vuoi: Game Boy, NES, SNES, PlayStation e
  [molti altri](https://onionui.github.io/docs/emulators/folders).

## Installazione

Ci vuole un quarto d'ora, quasi tutto di attesa.

### 1. Formatta la scheda in FAT32

- **Mac:** apri *Utility Disco*, seleziona la scheda (la voce di primo livello, non il volume sotto),
  clicca *Inizializza*, scegli Formato *MS-DOS (FAT)* e Schema *Master Boot Record*, clicca *Inizializza*.
- **Windows:** per schede oltre i 32 GB usa [Rufus](https://rufus.ie/): Selezione boot *Non avviabile*,
  File system *Large FAT32*, poi *Avvia*.

### 2. Copia KidsOS sulla scheda

1. Scarica l'ultimo `KidsOS-x.y.z.zip` dalla
   [pagina delle release](https://github.com/coorasse/onionKidsOS/releases).
2. Scompattalo. Ottieni sette cartelle e due piccoli file di testo.
3. Copia **tutto** nella radice della scheda. Una delle cartelle, `.tmp_update`, è nascosta: su Mac premi
   `Cmd + Maiusc + .` nel Finder per vedere i file nascosti, su Windows attiva *Elementi nascosti* nel menu
   *Visualizza*. Senza quella cartella non succede nulla.

### 3. Primo avvio

1. Metti la scheda nella console e accendila. Onion si installa da solo; ci vogliono alcuni minuti.
2. Compare una breve guida a Onion. Scorrila con **A**.
3. Compare il *Package Manager*: una lista di sistemi. Muoviti con la croce direzionale, premi **A** per
   spuntare quelli per cui hai dei giochi (per esempio *Game Boy*, *SNES*, *PlayStation*), poi premi
   **START** due volte.
4. La console si riavvia nella schermata dei bambini. È vuota: non è ancora stato scelto nessun gioco.

### 4. Aggiungi i giochi

1. Spegni la console e rimetti la scheda nel computer.
2. Copia i file dei giochi in `Roms/<SISTEMA>/`. I nomi delle cartelle sono sigle:
   `GB` (Game Boy), `GBC` (Game Boy Color), `GBA` (Game Boy Advance), `FC` (NES), `SFC` (SNES),
   `PS` (PlayStation). La tabella completa è nella
   [documentazione di Onion](https://onionui.github.io/docs/emulators/folders).
3. Facoltativo, ma è quello che rende bella la schermata: un'immagine per ogni gioco, un PNG con lo stesso
   nome del file del gioco, in `Roms/<SISTEMA>/Imgs/`. Esempio: `Roms/GB/Tetris.gb` e
   `Roms/GB/Imgs/Tetris.png`. Senza immagine il riquadro mostra il nome del gioco.
4. Rimetti la scheda e accendi la console.
5. Tieni premuti **SELECT + START** insieme per tre secondi: la schermata dei bambini sparisce e compare
   il menu di Onion.
6. In *Games*, apri un sistema, evidenzia un gioco, premi **SELECT** e scegli *Add to favorites*. Ripeti
   per ogni gioco che il bambino può usare. **I preferiti sono la lista del bambino.**
7. Spegni e riaccendi la console. I giochi sono lì.

Per togliere un gioco più avanti, stesso procedimento e *Remove from favorites*.

## Uso

| Tasto | Cosa fa |
|-------|---------|
| Sinistra / Destra | Sceglie un gioco |
| A | Gioca |
| MENU (durante un gioco) | Salva e torna alla lista dei giochi |
| Volume + / - | Volume |
| SELECT + START premuti 3 s | Genitore: esce dalla schermata bambini e apre Onion fino al prossimo spegnimento |

Quando un gioco viene riaperto, una schermata con due immagini chiede se **continuare** (bollino verde,
uno scatto di dove si era rimasti) o **ricominciare** (freccia arancione, l'immagine del gioco).

Tutto il resto, compreso il menu interno dell'emulatore, è fuori dalla portata del bambino. Il genitore può
comunque cambiare le impostazioni di Onion (luminosità, musica del menu, Wi-Fi) dopo la combinazione
segreta, in *Apps* › *Tweaks*.

## Aggiornare KidsOS

Scarica il nuovo zip, copia le cartelle sulla scheda sopra quelle esistenti (sostituisci quando viene
chiesto), rimetti la scheda e accendi la console. Onion si aggiorna da solo e conserva giochi, salvataggi e
preferiti.

Non usare l'app *OTA update* di Onion: installerebbe Onion liscio e la schermata dei bambini sparirebbe.
Copiare di nuovo uno zip di KidsOS sulla scheda la fa tornare.

## Tornare a Onion normale

Scarica [Onion](https://github.com/OnionUI/Onion/releases), copia le sue cartelle sulla scheda nello stesso
modo e cancella dalla scheda la cartella nascosta `.kidsos`. Giochi e salvataggi restano.

## Problemi

- **Schermo nero, o non reagisce più:** tieni premuto **POWER** finché la console si spegne, poi riaccendila.
- **La schermata dei bambini è vuota:** non ci sono ancora preferiti, vedi [Aggiungi i giochi](#4-aggiungi-i-giochi).
- **La console parte nel menu di Onion invece che nella schermata dei bambini:** copia di nuovo lo zip di
  KidsOS sulla scheda come in [Copia KidsOS sulla scheda](#2-copia-kidsos-sulla-scheda). Non serve altro.
- **Un gioco non parte:** provalo dal menu di Onion (SELECT + START). Se non parte neanche lì, il problema
  è il file del gioco o l'emulatore, non KidsOS.

## Come funziona, per i curiosi

Onion vive interamente sulla scheda SD; il firmware della console si limita ad avviarlo. KidsOS aggiunge un
piccolo launcher al pacchetto di installazione di Onion e uno script di avvio che mette il launcher davanti
al menu di Onion. Gli script di Onion non vengono modificati, quindi Onion continua a funzionare come
documentato, e copiare un nuovo zip di KidsOS sulla scheda riporta sempre la schermata dei bambini. Dettagli,
istruzioni di build e ambiente di sviluppo in [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) (in inglese).

KidsOS è software libero sotto licenza [GPL-3.0](LICENSE), come Onion. Non è affiliato a Miyoo né al team
di Onion.
