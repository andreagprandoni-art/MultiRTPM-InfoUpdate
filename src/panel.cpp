// Licenza: GPL-2.0-or-later (come il template ufficiale dei plugin OBS)
#include "panel.hpp"

#include <algorithm>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextDocumentFragment>
#include <QVBoxLayout>

#include <obs.h>
#include <plugin-support.h>

namespace {

const Platform kAll[kPlatformCount] = {Platform::Twitch, Platform::Kick, Platform::YouTube};

// ------------------------------------------------------------------ finestra impostazioni
class SettingsDialog : public QDialog {
public:
	explicit SettingsDialog(QWidget *parent) : QDialog(parent)
	{
		setWindowTitle("Impostazioni - Info Live");
		setMinimumWidth(560);
		auto *lay = new QVBoxLayout(this);

		auto *intro = new QLabel(
			"Per ogni piattaforma che vuoi usare crea una tua \"app sviluppatore\" (gratis, una volta sola: "
			"la guida passo-passo è nel README del plugin) e incolla qui Client ID e Client Secret. "
			"Come indirizzo di reindirizzamento (Redirect URI) usa esattamente quello indicato sotto. "
			"Dopo il salvataggio premi Collega.",
			this);
		intro->setWordWrap(true);
		lay->addWidget(intro);

		const char *where[kPlatformCount] = {
			"<a href=\"https://dev.twitch.tv/console\">dev.twitch.tv/console</a> · tipo client: Confidential",
			"kick.com → Impostazioni → Developer · permessi: user:read, channel:read, channel:write",
			"<a href=\"https://console.cloud.google.com/apis/credentials\">console.cloud.google.com</a> · "
			"ID client OAuth di tipo \"App desktop\" · YouTube Data API v3 abilitata"};

		for (int i = 0; i < kPlatformCount; ++i) {
			const Platform p = kAll[i];
			auto *box = new QGroupBox(svc::platformName(p), this);
			auto *form = new QFormLayout(box);

			auto *hint = new QLabel(QString::fromUtf8(where[i]), box);
			hint->setWordWrap(true);
			hint->setOpenExternalLinks(true);
			hint->setStyleSheet("color: gray;");
			form->addRow(hint);

			m_id[i] = new QLineEdit(svc::clientId(p), box);
			m_secret[i] = new QLineEdit(svc::clientSecret(p), box);
			m_secret[i]->setEchoMode(QLineEdit::Password);
			auto *uri = new QLineEdit(svc::redirectUri(p), box);
			uri->setReadOnly(true);
			auto *copy = new QPushButton("Copia", box);
			auto *uriRow = new QHBoxLayout();
			uriRow->addWidget(uri, 1);
			uriRow->addWidget(copy);
			connect(copy, &QPushButton::clicked, this,
				[uri]() { QApplication::clipboard()->setText(uri->text()); });

			form->addRow("Client ID", m_id[i]);
			form->addRow("Client Secret", m_secret[i]);
			form->addRow("Redirect URI", uriRow);
			lay->addWidget(box);
		}

		auto *buttons = new QHBoxLayout();
		buttons->addStretch();
		auto *cancel = new QPushButton("Annulla", this);
		auto *save = new QPushButton("Salva", this);
		save->setDefault(true);
		buttons->addWidget(cancel);
		buttons->addWidget(save);
		lay->addLayout(buttons);

		connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
		connect(save, &QPushButton::clicked, this, [this]() { onSave(); });
	}

private:
	void onSave()
	{
		// Controlli semplici per gli errori di copia/incolla piu' comuni.
		for (int i = 0; i < kPlatformCount; ++i) {
			const QString id = m_id[i]->text().trimmed();
			const QString secret = m_secret[i]->text().trimmed();
			const QString name = svc::platformName(kAll[i]);
			if (id.isEmpty() && secret.isEmpty())
				continue;
			if (id.contains(QRegularExpression("\\s"))) {
				QMessageBox::warning(this, "Impostazioni", name + ": il Client ID contiene degli spazi.");
				return;
			}
			if (secret.contains(QRegularExpression("\\s"))) {
				QMessageBox::warning(this, "Impostazioni", name + ": il Client Secret contiene degli spazi.");
				return;
			}
			if (id.isEmpty()) {
				QMessageBox::warning(this, "Impostazioni", name + ": manca il Client ID.");
				return;
			}
			if (kAll[i] != Platform::YouTube && secret.isEmpty()) {
				QMessageBox::warning(this, "Impostazioni", name + ": manca il Client Secret.");
				return;
			}
			if (kAll[i] == Platform::YouTube && !id.endsWith(".apps.googleusercontent.com") &&
			    QMessageBox::question(this, "Impostazioni",
						  "Il Client ID di YouTube di solito finisce con "
						  ".apps.googleusercontent.com. Salvare comunque?") != QMessageBox::Yes)
				return;
		}
		bool saved = true;
		for (int i = 0; i < kPlatformCount; ++i)
			saved = svc::setCredentials(kAll[i], m_id[i]->text(), m_secret[i]->text()) && saved;
		if (!saved) {
			QMessageBox::warning(this, "Impostazioni",
					     "Non riesco a salvare le impostazioni in:\n" + svc::configDir());
			return;
		}
		accept();
	}

	QLineEdit *m_id[kPlatformCount] = {};
	QLineEdit *m_secret[kPlatformCount] = {};
};

} // namespace

// ==================================================================================== pannello
InfoPanel::InfoPanel(QWidget *parent) : QScrollArea(parent)
{
	setWidgetResizable(true);
	setFrameShape(QFrame::NoFrame);
	buildUi();
	reloadPresets();
	refreshStatus();
}

void InfoPanel::buildUi()
{
	m_content = new QWidget(this);
	setWidget(m_content);
	auto *root = new QVBoxLayout(m_content);

	// --- piattaforme
	auto *platforms = new QGroupBox("Piattaforme", m_content);
	auto *pl = new QVBoxLayout(platforms);
	for (int i = 0; i < kPlatformCount; ++i) {
		const Platform p = kAll[i];
		auto *row = new QHBoxLayout();
		m_check[i] = new QCheckBox(svc::platformName(p), platforms);
		m_status[i] = new QLabel(platforms);
		m_connect[i] = new QPushButton("Collega", platforms);
		row->addWidget(m_check[i]);
		row->addWidget(m_status[i], 1);
		row->addWidget(m_connect[i]);
		pl->addLayout(row);
		connect(m_connect[i], &QPushButton::clicked, this, [this, p]() { doConnect(p); });
	}
	auto *tools = new QHBoxLayout();
	auto *settingsBtn = new QPushButton("Impostazioni (Client ID / Secret)…", platforms);
	auto *supportBtn = new QPushButton("Copia info per supporto", platforms);
	tools->addWidget(settingsBtn, 1);
	tools->addWidget(supportBtn);
	pl->addLayout(tools);
	connect(settingsBtn, &QPushButton::clicked, this, [this]() { openSettings(); });
	connect(supportBtn, &QPushButton::clicked, this, [this]() { copySupportInfo(); });
	root->addWidget(platforms);

	m_current = new QLabel(m_content);
	m_current->setWordWrap(true);
	m_current->setStyleSheet("color: gray;");
	root->addWidget(m_current);

	// --- preset
	root->addWidget(new QLabel("Preset", m_content));
	auto *presetRow = new QHBoxLayout();
	m_presets = new QComboBox(m_content);
	auto *savePreset = new QPushButton("Salva…", m_content);
	auto *delPreset = new QPushButton("Elimina", m_content);
	presetRow->addWidget(m_presets, 1);
	presetRow->addWidget(savePreset);
	presetRow->addWidget(delPreset);
	root->addLayout(presetRow);

	connect(m_presets, &QComboBox::activated, this, [this](int index) {
		if (index <= 0)
			return;
		applyPreset(svc::presets().value(m_presets->itemText(index)).toObject());
	});
	connect(savePreset, &QPushButton::clicked, this, [this]() {
		bool ok = false;
		const QString name = QInputDialog::getText(this, "Salva preset", "Nome del preset (es. il gioco):",
							   QLineEdit::Normal, QString(), &ok)
					     .trimmed();
		if (!ok || name.isEmpty())
			return;
		svc::savePreset(name, toPreset());
		reloadPresets(name);
	});
	connect(delPreset, &QPushButton::clicked, this, [this]() {
		const QString name = m_presets->currentText();
		if (m_presets->currentIndex() <= 0)
			return;
		if (QMessageBox::question(this, "Elimina preset", "Eliminare il preset \"" + name + "\"?") ==
		    QMessageBox::Yes) {
			svc::deletePreset(name);
			reloadPresets();
		}
	});

	// --- titolo
	auto *titleHead = new QHBoxLayout();
	titleHead->addWidget(new QLabel("Titolo", m_content));
	m_count = new QLabel("0/140", m_content);
	m_count->setAlignment(Qt::AlignRight);
	titleHead->addWidget(m_count, 1);
	root->addLayout(titleHead);
	m_title = new QLineEdit(m_content);
	m_title->setPlaceholderText("Titolo della live");
	root->addWidget(m_title);
	connect(m_title, &QLineEdit::textChanged, this, [this](const QString &t) {
		m_count->setText(QString("%1/140").arg(t.size()));
		m_count->setStyleSheet(t.size() > 100 ? "color: orange;" : "");
	});

	// --- categoria
	root->addWidget(new QLabel("Categoria / gioco (cerca su Twitch e Kick)", m_content));
	auto *searchRow = new QHBoxLayout();
	m_search = new QLineEdit(m_content);
	m_search->setPlaceholderText("Es. Minecraft");
	auto *searchBtn = new QPushButton("Cerca", m_content);
	searchRow->addWidget(m_search, 1);
	searchRow->addWidget(searchBtn);
	root->addLayout(searchRow);
	connect(searchBtn, &QPushButton::clicked, this, [this]() { doSearch(); });
	connect(m_search, &QLineEdit::returnPressed, this, [this]() { doSearch(); });

	auto *form = new QFormLayout();
	m_twitchCat = new QComboBox(m_content);
	m_kickCat = new QComboBox(m_content);
	m_ytCat = new QComboBox(m_content);
	const QList<QPair<QString, QString>> ytCats = {{"Gaming", "20"},
							{"Intrattenimento", "24"},
							{"Persone e blog", "22"},
							{"Musica", "10"},
							{"Istruzione", "27"},
							{"Scienza e tecnologia", "28"}};
	for (const auto &c : ytCats)
		m_ytCat->addItem(c.first, c.second);
	form->addRow("Twitch", m_twitchCat);
	form->addRow("Kick", m_kickCat);
	form->addRow("YouTube", m_ytCat);
	root->addLayout(form);

	// --- tag, lingua, descrizione
	root->addWidget(new QLabel("Tag (separati da virgola)", m_content));
	m_tags = new QLineEdit(m_content);
	m_tags->setPlaceholderText("italiano, gaming, relax");
	root->addWidget(m_tags);

	auto *langRow = new QFormLayout();
	m_lang = new QComboBox(m_content);
	const QList<QPair<QString, QString>> langs = {{"Italiano", "it"}, {"English", "en"}, {"Español", "es"},
						       {"Français", "fr"}, {"Deutsch", "de"}, {"Português", "pt"}};
	m_lang->addItem("— non cambiare —", QString());
	for (const auto &l : langs)
		m_lang->addItem(l.first, l.second);
	langRow->addRow("Lingua (Twitch)", m_lang);
	root->addLayout(langRow);

	root->addWidget(new QLabel("Descrizione (YouTube)", m_content));
	m_desc = new QPlainTextEdit(m_content);
	m_desc->setMaximumHeight(80);
	root->addWidget(m_desc);

	// --- azione
	m_go = new QPushButton("Aggiorna le piattaforme selezionate", m_content);
	m_go->setMinimumHeight(34);
	root->addWidget(m_go);
	connect(m_go, &QPushButton::clicked, this, [this]() { doUpdate(); });

	m_results = new QLabel(m_content);
	m_results->setWordWrap(true);
	m_results->setTextFormat(Qt::RichText);
	m_results->setTextInteractionFlags(Qt::TextSelectableByMouse);
	root->addWidget(m_results);

	auto *version = new QLabel(QString("Info Live · versione %1").arg(QString::fromUtf8(PLUGIN_VERSION)), m_content);
	version->setStyleSheet("color: gray;");
	root->addWidget(version);

	root->addStretch();
}

// ------------------------------------------------------------------------------- stato
void InfoPanel::refreshStatus()
{
	for (int i = 0; i < kPlatformCount; ++i) {
		const Platform p = kAll[i];
		const bool connected = svc::isConnected(p);
		const QString who = connected ? svc::accountLabel(p) : QString();
		m_status[i]->setText(connected ? (who.isEmpty() ? QString("● collegato") : "● collegato · " + who)
					       : (svc::isConfigured(p) ? "○ non collegato" : "○ da configurare"));
		m_status[i]->setStyleSheet(connected ? "color: #3ecf8e;" : "color: gray;");
		m_connect[i]->setText(connected ? "Scollega" : "Collega");
		m_check[i]->setEnabled(connected);
		if (!connected)
			m_check[i]->setChecked(false);
	}
}

void InfoPanel::refreshCurrent()
{
	QStringList lines;
	for (int i = 0; i < kPlatformCount; ++i) {
		const Platform p = kAll[i];
		if (!svc::isConnected(p))
			continue;
		const QString s = svc::currentSummary(p);
		if (!s.isEmpty())
			lines << svc::platformName(p) + ": " + s;
	}
	m_current->setText(lines.isEmpty() ? QString() : "Ora in live — " + lines.join("  |  "));
}

void InfoPanel::setBusy(bool busy)
{
	m_content->setEnabled(!busy);
}

void InfoPanel::showLines(const QStringList &lines)
{
	m_results->setText(lines.join("<br>"));
	QTextDocumentFragment plain = QTextDocumentFragment::fromHtml(lines.join("\n"));
	m_lastResults = plain.toPlainText();
}

void InfoPanel::copySupportInfo()
{
	QStringList out;
	out << QString("Multistream Info Panel %1").arg(QString::fromUtf8(PLUGIN_VERSION));
	out << QString("OBS %1 · Qt %2").arg(QString::fromUtf8(obs_get_version_string()), QString::fromLatin1(qVersion()));
	out << "Cartella impostazioni: " + svc::configDir();
	for (const Platform p : kAll)
		out << QString("%1: %2, %3").arg(svc::platformName(p),
						 svc::isConfigured(p) ? "credenziali inserite" : "credenziali mancanti",
						 svc::isConnected(p) ? "collegato" : "non collegato");
	if (!m_lastResults.trimmed().isEmpty())
		out << "Ultimo messaggio: " + m_lastResults.trimmed();
	QApplication::clipboard()->setText(out.join("\n"));
	showLines({"Info copiate negli appunti (nessun token o segreto incluso): incollale dove chiedi aiuto."});
}

// ------------------------------------------------------------------------------ azioni
void InfoPanel::doConnect(Platform p)
{
	if (svc::isConnected(p)) {
		svc::disconnectAccount(p);
		refreshStatus();
		showLines({});
		return;
	}
	if (!svc::isConfigured(p)) {
		showLines({"<b>" + svc::platformName(p) + "</b>: inserisci Client ID e Client Secret, poi premi Collega."});
		openSettings();
		return;
	}
	showLines({"Completa il login nel browser che si è aperto… (massimo 3 minuti)"});

	// Finestra modale con pulsante Annulla: blocca il pannello mentre aspettiamo il browser.
	QProgressDialog wait("Completa il login di " + svc::platformName(p) + " nel browser che si è aperto…", "Annulla",
			     0, 0, this);
	wait.setWindowTitle("Collegamento a " + svc::platformName(p));
	wait.setWindowModality(Qt::WindowModal);
	wait.setMinimumDuration(0);
	wait.setAutoClose(false);
	wait.setAutoReset(false);
	connect(&wait, &QProgressDialog::canceled, this, []() { svc::cancelLogin(); });
	wait.show();

	const Result r = svc::login(p);
	wait.close();
	refreshStatus();
	if (r.ok) {
		const QString who = svc::accountLabel(p);
		showLines({"✔ <b>" + svc::platformName(p) + "</b> collegato" +
			   (who.isEmpty() ? QString() : " come <b>" + who.toHtmlEscaped() + "</b>")});
		m_check[static_cast<int>(p)]->setChecked(true);
		setBusy(true);
		refreshCurrent();
		setBusy(false);
	} else {
		showLines({"✖ <b>" + svc::platformName(p) + "</b>: " + r.message.toHtmlEscaped()});
		if (r.message != "Collegamento annullato")
			QMessageBox::warning(this, "Collegamento a " + svc::platformName(p), r.message);
	}
}

void InfoPanel::fillCombo(QComboBox *box, const QList<Category> &items, const QString &query)
{
	box->clear();
	QList<Category> sorted = items;
	// Mette per prima la corrispondenza esatta con quello che hai scritto.
	std::stable_sort(sorted.begin(), sorted.end(), [&query](const Category &a, const Category &b) {
		const bool ea = a.name.compare(query, Qt::CaseInsensitive) == 0;
		const bool eb = b.name.compare(query, Qt::CaseInsensitive) == 0;
		return ea && !eb;
	});
	for (const Category &c : sorted)
		box->addItem(c.name, c.id);
}

void InfoPanel::doSearch()
{
	const QString q = m_search->text().trimmed();
	if (q.size() < 2)
		return;
	setBusy(true);
	QStringList problems;
	if (svc::isConnected(Platform::Twitch)) {
		QString err;
		const QList<Category> list = svc::searchCategories(Platform::Twitch, q, &err);
		fillCombo(m_twitchCat, list, q);
		if (!err.isEmpty())
			problems << "<b>Twitch</b>: " + err.toHtmlEscaped();
	}
	if (svc::isConnected(Platform::Kick)) {
		QString err;
		const QList<Category> list = svc::searchCategories(Platform::Kick, q, &err);
		fillCombo(m_kickCat, list, q);
		if (!err.isEmpty())
			problems << "<b>Kick</b>: " + err.toHtmlEscaped();
	}
	setBusy(false);
	showLines(problems);
}

StreamInfo InfoPanel::collect() const
{
	StreamInfo info;
	info.title = m_title->text().trimmed();
	info.description = m_desc->toPlainText();
	info.language = m_lang->currentData().toString();
	for (const QString &t : m_tags->text().split(',', Qt::SkipEmptyParts)) {
		QString tag = t.trimmed();
		if (tag.startsWith('#'))
			tag.remove(0, 1);
		if (!tag.isEmpty())
			info.tags << tag;
	}
	info.twitchCategoryId = m_twitchCat->currentData().toString();
	info.kickCategoryId = m_kickCat->currentData().toString();
	info.youtubeCategoryId = m_ytCat->currentData().toString();
	return info;
}

void InfoPanel::doUpdate()
{
	const StreamInfo info = collect();
	if (info.title.isEmpty()) {
		showLines({"✖ Scrivi un titolo."});
		return;
	}
	QList<Platform> chosen;
	for (int i = 0; i < kPlatformCount; ++i)
		if (m_check[i]->isChecked() && svc::isConnected(kAll[i]))
			chosen << kAll[i];
	if (chosen.isEmpty()) {
		showLines({"✖ Seleziona almeno una piattaforma collegata."});
		return;
	}

	setBusy(true);
	showLines({"Aggiorno…"});
	QStringList lines;
	for (Platform p : chosen) {
		const Result r = svc::updateChannel(p, info);
		lines << QString(r.ok ? "✔ " : "✖ ") + "<b>" + svc::platformName(p) + "</b> — " +
				 r.message.toHtmlEscaped();
	}
	showLines(lines);
	refreshCurrent();
	setBusy(false);
}

void InfoPanel::openSettings()
{
	SettingsDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted)
		refreshStatus();
}

// ---------------------------------------------------------------------------- preset
QJsonObject InfoPanel::toPreset() const
{
	QJsonObject o;
	o["title"] = m_title->text();
	o["description"] = m_desc->toPlainText();
	o["tags"] = m_tags->text();
	o["language"] = m_lang->currentData().toString();
	o["search"] = m_search->text();
	o["twitch_id"] = m_twitchCat->currentData().toString();
	o["twitch_name"] = m_twitchCat->currentText();
	o["kick_id"] = m_kickCat->currentData().toString();
	o["kick_name"] = m_kickCat->currentText();
	o["youtube_id"] = m_ytCat->currentData().toString();
	return o;
}

void InfoPanel::applyPreset(const QJsonObject &o)
{
	m_title->setText(o["title"].toString());
	m_desc->setPlainText(o["description"].toString());
	m_tags->setText(o["tags"].toString());
	m_search->setText(o["search"].toString());
	const int lang = m_lang->findData(o["language"].toString());
	if (lang >= 0)
		m_lang->setCurrentIndex(lang);
	const int yt = m_ytCat->findData(o["youtube_id"].toString());
	if (yt >= 0)
		m_ytCat->setCurrentIndex(yt);
	m_twitchCat->clear();
	if (!o["twitch_id"].toString().isEmpty())
		m_twitchCat->addItem(o["twitch_name"].toString(), o["twitch_id"].toString());
	m_kickCat->clear();
	if (!o["kick_id"].toString().isEmpty())
		m_kickCat->addItem(o["kick_name"].toString(), o["kick_id"].toString());
}

void InfoPanel::reloadPresets(const QString &select)
{
	m_presets->clear();
	m_presets->addItem("— nessuno —");
	const QJsonObject all = svc::presets();
	for (auto it = all.begin(); it != all.end(); ++it)
		m_presets->addItem(it.key());
	if (!select.isEmpty()) {
		const int idx = m_presets->findText(select);
		if (idx >= 0)
			m_presets->setCurrentIndex(idx);
	}
}
