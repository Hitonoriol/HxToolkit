#include "MarkdownEditor.h"

#include <3rdParty/maddy/parser.h>
#include <sstream>

MarkdownEditor::MarkdownEditor(QWidget *parent)
	: Component(parent, ToolType::MarkdownEditor)
	, markdownParser(std::make_unique<maddy::Parser>())
{
	ui.setupUi(this);
	connect(ui.EditorField, &QPlainTextEdit::textChanged, this, &MarkdownEditor::onEditorTextChanged);
}

MarkdownEditor::~MarkdownEditor()
{}

QJsonObject MarkdownEditor::SaveState()
{
	auto state = Component::SaveState();
	state["Text"] = ui.EditorField->toPlainText();
	return state;
}

void MarkdownEditor::LoadState(const QJsonObject& state)
{
	Component::LoadState(state);

	ui.EditorField->setPlainText(state["Text"].toString());
}

void MarkdownEditor::onEditorTextChanged()
{
	std::stringstream markdown(ui.EditorField->toPlainText().toStdString());
	auto html = markdownParser->Parse(markdown);
	ui.MarkdownField->setHtml(html.data());
	emit Modified(this);
}
