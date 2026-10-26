// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractListModel>
#include <QSet>
#include <QUrl>

namespace AginoGpu {

class AppCatalog;
class RuleStore;

// Model of the "Add application" page: the installed apps (filtered by the search text, with
// multi-select), plus one optional program picked from the file system. Nothing is written to
// the rules until commit().
class AddAppModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(bool hasFile READ hasFile NOTIFY selectionChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY selectionChanged)
    Q_PROPERTY(QString fileDirectory READ fileDirectory NOTIFY selectionChanged)
    Q_PROPERTY(bool fileIsDesktopEntry READ fileIsDesktopEntry NOTIFY selectionChanged)
    Q_PROPERTY(bool createLauncher READ createLauncher WRITE setCreateLauncher NOTIFY selectionChanged)

public:
    enum Role { AppIdRole = Qt::UserRole + 1, NameRole, IconRole, AlreadyAddedRole, CheckedRole };

    AddAppModel(AppCatalog *catalog, RuleStore *rules, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filter() const { return m_filter; }
    void setFilter(const QString &filter);
    int selectedCount() const;
    bool hasFile() const { return !m_file.isEmpty(); }
    QString fileName() const;
    QString fileDirectory() const;
    bool fileIsDesktopEntry() const;
    bool createLauncher() const { return m_createLauncher; }
    void setCreateLauncher(bool create);

    // Back to an empty selection and a fresh application list; called when the page opens.
    Q_INVOKABLE void reset();
    Q_INVOKABLE void toggle(int row);
    // `file` comes from the file dialog; anything that is not an existing file is ignored.
    Q_INVOKABLE void setFile(const QUrl &file);
    Q_INVOKABLE void clearFile();
    // Writes the selection into the rules. false (nothing written) when the file was refused.
    Q_INVOKABLE bool commit();

Q_SIGNALS:
    void filterChanged();
    void selectionChanged();

private:
    void rebuild();
    void refreshAdded();

    AppCatalog *m_catalog;
    RuleStore *m_rules;
    QString m_filter;
    QList<int> m_rows;           // indexes into the catalog that match the filter
    QSet<QString> m_selected;    // app ids ticked on this page
    QSet<QString> m_added;       // app ids that already have a rule
    QString m_file;
    bool m_createLauncher = true;
};

} // namespace AginoGpu
