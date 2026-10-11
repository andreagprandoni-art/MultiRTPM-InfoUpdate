# Multistream Info Panel

Plugin per **OBS Studio** (Windows) che aggiunge un pannello **"Info Live"**: da un solo posto aggiorni
**titolo, categoria/gioco, tag, lingua e descrizione** della tua live su **Twitch, Kick e YouTube** con un clic.

- Accedi con il **tuo account** su ogni piattaforma, una sola volta: nessuna app o chiave da creare.
- Salvi dei **preset** (es. uno per ogni gioco) e li richiami al volo.
- Il plugin parla direttamente con Twitch, Kick e Google: nessun server intermedio.

> Requisiti: OBS Studio 31 o successivo (64 bit) su Windows.

## Installazione

1. Scarica l'ultima versione dalla pagina **Releases** di questo repository.
2. Chiudi OBS.
3. **Con l'installer `.exe`**: avvialo. **Con lo zip**: estrailo dentro la cartella di OBS
   (di solito `C:\Program Files\obs-studio`), in modo che i file finiscano in `obs-plugins\64bit\`
   e `data\obs-plugins\`.
4. Apri OBS → menu **Docks** → attiva **Info Live**. Il pannello si può spostare e agganciare dove vuoi.

## Primo accesso

Nel pannello, accanto a ogni piattaforma, premi **Collega**: si apre il browser, accedi col tuo account
e accetta i permessi. Quando vedi "Collegato!" torna a OBS: il pallino diventa verde.
Si fa una volta sola. Con **Scollega** rimuovi l'accesso dal plugin.

- **YouTube**: se Google mostra "app non verificata", clicca **Avanzate → Continua**.
- Il login usa la porta **8426** del tuo computer: se un altro programma la occupa, il plugin te lo dice.

## YouTube con la tua app Google (facoltativo)

Di norma basta **Collega**. Se l'accesso con l'app del plugin non funziona, o se YouTube segnala che la quota
giornaliera condivisa è esaurita, puoi usare una tua app Google (gratis):

1. Su console.cloud.google.com crea un progetto e abilita **YouTube Data API v3**.
2. Schermata consenso OAuth: tipo **Esterno**; aggiungi il tuo account Google tra gli **utenti di test**.
3. Credenziali → **ID client OAuth** → tipo **App desktop**. Copia Client ID e Client secret.
4. Nel pannello apri **Impostazioni avanzate (YouTube)…**, incolla i due valori, salva, poi premi **Collega**.

Una "chiave API" semplice non basta: per modificare la tua diretta serve il login con il tuo account, quindi
servono Client ID e Client secret. Per tornare all'app del plugin svuota i due campi e ricollega.

## Uso quotidiano

1. Scrivi il **titolo**.
2. Scrivi il gioco in **Categoria** e premi *Cerca*: scegli la voce giusta per Twitch e per Kick
   (YouTube ha una lista fissa).
3. Aggiungi tag, lingua (Twitch) e descrizione (YouTube).
4. Spunta le piattaforme e premi **Aggiorna le piattaforme selezionate**: sotto vedi l'esito di ognuna.
5. **Preset**: *Salva…* memorizza tutto il modulo; scegli un preset dal menu per ricaricarlo.

## Cose da sapere

- **YouTube**: la diretta deve già esistere (attiva o programmata in YouTube Studio): il plugin modifica quella.
  Se la descrizione è vuota resta quella attuale.
- **Twitch**: titolo max 140 caratteri; tag max 10, senza spazi né simboli (li pulisce il plugin).
  **Kick**: titolo max 140 caratteri.
- Durante un login o un aggiornamento il pannello resta fermo qualche secondo: è normale.
- Se un aggiornamento fallisce, sotto il pulsante vedi il motivo.
- Il plugin salva i tuoi accessi **solo sul tuo PC** (`%APPDATA%\obs-studio\plugin_config\multistream-info-panel\settings.json`).
  Non condividere mai quel file. Vedi anche [PRIVACY.md](PRIVACY.md).

## Per chi mantiene il progetto

Guida alla build, ai GitHub Secrets, alle release e ai limiti di distribuzione: [MAINTAINER.md](MAINTAINER.md).

Basato su [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate). Licenza: GPL-2.0-or-later.
