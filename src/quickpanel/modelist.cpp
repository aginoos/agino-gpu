// SPDX-License-Identifier: GPL-3.0-or-later
#include "modelist.h"

#include "agino-gpu-client/texts.h"
#include "agino-gpu/mode.h"

#include "itemrow.h"

#include <QIcon>
#include <QTimer>
#include <QVBoxLayout>

namespace AginoGpu {

namespace {
constexpr int kRowSpacing = 10;
}

ModeList::ModeList(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(kRowSpacing);
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
    // The panel refreshes on every state change; rebuilding unchanged rows would drop the hover.
    if (m_built && offered == m_offered && configured == m_configured && active == m_active)
        return;
    m_built = true;
    m_offered = offered;
    m_configured = configured;
    m_active = active;
    clear();
    for (Mode mode : offered) {
        const bool selected = mode == configured;
        auto *row = new ItemRow(this);
        row->addLabel(Texts::modeName(mode), ItemRow::Text::Primary);
        if (selected)
            row->addIcon(QIcon::fromTheme(QStringLiteral("plugin_item_select")));
        else
            row->addLabel(Texts::pendingCost(pendingAction(mode, active)), ItemRow::Text::Secondary);
        connect(row, &ItemRow::clicked, this,
                [this, mode] { QTimer::singleShot(0, this, [this, mode] { Q_EMIT modeClicked(mode); }); });
        layout()->addWidget(row);
    }
}

} // namespace AginoGpu
