#pragma once

#include <QWidget>
#include "ui_SymlinkMover.h"

#include "UI/Component.h"

#include <filesystem>

class SymlinkMover : public Component
{
	Q_OBJECT

public:
	SymlinkMover(QWidget *parent = nullptr);
	~SymlinkMover();

	enum class FolderType {
		Source, Destination
	};

private slots:
	void OnChooseFolderClicked(FolderType type);
	void OnMoveClicked();

private:
	static bool IsEmptyDir(const std::filesystem::path& path);

	Ui::SymlinkMoverClass ui;

	std::filesystem::path srcPath;
	std::filesystem::path dstPath;
};

