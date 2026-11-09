#include "Freecloud.h"

#include "AesEncryptionAlgorithm.h"
#include "FreecloudService.h"
#include "Network/TelegramStorage.h"
#include "UI/ToolConfigurationParameter.h"

#include <QDesktopServices>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHash>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLineEdit>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QProcess>
#include <QProgressBar>
#include <QSet>
#include <QShortcut>
#include <QSizePolicy>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QUrl>
#include <QVBoxLayout>

#include <memory>

Freecloud::Freecloud(QWidget* parent)
	: Component(parent, ToolType::Freecloud)
	, telegramStorage(new TelegramStorage)
	, service(new FreecloudService(telegramStorage,
		EncryptionAlgorithmFactory(EncryptionAlgorithmFactory::Algorithm::Aes,
			std::make_unique<AesEncryptionAlgorithmParameters>(
				AesEncryptionAlgorithmParameters::Cipher::Aes256Gcm, 12, 16)), this))
	, pauseButton(new QToolButton(this))
	, stopButton(new QToolButton(this))
	, filesWidget(new QTreeWidget(this))
{
	setAcceptDrops(true);
	auto rootLayout = new QVBoxLayout(this);
	rootLayout->setContentsMargins(0, 0, 0, 0);
	auto toolbarLayout = new QHBoxLayout;
	auto addFileButton = new QToolButton(this);
	addFileButton->setIcon(QIcon(":/icons/freecloud-upload.svg"));
	addFileButton->setToolTip("Upload");
	auto uploadMenu = new QMenu(addFileButton);
	auto uploadFileAction = uploadMenu->addAction("Upload file");
	auto uploadFolderAction = uploadMenu->addAction("Upload folder");
	addFileButton->setMenu(uploadMenu);
	addFileButton->setPopupMode(QToolButton::InstantPopup);
	toolbarLayout->addWidget(addFileButton);
	auto addDirectoryButton = new QToolButton(this);
	addDirectoryButton->setIcon(QIcon(":/icons/folder-plus.svg"));
	addDirectoryButton->setToolTip("Create directory");
	toolbarLayout->addWidget(addDirectoryButton);
	auto separator = new QFrame(this);
	separator->setFrameShape(QFrame::VLine);
	separator->setFrameShadow(QFrame::Sunken);
	toolbarLayout->addWidget(separator);
	pauseButton->setIcon(QIcon(":/icons/freecloud-pause.svg"));
	pauseButton->setToolTip("Pause transfer");
	pauseButton->setEnabled(false);
	toolbarLayout->addWidget(pauseButton);
	stopButton->setIcon(QIcon(":/icons/freecloud-stop.svg"));
	stopButton->setToolTip("Stop transfer");
	stopButton->setEnabled(false);
	toolbarLayout->addWidget(stopButton);
	toolbarLayout->addStretch();
	rootLayout->addLayout(toolbarLayout);

	filesWidget->setHeaderLabels({"File", "Size", "State", "Progress", ""});
	filesWidget->setColumnWidth(3, 220);
	filesWidget->setProperty("qlementine_unfocused_selection", true);
	filesWidget->setRootIsDecorated(true);
	filesWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
	filesWidget->setContextMenuPolicy(Qt::CustomContextMenu);
	filesWidget->setDragDropMode(QAbstractItemView::InternalMove);
	filesWidget->setDefaultDropAction(Qt::MoveAction);
	filesWidget->setDropIndicatorShown(true);
	filesWidget->viewport()->installEventFilter(this);
	rootLayout->addWidget(filesWidget);
	auto deleteShortcut = new QShortcut(QKeySequence::Delete, filesWidget);
	deleteShortcut->setContext(Qt::WidgetWithChildrenShortcut);

	configurationParameters = {
		new ToolConfigurationParameter("Backend", [this](QWidget* parent) { return EditBackend(parent); }, this),
		new ToolConfigurationParameter("Encryption", [this](QWidget* parent) { return EditEncryption(parent); }, this),
		new ToolConfigurationParameter("Bot API key", [this](QWidget* parent) { return EditBotApiKey(parent); }, this,
			[this] { return HasTelegramBackend(); }),
		new ToolConfigurationParameter("Chat ID", [this](QWidget* parent) { return EditChatId(parent); }, this,
			[this] { return HasTelegramBackend(); }),
		new ToolConfigurationParameter("Storage folder", [this](QWidget* parent) { return EditStorageFolder(parent); }, this)
	};

	connect(addDirectoryButton, &QToolButton::clicked, this, &Freecloud::AddDirectory);
	connect(uploadFileAction, &QAction::triggered, this, qOverload<>(&Freecloud::AddFile));
	connect(uploadFolderAction, &QAction::triggered, this, qOverload<>(&Freecloud::AddFolder));
	connect(pauseButton, &QToolButton::clicked, this, &Freecloud::ActivateTransferAction);
	connect(stopButton, &QToolButton::clicked, this, &Freecloud::StopSelectedOperations);
	connect(filesWidget, &QTreeWidget::itemSelectionChanged, this, &Freecloud::UpdateTransferButton);
	connect(filesWidget, &QTreeWidget::itemDoubleClicked, this, &Freecloud::OpenFile);
	connect(filesWidget, &QTreeWidget::customContextMenuRequested, this, &Freecloud::ShowContextMenu);
	connect(deleteShortcut, &QShortcut::activated, this, &Freecloud::DeleteSelectedEntries);
	connect(service, &FreecloudService::ViewChanged, this, &Freecloud::UpdateItemStatuses);
	connect(service, &FreecloudService::StateChanged, this, &Freecloud::OnStateChanged);
	connect(service, &FreecloudService::Error, this, [this](const QString& error) {
		openWithDownloadedFile = false;
		QMessageBox::warning(this, "Freecloud", error);
	});
	connect(service, &FreecloudService::FileReady, this, &Freecloud::OpenReadyFile);
}

QList<ToolConfigurationParameter*> Freecloud::GetConfigurationParameters() const
{
	return configurationParameters;
}

QJsonObject Freecloud::SaveState()
{
	auto state = Component::SaveState();
	state["Backend"] = backend == Backend::Telegram ? "Telegram" : QString{};
	state["BotApiKey"] = telegramStorage->GetBotApiKey();
	state["ChatId"] = telegramStorage->GetChatId();
	const auto serviceState = service->SaveState();
	for (auto it = serviceState.begin(); it != serviceState.end(); ++it) {
		state[it.key()] = it.value();
	}

	return state;
}

void Freecloud::LoadState(const QJsonObject& state)
{
	Component::LoadState(state);
	telegramStorage->SetCredentials(state["BotApiKey"].toString(), state["ChatId"].toString());
	service->LoadState(state);
	SyncEntries();
}

bool Freecloud::eventFilter(QObject* watched, QEvent* event)
{
	if (watched != filesWidget->viewport()) {
		return Component::eventFilter(watched, event);
	}

	if (event->type() == QEvent::DragEnter) {
		auto dragEvent = static_cast<QDragEnterEvent*>(event);
		if (dragEvent->mimeData()->hasUrls()) {
			dragEvent->acceptProposedAction();
			return true;
		}
	} else if (event->type() == QEvent::DragMove) {
		auto dragEvent = static_cast<QDragMoveEvent*>(event);
		if (dragEvent->mimeData()->hasUrls()) {
			dragEvent->acceptProposedAction();
			return true;
		}
	} else if (event->type() == QEvent::Drop) {
		auto dropEvent = static_cast<QDropEvent*>(event);
		if (dropEvent->mimeData()->hasUrls()) {
			auto target = filesWidget->itemAt(dropEvent->position().toPoint());
			QString parentId;
			if (target && static_cast<EntryType>(target->data(0, EntryTypeRole).toInt()) == EntryType::Directory) {
				parentId = target->data(0, EntryIdRole).toString();
			} else if (target && target->parent()) {
				parentId = target->parent()->data(0, EntryIdRole).toString();
			}

			for (const auto& url : dropEvent->mimeData()->urls()) {
				const auto path = url.toLocalFile();
				const QFileInfo info(path);
				if (info.isFile()) {
					AddFile(path, parentId);
				} else if (info.isDir()) {
					AddFolder(path, parentId);
				}
			}

			dropEvent->acceptProposedAction();
			return true;
		}

		QTimer::singleShot(0, this, &Freecloud::UpdateParents);
	}

	return Component::eventFilter(watched, event);
}

void Freecloud::dragEnterEvent(QDragEnterEvent* event)
{
	for (const auto& url : event->mimeData()->urls()) {
		if (url.isLocalFile() && QFileInfo(url.toLocalFile()).exists()) {
			event->acceptProposedAction();
			return;
		}
	}
}

void Freecloud::dropEvent(QDropEvent* event)
{
	for (const auto& url : event->mimeData()->urls()) {
		const auto path = url.toLocalFile();
		const QFileInfo info(path);
		if (info.isFile()) {
			AddFile(path, GetSelectedDirectoryId());
		} else if (info.isDir()) {
			AddFolder(path, GetSelectedDirectoryId());
		}
	}

	event->acceptProposedAction();
}

void Freecloud::AddFile()
{
	const auto path = QFileDialog::getOpenFileName(this, "Upload file");
	if (!path.isEmpty()) {
		AddFile(path, GetSelectedDirectoryId());
	}
}

void Freecloud::AddFolder()
{
	const auto path = QFileDialog::getExistingDirectory(this, "Upload folder");
	if (!path.isEmpty()) {
		AddFolder(path, GetSelectedDirectoryId());
	}
}

void Freecloud::AddDirectory()
{
	bool accepted = false;
	const auto name = QInputDialog::getText(this, "Freecloud", "Directory name:", QLineEdit::Normal, {},
		&accepted).trimmed();
	if (!accepted || name.isEmpty()) {
		return;
	}

	const auto id = service->AddDirectory(name, GetSelectedDirectoryId());
	filesWidget->setCurrentItem(FindItem(EntryType::Directory, id));
}

void Freecloud::ShowContextMenu(const QPoint& position)
{
	auto item = filesWidget->itemAt(position);
	if (!item) {
		return;
	}

	if (!item->isSelected()) {
		filesWidget->setCurrentItem(item);
	}

	QMenu menu(this);
	const auto type = static_cast<EntryType>(item->data(0, EntryTypeRole).toInt());
	const auto id = item->data(0, EntryIdRole).toString();
	const bool hasLocalPath = !GetLocalPath(item).isEmpty();
	auto downloadAction = hasLocalPath ? nullptr : menu.addAction("Download");
	if (downloadAction) {
		menu.addSeparator();
	}

	auto openAction = menu.addAction("Open");
	auto openWithAction = menu.addAction("Open with...");
	auto showAction = menu.addAction("Show in folder");
	openAction->setEnabled(type == EntryType::File || hasLocalPath);
	openWithAction->setEnabled(type == EntryType::File || hasLocalPath);
	showAction->setEnabled(hasLocalPath);
	menu.addSeparator();
	auto renameAction = menu.addAction("Rename");
	auto deleteAction = menu.addAction("Delete");
	const auto selectedAction = menu.exec(filesWidget->viewport()->mapToGlobal(position));
	if (downloadAction && selectedAction == downloadAction) {
		if (type == EntryType::File) {
			service->DownloadFile(id);
		} else {
			service->DownloadDirectory(id);
		}
	} else if (selectedAction == openAction || selectedAction == openWithAction
		|| selectedAction == showAction || selectedAction == renameAction) {
		auto currentItem = FindItem(type, id);
		if (!currentItem) {
			return;
		}

		if (selectedAction == openAction) {
			OpenEntry(currentItem);
		} else if (selectedAction == openWithAction) {
			OpenWith(currentItem);
		} else if (selectedAction == showAction) {
			ShowInFolder(currentItem);
		} else {
			RenameEntry(currentItem);
		}
	} else if (selectedAction == deleteAction) {
		DeleteSelectedEntries();
	}
}

void Freecloud::UpdateParents()
{
	QHash<QString, QString> fileParents;
	QHash<QString, QString> directoryParents;
	QTreeWidgetItemIterator it(filesWidget);
	while (*it) {
		auto item = *it;
		const auto id = item->data(0, EntryIdRole).toString();
		const auto parentId = item->parent() ? item->parent()->data(0, EntryIdRole).toString() : QString{};
		if (static_cast<EntryType>(item->data(0, EntryTypeRole).toInt()) == EntryType::File) {
			fileParents.insert(id, parentId);
		} else {
			directoryParents.insert(id, parentId);
		}

		++it;
	}

	service->UpdateParents(fileParents, directoryParents);
}

void Freecloud::OpenFile(QTreeWidgetItem* item, int)
{
	if (static_cast<EntryType>(item->data(0, EntryTypeRole).toInt()) == EntryType::Directory) {
		return;
	}

	OpenEntry(item);
}

void Freecloud::OpenReadyFile(const QString& path)
{
	if (openWithDownloadedFile) {
		openWithDownloadedFile = false;
#ifdef _WIN32
		QProcess::startDetached("rundll32.exe", {"shell32.dll,OpenAs_RunDLL", QDir::toNativeSeparators(path)});
#elif defined(__APPLE__)
		QProcess::startDetached("open", {path});
#else
		QProcess::startDetached("xdg-open", {path});
#endif
	} else {
		QDesktopServices::openUrl(QUrl::fromLocalFile(path));
	}
}

void Freecloud::OpenEntry(QTreeWidgetItem* item)
{
	const auto type = static_cast<EntryType>(item->data(0, EntryTypeRole).toInt());
	const auto path = GetLocalPath(item);
	if (!path.isEmpty()) {
		QDesktopServices::openUrl(QUrl::fromLocalFile(path));
		return;
	}

	if (type == EntryType::Directory) {
		return;
	}

	const auto id = item->data(0, EntryIdRole).toString();
	openWithDownloadedFile = false;
	service->DownloadFile(id, true);
}

void Freecloud::OpenWith(QTreeWidgetItem* item)
{
	const auto path = GetLocalPath(item);
	if (path.isEmpty()) {
		if (static_cast<EntryType>(item->data(0, EntryTypeRole).toInt()) == EntryType::File) {
			openWithDownloadedFile = true;
			service->DownloadFile(item->data(0, EntryIdRole).toString(), true);
		}

		return;
	}

#ifdef _WIN32
	QProcess::startDetached("rundll32.exe", {"shell32.dll,OpenAs_RunDLL", QDir::toNativeSeparators(path)});
#elif defined(__APPLE__)
	QProcess::startDetached("open", {path});
#else
	QProcess::startDetached("xdg-open", {path});
#endif
}

void Freecloud::OnStateChanged(bool undoable)
{
	if (undoable) {
		SyncEntries();
	} else {
		UpdateItemStatuses();
	}

	emit Modified(this, undoable);
}

bool Freecloud::EditBackend(QWidget* parent)
{
	bool accepted = false;
	QInputDialog::getItem(parent, "Freecloud", "Backend:", {"Telegram"}, 0, false, &accepted);
	return accepted;
}

bool Freecloud::EditEncryption(QWidget* parent)
{
	bool accepted = false;
	QInputDialog::getItem(parent, "Freecloud", "Encryption:", {"AES-256-GCM"}, 0, false, &accepted);
	return accepted;
}

bool Freecloud::EditBotApiKey(QWidget* parent)
{
	bool accepted = false;
	const auto botApiKey = telegramStorage->GetBotApiKey();
	const auto value = QInputDialog::getText(parent, "Freecloud", "Bot API key:", QLineEdit::Password,
		botApiKey, &accepted).trimmed();
	if (!accepted || value == botApiKey) {
		return false;
	}

	telegramStorage->SetCredentials(value, telegramStorage->GetChatId());
	return true;
}

bool Freecloud::EditChatId(QWidget* parent)
{
	bool accepted = false;
	const auto chatId = telegramStorage->GetChatId();
	const auto value = QInputDialog::getText(parent, "Freecloud", "Chat ID:", QLineEdit::Normal,
		chatId, &accepted).trimmed();
	if (!accepted || value == chatId) {
		return false;
	}

	telegramStorage->SetCredentials(telegramStorage->GetBotApiKey(), value);
	return true;
}

bool Freecloud::EditStorageFolder(QWidget* parent)
{
	const auto storageFolder = service->GetStorageFolder();
	const auto value = QFileDialog::getExistingDirectory(parent, "Freecloud storage folder", storageFolder);
	if (value.isEmpty() || value == storageFolder) {
		return false;
	}

	service->SetStorageFolder(value);
	return true;
}

bool Freecloud::HasTelegramBackend() const
{
	return backend == Backend::Telegram;
}

void Freecloud::AddFile(const QString& path, const QString& parentId)
{
	const auto id = service->AddFile(path, parentId);
	filesWidget->setCurrentItem(FindItem(EntryType::File, id));
}

void Freecloud::AddFolder(const QString& path, const QString& parentId)
{
	const auto id = service->AddFolder(path, parentId);
	filesWidget->setCurrentItem(FindItem(EntryType::Directory, id));
}

void Freecloud::RenameEntry(QTreeWidgetItem* item)
{
	if (service->GetOperation() != FreecloudService::Operation::None) {
		QMessageBox::warning(this, "Freecloud", "Wait for the active transfer to finish.");
		return;
	}

	QDialog dialog(this);
	dialog.setWindowTitle("Rename");
	auto layout = new QFormLayout(&dialog);
	auto nameEdit = new QLineEdit(item->text(0), &dialog);
	layout->addRow("Name:", nameEdit);
	auto renameLocalCheck = new QCheckBox("Rename local", &dialog);
	const bool hasLocalPath = !GetLocalPath(item).isEmpty();
	renameLocalCheck->setEnabled(hasLocalPath);
	renameLocalCheck->setChecked(hasLocalPath);
	layout->addRow(renameLocalCheck);
	auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	layout->addRow(buttons);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

	const auto name = nameEdit->text().trimmed();
	if (name == item->text(0)) {
		return;
	}

	QString error;
	const auto type = static_cast<EntryType>(item->data(0, EntryTypeRole).toInt());
	const auto id = item->data(0, EntryIdRole).toString();
	const bool renamed = type == EntryType::File
		? service->RenameFile(id, name, renameLocalCheck->isChecked(), error)
		: service->RenameDirectory(id, name, renameLocalCheck->isChecked(), error);
	if (!renamed) {
		QMessageBox::warning(this, "Freecloud", error);
	}
}

void Freecloud::DeleteSelectedEntries()
{
	if (service->GetOperation() != FreecloudService::Operation::None) {
		QMessageBox::warning(this, "Freecloud", "Wait for the active transfer to finish.");
		return;
	}

	const auto items = filesWidget->selectedItems();
	if (items.isEmpty()) {
		return;
	}

	QStringList fileIds;
	QStringList directoryIds;
	for (auto item : items) {
		const auto id = item->data(0, EntryIdRole).toString();
		if (static_cast<EntryType>(item->data(0, EntryTypeRole).toInt()) == EntryType::File) {
			fileIds.append(id);
		} else {
			directoryIds.append(id);
		}
	}

	QDialog dialog(this);
	dialog.setWindowTitle("Delete");
	dialog.setMinimumWidth(440);
	auto layout = new QFormLayout(&dialog);
	const auto message = items.size() == 1
		? QString("Delete '%1'?").arg(items.front()->text(0))
		: QString("Delete %1 selected entries?").arg(items.size());
	auto label = new QLabel(message, &dialog);
	label->setWordWrap(true);
	layout->addRow(label);
	auto actionCombo = new QComboBox(&dialog);
	actionCombo->addItems({"Delete cloud and local", "Delete cloud", "Delete local"});
	actionCombo->setCurrentIndex(2);
	actionCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	layout->addRow("Action:", actionCombo);
	auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	layout->addRow(buttons);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

	auto deleteMode = FreecloudService::DeleteMode::CloudAndLocal;
	if (actionCombo->currentIndex() == 1) {
		deleteMode = FreecloudService::DeleteMode::Cloud;
	} else if (actionCombo->currentIndex() == 2) {
		deleteMode = FreecloudService::DeleteMode::Local;
	}

	service->DeleteEntries(fileIds, directoryIds, deleteMode);
}

void Freecloud::ActivateTransferAction()
{
	auto item = filesWidget->currentItem();
	if (!item) {
		return;
	}

	const auto id = item->data(0, EntryIdRole).toString();
	const auto type = static_cast<EntryType>(item->data(0, EntryTypeRole).toInt());
	const auto transferState = type == EntryType::File
		? service->GetFileTransferState(id) : service->GetDirectoryTransferState(id);
	if (transferState == FreecloudService::TransferState::Uploading
		|| transferState == FreecloudService::TransferState::Downloading) {
		if (service->IsPaused()) {
			service->Resume();
		} else {
			service->Pause();
		}

		return;
	}

	if (!GetLocalPath(item).isEmpty() || transferState == FreecloudService::TransferState::PendingDownload) {
		return;
	}

	if (type == EntryType::File) {
		service->DownloadFile(id);
	} else {
		service->DownloadDirectory(id);
	}
}

void Freecloud::StopSelectedOperations()
{
	QStringList fileIds;
	QStringList directoryIds;
	for (auto item : filesWidget->selectedItems()) {
		const auto id = item->data(0, EntryIdRole).toString();
		if (static_cast<EntryType>(item->data(0, EntryTypeRole).toInt()) == EntryType::File) {
			fileIds.append(id);
		} else {
			directoryIds.append(id);
		}
	}

	service->StopEntries(fileIds, directoryIds);
}

void Freecloud::UpdateTransferButton()
{
	bool canStop = false;
	for (auto selectedItem : filesWidget->selectedItems()) {
		const auto selectedId = selectedItem->data(0, EntryIdRole).toString();
		const auto selectedType = static_cast<EntryType>(selectedItem->data(0, EntryTypeRole).toInt());
		const auto selectedState = selectedType == EntryType::File
			? service->GetFileTransferState(selectedId) : service->GetDirectoryTransferState(selectedId);
		if (selectedState == FreecloudService::TransferState::PendingUpload
			|| selectedState == FreecloudService::TransferState::Uploading
			|| selectedState == FreecloudService::TransferState::PendingDownload
			|| selectedState == FreecloudService::TransferState::Downloading) {
			canStop = true;
			break;
		}
	}

	stopButton->setEnabled(canStop);
	auto item = filesWidget->currentItem();
	if (!item) {
		pauseButton->setEnabled(false);
		pauseButton->setIcon(QIcon(":/icons/freecloud-download.svg"));
		pauseButton->setToolTip("Download");
		return;
	}

	const auto id = item->data(0, EntryIdRole).toString();
	const auto type = static_cast<EntryType>(item->data(0, EntryTypeRole).toInt());
	const auto transferState = type == EntryType::File
		? service->GetFileTransferState(id) : service->GetDirectoryTransferState(id);
	if (transferState == FreecloudService::TransferState::PendingDelete
		|| transferState == FreecloudService::TransferState::Deleting) {
		pauseButton->setEnabled(false);
		pauseButton->setIcon(QIcon(":/icons/freecloud-download.svg"));
		pauseButton->setToolTip(transferState == FreecloudService::TransferState::PendingDelete
			? "Deletion pending" : "Deleting");
		return;
	}

	if (transferState == FreecloudService::TransferState::Uploading
		|| transferState == FreecloudService::TransferState::Downloading) {
		pauseButton->setEnabled(true);
		pauseButton->setIcon(QIcon(service->IsPaused()
			? ":/icons/freecloud-resume.svg" : ":/icons/freecloud-pause.svg"));
		pauseButton->setToolTip(service->IsPaused() ? "Resume transfer" : "Pause transfer");

		return;
	}

	pauseButton->setEnabled(GetLocalPath(item).isEmpty()
		&& transferState != FreecloudService::TransferState::PendingDownload);
	pauseButton->setIcon(QIcon(":/icons/freecloud-download.svg"));
	pauseButton->setToolTip(transferState == FreecloudService::TransferState::PendingDownload
		? "Download queued" : "Download");
}

void Freecloud::ShowInFolder(QTreeWidgetItem* item)
{
	const auto path = GetLocalPath(item);
	if (path.isEmpty()) {
		return;
	}

#ifdef _WIN32
	QProcess::startDetached("explorer.exe", {"/select,", QDir::toNativeSeparators(path)});
#elif defined(__APPLE__)
	QProcess::startDetached("open", {"-R", path});
#else
	QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}

QString Freecloud::GetLocalPath(QTreeWidgetItem* item) const
{
	const auto id = item->data(0, EntryIdRole).toString();
	return static_cast<EntryType>(item->data(0, EntryTypeRole).toInt()) == EntryType::File
		? service->GetFileLocalPath(id) : service->GetDirectoryLocalPath(id);
}

void Freecloud::SetItemStatus(QTreeWidgetItem* item, bool local, bool cloud,
	FreecloudService::TransferState transferState)
{
	const bool downloading = transferState == FreecloudService::TransferState::Downloading;
	const bool uploading = transferState == FreecloudService::TransferState::Uploading;
	const bool pendingDeletion = transferState == FreecloudService::TransferState::PendingDelete;
	const bool deleting = transferState == FreecloudService::TransferState::Deleting;
	const auto availableColor = QColor("#4caf50");
	const auto activeColor = QColor("#f9a825");
	const auto unavailableColor = QColor("#e53935");
	QColor localColor = unavailableColor;
	QString localTooltip = "Not available locally";
	if (local) {
		localColor = availableColor;
		localTooltip = "Available locally";
	} else if (downloading) {
		localColor = activeColor;
		localTooltip = "Downloading";
	}

	QColor cloudColor = unavailableColor;
	QString cloudTooltip = "Not available in cloud";
	if (pendingDeletion || deleting) {
		cloudColor = activeColor;
		cloudTooltip = pendingDeletion ? "Pending deletion" : "Deleting";
	} else if (cloud) {
		cloudColor = availableColor;
		cloudTooltip = "Available in cloud";
	} else if (uploading) {
		cloudColor = activeColor;
		cloudTooltip = "Uploading";
	}

	auto stateWidget = filesWidget->itemWidget(item, 2);
	QLabel* localLabel;
	QLabel* cloudLabel;
	if (stateWidget) {
		localLabel = stateWidget->findChild<QLabel*>("FreecloudLocalState");
		cloudLabel = stateWidget->findChild<QLabel*>("FreecloudCloudState");
	} else {
		stateWidget = new QWidget(filesWidget);
		stateWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
		auto stateLayout = new QHBoxLayout(stateWidget);
		stateLayout->setContentsMargins(4, 0, 4, 0);
		stateLayout->setSpacing(6);
		localLabel = new QLabel(stateWidget);
		localLabel->setObjectName("FreecloudLocalState");
		stateLayout->addWidget(localLabel);
		cloudLabel = new QLabel(stateWidget);
		cloudLabel->setObjectName("FreecloudCloudState");
		stateLayout->addWidget(cloudLabel);
		stateLayout->addStretch();
		filesWidget->setItemWidget(item, 2, stateWidget);
	}

	localLabel->setPixmap(GetColoredIcon(":/icons/freecloud-local.svg", localColor));
	localLabel->setToolTip(localTooltip);
	cloudLabel->setPixmap(GetColoredIcon(":/icons/freecloud-cloud.svg", cloudColor));
	cloudLabel->setToolTip(cloudTooltip);

	if (transferState == FreecloudService::TransferState::None) {
		if (auto progressWidget = filesWidget->itemWidget(item, 3)) {
			progressWidget->hide();
		}

		return;
	}

	auto progressWidget = filesWidget->itemWidget(item, 3);
	auto progressBar = progressWidget ? progressWidget->findChild<QProgressBar*>() : nullptr;
	if (!progressBar) {
		progressWidget = new QWidget(filesWidget);
		progressWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
		auto progressLayout = new QVBoxLayout(progressWidget);
		progressLayout->setContentsMargins(4, 0, 4, 0);
		progressBar = new QProgressBar(progressWidget);
		progressBar->setAttribute(Qt::WA_TransparentForMouseEvents);
		progressBar->setTextVisible(true);
		progressBar->setFixedHeight(16);
		auto progressFont = progressBar->font();
		progressFont.setPixelSize(10);
		progressBar->setFont(progressFont);
		progressLayout->addWidget(progressBar);
		filesWidget->setItemWidget(item, 3, progressWidget);
	}

	progressWidget->show();
	if (transferState == FreecloudService::TransferState::PendingUpload
		|| transferState == FreecloudService::TransferState::PendingDownload
		|| transferState == FreecloudService::TransferState::PendingDelete) {
		progressBar->setRange(0, 0);
		if (transferState == FreecloudService::TransferState::PendingUpload) {
			progressBar->setFormat("Pending upload");
		} else if (transferState == FreecloudService::TransferState::PendingDownload) {
			progressBar->setFormat("Pending download");
		} else {
			progressBar->setFormat("Pending deletion");
		}
	} else {
		progressBar->setRange(0, 100);
		progressBar->setValue(service->GetTransferProgress());
		QString operationLabel;
		if (uploading) {
			operationLabel = "Uploading";
		} else if (downloading) {
			operationLabel = "Downloading";
		} else {
			operationLabel = "Deleting";
		}

		if (service->IsPaused()) {
			operationLabel += " paused";
		}

		progressBar->setFormat(operationLabel + " %p%");
	}
}

QPixmap Freecloud::GetColoredIcon(const QString& path, const QColor& color)
{
	auto pixmap = QIcon(path).pixmap(16, 16);
	auto painter = QPainter(&pixmap);
	painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
	painter.fillRect(pixmap.rect(), color);
	painter.end();
	return pixmap;
}

void Freecloud::UpdateItemStatuses()
{
	for (const auto& directory : service->GetDirectories()) {
		auto item = FindItem(EntryType::Directory, directory.id);
		if (item) {
			SetItemStatus(item, service->IsDirectoryLocal(directory.id), service->IsDirectoryCloud(directory.id),
				service->GetDirectoryTransferState(directory.id));
		}
	}

	for (const auto& file : service->GetFiles()) {
		auto item = FindItem(EntryType::File, file.id);
		if (item) {
			SetItemStatus(item, service->IsFileLocal(file.id), service->IsFileCloud(file.id),
				service->GetFileTransferState(file.id));
		}
	}

	UpdateTransferButton();
}

void Freecloud::SyncEntries()
{
	const auto& directories = service->GetDirectories();
	const auto& files = service->GetFiles();
	QSet<QString> directoryIds;
	QSet<QString> fileIds;
	for (const auto& directory : directories) {
		directoryIds.insert(directory.id);
	}

	for (const auto& file : files) {
		fileIds.insert(file.id);
	}

	QList<QTreeWidgetItem*> removedItems;
	QTreeWidgetItemIterator itemIt(filesWidget);
	while (*itemIt) {
		auto item = *itemIt;
		const auto type = static_cast<EntryType>(item->data(0, EntryTypeRole).toInt());
		const auto id = item->data(0, EntryIdRole).toString();
		if ((type == EntryType::Directory && !directoryIds.contains(id))
			|| (type == EntryType::File && !fileIds.contains(id))) {
			removedItems.append(item);
		}

		++itemIt;
	}

	for (int idx = removedItems.size() - 1; idx >= 0; --idx) {
		delete removedItems[idx];
	}

	QHash<QString, QTreeWidgetItem*> directoryItems;
	QTreeWidgetItemIterator directoryIt(filesWidget);
	while (*directoryIt) {
		auto item = *directoryIt;
		if (static_cast<EntryType>(item->data(0, EntryTypeRole).toInt()) == EntryType::Directory) {
			directoryItems.insert(item->data(0, EntryIdRole).toString(), item);
		}

		++directoryIt;
	}

	QSet<QString> newDirectoryIds;
	for (const auto& directory : directories) {
		auto item = directoryItems.value(directory.id);
		if (!item) {
			item = new QTreeWidgetItem;
			directoryItems.insert(directory.id, item);
			newDirectoryIds.insert(directory.id);
		}

		item->setText(0, directory.name);
		item->setText(1, QLocale().formattedDataSize(service->GetDirectorySize(directory.id)));
		item->setIcon(0, QIcon(":/icons/folder.svg"));
		item->setData(0, EntryTypeRole, static_cast<int>(EntryType::Directory));
		item->setData(0, EntryIdRole, directory.id);
	}

	for (const auto& directory : directories) {
		auto item = directoryItems[directory.id];
		auto parent = directoryItems.value(directory.parentId);
		if (parent == item) {
			parent = nullptr;
		}

		if (item->parent() != parent || !item->treeWidget()) {
			if (item->parent()) {
				item->parent()->takeChild(item->parent()->indexOfChild(item));
			} else if (item->treeWidget()) {
				filesWidget->takeTopLevelItem(filesWidget->indexOfTopLevelItem(item));
			}

			if (parent) {
				parent->addChild(item);
			} else {
				filesWidget->addTopLevelItem(item);
			}
		}
	}

	for (const auto& directory : directories) {
		auto item = directoryItems[directory.id];
		SetItemStatus(item, service->IsDirectoryLocal(directory.id), service->IsDirectoryCloud(directory.id),
			service->GetDirectoryTransferState(directory.id));
	}

	for (int idx = 0; idx < files.size(); ++idx) {
		const auto& file = files[idx];
		auto item = FindItem(EntryType::File, file.id);
		if (!item) {
			item = new QTreeWidgetItem;
		}

		item->setText(0, file.name);
		item->setText(1, QLocale().formattedDataSize(file.size));
		item->setFlags(item->flags() & ~Qt::ItemIsDropEnabled);
		item->setData(0, EntryTypeRole, static_cast<int>(EntryType::File));
		item->setData(0, EntryIdRole, file.id);
		item->setData(0, FileIdxRole, idx);
		auto parent = directoryItems.value(file.parentId);
		if (item->parent() != parent || !item->treeWidget()) {
			if (item->parent()) {
				item->parent()->takeChild(item->parent()->indexOfChild(item));
			} else if (item->treeWidget()) {
				filesWidget->takeTopLevelItem(filesWidget->indexOfTopLevelItem(item));
			}

			if (parent) {
				parent->addChild(item);
			} else {
				filesWidget->addTopLevelItem(item);
			}
		}

		SetItemStatus(item, service->IsFileLocal(file.id), service->IsFileCloud(file.id),
			service->GetFileTransferState(file.id));
	}

	for (const auto& id : newDirectoryIds) {
		directoryItems[id]->setExpanded(true);
	}

	filesWidget->resizeColumnToContents(0);
	filesWidget->setColumnWidth(2, 64);
	filesWidget->setColumnWidth(3, 220);
	UpdateTransferButton();
}

QString Freecloud::GetSelectedDirectoryId() const
{
	auto item = filesWidget->currentItem();
	if (!item) {
		return {};
	}

	if (static_cast<EntryType>(item->data(0, EntryTypeRole).toInt()) == EntryType::File) {
		item = item->parent();
	}

	return item ? item->data(0, EntryIdRole).toString() : QString{};
}

QTreeWidgetItem* Freecloud::FindItem(EntryType type, const QString& id) const
{
	QTreeWidgetItemIterator it(filesWidget);
	while (*it) {
		auto item = *it;
		if (static_cast<EntryType>(item->data(0, EntryTypeRole).toInt()) == type
			&& item->data(0, EntryIdRole).toString() == id) {
			return item;
		}

		++it;
	}

	return nullptr;
}
