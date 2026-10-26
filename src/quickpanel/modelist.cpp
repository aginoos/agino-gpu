// SPDX-License-Identifier: GPL-3.0-or-later
#include "modelist.h"

#include "agino-gpu-client/texts.h"
#include "agino-gpu/mode.h"

#include <DFontSizeManager>
#include <DLabel>
#include <DPalette>

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QRadioButton>
#include <QTimer>
#include <QVBoxLayout>

#include <functional>

DGUI_USE_NAMESPACE
DWIDGET_USE_NAMESPACE

namespace AginoGpu {

namespace {

class ModeRow : public QWidget
{
public:
    ModeRow(Mode mode, bool selected, const QString &cost, std::function<void()> onClick, QWidget *parent)
        : QWidget(parent)
        , m_onClick(std::move(onClick))
    {
        auto *radio = new QRadioButton(Texts::modeName(mode), this);
        radio->setChecked(selected);
        radio->setAttribute(Qt::WA_TransparentForMouseEvents);
        radio->setFocusPolicy(Qt::NoFocus);
        auto *costLabel = new DLabel(cost, this);
        costLabel->setForegroundRole(DPalette::TextTips);
        DFontSizeManager::instance()->bind(costLabel, DFontSizeManager::T8);
        costLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(10, 0, 10, 0);
        layout->addWidget(radio);
        layout->addStretch(1);
        layout->addWidget(costLabel);
        setFixedHeight(36);
        setCursor(Qt::PointingHandCursor);
    }

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos()))
            m_onClick();
    }

private:
    std::function<void()> m_onClick;
};

} // namespace

ModeList::ModeList(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
}

void ModeList::clear()
{
    while (QLayoutItem *item = layout()->takeAt(0)) {
        item->widget()->hide();
        item->widget()->deleteLater(); // may be the row whose click got us here
        delete item;
    }
}

void ModeList::setModes(const QList<Mode> &offered, Mode configured, Mode active)
{
    clear();
    for (Mode mode : offered) {
        const bool selected = mode == configured;
        const QString cost = selected ? QString() : Texts::pendingCost(pendingAction(mode, active));
        auto *row = new ModeRow(mode, selected, cost, [this, mode] { QTimer::singleShot(0, this, [this, mode] { Q_EMIT modeClicked(mode); }); }, this);
        layout()->addWidget(row);
    }
}

} // namespace AginoGpu
