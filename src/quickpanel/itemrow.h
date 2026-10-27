// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFrame>
#include <QIcon>
#include <QList>

class QHBoxLayout;

namespace Dtk::Widget {
class DLabel;
}

namespace AginoGpu {

// A 16 px icon painted in the widget's WindowText colour (DTK's built-in "texts" icons such as
// plugin_item_select follow it), like dde-tray-loader's CommonIconButton.
class ItemIcon : public QWidget
{
    Q_OBJECT
public:
    explicit ItemIcon(const QIcon &icon, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QIcon m_icon;
};

// One row of a quick-panel sub-page, drawn like the Bluetooth and Wi-Fi rows of dde-tray-loader
// (PluginItemDelegate, JumpSettingButton): 36 px high, radius 8, the text colour at 5 % behind
// it, the accent colour with highlighted text while hovered. No animation (upstream has none).
class ItemRow : public QFrame
{
    Q_OBJECT
public:
    enum class Text { Primary, Secondary };

    explicit ItemRow(QWidget *parent = nullptr);

    Dtk::Widget::DLabel *addLabel(const QString &text, Text kind);
    void addIcon(const QIcon &icon);
    void addStretch();

Q_SIGNALS:
    void clicked();

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void setHovered(bool hovered);
    void applyColors();

    QHBoxLayout *m_layout;
    QList<Dtk::Widget::DLabel *> m_primary;
    QList<Dtk::Widget::DLabel *> m_secondary;
    QList<ItemIcon *> m_icons;
    bool m_hovered = false;
};

// A section caption (T10, black or white at 60 % by theme), like Bluetooth's "My Devices".
Dtk::Widget::DLabel *createSectionLabel(const QString &text, QWidget *parent);

} // namespace AginoGpu
