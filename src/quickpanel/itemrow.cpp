// SPDX-License-Identifier: GPL-3.0-or-later
#include "itemrow.h"

#include <DFontSizeManager>
#include <DGuiApplicationHelper>
#include <DLabel>
#include <DPaletteHelper>

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>

DGUI_USE_NAMESPACE
DWIDGET_USE_NAMESPACE

namespace AginoGpu {

namespace {
constexpr int kRowHeight = 36;
constexpr int kRowRadius = 8;
constexpr int kRowPadding = 10;
constexpr int kIconSize = 16;
constexpr qreal kRestBackgroundAlpha = 0.05;
constexpr qreal kSecondaryTextAlpha = 0.6;
constexpr qreal kSectionTextAlpha = 0.6;

void setTextColor(QWidget *widget, const QColor &color)
{
    QPalette palette = widget->palette();
    palette.setColor(QPalette::WindowText, color);
    widget->setPalette(palette);
}

QColor restBackground(const QPalette &palette)
{
    QColor color = palette.brightText().color();
    color.setAlphaF(kRestBackgroundAlpha);
    return color;
}

QColor secondaryText(const QPalette &palette)
{
    QColor color = palette.brightText().color();
    color.setAlphaF(kSecondaryTextAlpha);
    return color;
}

void paintRounded(QWidget *widget, const QColor &color)
{
    QPainter painter(widget);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawRoundedRect(widget->rect(), kRowRadius, kRowRadius);
}

void updateSectionColor(DLabel *label)
{
    QColor color = DGuiApplicationHelper::instance()->themeType() == DGuiApplicationHelper::LightType
        ? QColor(Qt::black)
        : QColor(Qt::white);
    color.setAlphaF(kSectionTextAlpha);
    DPalette palette = DPaletteHelper::instance()->palette(label);
    palette.setColor(QPalette::WindowText, color);
    DPaletteHelper::instance()->setPalette(label, palette);
}
} // namespace

ItemIcon::ItemIcon(const QIcon &icon, QWidget *parent)
    : QWidget(parent)
    , m_icon(icon)
{
    setFixedSize(kIconSize, kIconSize);
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void ItemIcon::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    m_icon.paint(&painter, rect());
}

ItemRow::ItemRow(QWidget *parent)
    : QFrame(parent)
    , m_layout(new QHBoxLayout(this))
{
    setFixedHeight(kRowHeight);
    setForegroundRole(QPalette::BrightText);
    m_layout->setContentsMargins(kRowPadding, 0, kRowPadding, 0);
    m_layout->setSpacing(kRowPadding);
}

DLabel *ItemRow::addLabel(const QString &text, Text kind)
{
    auto *label = new DLabel(text, this);
    label->setElideMode(Qt::ElideRight);
    label->setAttribute(Qt::WA_TransparentForMouseEvents);
    DFontSizeManager::instance()->bind(label, kind == Text::Primary ? DFontSizeManager::T6 : DFontSizeManager::T8);
    (kind == Text::Primary ? m_primary : m_secondary) << label;
    m_layout->addWidget(label, kind == Text::Primary ? 1 : 0);
    applyColors();
    return label;
}

void ItemRow::addIcon(const QIcon &icon)
{
    auto *widget = new ItemIcon(icon, this);
    m_icons << widget;
    m_layout->addWidget(widget);
    applyColors();
}

void ItemRow::addStretch()
{
    m_layout->addStretch(1);
}

bool ItemRow::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::Enter:
        setHovered(true);
        break;
    case QEvent::Leave:
    case QEvent::Hide:
        setHovered(false);
        break;
    case QEvent::PaletteChange:
        applyColors();
        break;
    default:
        break;
    }
    return QFrame::event(event);
}

void ItemRow::setHovered(bool hovered)
{
    if (hovered == m_hovered)
        return;
    m_hovered = hovered;
    applyColors();
    update();
}

// The children take their colours from the row's state; set them explicitly (as
// PluginItemWidget and JumpSettingButton do) so they follow hover and theme changes.
void ItemRow::applyColors()
{
    const QPalette &palette = this->palette();
    const QColor text = m_hovered ? palette.highlightedText().color() : palette.brightText().color();
    const QColor secondary = m_hovered ? text : secondaryText(palette);
    for (DLabel *label : std::as_const(m_primary))
        setTextColor(label, text);
    for (DLabel *label : std::as_const(m_secondary))
        setTextColor(label, secondary);
    for (ItemIcon *icon : std::as_const(m_icons))
        setTextColor(icon, text);
}

void ItemRow::paintEvent(QPaintEvent *)
{
    paintRounded(this, m_hovered ? palette().color(QPalette::Active, QPalette::Highlight)
                                 : restBackground(palette()));
}

void ItemRow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
        Q_EMIT clicked();
        return;
    }
    QFrame::mouseReleaseEvent(event);
}

InfoCard::InfoCard(QWidget *parent)
    : QFrame(parent)
{
    setForegroundRole(QPalette::BrightText);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
}

void InfoCard::setLines(const QList<Line> &lines)
{
    if (lines == m_lines && !m_titles.isEmpty())
        return;
    m_lines = lines;
    while (QLayoutItem *item = layout()->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    m_titles.clear();
    m_values.clear();
    for (const Line &line : lines) {
        auto *row = new QWidget(this);
        row->setFixedHeight(kRowHeight);
        auto *title = new DLabel(line.title, row);
        title->setElideMode(Qt::ElideRight);
        DFontSizeManager::instance()->bind(title, DFontSizeManager::T6);
        auto *value = new DLabel(line.value, row);
        DFontSizeManager::instance()->bind(value, DFontSizeManager::T8);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(kRowPadding, 0, kRowPadding, 0);
        rowLayout->setSpacing(kRowPadding);
        rowLayout->addWidget(title, 1);
        rowLayout->addWidget(value);
        layout()->addWidget(row);
        m_titles << title;
        m_values << value;
    }
    applyColors();
    setVisible(!lines.isEmpty());
}

bool InfoCard::event(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange)
        applyColors();
    return QFrame::event(event);
}

void InfoCard::applyColors()
{
    for (DLabel *label : std::as_const(m_titles))
        setTextColor(label, palette().brightText().color());
    for (DLabel *label : std::as_const(m_values))
        setTextColor(label, secondaryText(palette()));
}

void InfoCard::paintEvent(QPaintEvent *)
{
    paintRounded(this, restBackground(palette()));
}

DLabel *createSectionLabel(const QString &text, QWidget *parent)
{
    auto *label = new DLabel(text, parent);
    label->setElideMode(Qt::ElideRight);
    DFontSizeManager::instance()->bind(label, DFontSizeManager::T10);
    updateSectionColor(label);
    QObject::connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged, label,
                     [label] { updateSectionColor(label); });
    return label;
}

} // namespace AginoGpu
