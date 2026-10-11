// Licenza: GPL-2.0-or-later (come il template ufficiale dei plugin OBS)
#include "services.hpp"

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
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace svc {
namespace {

constexpr quint16 kPort = 8426;
QString g_configDir;

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
	return QJsonDocument::fromJson(f.readAll()).object();
}

void saveAll(const QJsonObject &obj)
{
	QDir().mkpath(g_configDir);
	QFile f(settingsPath());
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return;
	f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

QJsonObject section(Platform p)
{
	return loadAll().value(QString::fromLatin1(spec(p).key)).toObject();
}

void setSection(Platform p, const QJsonObject &s)
{
	QJsonObject all = loadAll();
	all[QString::fromLatin1(spec(p).key)] = s;
	saveAll(all);
}

// ------------------------------------------------------------------------- HTTP
using Headers = QList<QPair<QByteArray, QByteArray>>;

struct HttpResponse {
	int status = 0;
	QByteArray body;
	QString error;
	QJsonObject json() const { return QJsonDocument::fromJson(body).object(); }
};

QNetworkAccessManager &manager()
{
	static QNetworkAccessManager *nam = nullptr;
	if (!nam) {
		nam = new QNetworkAccessManager();
		nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
	}
	return *nam;
}

// Richiesta HTTP "bloccante" che pero' lascia lavorare l'interfaccia di OBS (event loop locale).
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

QString describeFailure(const HttpResponse &r)
{
	if (r.status == 0)
		return r.error.isEmpty() ? QString("Errore di rete") : r.error;
	return QString("Errore %1: %2").arg(r.status).arg(QString::fromUtf8(r.body.left(250)));
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
HttpResponse tokenRequest(Platform p, QList<QPair<QString, QString>> items)
{
	const QJsonObject s = section(p);
	items.emplaceBack("client_id", s["client_id"].toString());
	const QString secret = s["client_secret"].toString();
	if (!secret.isEmpty())
		items.emplaceBack("client_secret", secret);
	return http("POST", QUrl(QString::fromLatin1(spec(p).tokenUrl)),
		    {{"Content-Type", "application/x-www-form-urlencoded"}}, formBody(items));
}

// Salva la risposta del server dei token. Ritorna false se manca l'access token.
bool storeTokens(Platform p, const QJsonObject &resp)
{
	const QString access = resp["access_token"].toString();
	if (access.isEmpty())
		return false;
	QJsonObject s = section(p);
	s["access_token"] = access;
	const QString refresh = resp["refresh_token"].toString();
	if (!refresh.isEmpty())
		s["refresh_token"] = refresh;
	const qint64 life = resp.contains("expires_in") ? static_cast<qint64>(resp["expires_in"].toDouble()) : 3600;
	s["expires_at"] = static_cast<double>(QDateTime::currentSecsSinceEpoch() + life);
	setSection(p, s);
	return true;
}

QString accessToken(Platform p, QString *error)
{
	const QJsonObject s = section(p);
	const QString token = s["access_token"].toString();
	if (token.isEmpty()) {
		*error = "Non collegato";
		return QString();
	}
	const qint64 expiresAt = static_cast<qint64>(s["expires_at"].toDouble());
	if (expiresAt - 60 > QDateTime::currentSecsSinceEpoch())
		return token;

	const QString refresh = s["refresh_token"].toString();
	if (refresh.isEmpty()) {
		*error = "Sessione scaduta: premi Collega";
		return QString();
	}
	const HttpResponse r = tokenRequest(p, {{"grant_type", "refresh_token"}, {"refresh_token", refresh}});
	if (r.status != 200 || !storeTokens(p, r.json())) {
		*error = "Sessione scaduta: premi Collega";
		return QString();
	}
	return section(p)["access_token"].toString();
}

Headers bearer(const QString &token)
{
	return {{"Authorization", "Bearer " + token.toUtf8()}};
}

Headers twitchHeaders(const QString &token)
{
	return {{"Authorization", "Bearer " + token.toUtf8()}, {"Client-Id", clientId(Platform::Twitch).toUtf8()}};
}

QString twitchUserId(const QString &token, QString *error)
{
	QJsonObject s = section(Platform::Twitch);
	QString id = s["user_id"].toString();
	if (!id.isEmpty())
		return id;
	const HttpResponse r = http("GET", QUrl("https://api.twitch.tv/helix/users"), twitchHeaders(token));
	const QJsonArray data = r.json()["data"].toArray();
	if (r.status != 200 || data.isEmpty()) {
		*error = describeFailure(r);
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
	const QString token = accessToken(Platform::Twitch, &err);
	if (token.isEmpty())
		return {false, err};
	const QString uid = twitchUserId(token, &err);
	if (uid.isEmpty())
		return {false, err};

	QJsonObject body;
	if (!i.title.isEmpty())
		body["title"] = i.title.left(140);
	if (!i.twitchCategoryId.isEmpty())
		body["game_id"] = i.twitchCategoryId;
	if (!i.language.isEmpty())
		body["broadcaster_language"] = i.language;
	body["tags"] = toJsonArray(cleanTwitchTags(i.tags));

	QUrl url("https://api.twitch.tv/helix/channels");
	QUrlQuery q;
	q.addQueryItem("broadcaster_id", uid);
	url.setQuery(q);
	Headers h = twitchHeaders(token);
	h.emplaceBack("Content-Type", "application/json");
	const HttpResponse r = http("PATCH", url, h, jsonBody(body));
	if (r.status == 204 || r.status == 200)
		return {true, "Aggiornato"};
	return {false, describeFailure(r)};
}

Result updateKick(const StreamInfo &i)
{
	QString err;
	const QString token = accessToken(Platform::Kick, &err);
	if (token.isEmpty())
		return {false, err};

	QJsonObject body;
	if (!i.title.isEmpty())
		body["stream_title"] = i.title.left(140);
	if (!i.kickCategoryId.isEmpty())
		body["category_id"] = i.kickCategoryId.toInt();
	if (!i.tags.isEmpty())
		body["custom_tags"] = toJsonArray(i.tags.mid(0, 10));

	Headers h = bearer(token);
	h.emplaceBack("Content-Type", "application/json");
	const HttpResponse r = http("PATCH", QUrl("https://api.kick.com/public/v1/channels"), h, jsonBody(body));
	if (r.status == 204 || r.status == 200)
		return {true, "Aggiornato"};
	return {false, describeFailure(r)};
}

Result updateYouTube(const StreamInfo &i)
{
	QString err;
	const QString token = accessToken(Platform::YouTube, &err);
	if (token.isEmpty())
		return {false, err};
	const Headers h = bearer(token);

	// Cerca la diretta attiva, altrimenti quella programmata.
	QString videoId;
	for (const char *status : {"active", "upcoming"}) {
		QUrl url("https://www.googleapis.com/youtube/v3/liveBroadcasts");
		QUrlQuery q;
		q.addQueryItem("part", "id");
		q.addQueryItem("broadcastStatus", status);
		url.setQuery(q);
		const HttpResponse r = http("GET", url, h);
		if (r.status != 200)
			return {false, describeFailure(r)};
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
	const HttpResponse cur = http("GET", vurl, h);
	const QJsonArray vitems = cur.json()["items"].toArray();
	if (cur.status != 200 || vitems.isEmpty())
		return {false, "Video della diretta non trovato: " + describeFailure(cur)};
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
	Headers ph = h;
	ph.emplaceBack("Content-Type", "application/json");
	const HttpResponse r = http("PUT", purl, ph, jsonBody(body));
	if (r.status == 200)
		return {true, "Aggiornato"};
	return {false, describeFailure(r)};
}

} // namespace

// ============================================================================ API pubblica
void setConfigDir(const QString &dir)
{
	g_configDir = dir;
	while (g_configDir.endsWith('/') || g_configDir.endsWith('\\'))
		g_configDir.chop(1);
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

void setCredentials(Platform p, const QString &id, const QString &secret)
{
	QJsonObject s = section(p);
	if (s["client_id"].toString() != id) {
		// Un'altra app sviluppatore: i vecchi token non valgono piu'.
		s.remove("access_token");
		s.remove("refresh_token");
		s.remove("expires_at");
		s.remove("user_id");
	}
	s["client_id"] = id.trimmed();
	s["client_secret"] = secret.trimmed();
	setSection(p, s);
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

Result login(Platform p)
{
	const Spec &sp = spec(p);
	if (!isConfigured(p))
		return {false, "Inserisci prima Client ID e Secret in Impostazioni"};

	QTcpServer server;
	if (!server.listen(QHostAddress::LocalHost, kPort))
		return {false, QString("La porta %1 e' occupata: chiudi l'altra applicazione e riprova").arg(kPort)};

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
		q.addQueryItem("prompt", "consent");
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
		if (!server.hasPendingConnections()) {
			QEventLoop loop;
			QTimer tick;
			tick.setSingleShot(true);
			QObject::connect(&server, &QTcpServer::newConnection, &loop, &QEventLoop::quit);
			QObject::connect(&tick, &QTimer::timeout, &loop, &QEventLoop::quit);
			tick.start(500);
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
		if (cb.hasQueryItem("error"))
			failure = "Accesso negato o annullato (" + cb.queryItemValue("error") + ")";
		else if (cb.queryItemValue("state") != state)
			failure = "Risposta non valida (state diverso)";
		else
			code = cb.queryItemValue("code");
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
	const HttpResponse r = tokenRequest(p, items);
	if (r.status != 200 || !storeTokens(p, r.json()))
		return {false, "Scambio del codice fallito: " + describeFailure(r)};

	if (p == Platform::Twitch) {
		QString err;
		QJsonObject s = section(p);
		s.remove("user_id");
		setSection(p, s);
		twitchUserId(s["access_token"].toString(), &err);
	}
	return {true, "Collegato"};
}

QList<Category> searchCategories(Platform p, const QString &query, QString *error)
{
	QList<Category> out;
	QString err;
	const QString token = accessToken(p, &err);
	if (token.isEmpty()) {
		if (error)
			*error = err;
		return out;
	}
	HttpResponse r;
	QUrlQuery q;
	if (p == Platform::Twitch) {
		QUrl url("https://api.twitch.tv/helix/search/categories");
		q.addQueryItem("query", query);
		q.addQueryItem("first", "15");
		url.setQuery(q);
		r = http("GET", url, twitchHeaders(token));
	} else if (p == Platform::Kick) {
		QUrl url("https://api.kick.com/public/v1/categories");
		q.addQueryItem("q", query);
		url.setQuery(q);
		r = http("GET", url, bearer(token));
	} else {
		return out;
	}
	if (r.status != 200) {
		if (error)
			*error = describeFailure(r);
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
	const QString token = accessToken(p, &err);
	if (token.isEmpty())
		return QString();

	if (p == Platform::Twitch) {
		const QString uid = twitchUserId(token, &err);
		if (uid.isEmpty())
			return QString();
		QUrl url("https://api.twitch.tv/helix/channels");
		QUrlQuery q;
		q.addQueryItem("broadcaster_id", uid);
		url.setQuery(q);
		const HttpResponse r = http("GET", url, twitchHeaders(token));
		const QJsonArray data = r.json()["data"].toArray();
		if (r.status != 200 || data.isEmpty())
			return QString();
		const QJsonObject d = data.first().toObject();
		return QString("«%1» · %2").arg(d["title"].toString(), d["game_name"].toString());
	}
	if (p == Platform::Kick) {
		const HttpResponse r = http("GET", QUrl("https://api.kick.com/public/v1/channels"), bearer(token));
		const QJsonArray data = r.json()["data"].toArray();
		if (r.status != 200 || data.isEmpty())
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
