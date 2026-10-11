# Multistream Info Panel

Plugin per **OBS Studio** (Windows) che aggiunge un pannello **"Info Live"**: da un solo posto aggiorni
**titolo, categoria/gioco, tag, lingua e descrizione** di **Twitch, Kick e YouTube** con un clic.
Per **TikTok** ci sono i pulsanti "Copia titolo" e "Copia hashtag" (TikTok non offre un'API per modificarli).

- Salvi dei **preset** (es. uno per ogni gioco) e li richiami al volo.
- Il plugin parla direttamente con le API ufficiali: nessun server intermedio, nessun programma esterno.
- I token di accesso restano sul tuo PC, nella cartella di configurazione del plugin in OBS.

> Requisiti: OBS Studio 31 o successivo (64 bit) su Windows.

---

## 1) Ottenere il plugin (build automatica su GitHub)

1. Carica questo progetto in un tuo repository GitHub (vedi "Caricare su GitHub" in fondo).
2. Appena carichi i file, su GitHub parte da sola la build: vai nella scheda **Actions** del repository,
   apri l'ultima esecuzione "Push" e attendi il segno verde (circa 5-10 minuti).
3. In fondo alla pagina dell'esecuzione, sezione **Artifacts**, scarica quello che finisce con
   `windows-x64-…`. Dentro trovi lo **zip** e/o l'**installer .exe**.

Per ottenere una versione "ufficiale" scaricabile dalla scheda **Releases**: nel repository clicca
**Releases → Draft a new release**, crea un tag nuovo (es. `1.0.0`), pubblica; la build parte da sola e
allega i file alla release (come bozza: apri la bozza e premi *Publish*).

Puoi anche rilanciare la build a mano: **Actions → Dispatch → Run workflow**.

## 2) Installare in OBS

- **Con l'installer .exe**: chiudi OBS, avvia l'installer, riapri OBS.
- **Con lo zip**: chiudi OBS, estrai lo zip dentro la cartella di OBS (di solito `C:\Program Files\obs-studio`),
  in modo che i file finiscano in `obs-plugins\64bit\` e `data\obs-plugins\`. Riapri OBS.

Poi in OBS apri il menu **Docks** e attiva **Info Live**. Il pannello si può spostare e agganciare dove vuoi.

## 3) Creare le app sviluppatore (una sola volta)

Per poter modificare il tuo canale servono delle credenziali che crei tu, gratis, su ogni piattaforma.
Nel pannello premi **Impostazioni…**: lì incolli **Client ID** e **Client Secret** e trovi il **Redirect URI**
da usare. I tre indirizzi sono:

| Piattaforma | Redirect URI |
|---|---|
| Twitch | `http://localhost:8426/callback/twitch` |
| Kick | `http://localhost:8426/callback/kick` |
| YouTube | `http://localhost:8426/callback/youtube` |

**Twitch** – dev.twitch.tv/console → *Register Your Application*. Categoria: Application Integration.
Tipo client: **Confidential**. Inserisci il Redirect URI della tabella, poi *Manage* per copiare il Client ID
e *New Secret* per il Secret.

**Kick** – nelle impostazioni del tuo account Kick, sezione **Developer**, crea una nuova app, inserisci il
Redirect URI e abilita gli scope: lettura utente, lettura canale, modifica canale. Copia Client ID e Secret.

**YouTube** – console.cloud.google.com:
1. Crea un progetto e abilita **YouTube Data API v3**.
2. *Schermata consenso OAuth*: tipo **Esterno**; aggiungi il tuo account Google come **utente di test**.
3. *Credenziali → Crea credenziali → ID client OAuth* → tipo applicazione **App desktop**.
4. Copia Client ID e Client Secret.

> **Importante per YouTube:** finché l'app Google resta in stato "Test", il collegamento scade dopo **7 giorni**
> e devi premere di nuovo *Collega*. Per evitarlo, nella schermata di consenso premi **Pubblica app**
> (per uso personale non serve la verifica: al login Google mostra un avviso "app non verificata",
> clicca *Avanzate → Continua*).

## 4) Collegare gli account

Nel pannello, accanto a ogni piattaforma premi **Collega**: si apre il browser, accetti i permessi e
torni a OBS. Il pallino diventa verde. Si fa una volta sola (YouTube: vedi nota sopra).

## 5) Uso quotidiano

1. Scrivi il **titolo**.
2. Scrivi il gioco in **Categoria** e premi *Cerca*: scegli la voce giusta per Twitch e per Kick
   (YouTube ha una lista fissa).
3. Aggiungi tag, lingua (Twitch) e descrizione (YouTube).
4. Spunta le piattaforme e premi **Aggiorna le piattaforme selezionate**: sotto vedi l'esito di ognuna.
5. **Preset**: *Salva…* memorizza tutto il modulo; scegli un preset dal menu per ricaricarlo.

## Limiti da sapere

- **TikTok**: nessuna API pubblica per cambiare titolo o categoria. Usa i pulsanti di copia e incolla in TikTok LIVE Studio.
- **YouTube**: la diretta deve già esistere (attiva o programmata in YouTube Studio). Se la descrizione è vuota
  resta quella attuale; la categoria è una lista fissa di YouTube.
- **Twitch**: titolo max 140 caratteri; tag max 10, senza spazi né simboli (li pulisce il plugin).
  **Kick**: titolo max 140 caratteri.
- Durante un login o un aggiornamento il pannello si "congela" per qualche secondo: è normale.
- Le impostazioni e i token sono salvati in chiaro nella cartella di configurazione del plugin
  (`%APPDATA%\obs-studio\plugin_config\multistream-info-panel\settings.json`). Non condividere mai quel file.

---

## Caricare su GitHub (da zero)

1. Su github.com crea un repository **vuoto** (es. `multistream-info-panel`), senza README.
2. Dalla cartella di questo progetto (serve [Git](https://git-scm.com/download/win)):

```
git init
git add .
git commit -m "Primo commit"
git branch -M main
git remote add origin https://github.com/IL_TUO_UTENTE/multistream-info-panel.git
git push -u origin main
```

Alternativa senza comandi: **GitHub Desktop** → *Add local repository* → *Publish repository*.

Dopo il push la build parte da sola (scheda **Actions**).

## Per sviluppatori

- Basato su [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate) (CMake, Qt6, obs-frontend-api).
- `src/plugin-main.cpp` registra il dock; `src/panel.*` è l'interfaccia Qt; `src/services.*` contiene
  OAuth (PKCE dove richiesto, redirect su `localhost:8426`) e le chiamate alle API.
- Build locale: `cmake --preset windows-x64` poi `cmake --build --preset windows-x64`.
- Licenza: GPL-2.0-or-later.
