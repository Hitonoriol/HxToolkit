#include "SymlinkMover.h"
#include "Util/Scope.h"

#include "windows.h"
#include "shellapi.h"
#include <QFileDialog>
#include <QMessageBox>

SymlinkMover::SymlinkMover(QWidget *parent)
	: Component(parent, ToolType::SymlinkMover)
{
	ui.setupUi(this);
	ui.MoveButton->setEnabled(false);
	ui.OpDescription->setVisible(false);

	connect(ui.SrcPathButton, &QPushButton::clicked, this, std::bind(&SymlinkMover::OnChooseFolderClicked, this, FolderType::Source));
	connect(ui.DstPathButton, &QPushButton::clicked, this, std::bind(&SymlinkMover::OnChooseFolderClicked, this, FolderType::Destination));
	connect(ui.MoveButton, &QPushButton::clicked, this, &SymlinkMover::OnMoveClicked);
}

SymlinkMover::~SymlinkMover()
{}

void SymlinkMover::OnChooseFolderClicked(FolderType type)
{
	auto dialogTitle = QString("Choose %1 Folder").arg(type == FolderType::Source ? "Source" : "Destination");
	auto pathStr = QFileDialog::getExistingDirectory(this, dialogTitle, {}, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (pathStr.isNull() || pathStr.isEmpty()) {
		return;
	}

	std::filesystem::path path(pathStr.toStdString());

	QLineEdit* pathField = type == FolderType::Source ? ui.SrcPathField : ui.DstPathField;
	pathField->setText(path.make_preferred().u8string().data());

	auto srcPath = ui.SrcPathField->text();
	auto dstPath = ui.DstPathField->text();

	if (!srcPath.isEmpty() && !dstPath.isEmpty()) {
		ui.MoveButton->setEnabled(true);
		ui.OpDescription->setVisible(true);
		ui.OpDescription->setText(QString(
			"Contents of folder <b>\"%1\"</b> will be moved to <b>\"%2\"</b>.<br>"
			"After moving, the original folder will be deleted and the following symlink will be created: <b>\"%1\"</b> --> <b>\"%2\"</b>."
		).arg(srcPath).arg(dstPath));
	}
	else {
		ui.MoveButton->setEnabled(false);
		ui.OpDescription->setVisible(false);
	}
}

void SymlinkMover::OnMoveClicked()
{
	auto srcPath = ui.SrcPathField->text().toStdWString();
	auto dstPath = ui.DstPathField->text().toStdWString();

	if (!std::filesystem::exists(srcPath)) {
		return;
	}

	if (!std::filesystem::is_directory(dstPath)) {
		return;
	}

	if (!IsEmptyDir(dstPath)) {
		return;
	}

	if (std::filesystem::is_symlink(dstPath)) {
		return;
	}

	if (std::filesystem::is_symlink(srcPath)) {
		return;
	}

	auto srcWildcard = srcPath + L"\\*";

	std::vector<wchar_t> srcPathBuf(srcWildcard.size() + 2); // Double null terminator required
	std::copy(srcWildcard.begin(), srcWildcard.end(), srcPathBuf.begin());
	
	std::vector<wchar_t> dstPathBuf(dstPath.size() + 2); // Double null terminator required
	std::copy(dstPath.begin(), dstPath.end(), dstPathBuf.begin());

	SHFILEOPSTRUCTW moveOp{};
	moveOp.wFunc = FO_MOVE;
	moveOp.pFrom = srcPathBuf.data();
	moveOp.pTo = dstPathBuf.data();

	auto opResult = SHFileOperationW(&moveOp);

	if (opResult) {
		QMessageBox::critical(this, "Error", "File operation failed.");
		return;
	}

	try {
		std::filesystem::remove(srcPath); // If source dir is no longer empty, fail here
		std::filesystem::create_directory_symlink(ui.DstPathField->text().toStdWString(), ui.SrcPathField->text().toStdWString());
		ui.SrcPathField->clear();
		ui.DstPathField->clear();
		ui.MoveButton->setEnabled(false);
		ui.OpDescription->setVisible(false);
	}
	catch (const std::exception& ex) {
		QMessageBox::critical(this, "Error", QString("File operation error: \"%1\".").arg(ex.what()));
	}
}

bool SymlinkMover::IsEmptyDir(const std::filesystem::path& path)
{
	for (auto& item : std::filesystem::directory_iterator(path)) {
		return false;
	}

	return true;
}