#pragma once

#include "System/KeystrokeListener.h"
#include "UI/Component.h"

#include <QSet>

class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QTableWidget;
class QTableWidgetItem;
class ToolConfigurationParameter;
class Workspace;

class QuickMenu : public Component
{
	Q_OBJECT

public:
	explicit QuickMenu(Workspace* workspace, QWidget* parent = nullptr);
	~QuickMenu();

	QJsonObject SaveState() override;
	void LoadState(const QJsonObject& state) override;
	QList<ToolConfigurationParameter*> GetConfigurationParameters() const override;

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
	void RefreshComponents();
	void ComponentCheckStateChanged(QTableWidgetItem* item);
	void FilterItems(const QString& query);
	void ShowPopup();
	void ActivateItem(QListWidgetItem* item);

private:
	bool EditShortcut(QWidget* parent);
	void RegisterShortcut();
	QPoint GetPopupPosition() const;

	Workspace* workspace;
	QTableWidget* componentTable;
	QWidget* popup;
	QLineEdit* searchField;
	QListWidget* popupList;
	ToolConfigurationParameter* shortcutConfiguration;
	QPoint popupAnchor;
	KeystrokeListener::Chord chord;
	QSet<QString> excludedComponentIds;
	int shortcutId{};
	bool refreshingComponents{};
	bool showToolkitExcluded{};
};
