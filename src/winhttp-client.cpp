// Licenza: GPL-2.0-or-later (come il template ufficiale dei plugin OBS)
#include "winhttp-client.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>

namespace {

std::wstring widen(const std::string &s)
{
	if (s.empty())
		return std::wstring();
	const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
	std::wstring w(static_cast<size_t>(n), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), &w[0], n);
	return w;
}

// Chiude da solo l'handle WinHTTP.
struct Handle {
	HINTERNET h;
	explicit Handle(HINTERNET handle) : h(handle) {}
	~Handle()
	{
		if (h)
			WinHttpCloseHandle(h);
	}
	Handle(const Handle &) = delete;
	Handle &operator=(const Handle &) = delete;
	bool ok() const { return h != nullptr; }
};

std::string failure(const char *step)
{
	return std::string("Errore di rete (") + step + ", codice " + std::to_string(GetLastError()) + ")";
}

} // namespace

WinHttpResult winhttpRequest(const std::string &verb, const std::string &host, int port,
			     const std::string &pathAndQuery,
			     const std::vector<std::pair<std::string, std::string>> &headers,
			     const std::string &body, int timeoutMs)
{
	WinHttpResult out;

	Handle session(WinHttpOpen(L"MultistreamInfoPanel/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
				   WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
	if (!session.ok()) {
		out.error = failure("apertura sessione");
		return out;
	}
	WinHttpSetTimeouts(session.h, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

	const std::wstring wideHost = widen(host);
	Handle connection(WinHttpConnect(session.h, wideHost.c_str(), static_cast<INTERNET_PORT>(port), 0));
	if (!connection.ok()) {
		out.error = failure("connessione");
		return out;
	}

	const std::wstring wideVerb = widen(verb);
	const std::wstring widePath = widen(pathAndQuery);
	Handle request(WinHttpOpenRequest(connection.h, wideVerb.c_str(), widePath.c_str(), nullptr,
					  WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
	if (!request.ok()) {
		out.error = failure("creazione richiesta");
		return out;
	}

	std::wstring headerBlock;
	for (const auto &h : headers)
		headerBlock += widen(h.first) + L": " + widen(h.second) + L"\r\n";
	if (!headerBlock.empty())
		WinHttpAddRequestHeaders(request.h, headerBlock.c_str(), static_cast<DWORD>(-1L),
					 WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

	LPVOID data = body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char *>(body.data());
	const DWORD length = static_cast<DWORD>(body.size());
	if (!WinHttpSendRequest(request.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0, data, length, length, 0) ||
	    !WinHttpReceiveResponse(request.h, nullptr)) {
		out.error = failure("invio richiesta");
		return out;
	}

	DWORD status = 0;
	DWORD statusSize = sizeof(status);
	WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			    WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
	out.status = static_cast<int>(status);

	for (;;) {
		DWORD available = 0;
		if (!WinHttpQueryDataAvailable(request.h, &available) || available == 0)
			break;
		std::string chunk(available, '\0');
		DWORD read = 0;
		if (!WinHttpReadData(request.h, &chunk[0], available, &read) || read == 0)
			break;
		out.body.append(chunk, 0, read);
	}
	return out;
}
