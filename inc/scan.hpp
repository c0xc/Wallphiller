#ifndef SCAN_HPP
#define SCAN_HPP

#include <QObject>
#include <QStringList>
#include <QDir>
#include <QFileInfoList>

class Scan : public QObject
{
    Q_OBJECT

signals:

    void
    finished();

    void
    scanned(const QStringList &list);

public:

    enum class RecursionMode
    {
        NonRecursive,
        Recursive
    };

    static QStringList
    scan(const QString &path,
         const QStringList &filters,
         RecursionMode recursive = RecursionMode::NonRecursive);

    static QStringList
    scan(const QStringList &paths,
         const QStringList &filters,
         RecursionMode recursive = RecursionMode::NonRecursive);

    Scan(const QStringList &paths, const QStringList &filter);

    Scan(const QString &path, const QStringList &filter);

    Scan(const QString &path);

public slots:

    void
    setPath(const QStringList &paths);

    void
    setPath(const QString &path);

    void
    setFilter(const QStringList &filter);

    void
    process();

private:

    static QStringList
    scan(const QString &path,
         const QStringList &filters,
         RecursionMode recursive,
         QStringList &scanned_dirs);

    QStringList
    _paths;

    QStringList
    _typefilter;

    QStringList
    paths();

    QStringList
    filter();

};

#endif
