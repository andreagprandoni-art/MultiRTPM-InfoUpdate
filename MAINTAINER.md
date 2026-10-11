# Guida per chi mantiene e distribuisce il plugin

Gli utenti finali non creano nessuna app: **tu crei una sola app per piattaforma** (Twitch, Kick, Google),
ne salvi le credenziali nei **GitHub Secrets** e la build le inserisce dentro il plugin.
Ogni utente poi accede col proprio account e modifica solo il proprio canale.

## 1) Creare le app (una volta sola)

Il Redirect URI è fisso, perché il plugin ascolta sulla porta 8426 del PC dell'utente:

| Piattaforma | Redirect URI |
|---|---|
| Twitch | `http://localhost:8426/callback/twitch` |
| Kick | `http://localhost:8426/callback/kick` |
| YouTube | nessuno (vedi sotto) |

- **Twitch** – dev.twitch.tv/console → *Register Your Application*, tipo client **Confidential**. Permesso usato: `channel:manage:broadcast`.
- **Kick** – impostazioni account → Developer → nuova app. Permessi: `user:read`, `channel:read`, `channel:write`.
- **YouTube** – console.cloud.google.com: progetto → abilita **YouTube Data API v3** → schermata consenso OAuth
  (tipo **Esterno**) → credenziali → ID client OAuth tipo **App desktop** (non richiede Redirect URI).
  Permesso usato: `https://www.googleapis.com/auth/youtube`.

## 2) Salvare le credenziali nei GitHub Secrets

Nel repository: **Settings → Secrets and variables → Actions → New repository secret**. Crea questi sei
(nomi esatti, valori copiati senza spazi):

`TWITCH_CLIENT_ID` · `TWITCH_CLIENT_SECRET` · `KICK_CLIENT_ID` · `KICK_CLIENT_SECRET` ·
`YOUTUBE_CLIENT_ID` · `YOUTUBE_CLIENT_SECRET`

La build li passa a CMake (variabili `MSIP_*`) e finiscono nel plugin, **non nel codice del repository**.
Se un valore contiene caratteri strani la build si ferma con un messaggio chiaro.

> Senza Secrets la build funziona lo stesso, ma in "modalità sviluppatore": nel pannello compare
> **Impostazioni…** per inserire a mano le credenziali di una propria app. Utile per provare in locale.

## 3) Pubblicare una versione

1. Cambia `version` in `buildspec.json` (es. `1.1.0`).
2. Su GitHub: **Releases → Draft a new release**, crea un tag uguale alla versione (es. `1.1.0`), pubblica.
3. La build parte da sola e allega zip e installer alla release (come bozza: aprila e premi *Publish*).

## 4) Limiti da conoscere prima di distribuire

**YouTube – quota condivisa.** Google conta le chiamate **per progetto**, non per utente: tutti gli utenti del
plugin condividono lo stesso budget di default di **10.000 unità al giorno**. Costi: `liveBroadcasts.list` 1,
`videos.list` 1, `videos.update` 50 ([fonte](https://developers.google.com/youtube/v3/determine_quota_cost)).
Un aggiornamento costa circa 52 unità → circa **190 aggiornamenti al giorno in totale**. Oltre, YouTube risponde
"quota esaurita" a tutti fino al reset. Per aumentarla serve passare un controllo di conformità con il modulo
*YouTube API Services – Audit and Quota Extension Form*
([guida](https://developers.google.com/youtube/v3/guides/quota_and_compliance_audits)).

**YouTube – verifica dell'app Google.** Un'app che chiede permessi "sensibili" senza verifica mostra
l'avviso "app non verificata" ed è limitata a **100 nuovi utenti in totale**
([fonte](https://support.google.com/cloud/answer/7454865)). Per superarlo bisogna completare la verifica Google
(servono, tra l'altro, una pagina con l'informativa privacy: usa [PRIVACY.md](PRIVACY.md) come base e un sito tuo
dove pubblicarla; i requisiti esatti, come il dominio, controllali sulla pagina ufficiale della verifica).
Finché l'app Google è in stato "Test", inoltre, i collegamenti scadono dopo 7 giorni:
nella schermata di consenso usa **Pubblica app**.

**Credenziali dentro il plugin.** Chi scarica il plugin può estrarre Client ID e Secret dal file. È la pratica
comune per le app desktop (per Google le app desktop non hanno un vero segreto), ma per Twitch e Kick non è
l'ideale: leggi i loro termini per sviluppatori. Chi estrae il secret **non** ottiene l'accesso agli account degli
utenti (servono i loro token), ma potrebbe usare la tua app a tuo nome; se succede, rigenera il secret e pubblica
una nuova versione. Una soluzione più robusta è un piccolo servizio online che tiene i secret lato server.

**Altro.**
- La porta 8426 è fissa (le piattaforme confrontano il Redirect URI carattere per carattere): se è occupata il login fallisce con un messaggio.
- I token degli utenti sono salvati in chiaro in `settings.json` nella cartella di configurazione del plugin. È dichiarato in README e PRIVACY.
- Se cambi app (Client ID) gli utenti devono premere di nuovo **Collega**.
