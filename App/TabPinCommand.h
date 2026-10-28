#pragma once

#include <QPointer>
#include <QUndoCommand>

class DynamicTabWidget;
class QWidget;

class TabPinCommand : public QUndoCommand
{
public:
	TabPinCommand(DynamicTabWidget* tabs, QWidget* page, bool pinned);

	void undo() override;
	void redo() override;

private:
	void SetPinned(bool pinned);

	QPointer<DynamicTabWidget> tabs;
	QPointer<QWidget> page;
	bool pinned;
};
