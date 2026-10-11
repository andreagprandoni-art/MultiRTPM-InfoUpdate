// Accesso alle API di Twitch, Kick e YouTube (login OAuth, ricerca categorie, aggiornamento info live).
// Licenza: GPL-2.0-or-later (come il template ufficiale dei plugin OBS)
#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

enum class Platform { Twitch = 0, Kick = 1, YouTube = 2 };
constexpr int kPlatformCount = 3;

struct Category {
	QString id;
	QString name;
};

struct StreamInfo {
	QString title;
	QString description;
	QString language;
	QStringList tags;
	QString twitchCategoryId;
	QString kickCategoryId;
	QString youtubeCategoryId;
};

struct Result {
	bool ok = false;
	QString message;
};

namespace svc {

// Cartella dove salvare impostazioni e token (da chiamare una volta all'avvio del plugin).
void setConfigDir(const QString &dir);

QString platformName(Platform p);
QString redirectUri(Platform p);

// Credenziali dell'app sviluppatore create dall'utente su ogni piattaforma.
QString clientId(Platform p);
QString clientSecret(Platform p);
void setCredentials(Platform p, const QString &clientId, const QString &clientSecret);

// Credenziali inserite dall'utente nelle Impostazioni: se presenti hanno la precedenza su quelle integrate.
QString customClientId(Platform p);
QString customClientSecret(Platform p);

// true se le credenziali dell'app OAuth sono gia' dentro il plugin (build di distribuzione).
bool hasBuiltInCredentials(Platform p);
bool isConfigured(Platform p);
bool isConnected(Platform p);
void disconnectAccount(Platform p);

// Apre il browser per il login e attende il ritorno (massimo 3 minuti, l'interfaccia resta reattiva).
Result login(Platform p);

// Cerca categorie/giochi (solo Twitch e Kick).
QList<Category> searchCategories(Platform p, const QString &query, QString *error);

// Applica titolo, categoria, tag, ecc. alla piattaforma.
Result updateChannel(Platform p, const StreamInfo &info);

// Informazioni attuali del canale (best effort), es. "«titolo» · categoria".
QString currentSummary(Platform p);

// Preset salvati dall'utente.
QJsonObject presets();
void savePreset(const QString &name, const QJsonObject &data);
void deletePreset(const QString &name);

} // namespace svc
