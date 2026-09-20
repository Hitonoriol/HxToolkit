#pragma once

#include "UI/Component.h"
#include "ui_NetworkInterfaces.h"

#include <QJsonObject>
#include <QFutureWatcher>
#include <QList>
#include <QMap>
#include <QTimer>

class QTreeWidgetItem;

class NetworkInterfaces : public Component
{
	Q_OBJECT

public:
	explicit NetworkInterfaces(QWidget* parent = nullptr);
	QJsonObject SaveState() override;
	void LoadState(const QJsonObject& state) override;

private slots:
	void RefreshInterfaces();
	void SavePreset();
	void LoadPreset();
	void DeletePreset();
	void ApplySelectedInterface();
	void SetSelectedInterfaceEnabled(bool enabled);

private:
	struct AdapterSettings {
		QString id;
		QString name;
		QString description;
		QString status;
		bool enabled{};
		bool dhcp{};
		QString address;
		QString gateway;
		QString subnet;
		QStringList dns;
	};

	AdapterSettings SettingsFromItem(QTreeWidgetItem* item) const;
	void AddAdapter(const AdapterSettings& settings);
	void UpdateAdapter(QTreeWidgetItem* item, const AdapterSettings& settings);
	void SelectAdapter(QTreeWidgetItem* item);
	bool RunNetsh(const QStringList& arguments, QString* error = nullptr) const;
	QString SelectedAdapterName() const;
	QJsonObject SettingsToJson(const AdapterSettings& settings) const;
	AdapterSettings SettingsFromJson(const QJsonObject& value) const;
	void UpdatePresetBox();
	void SetBusy(bool busy);
	static QList<AdapterSettings> DiscoverAdapters();

	Ui::NetworkInterfacesClass ui;
	QMap<QString, QJsonObject> presets;
	QFutureWatcher<QList<AdapterSettings>> discoveryWatcher;
	QTimer refreshTimer;
};
