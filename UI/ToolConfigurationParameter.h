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
	using Visibility = std::function<bool()>;

	ToolConfigurationParameter(const QString& name, Editor editor, QObject* parent = nullptr, Visibility visibility = {});

	QString GetName() const;
	bool IsVisible() const;
	void Edit(QWidget* parent = nullptr);

signals:
	void Changed();

private:
	QString name;
	Editor editor;
	Visibility visibility;
};
