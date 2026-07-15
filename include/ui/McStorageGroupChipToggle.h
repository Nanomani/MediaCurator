#pragma once
#include <QWidget>

namespace Mc {

// Independently-toggleable storage-group chip — same McCardDelegate::drawGroupChip
// visual as the card badge and McManageFoldersDialog's group picker, so filter-bar
// chips read as the same design language. Unlike the folder-assignment picker
// (one-of-many), each chip here toggles its own group's visibility independently.
class McStorageGroupChipToggle final : public QWidget
{
	Q_OBJECT
public:
	explicit McStorageGroupChipToggle(int group, QWidget* parent = nullptr);

	int  group() const { return m_group; }
	bool isChecked() const { return m_checked; }
	void setChecked(bool on);

signals:
	void toggled(bool checked);

protected:
	void paintEvent(QPaintEvent*) override;
	void mousePressEvent(QMouseEvent* event) override;
	void enterEvent(QEnterEvent* event) override;
	void leaveEvent(QEvent* event) override;
	QSize sizeHint() const override;

private:
	static constexpr int kChipH = 18; // matches McCardDelegate::kBadgeH
	int  m_group;
	bool m_checked = true;
	bool m_hovered = false;
};

} // namespace Mc
