# Privacy / Informativa sulla privacy

## Italiano
**Multistream Info Panel** è un plugin locale di OBS che comunica direttamente con le API ufficiali di Twitch e Kick. Legge il nome e i metadati del canale necessari e aggiorna titolo, categoria, tag e (per Twitch) lingua solo su comando dell'utente.

Non è previsto alcun server gestito dagli autori. Client ID, Client Secret, access token, refresh token e preset restano sul computer in `%APPDATA%\obs-studio\plugin_config\multistream-info-panel\settings.json`. **I segreti sono memorizzati in chiaro.** Non condividere questo file.

Le precedenti versioni supportavano YouTube. Questa versione non comunica con YouTube né mostra tale piattaforma, ma **non cancella automaticamente** la vecchia sezione `youtube` dalle impostazioni locali. Per eliminarla: fai backup del file, chiudi OBS, rimuovi manualmente la sezione `youtube` dal JSON, quindi riapri OBS. Revoca separatamente i permessi Google dal tuo account.

Per revocare Twitch o Kick, usa **Scollega** per cancellare i token locali e revoca l'app anche nelle impostazioni del servizio.

## English
This local OBS Studio plugin directly contacts Twitch and Kick official APIs to read and update channel metadata at the user's request. No project-operated server handles the data. Client IDs, secrets, tokens and presets are stored **unencrypted** in the local OBS plugin configuration file. Do not share it.

Legacy YouTube settings may remain in an existing local configuration file; this version does not use them or call YouTube. Back up the file before manually removing the `youtube` JSON section. Revoke old Google permissions separately. Disconnecting locally does not revoke access on the platform's side.
