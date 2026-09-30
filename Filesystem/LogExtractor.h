#pragma once

#include "UI/Component.h"
#include "ui_LogExtractor.h"

#include <stop_token>
#include <thread>

class LogExtractor : public Component
{
	Q_OBJECT

public:
	LogExtractor(QWidget* parent = nullptr);
	~LogExtractor();

	virtual QJsonObject SaveState() override;
	virtual void LoadState(const QJsonObject& state) override;

signals:
	void OperationFinished(QString preview, quint64 matchCount, bool previewTruncated,
		QString savePath, QString error, bool cancelled);

private slots:
	void OnBrowsePress();
	void OnExtractPress();
	void OnSavePress();
	void OnCancelPress();
	void OnInputChanged();
	void OnOperationFinished(QString preview, quint64 matchCount, bool previewTruncated,
		QString savePath, QString error, bool cancelled);

private:
	static constexpr qsizetype MaxPreviewCharacters = 2'500'000;

	bool ValidateInput(QString& filePath, QString& pattern);
	void StartExtraction(const QString& filePath, const QString& pattern, const QString& savePath);
	void ExtractFile(const QString& filePath, const QString& pattern, const QString& savePath,
		std::stop_token stopToken);
	void SetOperationRunning(bool running);

	Ui::LogExtractorClass ui;
	std::jthread worker;
	QString extractedPath;
	QString extractedPattern;
};
