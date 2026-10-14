#include "ToolConfigurationParameter.h"

#include <utility>

ToolConfigurationParameter::ToolConfigurationParameter(const QString& name, Editor editor, QObject* parent)
	: QObject(parent)
	, name(name)
	, editor(std::move(editor))
{
}

QString ToolConfigurationParameter::GetName() const
{
	return name;
}

void ToolConfigurationParameter::Edit(QWidget* parent)
{
	if (editor && editor(parent)) {
		emit Changed();
	}
}
