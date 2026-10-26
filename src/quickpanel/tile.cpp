// SPDX-License-Identifier: GPL-3.0-or-later
#include "tile.h"

#include "agino-gpu-client/texts.h"

#include <DFloatingButton>
#include <DFontSizeManager>
#include <DIconButton>
#include <DLabel>
#include <DStyle>
#include <DToolTip>

#include <QHBoxLayout>
#include <QIcon>
#include <QMouseEvent>
#include <QVBoxLayout>

DWIDGET_USE_NAMESPACE

namespace AginoGpu {

namespace {

QIcon modeIcon(Mode mode)
{
    switch (mode) {
    case Mode::Integrated:
        return QIcon::fromTheme(QStringLiteral("cpu"), QIcon::fromTheme(QStringLiteral("computer")));
    case Mode::Dedicated:
    case Mode::MuxDedicated:
        return QIcon::fromTheme(QStringLiteral("video-card"), QIcon::fromTheme(QStringLiteral("computer")));
    case Mode::Hybrid:
        break;
    }
    return QIcon::fromTheme(QStringLiteral("video-display"), QIcon::fromTheme(QStringLiteral("computer")));
}

void prepareLabel(DLabel *label, DFontSizeManager::SizeType size)
{
    DFontSizeManager::instance()->bind(label, size);
    DToolTip::setToolTipShowMode(label, DToolTip::ShowWhenElided);
    label->setElideMode(Qt::ElideRight);
}

} // namespace

Tile::Tile(GpuState *state, QWidget *parent)
    : QWidget(parent)
    , m_state(state)
    , m_icon(new DFloatingButton(this))
    , m_title(new DLabel(this))
    , m_subtitle(new DLabel(this))
{
    prepareLabel(m_title, DFontSizeManager::T6);
    m_title->setContentsMargins(0, 2, 0, 0);
    m_title->setForegroundRole(QPalette::BrightText);
    prepareLabel(m_subtitle, DFontSizeManager::T10);

    auto *labels = new QWidget(this);
    auto *labelLayout = new QVBoxLayout(labels);
    labelLayout->setContentsMargins(0, 8, 8, 8);
    labelLayout->setSpacing(0);
    labelLayout->addWidget(m_title);
    labelLayout->addWidget(m_subtitle);

    m_icon->setEnabledCircle(true);
    m_icon->setIconSize(QSize(22, 22));
    m_icon->setFixedSize(40, 40);
    m_icon->setFocusPolicy(Qt::NoFocus);
    m_icon->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto *enter = new DIconButton(this);
    enter->setIcon(DStyle::standardIcon(style(), DStyle::SP_ArrowEnter));
    enter->setFlat(true);
    enter->setFocusPolicy(Qt::NoFocus);
    enter->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 10, 0);
    layout->setSpacing(0);
    layout->addWidget(m_icon);
    layout->addSpacing(10);
    layout->addWidget(labels);
    layout->addStretch(1);
    layout->addWidget(enter);
    setFixedSize(150, 60);

    refresh();
}

void Tile::refresh()
{
    m_title->setText(Texts::title());
    m_icon->setIcon(modeIcon(m_state->configuredMode()));

    const PendingAction pending = m_state->pendingAction();
    QPalette subtitlePalette; // empty: inherit the theme colour
    if (!m_state->lastFailure().isEmpty()) {
        m_subtitle->setText(Texts::switchFailed());
        subtitlePalette = m_subtitle->palette();
        subtitlePalette.setColor(QPalette::WindowText, QColor(0xff, 0x57, 0x36));
    } else if (pending != PendingAction::None) {
        m_subtitle->setText(Texts::pendingStatus(pending));
        subtitlePalette = m_subtitle->palette();
        subtitlePalette.setColor(QPalette::WindowText, QColor(0xf5, 0xa6, 0x23));
    } else {
        m_subtitle->setText(Texts::modeName(m_state->configuredMode()));
    }
    m_subtitle->setPalette(subtitlePalette);
}

void Tile::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_pressPoint = event->pos();
    QWidget::mousePressEvent(event);
}

void Tile::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_pressPoint == event->pos())
        Q_EMIT clicked();
    m_pressPoint = QPoint();
    QWidget::mouseReleaseEvent(event);
}

} // namespace AginoGpu
