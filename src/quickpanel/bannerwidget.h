// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFrame>
#include <QList>
#include <QString>

#include <functional>

namespace Dtk::Widget {
class DLabel;
}
class QHBoxLayout;

namespace AginoGpu {

struct BannerAction
{
    QString text;
    std::function<void()> run;
};

struct BannerSpec
{
    QString text;
    QList<BannerAction> actions;
    bool isEmpty() const { return text.isEmpty(); }
};

// The single message strip of the sub-page: text and a row of buttons.
class BannerWidget : public QFrame
{
    Q_OBJECT
public:
    explicit BannerWidget(QWidget *parent = nullptr);

    void setSpec(const BannerSpec &spec);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Dtk::Widget::DLabel *m_label;
    QHBoxLayout *m_buttons;
};

} // namespace AginoGpu
