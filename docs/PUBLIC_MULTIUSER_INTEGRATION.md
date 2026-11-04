# Public multi-user integration requirements

The OBS plugin is distributed to unrelated users. Never ship a hardcoded streamer
username, access token, API key, or private OAuth secret.

## Per-installation accounts

- Twitch and Kick: each user supplies their own developer-app credentials and
  completes their own OAuth login, independently for each platform.
- TikTok / Euler Stream (planned): each user enters their own TikTok username
  and their own Euler Stream API key, where required. An Euler API key does
  **not** constitute a TikTok account login or grant write permissions.
- Each account can be configured, replaced, and disconnected independently.
- No personal username (including dashup_ita) is a shipped default.

## Capabilities and release gates

- Preserve existing Twitch/Kick title and category updates.
- Only enable TikTok read/status features after validating the documented
  Euler endpoints and Free-tier limits.
- Keep TikTok title/category write disabled unless a supported, authorized
  endpoint is verified. Never display a false successful update.
- Keep per-platform results and errors separate, including 401/403/429.
- Never log credentials, OAuth tokens or Euler API keys.
- Existing Twitch/Kick secrets are stored in plaintext JSON: migrate to
  OS credential storage before claiming secure storage.
- Test Windows CI, independent accounts, disconnect, malformed responses,
  offline mode, rate limiting, and preset backward compatibility.
