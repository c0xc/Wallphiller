#include "scan.hpp"

QStringList
Scan::scan(const QString &path,
           const QStringList &filters,
           RecursionMode recursive)
{
    QStringList scanned_dirs;
    return scan(path, filters, recursive, scanned_dirs);
}

QStringList
Scan::scan(const QStringList &paths,
           const QStringList &filters,
           RecursionMode recursive)
{
    QStringList result_list;
    foreach (QString path, paths)
        result_list << scan(path, filters, recursive);
    return result_list;
}

Scan::Scan(const QStringList &paths, const QStringList &filter)
{
    setPath(paths);
    setFilter(filter);
}

Scan::Scan(const QString &path, const QStringList &filter)
{
    setPath(path);
    setFilter(filter);
}

Scan::Scan(const QString &path)
{
    setPath(path);
}

void
Scan::setPath(const QStringList &paths)
{
    _paths = paths;
}

void
Scan::setPath(const QString &path)
{
    _paths.clear();
    _paths << path;
}

void
Scan::setFilter(const QStringList &filter)
{
    _typefilter = filter;
}

void
Scan::process()
{
    QStringList list;
    list = scan(paths(), filter(), RecursionMode::Recursive);
    emit scanned(list);
    emit finished();
}

QStringList
Scan::scan(const QString &path,
           const QStringList &filters,
           RecursionMode recursive,
           QStringList &scanned_dirs)
{
    QStringList result_list;
    if (path.isEmpty()) return result_list;
    QDir::Filters flags;
    flags = QDir::AllEntries; //Files + Directories
    flags |= QDir::NoDotAndDotDot;
    flags |= QDir::Hidden;
    flags |= QDir::System;
    QDir dir(path);
    //"*.jpg" filter -> no directories (well, except "dir.jpg" maybe)
    QFileInfoList filelist = dir.entryInfoList(filters, flags);
    QFileInfoList dirlist = dir.entryInfoList(flags);
    //Both entry lists contain both files and directories
    //(as well as other file types, which are eventually ignored).

    //Add files
    foreach (QFileInfo info, filelist)
    {
        if (!info.isFile()) continue; //Because of QDir::AllEntries
        QString item = info.filePath();
        result_list << item;
    }

    //Add directories
    foreach (QFileInfo info, dirlist)
    {
        if (info.isFile()) continue;
        if (!info.isDir()) continue; //Because of QDir::AllEntries
        QString item = info.filePath();
        if (recursive == RecursionMode::Recursive && info.isDir())
        {
            //Note: Scanning a directory which contains a symlink
            //pointing to itself (or higher, within path) will lead
            //to an INFINITE LOOP!
            //We keep track of the scanned directories to prevent duplicates
            if (!scanned_dirs.contains(item))
            {
                //Add files in (sub)directory (not the directory itself)
                result_list << scan(
                    item, filters, RecursionMode::Recursive, scanned_dirs);
                scanned_dirs << item;
            }
        }
    }

    //Files in subdirectories added by parent overload of scan()!

    return result_list;
}

QStringList
Scan::paths()
{
    return _paths;
}

QStringList
Scan::filter()
{
    return _typefilter;
}

