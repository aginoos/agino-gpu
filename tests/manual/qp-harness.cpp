// SPDX-License-Identifier: GPL-3.0-or-later
// Loads the quick-panel plugin outside the dock and saves the tile and the sub-page as PNGs:
//   AGINO_GPU_ROOT=<fixture> QT_QPA_PLATFORM=offscreen qp-harness <plugin.so> <out-dir>
#include "pluginproxyinterface.h"
#include "pluginsiteminterface_v2.h"

#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPluginLoader>
#include <QTimer>
#include <QWidget>

#include <cstdio>

namespace {

class Proxy : public PluginProxyInterface
{
public:
    QStringList added;
    void itemAdded(PluginsItemInterface *const, const QString &key) override { added << key; }
    void itemUpdate(PluginsItemInterface *const, const QString &) override {}
    void itemRemoved(PluginsItemInterface *const, const QString &key) override { added.removeAll(key); }
    void requestWindowAutoHide(PluginsItemInterface *const, const QString &, const bool) override {}
    void requestRefreshWindowVisible(PluginsItemInterface *const, const QString &) override {}
    void requestSetAppletVisible(PluginsItemInterface *const, const QString &key, const bool visible) override
    {
        std::printf("requestSetAppletVisible %s %d\n", qPrintable(key), visible);
    }
    void saveValue(PluginsItemInterface *const, const QString &, const QVariant &) override {}
    const QVariant getValue(PluginsItemInterface *const, const QString &, const QVariant &fallback) override
    {
        return fallback;
    }
    void removeValue(PluginsItemInterface *const, const QStringList &) override {}
};

bool save(QWidget *w, const QString &file)
{
    if (!w)
        return false;
    w->adjustSize();
    w->show();
    QApplication::processEvents();
    std::printf("%s: %dx%d\n", qPrintable(file), w->width(), w->height());
    return w->grab().save(file);
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (argc != 3) {
        std::fprintf(stderr, "usage: qp-harness <plugin.so> <out-dir>\n");
        return 2;
    }
    QPluginLoader loader(QString::fromLocal8Bit(argv[1]));
    auto *plugin = qobject_cast<PluginsItemInterfaceV2 *>(loader.instance());
    if (!plugin) {
        std::fprintf(stderr, "load failed: %s\n", qPrintable(loader.errorString()));
        return 1;
    }
    std::printf("metadata: %s\n", qPrintable(QString::fromUtf8(
        QJsonDocument(loader.metaData().value(QStringLiteral("MetaData")).toObject()).toJson(QJsonDocument::Compact))));
    std::printf("name=%s flags=0x%x\n", qPrintable(plugin->pluginName()), int(plugin->flags()));
    Proxy proxy;
    plugin->init(&proxy);
    QTimer::singleShot(1500, &app, [&] {
        std::printf("items: %s\n", qPrintable(proxy.added.join(QLatin1Char(' '))));
        const QString out = QString::fromLocal8Bit(argv[2]);
        bool ok = save(plugin->itemWidget(Dock::QUICK_ITEM_KEY), out + QStringLiteral("/tile.png"));
        for (const QString &key : std::as_const(proxy.added))
            ok = save(plugin->itemPopupApplet(key), out + QStringLiteral("/panel-") + key + QStringLiteral(".png")) || ok;
        app.exit(ok ? 0 : 1);
    });
    return app.exec();
}
