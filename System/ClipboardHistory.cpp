#include "ClipboardHistory.h"
#include "Util/Qt.h"

#include <QMimeData>
#include <QMessageBox>
#include <QJsonArray>
#include <filesystem>

ClipboardHistory::ClipboardHistory(QWidget *parent)
	: Component(parent, ToolType::ClipboardManager)
	, clipboardPath(std::filesystem::path(QApplication::applicationDirPath().toStdString()) / "Clipboard")
{
	ui.setupUi(this);
	ui.splitter->setStretchFactor(0, 0);
	ui.splitter->setStretchFactor(1, 1);

	connect(QGuiApplication::clipboard(), &QClipboard::changed, this, &ClipboardHistory::OnClipboardChanged);
	connect(ui.ClipboardList, &QListWidget::currentItemChanged, this, &ClipboardHistory::OnSelectedItemChanged);
	connect(ui.ClipboardList, &QListWidget::itemDoubleClicked, this, &ClipboardHistory::OnItemDoubleClick);
	connect(ui.ClearButton, &QPushButton::clicked, this, &ClipboardHistory::OnClearHistoryClicked);

	std::error_code ec;
	std::filesystem::create_directories(clipboardPath, ec);
}

ClipboardHistory::~ClipboardHistory()
{
}

QJsonObject ClipboardHistory::SaveState()
{
	auto state = Component::SaveState();
	QJsonArray itemsJson;

	for (int itemIdx = 0; itemIdx < ui.ClipboardList->count(); ++itemIdx) {
		auto item = ui.ClipboardList->item(itemIdx);

		QJsonObject itemJson;
		itemJson["Title"] = item->text();

		auto dataVar = item->data(Qt::UserRole);

		if (HasText(item)) {
			itemJson["Text"] = dataVar.value<QString>();
		}
		else if (HasImage(item)) {
			auto imageItem = dataVar.value<ImageItem>();
			itemJson["ImgURL"] = imageItem.url.toString();
		}

		itemsJson.append(itemJson);
	}

	state["Items"] = itemsJson;

	return state;
}

void ClipboardHistory::LoadState(const QJsonObject& state)
{
	Component::LoadState(state);

	for (auto itemRef : state["Items"].toArray()) {
		auto itemJson = itemRef.toObject();
		auto item = new QListWidgetItem(itemJson["Title"].toString());

		if (itemJson.contains("Text")) {
			HxUtil::Qt::setUserObject(item, itemJson["Text"].toString());
		}
		else if (itemJson.contains("ImgURL")) {
			ImageItem imageItem{};
			imageItem.url = QUrl(itemJson["ImgURL"].toString());
			HxUtil::Qt::setUserObject(item, imageItem);
		}

		ui.ClipboardList->insertItem(ui.ClipboardList->count(), item);
	}
}

void ClipboardHistory::OnClipboardChanged(QClipboard::Mode mode)
{
	if (mode != QClipboard::Mode::Clipboard) {
		return;
	}

	if (ignoreCopyEvent) {
		ignoreCopyEvent = false;
		return;
	}

	auto clipboard = QGuiApplication::clipboard();
	auto mime = clipboard->mimeData();
	auto dateTime = QDateTime::currentDateTime();

	if (mime->hasText()) {
		auto text = mime->text();

		if (ui.ClipboardList->count() && HasText(ui.ClipboardList->item(0))) {
			if (GetText(ui.ClipboardList->item(0)) == text) {
				return;
			}
		}

		auto textPreview = QString("[%1]\n%2").arg(dateTime.toString()).arg(text.size() < 50 ? text : (text.left(50) + "..."));
		auto item = new QListWidgetItem(textPreview);
		HxUtil::Qt::setUserObject(item, std::move(text));
		ui.ClipboardList->insertItem(0, item);
	}
	else if (mime->hasImage()) {
		auto image = clipboard->pixmap();

		if (ui.ClipboardList->count() && HasImage(ui.ClipboardList->item(0))) {
			auto prevPixmap = GetImage(ui.ClipboardList->item(0));
			if (prevPixmap.toImage() == image.toImage()) {
				return;
			}
		}

		auto imagePath = clipboardPath / (std::to_string(dateTime.toMSecsSinceEpoch()) + ".png");
		auto saved = image.save(QString::fromStdWString(imagePath.wstring()));

		if (!saved) {
			QMessageBox::critical(QApplication::activeWindow(), "Error", "Unable to save image.");
			return;
		}

		auto item = new QListWidgetItem(QString("[%1]\nImage [%2x%3]").arg(dateTime.toString()).arg(image.width()).arg(image.height()));
		ImageItem imgItem{};
		imgItem.url = QUrl::fromLocalFile(QString::fromStdWString(imagePath.wstring()));
		HxUtil::Qt::setUserObject(item, imgItem);
		ui.ClipboardList->insertItem(0, item);
	}

	emit Modified(this);
}

void ClipboardHistory::OnSelectedItemChanged(QListWidgetItem* current, QListWidgetItem*)
{
	ui.Preview->clear();

	if (!current) {
		return;
	}

	auto dataVar = current->data(Qt::UserRole);

	if (HasText(current)) {
		ui.Preview->setText(dataVar.value<QString>());
	}
	else if (HasImage(current)) {
		auto imageItem = dataVar.value<ImageItem>();
		QPixmap pixmap;
		auto loaded = pixmap.load(imageItem.url.toLocalFile());

		if (!loaded) {
			QMessageBox::critical(QApplication::activeWindow(), "Error", "Unable to load image.");
			return;
		}

		QUrl imgUrl("hx-img://0");
		auto doc = ui.Preview->document();
		doc->addResource(QTextDocument::ImageResource, imgUrl, QVariant(pixmap.toImage()));
		QTextCursor cursor = ui.Preview->textCursor();
		QTextImageFormat imageFormat;
		imageFormat.setMaximumWidth(QTextLength(QTextLength::Type::PercentageLength, 100.0f));
		imageFormat.setName(imgUrl.toString());
		cursor.insertImage(imageFormat);
	}
	else {
		assert(false);
	}
}

void ClipboardHistory::OnItemDoubleClick(QListWidgetItem* item)
{
	ignoreCopyEvent = true;

	auto clipboard = QGuiApplication::clipboard();

	auto dataVar = item->data(Qt::UserRole);
	if (HasText(item)) {
		clipboard->setText(dataVar.value<QString>());
	}
	else if (HasImage(item)) {
		auto imageItem = dataVar.value<ImageItem>();
		QPixmap pixmap;
		auto loaded = pixmap.load(imageItem.url.toLocalFile());

		if (!loaded) {
			QMessageBox::critical(QApplication::activeWindow(), "Error", "Unable to load image.");
			return;
		}

		clipboard->setPixmap(pixmap);
	}
	else {
		assert(false);
	}
}

void ClipboardHistory::OnClearHistoryClicked()
{
	for (int itemIdx = 0; itemIdx < ui.ClipboardList->count(); ++itemIdx) {
		auto item = ui.ClipboardList->item(itemIdx);
		auto dataVar = item->data(Qt::UserRole);

		if (HasImage(item)) {
			auto imageItem = dataVar.value<ImageItem>();
			std::filesystem::path imagePath(imageItem.url.toLocalFile().toStdString());
			std::error_code ec;
			std::filesystem::remove(imagePath, ec);
		}
	}

	ui.ClipboardList->clear();
}

bool ClipboardHistory::HasText(QListWidgetItem* item)
{
	auto dataVar = item->data(Qt::UserRole);
	return dataVar.type() == QVariant::Type::String;
}

bool ClipboardHistory::HasImage(QListWidgetItem* item)
{
	auto dataVar = item->data(Qt::UserRole);
	return dataVar.userType() == QMetaType::fromType<ImageItem>().id();
}

QString ClipboardHistory::GetText(QListWidgetItem* item)
{
	auto dataVar = item->data(Qt::UserRole);
	return dataVar.value<QString>();
}

QPixmap ClipboardHistory::GetImage(QListWidgetItem* item)
{
	auto dataVar = item->data(Qt::UserRole);
	auto imageItem = dataVar.value<ImageItem>();

	QPixmap pixmap;
	pixmap.load(imageItem.url.toLocalFile());

	return pixmap;
}
