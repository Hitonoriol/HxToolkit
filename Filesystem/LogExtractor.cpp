#include "LogExtractor.h"

#include "JSON/Serialization.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSaveFile>

#include <memory>

LogExtractor::LogExtractor(QWidget* parent)
	: Component(parent, ToolType::LogExtractor)
{
	ui.setupUi(this);
	ui.SaveButton->setEnabled(false);
	ui.CancelButton->setVisible(false);
	ui.ProgressBar->setVisible(false);

	connect(ui.BrowseButton, &QPushButton::clicked, this, &LogExtractor::OnBrowsePress);
	connect(ui.ExtractButton, &QPushButton::clicked, this, &LogExtractor::OnExtractPress);
	connect(ui.SaveButton, &QPushButton::clicked, this, &LogExtractor::OnSavePress);
	connect(ui.CancelButton, &QPushButton::clicked, this, &LogExtractor::OnCancelPress);
	connect(ui.RegexField, &QLineEdit::returnPressed, this, &LogExtractor::OnExtractPress);
	connect(ui.PathField, &QLineEdit::textChanged, this, &LogExtractor::OnInputChanged);
	connect(ui.RegexField, &QLineEdit::textChanged, this, &LogExtractor::OnInputChanged);
	connect(this, &LogExtractor::OperationFinished, this, &LogExtractor::OnOperationFinished);
}

LogExtractor::~LogExtractor()
{
	worker.request_stop();
}

QJsonObject LogExtractor::SaveState()
{
	auto state = Component::SaveState();
	HX_JSON_SCOPE(state,
		HX_SERIALIZE(ui.PathField, text);
		HX_SERIALIZE(ui.RegexField, text);
	);

	return state;
}

void LogExtractor::LoadState(const QJsonObject& state)
{
	Component::LoadState(state);

	HX_JSON_SCOPE(state,
		HX_DESERIALIZE(ui.PathField, setText, toString);
		HX_DESERIALIZE(ui.RegexField, setText, toString);
	);
}

void LogExtractor::OnBrowsePress()
{
	auto filePath = QFileDialog::getOpenFileName(this, "Select log file", ui.PathField->text(), "All files (*)");
	if (filePath.isEmpty()) {
		return;
	}

	ui.PathField->setText(filePath);
}

void LogExtractor::OnExtractPress()
{
	QString filePath;
	QString pattern;
	if (!ValidateInput(filePath, pattern)) {
		return;
	}

	ui.PreviewField->clear();
	ui.StatusLabel->setText("Extracting...");
	StartExtraction(filePath, pattern, {});
}

void LogExtractor::OnSavePress()
{
	auto sourceInfo = QFileInfo(extractedPath);
	auto suffix = sourceInfo.completeSuffix();
	auto suggestedName = sourceInfo.completeBaseName() + "_extracted";
	if (!suffix.isEmpty()) {
		suggestedName += "." + suffix;
	}

	auto suggestedPath = sourceInfo.absoluteDir().filePath(suggestedName);
	auto savePath = QFileDialog::getSaveFileName(this, "Save extracted lines", suggestedPath, "All files (*)");
	if (savePath.isEmpty()) {
		return;
	}

	if (QFileInfo(savePath).absoluteFilePath().compare(sourceInfo.absoluteFilePath(), Qt::CaseInsensitive) == 0) {
		QMessageBox::warning(this, "Unable to save", "Choose a file other than the source log.");
		return;
	}

	ui.StatusLabel->setText("Saving extracted lines...");
	StartExtraction(extractedPath, extractedPattern, savePath);
}

void LogExtractor::OnCancelPress()
{
	worker.request_stop();
	ui.StatusLabel->setText("Cancelling...");
}

void LogExtractor::OnInputChanged()
{
	extractedPath.clear();
	extractedPattern.clear();
	ui.SaveButton->setEnabled(false);
	emit Modified(this);
}

void LogExtractor::OnOperationFinished(QString preview, quint64 matchCount, bool previewTruncated,
	QString savePath, QString error, bool cancelled)
{
	SetOperationRunning(false);

	if (cancelled) {
		ui.StatusLabel->setText("Operation cancelled.");
		return;
	}

	if (!error.isEmpty()) {
		ui.StatusLabel->setText(error);
		QMessageBox::critical(this, "Log Extractor", error);
		return;
	}

	if (!savePath.isEmpty()) {
		ui.StatusLabel->setText(QString("Saved %1 matching lines to %2").arg(matchCount).arg(savePath));
		return;
	}

	ui.PreviewField->setPlainText(preview);
	extractedPath = QFileInfo(ui.PathField->text()).absoluteFilePath();
	extractedPattern = ui.RegexField->text();
	ui.SaveButton->setEnabled(true);
	auto status = QString("%1 matching lines.").arg(matchCount);
	if (previewTruncated) {
		status += " Preview limited to the first 2,500,000 characters.";
	}

	ui.StatusLabel->setText(status);
}

bool LogExtractor::ValidateInput(QString& filePath, QString& pattern)
{
	filePath = QFileInfo(ui.PathField->text()).absoluteFilePath();
	pattern = ui.RegexField->text();

	auto fileInfo = QFileInfo(filePath);
	if (!fileInfo.isFile()) {
		QMessageBox::warning(this, "Invalid file", "Select an existing file to extract lines from.");
		return false;
	}

	QRegularExpression regex(pattern);
	if (!regex.isValid()) {
		QMessageBox::warning(this, "Invalid regular expression", regex.errorString());
		return false;
	}

	return true;
}

void LogExtractor::StartExtraction(const QString& filePath, const QString& pattern, const QString& savePath)
{
	if (worker.joinable()) {
		worker.join();
	}

	SetOperationRunning(true);
	worker = std::jthread([this, filePath, pattern, savePath](std::stop_token stopToken) {
		ExtractFile(filePath, pattern, savePath, stopToken);
	});
}

void LogExtractor::ExtractFile(const QString& filePath, const QString& pattern, const QString& savePath,
	std::stop_token stopToken)
{
	QFile input(filePath);
	if (!input.open(QIODevice::ReadOnly)) {
		emit OperationFinished({}, 0, false, savePath, "The source file could not be opened.", false);
		return;
	}

	std::unique_ptr<QSaveFile> output;
	if (!savePath.isEmpty()) {
		output = std::make_unique<QSaveFile>(savePath);
		if (!output->open(QIODevice::WriteOnly)) {
			emit OperationFinished({}, 0, false, savePath, "The destination file could not be opened.", false);
			return;
		}
	}

	QRegularExpression regex(pattern);
	QString preview;
	if (savePath.isEmpty()) {
		preview.reserve(MaxPreviewCharacters);
	}

	quint64 matchCount = 0;
	bool previewTruncated = false;
	while (!input.atEnd() && !stopToken.stop_requested()) {
		auto line = input.readLine();
		auto decodedLine = QString::fromUtf8(line);
		if (!regex.match(decodedLine).hasMatch()) {
			continue;
		}

		++matchCount;
		if (output) {
			if (output->write(line) != line.size()) {
				output->cancelWriting();
				emit OperationFinished({}, 0, false, savePath, "The extracted file could not be written.", false);
				return;
			}
		} else if (preview.size() < MaxPreviewCharacters) {
			auto remainingCharacters = MaxPreviewCharacters - preview.size();
			preview += decodedLine.left(remainingCharacters);
			previewTruncated = decodedLine.size() > remainingCharacters;
		} else {
			previewTruncated = true;
		}
	}

	if (stopToken.stop_requested()) {
		if (output) {
			output->cancelWriting();
		}

		emit OperationFinished({}, 0, false, savePath, {}, true);
		return;
	}

	if (input.error() != QFileDevice::NoError) {
		if (output) {
			output->cancelWriting();
		}

		emit OperationFinished({}, 0, false, savePath, "The source file could not be fully read.", false);
		return;
	}

	if (output && !output->commit()) {
		emit OperationFinished({}, 0, false, savePath, "The extracted file could not be finalized.", false);
		return;
	}

	emit OperationFinished(preview, matchCount, previewTruncated, savePath, {}, false);
}

void LogExtractor::SetOperationRunning(bool running)
{
	ui.PathField->setEnabled(!running);
	ui.BrowseButton->setEnabled(!running);
	ui.RegexField->setEnabled(!running);
	ui.ExtractButton->setEnabled(!running);
	ui.SaveButton->setEnabled(!running && !extractedPath.isEmpty());
	ui.CancelButton->setVisible(running);
	ui.ProgressBar->setVisible(running);
}
