#pragma once

#include <QObject>
#include <QString>

#include <functional>

class QWidget;

class ToolConfigurationParameter : public QObject
{
	Q_OBJECT

public:
	using Editor = std::function<bool(QWidget* parent)>;

	ToolConfigurationParameter(const QString& name, Editor editor, QObject* parent = nullptr);

	QString GetName() const;
	void Edit(QWidget* parent = nullptr);

signals:
	void Changed();

private:
	QString name;
	Editor editor;
};
