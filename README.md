# Multistream Info Panel — plugin per OBS Studio

Un pannello **"Info Live"** dentro OBS per cambiare in un colpo solo **titolo, categoria/gioco, tag, lingua e descrizione**
della tua diretta su **Twitch**, **Kick** e **YouTube**.

- Windows 64 bit, OBS Studio 31 o più recente.
- Nessun nostro server: il plugin parla direttamente tra il tuo PC e le piattaforme.
- Ogni utente collega **il proprio account** usando **una propria "app sviluppatore"** (gratis, si crea una volta sola:
  guida qui sotto). Così nessuno condivide limiti o credenziali con altri.

## 1) Installazione

1. Scarica l'installer (`.exe`) o lo zip dalla pagina **Releases** del repository.
2. Chiudi OBS, esegui l'installer (o copia il contenuto dello zip nella cartella di OBS, mantenendo le sottocartelle).
3. Apri OBS → menu **Visualizza → Pannelli (Docks) → Info Live**.

## 2) Crea le tue app (una volta sola, solo per le piattaforme che usi)

In tutte e tre c'è un campo **Redirect URI** (indirizzo di reindirizzamento): usa **esattamente** questi valori,
carattere per carattere, senza barra finale e senza spazi.

| Piattaforma | Redirect URI |
|---|---|
| Twitch | `http://localhost:8426/callback/twitch` |
| Kick | `http://localhost:8426/callback/kick` |
| YouTube | `http://localhost:8426/callback/youtube` (se la tua app Google è di tipo "App desktop" non serve inserirlo) |

(Le stesse stringhe si copiano dal pulsante **Copia** nelle Impostazioni del plugin.)

### Twitch
1. Vai su <https://dev.twitch.tv/console> e accedi (Twitch può chiedere di attivare l'autenticazione a due fattori).
2. **Register Your Application** → un nome a piacere, **OAuth Redirect URLs** = il valore della tabella,
   **Category** = Application Integration, **Client Type** = **Confidential**.
3. Apri l'app con **Manage**: copia il **Client ID** e premi **New Secret** per ottenere il **Client Secret**.

### Kick
1. Su kick.com apri **Impostazioni → Developer** (può servire l'autenticazione a due fattori) e crea una nuova app.
2. Come Redirect URL usa il valore della tabella. Attiva i permessi `user:read`, `channel:read`, `channel:write`.
3. Copia **Client ID** e **Client Secret**.

### YouTube (Google)
1. Vai su <https://console.cloud.google.com> e crea un **nuovo progetto**.
2. **API e servizi → Libreria** → cerca **YouTube Data API v3** → **Abilita**.
3. **Schermata consenso OAuth** (o "Google Auth Platform"): tipo **Esterno**, compila nome app ed email.
   Se l'app resta in stato **Test**: aggiungi il tuo indirizzo tra gli **Utenti di test**, ma tieni presente che in questo
   stato il collegamento **scade dopo 7 giorni**. Per un uso duraturo premi **Pubblica app**
   (per uso personale non serve la verifica Google: vedrai solo l'avviso "app non verificata", scegli *Avanzate → Continua*).
4. **Credenziali → Crea credenziali → ID client OAuth** → tipo **App desktop**. Copia **Client ID** e **Client Secret**.
5. **Account brand:** se il tuo canale è un account "brand", al login Google mostra un elenco di account/canali (o il
   passaggio "Scegli un canale"): scegli **il canale del brand**, non l'account personale (che non ha un canale e dà
   `youtubeSignupRequired`). Dopo il collegamento il plugin scrive il **nome del canale** collegato: controlla che sia quello giusto.
   Il tuo account Google deve essere tra i gestori del brand account.

## 3) Primo collegamento

1. Nel pannello premi **Impostazioni (Client ID / Secret)…**, incolla i dati delle piattaforme che usi e **Salva**.
2. Premi **Collega** accanto alla piattaforma: si apre il browser, accetti, e torni su OBS. Compare "● collegato".
   Puoi annullare l'attesa con **Annulla**.
3. Spunta le piattaforme da aggiornare, scrivi titolo/categoria/tag e premi **Aggiorna le piattaforme selezionate**.

Note d'uso:
- **YouTube** aggiorna la diretta **attiva** o, se non c'è, la prossima **programmata**: creala prima in YouTube Studio.
- **Categoria**: cerca per nome (Twitch e Kick). Per YouTube c'è un elenco fisso di categorie.
- **Tag**: separati da virgola. Se il campo è vuoto i tag esistenti **non** vengono toccati.
- **Lingua**: "— non cambiare —" lascia invariata quella attuale (vale solo per Twitch).
- **Descrizione**: solo YouTube. Se vuota, non viene toccata.
- **Preset**: salva e richiama combinazioni di campi (es. uno per gioco).
- **Quota YouTube**: ogni aggiornamento costa circa 52 delle 10.000 unità giornaliere **del tuo progetto Google**
  (≈ 190 aggiornamenti al giorno, abbondanti per un uso normale).

## 4) Se qualcosa non funziona

Premi **Copia info per supporto** nel pannello e incolla il testo quando chiedi aiuto (non contiene token né segreti).

| Messaggio | Cosa fare |
|---|---|
| `youtubeSignupRequired` / "non ha un canale YouTube" | Google ti ha collegato con l'account personale e non con il canale. **Scollega**, poi **Collega** e, quando Google lo chiede, scegli il **canale del brand**. |
| `quotaExceeded` / "quota esaurita" | Hai usato le 10.000 unità del giorno: si azzerano a mezzanotte (fuso del Pacifico). |
| `accessNotConfigured` / "API non abilitata" | Nel progetto Google abilita **YouTube Data API v3** (passo 2 della guida YouTube). |
| `access_denied` su YouTube | App in stato Test: aggiungi il tuo account tra gli **Utenti di test**, oppure **Pubblica app**. |
| "Sessione scaduta" dopo pochi giorni (YouTube) | App Google ancora in stato Test (scade dopo 7 giorni): **Pubblica app**, poi Scollega e Collega. |
| `redirect_uri` non valido / mismatch | Il Redirect URI nell'app non è identico a quello della tabella. Copialo di nuovo. |
| `invalid_client` | Client ID o Client Secret sbagliati o con spazi. Ricontrollali nelle Impostazioni. |
| "La porta 8426 è occupata" | Chiudi altre copie di OBS o programmi che usano quella porta, poi riprova. |
| "Nessuna diretta YouTube attiva o programmata" | Crea o programma la diretta in YouTube Studio, poi riprova. |
| "nessuna risposta da …" | Rete, antivirus o proxy che bloccano OBS. Riprova; controlla che OBS possa accedere a Internet. |
| 5xx / "non disponibile" | Problema temporaneo della piattaforma: il plugin ha già riprovato 3 volte. Aspetta qualche minuto. |

## Dove sono salvati i dati

In `%APPDATA%\obs-studio\plugin_config\multistream-info-panel\settings.json`: i tuoi Client ID/Secret, i token di accesso e i
preset. **I token sono in chiaro: non condividere quel file.** Il salvataggio è "atomico" e, se il file si rovina,
ne resta una copia `settings.json.bak`. **Scollega** rimuove i token dal PC; per revocare del tutto il permesso:
Google → myaccount.google.com/permissions · Twitch → twitch.tv/settings/connections · Kick → impostazioni, app collegate.

Vedi anche [PRIVACY.md](PRIVACY.md). TikTok non è supportato (non esiste un'API pubblica per cambiare titolo e categoria).

## Compilare da sé (GitHub Actions)

1. Carica il progetto su un repository GitHub (la cartella `.github/` deve esserci).
2. Per una versione scaricabile: **Releases → Draft a new release**, crea un tag uguale alla `version` di `buildspec.json`
   (es. `2.0.0`) e pubblica. La build parte da sola e allega zip e installer (come bozza: aprila e premi *Publish*).
   In alternativa **Actions → dispatch → Run workflow** per una build manuale.
3. Non servono Secrets: le credenziali le inserisce ogni utente nel plugin.

Licenza: GPL-2.0-or-later (come il template ufficiale dei plugin OBS).
