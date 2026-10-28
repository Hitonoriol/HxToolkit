#include "TabPinCommand.h"

#include "UI/DynamicTabWidget.h"

TabPinCommand::TabPinCommand(DynamicTabWidget* tabs, QWidget* page, bool pinned)
	: QUndoCommand(pinned ? "Pin tab" : "Unpin tab")
	, tabs(tabs)
	, page(page)
	, pinned(pinned)
{
}

void TabPinCommand::undo()
{
	SetPinned(!pinned);
}

void TabPinCommand::redo()
{
	SetPinned(pinned);
}

void TabPinCommand::SetPinned(bool pinned)
{
	if (!tabs || !page) {
		return;
	}

	auto tabIdx = tabs->GetIndexOf(page);
	if (tabIdx >= 0) {
		tabs->SetTabPinned(tabIdx, pinned);
	}
}
