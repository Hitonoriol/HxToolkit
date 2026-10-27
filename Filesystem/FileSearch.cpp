#include "FileSearch.h"

#include "JSON/Serialization.h"

#include <QRegularExpression>
#include <QFileDialog>

#include <locale>

// TODO: maybe hide this behind macros for portability? if anybody even cares...
#include <windows.h>
#include <shlobj.h>

FileSearch::FileSearch(QWidget *parent)
	: Component(parent, ToolType::FileSearch)
{
	ui.setupUi(this);
	ui.WalkedFileslabel->setVisible(false);
	ui.StopButton->setVisible(false);
	OnNameSearchSelected();

	connect(this, &FileSearch::FileFound, this, &FileSearch::OnFileFound);
	connect(this, &FileSearch::ContentsFound, this, &FileSearch::OnContentsFound);
	connect(this, &FileSearch::SearchFinished, this, &FileSearch::OnSearchFinished);
	connect(this, &FileSearch::FilesWalked, this, &FileSearch::OnFilesWalked);
}

FileSearch::~FileSearch()
{
	if (searchThread.joinable()) {
		searchThread.join();
	}
}

QJsonObject FileSearch::SaveState()
{
	auto state = Component::SaveState();
	HX_JSON_SCOPE(state,
		HX_SERIALIZE(ui.PathField, text);
	    HX_SERIALIZE(ui.SearchPatternField, text);
	);

	QJsonArray searchResults;
	for (size_t i = 0; i < ui.ResultList->count(); ++i) {
		auto pathItem = ui.ResultList->item(i);
		searchResults.append(pathItem->text());
	}
	state["SearchResults"] = searchResults;

	return state;
}

void FileSearch::LoadState(const QJsonObject& state)
{
	Component::LoadState(state);

	HX_JSON_SCOPE(state,
		HX_DESERIALIZE(ui.PathField, setText, toString);
	    HX_DESERIALIZE(ui.SearchPatternField, setText, toString);
	);

	ui.ResultList->clear();
	auto searchResults = state["SearchResults"].toArray();
	for (auto searchResultJson : searchResults) {
		ui.ResultList->addItem(searchResultJson.toString());
	}
}

void FileSearch::OnBrowsePress()
{
	auto dir = QFileDialog::getExistingDirectory(this, "Select base directory for search");
	if (dir.isEmpty()) {
		return;
	}

	ui.PathField->setText(dir);
	emit Modified(this);
}

void FileSearch::OnSearchPress()
{
	auto baseDirPath = ui.PathField->text();
	auto searchPattern = ui.SearchPatternField->text();

	if (ui.NameRadio->isChecked()) {
		FindFiles(baseDirPath, searchPattern);
	} else if (ui.ContentsRadio->isChecked()) {
		FindContents(baseDirPath, searchPattern);
	}

	emit Modified(this);
}

void FileSearch::OnStopPress()
{
	stop = true;
}

void FileSearch::OnFileFound(std::filesystem::path foundPath)
{
	ui.ResultList->addItem(QString::fromStdString(foundPath.string()));
	emit Modified(this);
}

void FileSearch::OnContentsFound(std::filesystem::path foundPath, size_t line, QString context)
{
	auto newIdx = ui.ResultTable->rowCount();
	ui.ResultTable->insertRow(newIdx);
	ui.ResultTable->setItem(newIdx, 0, new QTableWidgetItem(QString::number(line)));
	ui.ResultTable->setItem(newIdx, 1, new QTableWidgetItem(QString::fromStdString(foundPath.string())));
	ui.ResultTable->setItem(newIdx, 2, new QTableWidgetItem(context));

	for (size_t i = 0; i < 3; ++i) {
		auto item = ui.ResultTable->item(newIdx, i);
		item->setFlags(item->flags() & ~Qt::ItemIsEditable);
	}

	ui.ResultTable->resizeRowToContents(newIdx);
}

void FileSearch::OnSearchFinished()
{
	ui.SearchButton->setEnabled(true);
	ui.WalkedFileslabel->setVisible(false);
	ui.StopButton->setVisible(false);
	stop = false;
}

void FileSearch::OnFilesWalked(size_t count)
{
	ui.WalkedFileslabel->setText(QString("[walked %1 files]").arg(count));
}

void FileSearch::OnNameSearchSelected()
{
	ui.ResultList->setVisible(true);
	ui.ResultTable->setVisible(false);
}

void FileSearch::OnContentsSearchSelected()
{
	ui.ResultList->setVisible(false);
	ui.ResultTable->setVisible(true);
}

void FileSearch::OnSearchResultDoubleClicked(QListWidgetItem* item)
{
	auto pathStr = item->text();
	OpenInExplorer(pathStr);
}

void FileSearch::OnSearchResultDoubleClicked(QTableWidgetItem* item)
{
	auto row = item->row();
	auto pathStr = ui.ResultTable->item(row, 1)->text();
	OpenInExplorer(pathStr);
}

void FileSearch::FindFiles(
	const QString& baseDirPath,
	const QString& searchPattern)
{
	PrepareForSearch();
	searchThread = std::thread([=] {
		std::filesystem::recursive_directory_iterator dirIt(baseDirPath.toStdString());
		auto searchRegexStr = QRegularExpression::wildcardToRegularExpression(
			searchPattern, QRegularExpression::UnanchoredWildcardConversion);
		auto flags = ui.CaseSensitiveBox->isChecked()
			? QRegularExpression::NoPatternOption
			: QRegularExpression::CaseInsensitiveOption;
		QRegularExpression searchRegex(searchRegexStr, flags);
		size_t walkedFiles{};
		for (auto& dirEntry : dirIt) {
			if (stop) {
				break;
			}

			if (dirEntry.is_directory()) {
				continue;
			}

			auto path = dirEntry.path();
			path.make_preferred();
			if (searchRegex.match(path.filename().c_str()).hasMatch()) {
				emit FileFound(path);
			}

			emit FilesWalked(++walkedFiles);
		}

		emit SearchFinished();
	});
}

void FileSearch::FindContents(const QString& baseDirPath, const QString& searchPattern)
{
	PrepareForSearch();
	searchThread = std::thread([=] {
		std::filesystem::recursive_directory_iterator dirIt(baseDirPath.toStdString());
		auto searchRegexStr = QRegularExpression::wildcardToRegularExpression(
			searchPattern, QRegularExpression::UnanchoredWildcardConversion);
		auto flags = ui.CaseSensitiveBox->isChecked()
			? QRegularExpression::NoPatternOption
			: QRegularExpression::CaseInsensitiveOption;
		QRegularExpression searchRegex(searchRegexStr, flags);
		size_t walkedFiles{};
		for (auto& dirEntry : dirIt) {
			if (stop) {
				break;
			}

			if (dirEntry.is_directory()) {
				continue;
			}

			auto path = dirEntry.path();
			path.make_preferred();
			QFile foundFile(path);
			if (!foundFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
				continue;
			}

			std::locale locale;
			QTextStream in(&foundFile);
			size_t lineNumber{};
			while (!in.atEnd()) {
				++lineNumber;
				auto line = in.readLine();
				if (searchRegex.match(line).hasMatch()) {
					static constexpr size_t MaxLength = 100;
					line.truncate(MaxLength);
					auto preview = line.toStdWString();
					auto nonPrintableCharIt = std::find_if(preview.begin(), preview.end(), [&](wchar_t c) {
						if (!std::isspace(c, locale) && !std::isgraph(c, locale))
							return true;
						return false;
					});
					if (nonPrintableCharIt != preview.end()) {
						continue;
					}

					emit ContentsFound(path, lineNumber, line);
				}
			}

			foundFile.close();
			emit FilesWalked(++walkedFiles);
		}

		emit SearchFinished();
	});
}

void FileSearch::PrepareForSearch()
{
	if (searchThread.joinable()) {
		searchThread.join();
	}

	ui.SearchButton->setEnabled(false);
	ui.WalkedFileslabel->setVisible(true);
	ui.StopButton->setVisible(true);

	ui.ResultList->clear();
	ui.ResultTable->setRowCount(0);
}

void FileSearch::OpenInExplorer(const QString& pathStr)
{
	// TODO: ...and this too, obviously
	if (auto idList = ILCreateFromPath(pathStr.toStdWString().data())) {
		SHOpenFolderAndSelectItems(idList, 0, nullptr, 0);
		ILFree(idList);
	}
}
