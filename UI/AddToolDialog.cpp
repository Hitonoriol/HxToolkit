#include "AddToolDialog.h"

#include <oclero/qlementine/widgets/Expander.hpp>

#include <QFrame>
#include <QLabel>
#include <QButtonGroup>
#include <QMap>
#include <QPushButton>
#include <QPainter>
#include <QShowEvent>
#include <QStyle>
#include <QStyleOptionButton>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {
class ToolSelectionButton final : public QPushButton
{
public:
	using QPushButton::QPushButton;

protected:
	void paintEvent(QPaintEvent*) override
	{
		QStyleOptionButton option;
		initStyleOption(&option);
		option.text.clear();
		option.icon = {};
		option.iconSize = {};

		QPainter painter(this);
		style()->drawControl(QStyle::CE_PushButton, &option, &painter, this);

		auto content = style()->subElementRect(QStyle::SE_PushButtonContents, &option, this);
		constexpr int padding = 8;
		int x = content.left() + padding;
		if (!icon().isNull()) {
			const auto iconRect = QRect(x, content.center().y() - iconSize().height() / 2, iconSize().width(), iconSize().height());
			icon().paint(&painter, iconRect);
			x = iconRect.right() + padding;
		}

		painter.setPen(Qt::white);
		painter.drawText(QRect(x, content.top(), content.right() - x, content.height()), Qt::AlignLeft | Qt::AlignVCenter, text());
	}
};
}

AddToolDialog::AddToolDialog(const QList<ToolInfo>& tools, bool verticalSplit, QWidget* parent)
	: QDialog(parent), tools(tools)
{
	ui.setupUi(this);
	auto* splitButtonGroup = new QButtonGroup(this);
	splitButtonGroup->addButton(ui.HorizontalSplitBtn);
	splitButtonGroup->addButton(ui.VerticalSplitBtn);
	ui.VerticalSplitBtn->setChecked(verticalSplit);
	ui.HorizontalSplitBtn->setChecked(!verticalSplit);
	connect(ui.SearchField, &QLineEdit::textChanged, this, &AddToolDialog::UpdateResults);
	UpdateResults({});
}

void AddToolDialog::showEvent(QShowEvent* event)
{
	QDialog::showEvent(event);
	QTimer::singleShot(0, ui.SearchField, [this] { ui.SearchField->setFocus(); });
}

void AddToolDialog::UpdateResults(const QString& query)
{
	while (auto* item = ui.ResultsLayout->takeAt(0)) {
		delete item->widget();
		delete item;
	}

	if (query.isEmpty()) {
		QMap<QString, QList<ToolInfo>> categories;
		for (const auto& tool : tools) {
			categories[tool.CategoryName].append(tool);
		}

		for (auto category = categories.cbegin(); category != categories.cend(); ++category) {
			auto* section = new QFrame;
			section->setFrameShape(QFrame::StyledPanel);
			auto* sectionLayout = new QVBoxLayout(section);
			sectionLayout->setContentsMargins(0, 0, 0, 0);
			sectionLayout->setSpacing(4);

			auto* headerContainer = new QWidget(section);
			auto* headerLayout = new QHBoxLayout(headerContainer);
			headerLayout->setContentsMargins(4, 0, 4, 0);
			headerLayout->setSpacing(0);
			auto* leftPadding = new QWidget(headerContainer);
			leftPadding->setFixedWidth(28);
			auto* header = new QToolButton(headerContainer);
			header->setText(QString("%1  ·  %2 tools").arg(category.key()).arg(category.value().size()));
			header->setToolButtonStyle(Qt::ToolButtonTextOnly);
			header->setAutoRaise(true);
			header->setMinimumHeight(36);
			auto headerFont = header->font();
			headerFont.setBold(true);
			header->setFont(headerFont);
			auto* chevron = new QToolButton(headerContainer);
			chevron->setIcon(QIcon(":/icons/chevron-down.svg"));
			chevron->setIconSize({16, 16});
			chevron->setAutoRaise(true);
			chevron->setCheckable(true);
			chevron->setChecked(true);
			chevron->setFixedSize(28, 28);
			headerLayout->addWidget(leftPadding);
			headerLayout->addWidget(header, 1);
			headerLayout->addWidget(chevron);
			sectionLayout->addWidget(headerContainer);

			auto* toolsWidget = new QWidget;
			auto* toolsLayout = new QVBoxLayout(toolsWidget);
			toolsLayout->setContentsMargins(6, 0, 6, 6);
			toolsLayout->setSpacing(2);
			for (const auto& tool : category.value()) {
				AddToolButton(toolsLayout, tool);
			}

			auto* expander = new oclero::qlementine::Expander(section);
			expander->setContent(toolsWidget);
			expander->setExpanded(true);
			connect(chevron, &QToolButton::toggled, expander, [chevron, expander](bool expanded) {
				chevron->setIcon(QIcon(expanded ? ":/icons/chevron-down.svg" : ":/icons/chevron-right.svg"));
				expander->setExpanded(expanded);
			});
			connect(header, &QToolButton::clicked, chevron, [chevron] { chevron->setChecked(!chevron->isChecked()); });
			sectionLayout->addWidget(expander);
			ui.ResultsLayout->addWidget(section);
		}
	} else {
		auto* resultsLabel = new QLabel("Search results");
		auto resultsFont = resultsLabel->font();
		resultsFont.setBold(true);
		resultsLabel->setFont(resultsFont);
		ui.ResultsLayout->addWidget(resultsLabel);

		bool found = false;
		for (const auto& tool : tools) {
			if (MatchesFuzzy(tool.ToolName, query)) {
				AddToolButton(ui.ResultsLayout, tool, true);
				found = true;
			}
		}
		if (!found) {
			auto* emptyLabel = new QLabel("No tools found.");
			emptyLabel->setAlignment(Qt::AlignCenter);
			ui.ResultsLayout->addWidget(emptyLabel);
		}
	}
	ui.ResultsLayout->addStretch();
}

void AddToolDialog::AddToolButton(QVBoxLayout* layout, const ToolInfo& tool, bool showCategory)
{
	auto* button = new ToolSelectionButton(showCategory ? QString("%1  ·  %2").arg(tool.ToolName, tool.CategoryName) : tool.ToolName, this);
	button->setIcon(ToolIcon(tool.Type));
	button->setIconSize({18, 18});
	button->setAutoDefault(false);
	button->setDefault(false);
	button->setMinimumHeight(30);
	button->setMaximumHeight(34);
	button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	connect(button, &QPushButton::clicked, this, [this, tool] {
		emit ToolSelected(tool.Type, ui.VerticalSplitBtn->isChecked());
		accept();
	});
	layout->addWidget(button);
}

QIcon AddToolDialog::ToolIcon(ToolType toolType) const
{
	switch (toolType) {
	case ToolType::BaseConverter:
	case ToolType::Calculator: return QIcon(":/icons/calculator.svg");
	case ToolType::ColorPicker: return QIcon(":/icons/pipette.svg");
	case ToolType::MarkdownEditor: return QIcon(":/icons/text.svg");
	case ToolType::Checklist:
	case ToolType::TaskTracker: return QIcon(":/icons/checklist.svg");
	case ToolType::GitlabTasks:
	case ToolType::GitlabMergeRequests:
	case ToolType::GitlabTimeStats: return QIcon(":/icons/gitlab.svg");
	case ToolType::Stopwatch:
	case ToolType::Timer:
	case ToolType::DateCountdown: return QIcon(":/icons/clock.svg");
	case ToolType::RandomNumber:
	case ToolType::RandomString: return QIcon(":/icons/random.svg");
	case ToolType::FileSearch: return QIcon(":/icons/folder-search.svg");
	case ToolType::SymlinkMover: return QIcon(":/icons/link.svg");
	case ToolType::Ping:
	case ToolType::NetworkInterfaces: return QIcon(":/icons/network.svg");
	case ToolType::MeltingScreen: return QIcon(":/icons/melting-screen.svg");
	case ToolType::RamMonitor: return QIcon(":/icons/memory.svg");
	case ToolType::ClipboardManager: return QIcon(":/icons/clipboard.svg");
	case ToolType::SystemShortcuts: return QIcon(":/icons/keyboard.svg");
	default: return {};
	}
}

bool AddToolDialog::MatchesFuzzy(const QString& name, const QString& query) const
{
	const auto normalizedName = name.toCaseFolded();
	const auto normalizedQuery = query.trimmed().toCaseFolded();
	if (normalizedQuery.isEmpty() || normalizedName.contains(normalizedQuery)) {
		return true;
	}

	int nameIndex = 0;
	for (const auto character : normalizedQuery) {
		nameIndex = normalizedName.indexOf(character, nameIndex);
		if (nameIndex < 0) {
			return false;
		}
		++nameIndex;
	}
	return true;
}
