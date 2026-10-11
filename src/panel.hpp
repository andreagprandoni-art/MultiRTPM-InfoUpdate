// Pannello (dock) "Info Live" mostrato dentro OBS.
// Licenza: GPL-2.0-or-later (come il template ufficiale dei plugin OBS)
#pragma once

#include <QScrollArea>

#include "services.hpp"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

class InfoPanel : public QScrollArea {
public:
	explicit InfoPanel(QWidget *parent = nullptr);

private:
	void buildUi();
	void refreshStatus();
	void refreshCurrent();
	void setBusy(bool busy);
	void showLines(const QStringList &lines);

	void doConnect(Platform p);
	void doSearch();
	void doUpdate();
	void openSettings();

	StreamInfo collect() const;
	QJsonObject toPreset() const;
	void applyPreset(const QJsonObject &o);
	void reloadPresets(const QString &select = QString());
	void fillCombo(QComboBox *box, const QList<Category> &items, const QString &query);

	QWidget *m_content = nullptr;
	QCheckBox *m_check[kPlatformCount] = {};
	QLabel *m_status[kPlatformCount] = {};
	QPushButton *m_connect[kPlatformCount] = {};
	QLabel *m_current = nullptr;

	QComboBox *m_presets = nullptr;
	QLineEdit *m_title = nullptr;
	QLabel *m_count = nullptr;
	QLineEdit *m_search = nullptr;
	QComboBox *m_twitchCat = nullptr;
	QComboBox *m_kickCat = nullptr;
	QComboBox *m_ytCat = nullptr;
	QLineEdit *m_tags = nullptr;
	QComboBox *m_lang = nullptr;
	QPlainTextEdit *m_desc = nullptr;
	QPushButton *m_go = nullptr;
	QLabel *m_results = nullptr;
};
