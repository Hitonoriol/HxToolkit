#include "ToolConfigurationParameter.h"

#include <utility>

ToolConfigurationParameter::ToolConfigurationParameter(const QString& name, Editor editor, QObject* parent, Visibility visibility)
	: QObject(parent)
	, name(name)
	, editor(std::move(editor))
	, visibility(std::move(visibility))
{
}

QString ToolConfigurationParameter::GetName() const
{
	return name;
}

bool ToolConfigurationParameter::IsVisible() const
{
	return !visibility || visibility();
}

void ToolConfigurationParameter::Edit(QWidget* parent)
{
	if (editor && editor(parent)) {
		emit Changed();
	}
}
