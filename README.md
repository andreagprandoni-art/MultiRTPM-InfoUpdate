# Multistream Info Panel — Twitch + Kick

Plugin dock **Info Live** per OBS Studio 31+ su Windows 64-bit, dedicato a **Twitch** e **Kick**. Modifica soltanto le informazioni della live, non trasmette il video.

## Cosa fa
- Aggiorna titolo, categoria/gioco e tag di Twitch e Kick; lingua soltanto su Twitch.
- Ricerca categorie separata e risultati specifici per piattaforma.
- Preset per gioco: i vecchi preset mantengono titolo, tag, lingua e categorie Twitch/Kick.
- OAuth indipendente per ogni piattaforma, con token aggiornabili.
- Titoli da 1 a 140 caratteri e validazione degli ID categoria Kick.
- YouTube non viene più visualizzato né contattato.

## Installazione
1. Scarica installer o ZIP Windows da **Releases** quando sarà pubblicata una build.
2. Chiudi OBS e installa (oppure estrai lo ZIP mantenendo le directory).
3. Apri OBS → Visualizza → Pannelli → **Info Live · Kick / Twitch**.

## Collega gli account
Ogni utente crea la propria app sviluppatore, senza condividere credenziali.

| Servizio | Redirect URI | Dove creare l'app |
|---|---|---|
| Twitch | `http://localhost:8426/callback/twitch` | https://dev.twitch.tv/console (client Confidential) |
| Kick | `http://localhost:8426/callback/kick` | Kick → Impostazioni → Developer |

Twitch richiede `channel:manage:broadcast`; Kick usa `user:read channel:read channel:write`. Copia Client ID e Secret in **Impostazioni**, poi premi **Collega** e autorizza nel browser.

## Uso
1. Seleziona Twitch e/o Kick.
2. Scrivi il titolo (massimo 140 caratteri).
3. Cerca categoria/gioco e seleziona il risultato corretto su **ciascuna** piattaforma.
4. Facoltativo: tag separati da virgola. Campo vuoto significa **non modificare i tag esistenti**.
5. Facoltativo: lingua del canale Twitch.
6. Premi **Aggiorna le piattaforme selezionate**: riceverai risultati separati per piattaforma.

Il plugin non supporta l'avvio del flusso video né la chat unificata.

## Problemi frequenti
- **401**: riconnetti l'account dal pannello.
- **403**: verifica i permessi dell'app e autorizza nuovamente.
- **429**: richieste limitate dalla piattaforma; riprova più tardi.
- **Porta 8426 occupata**: chiudi altri programmi/istanze OBS che usano la porta.
- **Categorie errate**: cerca nuovamente e seleziona i risultati per Twitch e Kick.
- **Diagnostica**: premi **Copia info per supporto** (non include i token).

## Privacy e migrazione
Il file `%APPDATA%\obs-studio\plugin_config\multistream-info-panel\settings.json` contiene Client Secret e token in **chiaro**; non condividerlo. Questa versione ignora i dati YouTube dei file preesistenti, senza eliminarli automaticamente. Per cancellarli in sicurezza fai backup, chiudi OBS ed elimina la sezione `youtube` dal JSON. Leggi [PRIVACY.md](PRIVACY.md).

Compilazione tramite GitHub Actions o con CMake, Qt 6 e SDK OBS come nel repository originale. Licenza: GPL-2.0-or-later.
