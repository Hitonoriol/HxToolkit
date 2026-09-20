#include "NetworkInterfaces.h"
#include "System/WindowsPermissionService.h"

#include <QComboBox>
#include <QFutureWatcher>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QProcess>
#include <QSignalBlocker>
#include <QTreeWidgetItem>
#include <QScrollBar>
#include <QSet>
#include <QtConcurrentRun>

#include <algorithm>

#include <winsock2.h>
#include <iphlpapi.h>

namespace {
constexpr auto NameRole = Qt::UserRole;
constexpr auto SettingsRole = Qt::UserRole + 1;
constexpr auto IdRole = Qt::UserRole + 2;

QString FormatIpv4Address(const SOCKADDR* address)
{
	if (!address || address->sa_family != AF_INET) return {};
	const auto* ipv4 = reinterpret_cast<const SOCKADDR_IN*>(address);
	return QHostAddress(ntohl(ipv4->sin_addr.s_addr)).toString();
}

QString SubnetMask(ULONG prefixLength)
{
	if (prefixLength > 32) return {};
	const quint32 mask = prefixLength == 0 ? 0 : 0xffffffffu << (32 - prefixLength);
	return QString("%1.%2.%3.%4").arg((mask >> 24) & 255).arg((mask >> 16) & 255).arg((mask >> 8) & 255).arg(mask & 255);
}

QString AdapterStatus(IF_OPER_STATUS status)
{
	switch (status) {
	case IfOperStatusUp: return "Connected";
	case IfOperStatusDown: return "Disconnected";
	case IfOperStatusDormant: return "Dormant";
	case IfOperStatusLowerLayerDown: return "Disabled";
	default: return "Unknown";
	}
}
}

NetworkInterfaces::NetworkInterfaces(QWidget* parent)
	: Component(parent, ToolType::NetworkInterfaces)
{
	ui.setupUi(this);
	ui.InterfacesTree->setColumnWidth(0, 240);
	connect(ui.RefreshButton, &QPushButton::clicked, this, &NetworkInterfaces::RefreshInterfaces);
	connect(ui.SavePresetButton, &QPushButton::clicked, this, &NetworkInterfaces::SavePreset);
	connect(ui.LoadPresetButton, &QPushButton::clicked, this, &NetworkInterfaces::LoadPreset);
	connect(ui.DeletePresetButton, &QPushButton::clicked, this, &NetworkInterfaces::DeletePreset);
	connect(ui.ApplyButton, &QPushButton::clicked, this, &NetworkInterfaces::ApplySelectedInterface);
	connect(ui.EnabledBox, &QCheckBox::toggled, this, &NetworkInterfaces::SetSelectedInterfaceEnabled);
	connect(&discoveryWatcher, &QFutureWatcher<QList<AdapterSettings>>::finished, this, [this] {
		const auto selectedId = ui.InterfacesTree->currentItem()
			? (ui.InterfacesTree->currentItem()->parent() ? ui.InterfacesTree->currentItem()->parent() : ui.InterfacesTree->currentItem())->data(0, IdRole).toString()
			: QString{};
		QSet<QString> expandedIds;
		for (int index = 0; index < ui.InterfacesTree->topLevelItemCount(); ++index) {
			auto* item = ui.InterfacesTree->topLevelItem(index);
			if (item->isExpanded()) expandedIds.insert(item->data(0, IdRole).toString());
		}
		const auto scrollPosition = ui.InterfacesTree->verticalScrollBar()->value();
		QMap<QString, QTreeWidgetItem*> existing;
		while (ui.InterfacesTree->topLevelItemCount() > 0) {
			auto* item = ui.InterfacesTree->takeTopLevelItem(0);
			existing.insert(item->data(0, IdRole).toString(), item);
		}

		auto adapters = discoveryWatcher.result();
		std::sort(adapters.begin(), adapters.end(), [](const auto& left, const auto& right) { return left.id < right.id; });
		QTreeWidgetItem* selectedItem = nullptr;
		for (const auto& adapter : adapters) {
			auto* item = existing.take(adapter.id);
			if (!item) item = new QTreeWidgetItem;
			UpdateAdapter(item, adapter);
			item->setExpanded(expandedIds.contains(adapter.id));
			ui.InterfacesTree->addTopLevelItem(item);
			if (adapter.id == selectedId) selectedItem = item;
		}
		qDeleteAll(existing);
		ui.InterfacesTree->setCurrentItem(selectedItem);
		ui.InterfacesTree->verticalScrollBar()->setValue(scrollPosition);
		SelectAdapter(selectedItem);
		ui.StatusLabel->setText(QString("%1 interface(s) found. Administrator rights are required to apply changes.").arg(adapters.size()));
		SetBusy(false);
	});
	connect(ui.InterfacesTree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* current, QTreeWidgetItem*) {
		if (current && current->parent()) current = current->parent();
		SelectAdapter(current);
	});
	connect(&refreshTimer, &QTimer::timeout, this, &NetworkInterfaces::RefreshInterfaces);
	refreshTimer.start(std::chrono::seconds{5});
	SelectAdapter(nullptr);
	RefreshInterfaces();
}

QJsonObject NetworkInterfaces::SaveState()
{
	auto state = Component::SaveState();
	QJsonObject savedPresets;
	for (auto it = presets.cbegin(); it != presets.cend(); ++it) savedPresets[it.key()] = it.value();
	state["Presets"] = savedPresets;
	return state;
}

void NetworkInterfaces::LoadState(const QJsonObject& state)
{
	Component::LoadState(state);
	presets.clear();
	const auto savedPresets = state["Presets"].toObject();
	for (auto it = savedPresets.begin(); it != savedPresets.end(); ++it) presets[it.key()] = it.value().toObject();
	UpdatePresetBox();
}

void NetworkInterfaces::RefreshInterfaces()
{
	if (discoveryWatcher.isRunning()) return;
	SetBusy(true);
	ui.StatusLabel->setText("Retrieving network interfaces…");
	discoveryWatcher.setFuture(QtConcurrent::run(&NetworkInterfaces::DiscoverAdapters));
}

QList<NetworkInterfaces::AdapterSettings> NetworkInterfaces::DiscoverAdapters()
{
	ULONG bufferSize = 0;
	const auto flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_INCLUDE_ALL_INTERFACES;
	if (GetAdaptersAddresses(AF_INET, flags, nullptr, nullptr, &bufferSize) != ERROR_BUFFER_OVERFLOW) return {};
	QByteArray buffer(static_cast<qsizetype>(bufferSize), '\0');
	auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
	if (GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &bufferSize) != NO_ERROR) return {};

	QList<AdapterSettings> results;
	for (auto* adapter = adapters; adapter; adapter = adapter->Next) {
		AdapterSettings settings;
		settings.id = QString::fromLocal8Bit(adapter->AdapterName);
		settings.name = QString::fromWCharArray(adapter->FriendlyName);
		settings.description = QString::fromWCharArray(adapter->Description);
		settings.status = AdapterStatus(adapter->OperStatus);
		settings.dhcp = adapter->Dhcpv4Enabled;
		MIB_IFROW row{};
		row.dwIndex = adapter->IfIndex;
		settings.enabled = GetIfEntry(&row) == NO_ERROR ? row.dwAdminStatus == MIB_IF_ADMIN_STATUS_UP : adapter->OperStatus != IfOperStatusLowerLayerDown;

		for (auto* address = adapter->FirstUnicastAddress; address; address = address->Next) {
			const auto text = FormatIpv4Address(address->Address.lpSockaddr);
			if (!text.isEmpty() && !text.startsWith("169.254.")) { settings.address = text; settings.subnet = SubnetMask(address->OnLinkPrefixLength); break; }
		}
		if (adapter->FirstGatewayAddress) settings.gateway = FormatIpv4Address(adapter->FirstGatewayAddress->Address.lpSockaddr);
		for (auto* dns = adapter->FirstDnsServerAddress; dns; dns = dns->Next) {
			const auto text = FormatIpv4Address(dns->Address.lpSockaddr);
			if (!text.isEmpty()) settings.dns.append(text);
		}
		results.append(settings);
	}
	return results;
}

void NetworkInterfaces::AddAdapter(const AdapterSettings& settings)
{
	auto* item = new QTreeWidgetItem(ui.InterfacesTree);
	UpdateAdapter(item, settings);
}

void NetworkInterfaces::UpdateAdapter(QTreeWidgetItem* item, const AdapterSettings& settings)
{
	item->setText(0, settings.name);
	item->setText(1, settings.status + (settings.address.isEmpty() ? "" : " — " + settings.address));
	item->setData(0, NameRole, settings.name);
	item->setData(0, IdRole, settings.id);
	item->setData(0, SettingsRole, SettingsToJson(settings));
	item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
	qDeleteAll(item->takeChildren());
	auto addDetail = [item](const QString& label, const QString& value) {
		auto* detail = new QTreeWidgetItem(item);
		detail->setText(0, label);
		detail->setText(1, value.isEmpty() ? "—" : value);
	};
	addDetail("Description", settings.description);
	addDetail("IPv4 mode", settings.dhcp ? "DHCP" : "Custom");
	addDetail("IPv4 address", settings.address);
	addDetail("Gateway", settings.gateway);
	addDetail("Subnet mask", settings.subnet);
	addDetail("DNS servers", settings.dns.join(", "));
}

void NetworkInterfaces::SelectAdapter(QTreeWidgetItem* item)
{
	ui.SettingsGroup->setVisible(item);
	if (!item) return;
	const auto settings = SettingsFromItem(item);
	QSignalBlocker blocker(ui.EnabledBox);
	ui.EnabledBox->setChecked(settings.enabled);
	ui.DhcpBox->setChecked(settings.dhcp);
	ui.AddressField->setText(settings.address);
	ui.GatewayField->setText(settings.gateway);
	ui.SubnetField->setText(settings.subnet);
	ui.DnsField->setText(settings.dns.join(", "));
	ui.SelectedInterfaceLabel->setText(settings.name);
}

void NetworkInterfaces::SavePreset()
{
	const auto name = ui.PresetNameField->text().trimmed();
	if (name.isEmpty() || !ui.InterfacesTree->currentItem()) { QMessageBox::information(this, "Save preset", "Select an interface and enter a preset name."); return; }
	auto settings = SettingsFromItem(ui.InterfacesTree->currentItem()->parent() ? ui.InterfacesTree->currentItem()->parent() : ui.InterfacesTree->currentItem());
	settings.dhcp = ui.DhcpBox->isChecked(); settings.address = ui.AddressField->text().trimmed(); settings.gateway = ui.GatewayField->text().trimmed(); settings.subnet = ui.SubnetField->text().trimmed(); settings.dns = ui.DnsField->text().split(',', Qt::SkipEmptyParts);
	for (auto& dns : settings.dns) dns = dns.trimmed();
	presets[name] = SettingsToJson(settings);
	UpdatePresetBox(); ui.PresetBox->setCurrentText(name); emit Modified(this);
}

void NetworkInterfaces::LoadPreset()
{
	if (!presets.contains(ui.PresetBox->currentText()) || SelectedAdapterName().isEmpty()) return;
	const auto settings = SettingsFromJson(presets[ui.PresetBox->currentText()]);
	ui.DhcpBox->setChecked(settings.dhcp); ui.AddressField->setText(settings.address); ui.GatewayField->setText(settings.gateway); ui.SubnetField->setText(settings.subnet); ui.DnsField->setText(settings.dns.join(", "));
	ui.StatusLabel->setText("Preset loaded. Click Apply to change " + SelectedAdapterName() + ".");
}

void NetworkInterfaces::DeletePreset()
{
	if (presets.remove(ui.PresetBox->currentText())) { UpdatePresetBox(); emit Modified(this); }
}

void NetworkInterfaces::ApplySelectedInterface()
{
	const auto adapter = SelectedAdapterName();
	if (adapter.isEmpty()) return;
	const auto elevation = WindowsPermissionService::requestElevation("Applying network interface settings");
	if (elevation == WindowsPermissionService::ElevationResult::ElevationRequested) {
		return;
	}
	if (elevation == WindowsPermissionService::ElevationResult::Cancelled) return;
	if (elevation == WindowsPermissionService::ElevationResult::Denied) {
		ui.StatusLabel->setText("Administrator permission is required to apply network settings.");
		return;
	}
	QString error;
	bool ok;
	if (ui.DhcpBox->isChecked()) {
		ok = RunNetsh({"interface", "ipv4", "set", "address", "name=" + adapter, "source=dhcp"}, &error);
		if (ok) ok = RunNetsh({"interface", "ipv4", "set", "dnsservers", "name=" + adapter, "source=dhcp"}, &error);
	} else {
		ok = RunNetsh({"interface", "ipv4", "set", "address", "name=" + adapter, "source=static", "address=" + ui.AddressField->text().trimmed(), "mask=" + ui.SubnetField->text().trimmed(), "gateway=" + ui.GatewayField->text().trimmed()}, &error);
	}
	if (ok && !ui.DhcpBox->isChecked()) {
		const auto dns = ui.DnsField->text().split(',', Qt::SkipEmptyParts);
		if (!dns.isEmpty()) ok = RunNetsh({"interface", "ipv4", "set", "dnsservers", "name=" + adapter, "source=static", "address=" + dns[0].trimmed(), "validate=no"}, &error);
		for (int i = 1; ok && i < dns.size(); ++i) ok = RunNetsh({"interface", "ipv4", "add", "dnsservers", "name=" + adapter, "address=" + dns[i].trimmed(), "index=" + QString::number(i + 1), "validate=no"}, &error);
	}
	ui.StatusLabel->setText(ok ? "Settings applied. Refreshing interfaces…" : "Could not apply settings: " + error);
	if (ok) RefreshInterfaces();
}

void NetworkInterfaces::SetSelectedInterfaceEnabled(bool enabled)
{
	const auto adapter = SelectedAdapterName(); if (adapter.isEmpty()) return;
	const auto elevation = WindowsPermissionService::requestElevation(enabled ? "Enabling this network interface" : "Disabling this network interface");
	if (elevation == WindowsPermissionService::ElevationResult::ElevationRequested) {
		return;
	}
	if (elevation == WindowsPermissionService::ElevationResult::Cancelled) return;
	if (elevation == WindowsPermissionService::ElevationResult::Denied) {
		ui.StatusLabel->setText("Administrator permission is required to change interface state.");
		return;
	}
	QString error;
	if (!RunNetsh({"interface", "set", "interface", "name=" + adapter, "admin=" + QString(enabled ? "enabled" : "disabled")}, &error)) ui.StatusLabel->setText("Could not change interface state: " + error);
	else { ui.StatusLabel->setText("Interface state changed. Refreshing…"); RefreshInterfaces(); }
}

NetworkInterfaces::AdapterSettings NetworkInterfaces::SettingsFromItem(QTreeWidgetItem* item) const { return SettingsFromJson(item ? item->data(0, SettingsRole).toJsonObject() : QJsonObject{}); }
QString NetworkInterfaces::SelectedAdapterName() const { auto* item = ui.InterfacesTree->currentItem(); if (item && item->parent()) item = item->parent(); return item ? item->data(0, NameRole).toString() : QString{}; }
bool NetworkInterfaces::RunNetsh(const QStringList& arguments, QString* error) const { QProcess process; process.start("netsh.exe", arguments); process.waitForFinished(15'000); if (process.exitCode() == 0) return true; if (error) *error = QString::fromLocal8Bit(process.readAllStandardError() + process.readAllStandardOutput()).trimmed(); return false; }
QJsonObject NetworkInterfaces::SettingsToJson(const AdapterSettings& s) const { QJsonObject value{{"Id",s.id},{"Name",s.name},{"Description",s.description},{"Status",s.status},{"Enabled",s.enabled},{"Dhcp",s.dhcp},{"Address",s.address},{"Gateway",s.gateway},{"Subnet",s.subnet}}; QJsonArray dns; for (const auto& entry : s.dns) dns.append(entry); value["Dns"] = dns; return value; }
NetworkInterfaces::AdapterSettings NetworkInterfaces::SettingsFromJson(const QJsonObject& v) const { AdapterSettings s; s.id=v["Id"].toString(); s.name=v["Name"].toString(); s.description=v["Description"].toString(); s.status=v["Status"].toString(); s.enabled=v["Enabled"].toBool(); s.dhcp=v["Dhcp"].toBool(); s.address=v["Address"].toString(); s.gateway=v["Gateway"].toString(); s.subnet=v["Subnet"].toString(); const auto prefix=v["Prefix"].toInt(-1); if (prefix >= 0) { quint32 mask = prefix == 0 ? 0 : 0xffffffffu << (32-prefix); s.subnet=QString("%1.%2.%3.%4").arg((mask>>24)&255).arg((mask>>16)&255).arg((mask>>8)&255).arg(mask&255); } for (const auto& dns:v["Dns"].toArray()) s.dns.append(dns.toString()); return s; }
void NetworkInterfaces::UpdatePresetBox() { const auto current=ui.PresetBox->currentText(); ui.PresetBox->clear(); ui.PresetBox->addItems(presets.keys()); ui.PresetBox->setCurrentText(current); }
void NetworkInterfaces::SetBusy(bool busy) { ui.RefreshButton->setDisabled(busy); ui.ApplyButton->setDisabled(busy); }
