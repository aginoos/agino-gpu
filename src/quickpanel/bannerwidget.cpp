// SPDX-License-Identifier: GPL-3.0-or-later
#include "bannerwidget.h"

#include <DLabel>
#include <DPushButton>

#include <QHBoxLayout>
#include <QPainter>
#include <QVBoxLayout>

DWIDGET_USE_NAMESPACE

namespace AginoGpu {

BannerWidget::BannerWidget(QWidget *parent)
    : QFrame(parent)
    , m_label(new DLabel(this))
    , m_buttons(new QHBoxLayout)
{
    m_label->setWordWrap(true);
    m_buttons->setContentsMargins(0, 0, 0, 0);
    m_buttons->setSpacing(8);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(8);
    layout->addWidget(m_label);
    layout->addLayout(m_buttons);
    setVisible(false);
}

void BannerWidget::setSpec(const BannerSpec &spec)
{
    while (QLayoutItem *item = m_buttons->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->hide();
            widget->deleteLater(); // may be the button whose click got us here
        }
        delete item;
    }
    setVisible(!spec.isEmpty());
    m_label->setText(spec.text);
    for (const BannerAction &action : spec.actions) {
        auto *button = new DPushButton(action.text, this);
        button->setFocusPolicy(Qt::NoFocus);
        connect(button, &QAbstractButton::clicked, this, [run = action.run] { run(); });
        m_buttons->addWidget(button);
    }
    m_buttons->addStretch(1);
}

void BannerWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor fill = palette().color(QPalette::Text);
    fill.setAlphaF(0.08);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawRoundedRect(rect(), 8, 8);
}

} // namespace AginoGpu
