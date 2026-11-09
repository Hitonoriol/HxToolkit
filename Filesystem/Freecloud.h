#pragma once

#include "FreecloudService.h"
#include "UI/Component.h"

#include <QList>

class TelegramStorage;
class QColor;
class QDragEnterEvent;
class QDropEvent;
class QEvent;
class QPoint;
class QPixmap;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

class Freecloud : public Component
{
	Q_OBJECT

public:
	explicit Freecloud(QWidget* parent = nullptr);

	QList<ToolConfigurationParameter*> GetConfigurationParameters() const override;
	QJsonObject SaveState() override;
	void LoadState(const QJsonObject& state) override;

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dropEvent(QDropEvent* event) override;

private slots:
	void AddFile();
	void AddFolder();
	void AddDirectory();
	void ShowContextMenu(const QPoint& position);
	void UpdateParents();
	void OpenFile(QTreeWidgetItem* item, int column);
	void OpenReadyFile(const QString& path);
	void DeleteSelectedEntries();
	void ActivateTransferAction();
	void StopSelectedOperations();
	void OnStateChanged(bool undoable);

private:
	enum class Backend { Telegram };
	enum class EntryType { File, Directory };

	bool EditBackend(QWidget* parent);
	bool EditEncryption(QWidget* parent);
	bool EditBotApiKey(QWidget* parent);
	bool EditChatId(QWidget* parent);
	bool EditStorageFolder(QWidget* parent);
	bool HasTelegramBackend() const;
	void AddFile(const QString& path, const QString& parentId = {});
	void AddFolder(const QString& path, const QString& parentId = {});
	void OpenEntry(QTreeWidgetItem* item);
	void OpenWith(QTreeWidgetItem* item);
	void RenameEntry(QTreeWidgetItem* item);
	void ShowInFolder(QTreeWidgetItem* item);
	QString GetLocalPath(QTreeWidgetItem* item) const;
	void SetItemStatus(QTreeWidgetItem* item, bool local, bool cloud,
		FreecloudService::TransferState transferState);
	static QPixmap GetColoredIcon(const QString& path, const QColor& color);
	void UpdateItemStatuses();
	void UpdateTransferButton();
	void SyncEntries();
	QString GetSelectedDirectoryId() const;
	QTreeWidgetItem* FindItem(EntryType type, const QString& id) const;

	static constexpr int EntryTypeRole = Qt::UserRole;
	static constexpr int EntryIdRole = Qt::UserRole + 1;
	static constexpr int FileIdxRole = Qt::UserRole + 2;

	Backend backend{Backend::Telegram};
	TelegramStorage* telegramStorage;
	FreecloudService* service;
	QToolButton* pauseButton;
	QToolButton* stopButton;
	QTreeWidget* filesWidget;
	QList<ToolConfigurationParameter*> configurationParameters;
	bool openWithDownloadedFile{};
};
