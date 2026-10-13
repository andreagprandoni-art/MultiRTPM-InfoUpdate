// Client HTTPS minimale basato su WinHTTP (solo Windows).
// Serve perche' il Qt incluso in OBS non ha il componente TLS: con WinHTTP si usa il TLS di Windows.
// Licenza: GPL-2.0-or-later (come il template ufficiale dei plugin OBS)
#pragma once

#include <string>
#include <utility>
#include <vector>

struct WinHttpResult {
	int status = 0;     // codice HTTP, 0 se la richiesta non e' arrivata
	std::string body;   // corpo della risposta
	std::string error;  // descrizione dell'errore di rete (se status == 0)
};

// Richiesta HTTPS bloccante. `pathAndQuery` e' gia' codificato (es. "/helix/channels?broadcaster_id=1").
WinHttpResult winhttpRequest(const std::string &verb, const std::string &host, int port,
			     const std::string &pathAndQuery,
			     const std::vector<std::pair<std::string, std::string>> &headers,
			     const std::string &body, int timeoutMs);
