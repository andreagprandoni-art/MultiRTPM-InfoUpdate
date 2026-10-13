// Licenza: GPL-2.0-or-later (come il template ufficiale dei plugin OBS)
#include "services.hpp"

#include <plugin-support.h>
#include <util/base.h>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPair>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include <atomic>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32) || defined(MSIP_FORCE_WINHTTP)
#include "winhttp-client.hpp"
#endif

namespace svc {
namespace {

constexpr quint16 kPort = 8426;
QString g_configDir;
std::atomic<bool> g_cancelLogin(false);
bool g_loginActive = false;

struct Spec {
	const char *key;
	const char *name;
	const char *authUrl;
	const char *tokenUrl;
	const char *scope;
	bool pkce;
};

const Spec &spec(Platform p)
{
	static const Spec specs[kPlatformCount] = {
		{"twitch", "Twitch", "https://id.twitch.tv/oauth2/authorize", "https://id.twitch.tv/oauth2/token",
		 "channel:manage:broadcast", false},
		{"kick", "Kick", "https://id.kick.com/oauth/authorize", "https://id.kick.com/oauth/token",
		 "user:read channel:read channel:write", true},
		{"youtube", "YouTube", "https://accounts.google.com/o/oauth2/v2/auth",
		 "https://oauth2.googleapis.com/token", "https://www.googleapis.com/auth/youtube", true},
	};
	return specs[static_cast<int>(p)];
}

// ------------------------------------------------------------------ file impostazioni
QString settingsPath()
{
	return g_configDir + "/settings.json";
}

QJsonObject loadAll()
{
	QFile f(settingsPath());
	if (!f.open(QIODevice::ReadOnly))
		return QJsonObject();
	const QByteArray raw = f.readAll();
	f.close();
	QJsonParseError perr;
	const QJsonDocument doc = QJsonDocument::fromJson(raw, &perr);
	if (perr.error != QJsonParseError::NoError && !raw.trimmed().isEmpty()) {
		// File rovinato: ne teniamo una copia invece di sovrascriverlo al prossimo salvataggio.
		const QString bak = settingsPath() + ".bak";
		QFile::remove(bak);
		QFile::copy(settingsPath(), bak);
		obs_log(LOG_WARNING, "settings.json illeggibile, copia salvata in settings.json.bak");
		return QJsonObject();
	}
	return doc.object();
}

// Scrittura "atomica": o il file nuovo e' completo, o resta quello vecchio (niente file troncati).
bool saveAll(const QJsonObject &obj)
{
	if (!QDir().mkpath(g_configDir))
		return false;
	QSaveFile f(settingsPath());
	if (!f.open(QIODevice::WriteOnly))
		return false;
	f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
	return f.commit();
}

QJsonObject section(Platform p)
{
	return loadAll().value(QString::fromLatin1(spec(p).key)).toObject();
}

bool setSection(Platform p, const QJsonObject &s)
{
	QJsonObject all = loadAll();
	all[QString::fromLatin1(spec(p).key)] = s;
	return saveAll(all);
}

// ------------------------------------------------------------------------- HTTP
using Headers = QList<QPair<QByteArray, QByteArray>>;

struct HttpResponse {
	int status = 0;
	QByteArray body;
	QString error;
	QJsonObject json() const { return QJsonDocument::fromJson(body).object(); }
};

#if defined(_WIN32) || defined(MSIP_FORCE_WINHTTP)
// Su Windows usiamo WinHTTP (TLS di Windows): il Qt incluso in OBS non ha il backend TLS e
// QNetworkAccessManager darebbe "TLS initialization failed". La richiesta gira in un thread
// e intanto lasciamo lavorare l'interfaccia di OBS (event loop locale).
HttpResponse http(const QByteArray &verb, const QUrl &url, const Headers &headers = Headers(),
		  const QByteArray &body = QByteArray(), int timeoutMs = 15000)
{
	QString pathAndQuery = url.path(QUrl::FullyEncoded);
	if (pathAndQuery.isEmpty())
		pathAndQuery = "/";
	if (url.hasQuery())
		pathAndQuery += "?" + url.query(QUrl::FullyEncoded);

	const std::string verbStd(verb.constData(), static_cast<size_t>(verb.size()));
	const std::string hostStd = url.host().toUtf8().toStdString();
	const std::string pathStd = pathAndQuery.toUtf8().toStdString();
	const std::string bodyStd(body.constData(), static_cast<size_t>(body.size()));
	const int port = url.port(443);
	std::vector<std::pair<std::string, std::string>> headerList;
	for (const auto &h : headers)
		headerList.emplace_back(std::string(h.first.constData(), static_cast<size_t>(h.first.size())),
					std::string(h.second.constData(), static_cast<size_t>(h.second.size())));

	WinHttpResult res;
	std::atomic<bool> done(false);
	std::thread worker([&]() {
		res = winhttpRequest(verbStd, hostStd, port, pathStd, headerList, bodyStd, timeoutMs);
		done = true;
	});

	QEventLoop loop;
	QTimer poll;
	QObject::connect(&poll, &QTimer::timeout, &loop, [&]() {
		if (done)
			loop.quit();
	});
	poll.start(25);
	loop.exec();
	worker.join();

	HttpResponse out;
	out.status = res.status;
	out.body = QByteArray(res.body.data(), static_cast<int>(res.body.size()));
	out.error = QString::fromUtf8(res.error.c_str());
	return out;
}
#else
QNetworkAccessManager &manager()
{
	static QNetworkAccessManager *nam = nullptr;
	if (!nam) {
		nam = new QNetworkAccessManager();
		nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
	}
	return *nam;
}

// Richiesta HTTP "bloccante" che pero' lascia lavorare l'interfaccia (event loop locale).
HttpResponse http(const QByteArray &verb, const QUrl &url, const Headers &headers = Headers(),
		  const QByteArray &body = QByteArray(), int timeoutMs = 15000)
{
	QNetworkRequest req(url);
	for (const auto &h : headers)
		req.setRawHeader(h.first, h.second);

	QNetworkReply *reply = nullptr;
	if (verb == "GET")
		reply = manager().get(req);
	else if (verb == "POST")
		reply = manager().post(req, body);
	else
		reply = manager().sendCustomRequest(req, verb, body);

	QEventLoop loop;
	QTimer timer;
	timer.setSingleShot(true);
	QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
	timer.start(timeoutMs);
	loop.exec();

	HttpResponse out;
	if (!reply->isFinished()) {
		out.error = "Tempo scaduto (nessuna risposta dal server)";
		reply->abort();
	} else {
		out.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		out.body = reply->readAll();
		if (out.status == 0)
			out.error = reply->errorString();
	}
	reply->deleteLater();
	return out;
}
#endif

// Attesa che non blocca l'interfaccia di OBS.
void pauseMs(int ms)
{
	QEventLoop loop;
	QTimer::singleShot(ms, &loop, &QEventLoop::quit);
	loop.exec();
}

// Come http(), ma riprova (fino a 3 tentativi in tutto) se il server risponde con un errore
// temporaneo (rete assente, 429, 5xx). Da usare solo per richieste che si possono ripetere senza effetti collaterali.
HttpResponse httpRetry(const QByteArray &verb, const QUrl &url, const Headers &headers = Headers(),
		       const QByteArray &body = QByteArray())
{
	HttpResponse r = http(verb, url, headers, body);
	// Riprova anche se la rete cade del tutto (status 0) o se il server chiede di rallentare (429).
	auto temporary = [](const HttpResponse &x) { return x.status == 0 || x.status == 429 || (x.status >= 500 && x.status < 600); };
	for (int attempt = 1; attempt < 3 && temporary(r); ++attempt) {
		pauseMs(1500 * attempt);
		r = http(verb, url, headers, body);
	}
	return r;
}

QByteArray formBody(const QList<QPair<QString, QString>> &items)
{
	QByteArray out;
	for (const auto &kv : items) {
		if (!out.isEmpty())
			out += '&';
		out += QUrl::toPercentEncoding(kv.first) + '=' + QUrl::toPercentEncoding(kv.second);
	}
	return out;
}

QByteArray jsonBody(const QJsonObject &o)
{
	return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

// Testo breve dell'errore: se il server manda un JSON con "message"/"error_description" usiamo quello.
QString describeFailure(const HttpResponse &r)
{
	if (r.status == 0)
		return r.error.isEmpty() ? QString("Errore di rete") : r.error;
	const QJsonObject o = r.json();
	QString msg = o["error_description"].toString();
	if (msg.isEmpty())
		msg = o["message"].toString();
	if (msg.isEmpty() && o["error"].isObject())
		msg = o["error"].toObject()["message"].toString();
	if (msg.isEmpty() && o["error"].isString())
		msg = o["error"].toString();
	if (msg.isEmpty())
		msg = QString::fromUtf8(r.body.left(250));
	return QString("Errore %1: %2").arg(r.status).arg(msg);
}

// Messaggio per l'utente quando una chiamata alle API fallisce: dice il passaggio e, dove possibile, cosa fare.
QString failureText(Platform p, const QString &step, const HttpResponse &r)
{
	const QString name = QString::fromLatin1(spec(p).name);
	const QString prefix = step.isEmpty() ? QString() : step + ": ";
	const QByteArray &b = r.body;
	if (r.status == 0)
		return prefix + "nessuna risposta da " + name + " (" + describeFailure(r) +
		       "). Controlla la connessione, antivirus o proxy, poi riprova.";
	if (p == Platform::YouTube) {
		if (b.contains("youtubeSignupRequired"))
			return prefix + "l'account Google scelto non ha un canale YouTube. Premi Scollega, poi Collega e "
					"scegli l'account (o il canale) che usi per le dirette.";
		if (b.contains("quotaExceeded") || b.contains("rateLimitExceeded"))
			return prefix + "quota giornaliera di YouTube esaurita per la tua app Google. Si azzera ogni "
					"giorno (mezzanotte, fuso orario del Pacifico); riprova piu' tardi.";
		if (b.contains("accessNotConfigured") || b.contains("SERVICE_DISABLED"))
			return prefix + "la YouTube Data API v3 non e' abilitata nel tuo progetto Google Cloud. "
					"Abilitala da \"API e servizi > Libreria\" e riprova.";
		if (b.contains("liveStreamingNotEnabled"))
			return prefix + "il canale non ha le dirette abilitate: attivale in YouTube Studio "
					"(puo' servire la verifica del telefono e fino a 24 ore).";
		if (b.contains("invalidTitle"))
			return prefix + "titolo non valido per YouTube (massimo 100 caratteri, senza < e >).";
		if (b.contains("invalidDescription"))
			return prefix + "descrizione non valida per YouTube (massimo 5000 byte, senza < e >).";
		if (b.contains("invalidTags"))
			return prefix + "tag non validi per YouTube (massimo 500 caratteri in tutto).";
	}
	if (r.status == 401)
		return prefix + name + " ha rifiutato l'accesso. Premi Scollega, poi Collega per rifare il login.";
	if (r.status == 403)
		return prefix + "permesso negato da " + name + " (" + describeFailure(r) +
		       "). Premi Scollega e Collega per rinnovare i permessi.";
	if (r.status == 429)
		return prefix + name + " chiede di rallentare (troppe richieste). Riprova tra un minuto.";
	if (r.status >= 500 && r.status < 600)
		return prefix + name + " non e' disponibile al momento (errore " + QString::number(r.status) +
		       ", anche dopo piu' tentativi). Riprova tra qualche minuto.";
	return prefix + describeFailure(r);
}

void logFailure(const char *what, const HttpResponse &r)
{
	// Mai i token: nelle risposte d'errore ci sono solo codici e messaggi.
	obs_log(LOG_WARNING, "%s: stato %d %s %s", what, r.status, r.error.toUtf8().constData(),
		QString::fromUtf8(r.body.left(200)).simplified().toUtf8().constData());
}

QString idString(const QJsonValue &v)
{
	if (v.isString())
		return v.toString();
	return QString::number(static_cast<qint64>(v.toDouble()));
}

QString randomToken(int bytes)
{
	QByteArray raw;
	for (int i = 0; i < bytes; ++i)
		raw.append(static_cast<char>(QRandomGenerator::system()->bounded(256)));
	return QString::fromLatin1(raw.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

// ------------------------------------------------------------------------ token
HttpResponse tokenRequest(Platform p, QList<QPair<QString, QString>> items, bool retry)
{
	items.emplaceBack("client_id", clientId(p));
	const QString secret = clientSecret(p);
	if (!secret.isEmpty())
		items.emplaceBack("client_secret", secret);
	const QUrl url(QString::fromLatin1(spec(p).tokenUrl));
	const Headers h = {{"Content-Type", "application/x-www-form-urlencoded"}};
	const QByteArray body = formBody(items);
	return retry ? httpRetry("POST", url, h, body) : http("POST", url, h, body);
}

enum class Stored { Ok, NoToken, SaveFailed };

// Salva la risposta del server dei token.
Stored storeTokens(Platform p, const QJsonObject &resp)
{
	const QString access = resp["access_token"].toString();
	if (access.isEmpty())
		return Stored::NoToken;
	QJsonObject s = section(p);
	s["access_token"] = access;
	const QString refresh = resp["refresh_token"].toString();
	if (!refresh.isEmpty())
		s["refresh_token"] = refresh;
	const qint64 life = resp.contains("expires_in") ? static_cast<qint64>(resp["expires_in"].toDouble()) : 3600;
	s["expires_at"] = static_cast<double>(QDateTime::currentSecsSinceEpoch() + life);
	return setSection(p, s) ? Stored::Ok : Stored::SaveFailed;
}

// Ritorna un access token valido (lo rinnova se scaduto o se force = true). In caso di problema
// ritorna una stringa vuota e scrive il motivo in *error.
QString accessToken(Platform p, QString *error, bool force = false)
{
	const QJsonObject s = section(p);
	const QString token = s["access_token"].toString();
	if (token.isEmpty()) {
		*error = "Non collegato: premi Collega";
		return QString();
	}
	const qint64 expiresAt = static_cast<qint64>(s["expires_at"].toDouble());
	if (!force && expiresAt - 60 > QDateTime::currentSecsSinceEpoch())
		return token;

	const QString refresh = s["refresh_token"].toString();
	if (refresh.isEmpty()) {
		*error = "Sessione scaduta: premi Scollega e poi Collega";
		return QString();
	}
	const HttpResponse r = tokenRequest(p, {{"grant_type", "refresh_token"}, {"refresh_token", refresh}}, true);
	if (r.status == 200) {
		const Stored st = storeTokens(p, r.json());
		if (st == Stored::Ok)
			return section(p)["access_token"].toString();
		*error = st == Stored::SaveFailed ? "Impossibile salvare le impostazioni su disco"
						  : "Risposta inattesa dal server durante il rinnovo dell'accesso";
		return QString();
	}
	logFailure("rinnovo token", r);
	if (r.status == 400 || r.status == 401) {
		// Il refresh token non vale piu' (revocato, scaduto, password cambiata...).
		*error = "Sessione scaduta: premi Scollega e poi Collega.";
		if (p == Platform::YouTube)
			*error += " (Se la tua app Google e' in modalita' \"Test\" il collegamento dura solo 7 giorni: "
				  "pubblica l'app nella schermata di consenso.)";
		return QString();
	}
	*error = "Impossibile rinnovare l'accesso a " + QString::fromLatin1(spec(p).name) + " ora (" +
		 describeFailure(r) + "). Riprova tra poco.";
	return QString();
}

Headers bearer(const QString &token)
{
	return {{"Authorization", "Bearer " + token.toUtf8()}};
}

Headers authHeaders(Platform p, const QString &token)
{
	Headers h = bearer(token);
	if (p == Platform::Twitch)
		h.emplaceBack("Client-Id", clientId(p).toUtf8());
	return h;
}

// Chiamata alle API con il token giusto. Riprova sugli errori temporanei e, se il server dice 401,
// rinnova il token una volta e rifa' la chiamata. Se il problema e' l'accesso, *error resta valorizzato.
HttpResponse apiCall(Platform p, const QByteArray &verb, const QUrl &url, const QByteArray &body, QString *error)
{
	error->clear();
	const QString token = accessToken(p, error);
	if (token.isEmpty())
		return HttpResponse();
	auto send = [&](const QString &t) {
		Headers h = authHeaders(p, t);
		if (!body.isEmpty())
			h.emplaceBack("Content-Type", "application/json");
		return httpRetry(verb, url, h, body);
	};
	HttpResponse r = send(token);
	if (r.status == 401 && !r.body.contains("youtubeSignupRequired")) {
		const QString fresh = accessToken(p, error, true);
		if (fresh.isEmpty())
			return HttpResponse();
		r = send(fresh);
	}
	if (r.status != 200 && r.status != 204)
		logFailure((verb + " " + url.host().toUtf8()).constData(), r);
	return r;
}

QString twitchUserId(QString *error)
{
	QJsonObject s = section(Platform::Twitch);
	QString id = s["user_id"].toString();
	if (!id.isEmpty())
		return id;
	QString authErr;
	const HttpResponse r = apiCall(Platform::Twitch, "GET", QUrl("https://api.twitch.tv/helix/users"), QByteArray(), &authErr);
	if (!authErr.isEmpty()) {
		*error = authErr;
		return QString();
	}
	const QJsonArray data = r.json()["data"].toArray();
	if (r.status != 200 || data.isEmpty()) {
		*error = failureText(Platform::Twitch, "Lettura del profilo", r);
		return QString();
	}
	id = data.first().toObject()["id"].toString();
	s["user_id"] = id;
	setSection(Platform::Twitch, s);
	return id;
}

void replyHtml(QTcpSocket *sock, int status, const QString &html)
{
	const QByteArray body = html.toUtf8();
	QByteArray head = "HTTP/1.1 " + QByteArray::number(status) + (status == 200 ? " OK" : " Not Found") +
			  "\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\nContent-Length: " +
			  QByteArray::number(body.size()) + "\r\n\r\n";
	sock->write(head + body);
	sock->flush();
	QObject::connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
	sock->disconnectFromHost();
}

QStringList cleanTwitchTags(const QStringList &tags)
{
	QStringList out;
	static const QRegularExpression bad("[^\\p{L}\\p{N}]");
	for (QString t : tags) {
		t.remove(bad);
		t = t.left(25);
		if (!t.isEmpty() && !out.contains(t))
			out << t;
	}
	return out.mid(0, 10);
}

QJsonArray toJsonArray(const QStringList &list)
{
	QJsonArray a;
	for (const QString &s : list)
		a.append(s);
	return a;
}

// -------------------------------------------------------------------- aggiornamenti
Result updateTwitch(const StreamInfo &i)
{
	QString err;
	const QString uid = twitchUserId(&err);
	if (uid.isEmpty())
		return {false, err};

	QJsonObject body;
	if (!i.title.isEmpty())
		body["title"] = i.title.left(140);
	if (!i.twitchCategoryId.isEmpty())
		body["game_id"] = i.twitchCategoryId;
	if (!i.language.isEmpty())
		body["broadcaster_language"] = i.language;
	const QStringList tags = cleanTwitchTags(i.tags);
	if (!tags.isEmpty())
		body["tags"] = toJsonArray(tags);

	QUrl url("https://api.twitch.tv/helix/channels");
	QUrlQuery q;
	q.addQueryItem("broadcaster_id", uid);
	url.setQuery(q);
	const HttpResponse r = apiCall(Platform::Twitch, "PATCH", url, jsonBody(body), &err);
	if (!err.isEmpty())
		return {false, err};
	if (r.status == 204 || r.status == 200)
		return {true, "Aggiornato"};
	return {false, failureText(Platform::Twitch, QString(), r)};
}

Result updateKick(const StreamInfo &i)
{
	QJsonObject body;
	if (!i.title.isEmpty())
		body["stream_title"] = i.title.left(140);
	if (!i.kickCategoryId.isEmpty())
		body["category_id"] = i.kickCategoryId.toInt();
	if (!i.tags.isEmpty())
		body["custom_tags"] = toJsonArray(i.tags.mid(0, 10));

	QString err;
	const HttpResponse r = apiCall(Platform::Kick, "PATCH", QUrl("https://api.kick.com/public/v1/channels"),
				       jsonBody(body), &err);
	if (!err.isEmpty())
		return {false, err};
	if (r.status == 204 || r.status == 200)
		return {true, "Aggiornato"};
	return {false, failureText(Platform::Kick, QString(), r)};
}

Result updateYouTube(const StreamInfo &i)
{
	QString err;

	// Cerca la diretta attiva, altrimenti quella programmata.
	QString videoId;
	for (const char *status : {"active", "upcoming"}) {
		QUrl url("https://www.googleapis.com/youtube/v3/liveBroadcasts");
		QUrlQuery q;
		q.addQueryItem("part", "id");
		q.addQueryItem("broadcastStatus", status);
		url.setQuery(q);
		const HttpResponse r = apiCall(Platform::YouTube, "GET", url, QByteArray(), &err);
		if (!err.isEmpty())
			return {false, err};
		if (r.status != 200)
			return {false, failureText(Platform::YouTube, "Ricerca della diretta", r)};
		const QJsonArray items = r.json()["items"].toArray();
		if (!items.isEmpty()) {
			videoId = items.first().toObject()["id"].toString();
			break;
		}
	}
	if (videoId.isEmpty())
		return {false, "Nessuna diretta YouTube attiva o programmata (creala in YouTube Studio)"};

	// Legge lo stato attuale del video per non perdere i campi che non vogliamo cambiare.
	QUrl vurl("https://www.googleapis.com/youtube/v3/videos");
	QUrlQuery vq;
	vq.addQueryItem("part", "snippet");
	vq.addQueryItem("id", videoId);
	vurl.setQuery(vq);
	const HttpResponse cur = apiCall(Platform::YouTube, "GET", vurl, QByteArray(), &err);
	if (!err.isEmpty())
		return {false, err};
	const QJsonArray vitems = cur.json()["items"].toArray();
	if (cur.status != 200 || vitems.isEmpty())
		return {false, failureText(Platform::YouTube, "Lettura della diretta", cur)};
	const QJsonObject old = vitems.first().toObject()["snippet"].toObject();

	QJsonObject snippet;
	snippet["title"] = (i.title.isEmpty() ? old["title"].toString() : i.title).left(100);
	snippet["description"] = i.description.isEmpty() ? old["description"].toString() : i.description;
	snippet["categoryId"] = i.youtubeCategoryId.isEmpty() ? (old["categoryId"].toString().isEmpty()
									  ? QString("20")
									  : old["categoryId"].toString())
							       : i.youtubeCategoryId;
	snippet["tags"] = i.tags.isEmpty() ? old["tags"].toArray() : toJsonArray(i.tags);
	if (old.contains("defaultLanguage"))
		snippet["defaultLanguage"] = old["defaultLanguage"];

	QJsonObject body;
	body["id"] = videoId;
	body["snippet"] = snippet;

	QUrl purl("https://www.googleapis.com/youtube/v3/videos");
	QUrlQuery pq;
	pq.addQueryItem("part", "snippet");
	purl.setQuery(pq);
	const HttpResponse r = apiCall(Platform::YouTube, "PUT", purl, jsonBody(body), &err);
	if (!err.isEmpty())
		return {false, err};
	if (r.status == 200)
		return {true, "Aggiornato"};
	return {false, failureText(Platform::YouTube, "Aggiornamento della diretta", r)};
}

} // namespace

// ============================================================================ API pubblica
void setConfigDir(const QString &dir)
{
	g_configDir = dir;
	while (g_configDir.endsWith('/') || g_configDir.endsWith('\\'))
		g_configDir.chop(1);
	if (g_configDir.isEmpty()) {
		// OBS non ha dato una cartella: usiamo quella dei dati dell'applicazione.
		g_configDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
			      "/multistream-info-panel";
		obs_log(LOG_WARNING, "cartella di configurazione non fornita da OBS, uso %s",
			g_configDir.toUtf8().constData());
	}
}

QString configDir()
{
	return g_configDir;
}

QString platformName(Platform p)
{
	return QString::fromLatin1(spec(p).name);
}

QString redirectUri(Platform p)
{
	return QString("http://localhost:%1/callback/%2").arg(kPort).arg(QString::fromLatin1(spec(p).key));
}

QString clientId(Platform p)
{
	return section(p)["client_id"].toString();
}

QString clientSecret(Platform p)
{
	return section(p)["client_secret"].toString();
}

bool setCredentials(Platform p, const QString &id, const QString &secret)
{
	QJsonObject s = section(p);
	if (s["client_id"].toString() != id.trimmed()) {
		// Un'altra app sviluppatore: i vecchi token non valgono piu'.
		s.remove("access_token");
		s.remove("refresh_token");
		s.remove("expires_at");
		s.remove("user_id");
	}
	s["client_id"] = id.trimmed();
	s["client_secret"] = secret.trimmed();
	return setSection(p, s);
}

bool isConfigured(Platform p)
{
	return !clientId(p).isEmpty();
}

bool isConnected(Platform p)
{
	const QJsonObject s = section(p);
	return !s["access_token"].toString().isEmpty() &&
	       (!s["refresh_token"].toString().isEmpty() ||
		static_cast<qint64>(s["expires_at"].toDouble()) > QDateTime::currentSecsSinceEpoch());
}

void disconnectAccount(Platform p)
{
	QJsonObject s = section(p);
	s.remove("access_token");
	s.remove("refresh_token");
	s.remove("expires_at");
	s.remove("user_id");
	setSection(p, s);
}

void cancelLogin()
{
	g_cancelLogin = true;
}

Result login(Platform p)
{
	const Spec &sp = spec(p);
	if (!isConfigured(p))
		return {false, "Inserisci prima Client ID e Secret in Impostazioni"};
	if (g_loginActive)
		return {false, "Un collegamento e' gia' in corso"};

	struct Guard {
		Guard() { g_loginActive = true; g_cancelLogin = false; }
		~Guard() { g_loginActive = false; }
	} guard;

	QTcpServer server;
	if (!server.listen(QHostAddress::LocalHost, kPort))
		return {false, QString("La porta %1 e' occupata: chiudi l'altra applicazione (o un altro OBS) e riprova")
				       .arg(kPort)};

	const QString state = randomToken(16);
	QString verifier;

	QUrlQuery q;
	q.addQueryItem("client_id", clientId(p));
	q.addQueryItem("redirect_uri", redirectUri(p));
	q.addQueryItem("response_type", "code");
	q.addQueryItem("scope", QString::fromLatin1(sp.scope));
	q.addQueryItem("state", state);
	if (p == Platform::YouTube) {
		q.addQueryItem("access_type", "offline");
		q.addQueryItem("prompt", "select_account consent");
	}
	if (sp.pkce) {
		verifier = randomToken(48);
		const QByteArray challenge = QCryptographicHash::hash(verifier.toLatin1(), QCryptographicHash::Sha256)
						     .toBase64(QByteArray::Base64UrlEncoding |
							       QByteArray::OmitTrailingEquals);
		q.addQueryItem("code_challenge", QString::fromLatin1(challenge));
		q.addQueryItem("code_challenge_method", "S256");
	}
	QUrl authUrl(QString::fromLatin1(sp.authUrl));
	authUrl.setQuery(q);
	if (!QDesktopServices::openUrl(authUrl))
		return {false, "Impossibile aprire il browser"};

	QString code;
	QString failure;
	QElapsedTimer clock;
	clock.start();
	while (clock.elapsed() < 180000 && code.isEmpty() && failure.isEmpty()) {
		if (g_cancelLogin)
			return {false, "Collegamento annullato"};
		if (!server.hasPendingConnections()) {
			QEventLoop loop;
			QTimer tick;
			tick.setSingleShot(true);
			QObject::connect(&server, &QTcpServer::newConnection, &loop, &QEventLoop::quit);
			QObject::connect(&tick, &QTimer::timeout, &loop, &QEventLoop::quit);
			tick.start(300);
			loop.exec();
			continue;
		}
		QTcpSocket *sock = server.nextPendingConnection();
		if (sock->bytesAvailable() == 0) {
			QEventLoop loop;
			QTimer tick;
			tick.setSingleShot(true);
			QObject::connect(sock, &QTcpSocket::readyRead, &loop, &QEventLoop::quit);
			QObject::connect(&tick, &QTimer::timeout, &loop, &QEventLoop::quit);
			tick.start(3000);
			loop.exec();
		}
		const QByteArray request = sock->readAll();
		const QList<QByteArray> parts = request.left(request.indexOf("\r\n")).split(' ');
		const QUrl target = parts.size() >= 2 ? QUrl("http://localhost" + QString::fromUtf8(parts[1]))
						      : QUrl();
		if (target.path() != "/callback/" + QString::fromLatin1(sp.key)) {
			replyHtml(sock, 404, "<p>Non trovato</p>");
			continue;
		}
		const QUrlQuery cb(target);
		if (cb.hasQueryItem("error")) {
			const QString e = cb.queryItemValue("error");
			failure = "Accesso negato o annullato (" + e + ")";
			if (p == Platform::YouTube && e == "access_denied")
				failure += ". Se la tua app Google e' in modalita' \"Test\", aggiungi il tuo account tra gli "
					   "\"Utenti di test\" nella schermata di consenso OAuth.";
		} else if (cb.queryItemValue("state") != state) {
			failure = "Risposta non valida (state diverso): riprova a premere Collega";
		} else {
			code = cb.queryItemValue("code");
			if (code.isEmpty())
				failure = "La piattaforma non ha restituito il codice di autorizzazione";
		}
		replyHtml(sock, 200,
			  "<html><head><meta charset='utf-8'></head><body style='font-family:sans-serif;"
			  "text-align:center;margin-top:15vh'><h2>" +
				  QString(failure.isEmpty() ? "Collegato! Puoi chiudere questa scheda e tornare a OBS."
							    : "Qualcosa non ha funzionato. Torna a OBS e riprova.") +
				  "</h2></body></html>");
	}
	if (code.isEmpty())
		return {false, failure.isEmpty() ? QString("Tempo scaduto: login non completato") : failure};

	QList<QPair<QString, QString>> items = {
		{"grant_type", "authorization_code"}, {"code", code}, {"redirect_uri", redirectUri(p)}};
	if (sp.pkce)
		items.emplaceBack("code_verifier", verifier);
	const HttpResponse r = tokenRequest(p, items, false);
	if (r.status != 200) {
		logFailure("scambio codice", r);
		QString msg = "Scambio del codice fallito: " + describeFailure(r);
		if (r.body.contains("invalid_client"))
			msg += ". Controlla Client ID e Client Secret in Impostazioni (nessuno spazio, valori giusti).";
		else if (r.body.contains("redirect_uri"))
			msg += ". L'indirizzo di reindirizzamento della tua app deve essere esattamente " + redirectUri(p);
		return {false, msg};
	}
	const Stored st = storeTokens(p, r.json());
	if (st == Stored::NoToken)
		return {false, "Il server non ha restituito il token di accesso"};
	if (st == Stored::SaveFailed || !isConnected(p))
		return {false, "Collegato, ma non riesco a salvare le impostazioni in " + g_configDir};

	QJsonObject s = section(p);
	s.remove("user_id");
	setSection(p, s);
	if (p == Platform::Twitch) {
		QString err;
		twitchUserId(&err);
	}
	return {true, "Collegato"};
}

QList<Category> searchCategories(Platform p, const QString &query, QString *error)
{
	QList<Category> out;
	QString err;
	HttpResponse r;
	QUrlQuery q;
	if (p == Platform::Twitch) {
		QUrl url("https://api.twitch.tv/helix/search/categories");
		q.addQueryItem("query", query);
		q.addQueryItem("first", "15");
		url.setQuery(q);
		r = apiCall(p, "GET", url, QByteArray(), &err);
	} else if (p == Platform::Kick) {
		QUrl url("https://api.kick.com/public/v1/categories");
		q.addQueryItem("q", query);
		url.setQuery(q);
		r = apiCall(p, "GET", url, QByteArray(), &err);
	} else {
		return out;
	}
	if (!err.isEmpty()) {
		if (error)
			*error = err;
		return out;
	}
	if (r.status != 200) {
		if (error)
			*error = failureText(p, "Ricerca categorie", r);
		return out;
	}
	const QJsonArray data = r.json()["data"].toArray();
	for (const QJsonValue &v : data) {
		const QJsonObject o = v.toObject();
		out.append(Category{idString(o["id"]), o["name"].toString()});
		if (out.size() >= 15)
			break;
	}
	return out;
}

Result updateChannel(Platform p, const StreamInfo &info)
{
	switch (p) {
	case Platform::Twitch:
		return updateTwitch(info);
	case Platform::Kick:
		return updateKick(info);
	case Platform::YouTube:
		return updateYouTube(info);
	}
	return {false, "Piattaforma sconosciuta"};
}

QString currentSummary(Platform p)
{
	QString err;
	if (p == Platform::Twitch) {
		const QString uid = twitchUserId(&err);
		if (uid.isEmpty())
			return QString();
		QUrl url("https://api.twitch.tv/helix/channels");
		QUrlQuery q;
		q.addQueryItem("broadcaster_id", uid);
		url.setQuery(q);
		const HttpResponse r = apiCall(p, "GET", url, QByteArray(), &err);
		const QJsonArray data = r.json()["data"].toArray();
		if (!err.isEmpty() || r.status != 200 || data.isEmpty())
			return QString();
		const QJsonObject d = data.first().toObject();
		return QString("«%1» · %2").arg(d["title"].toString(), d["game_name"].toString());
	}
	if (p == Platform::Kick) {
		const HttpResponse r = apiCall(p, "GET", QUrl("https://api.kick.com/public/v1/channels"), QByteArray(), &err);
		const QJsonArray data = r.json()["data"].toArray();
		if (!err.isEmpty() || r.status != 200 || data.isEmpty())
			return QString();
		const QJsonObject d = data.first().toObject();
		return QString("«%1» · %2")
			.arg(d["stream_title"].toString(), d["category"].toObject()["name"].toString());
	}
	return QString();
}

QJsonObject presets()
{
	return loadAll().value("presets").toObject();
}

void savePreset(const QString &name, const QJsonObject &data)
{
	QJsonObject all = loadAll();
	QJsonObject pr = all.value("presets").toObject();
	pr[name] = data;
	all["presets"] = pr;
	saveAll(all);
}

void deletePreset(const QString &name)
{
	QJsonObject all = loadAll();
	QJsonObject pr = all.value("presets").toObject();
	pr.remove(name);
	all["presets"] = pr;
	saveAll(all);
}

} // namespace svc
