#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QProcessEnvironment>
#include <QHash>
#include <QDebug>
#include <QDateTime>
#include <QThread>
#include <QElapsedTimer>
#include <string>
#include <stdexcept>

void print_separator_line(QString c="-", int ntimes = 100 ) {
    QStringList line;
    for (int i = 0; i < ntimes; ++i) {
        line << c;
    }
    qDebug() << line.join("");
}

qint64 get_max_fileSize_forExtension(const QHash<QString, qint64>& exts_sizesMax, const QString& file_name) {
    QHashIterator<QString, qint64> it_exts_sizesMax(exts_sizesMax);
    while (it_exts_sizesMax.hasNext()) {
        it_exts_sizesMax.next();
        QString extension = it_exts_sizesMax.key();
        if (file_name.endsWith("." + it_exts_sizesMax.key(), Qt::CaseInsensitive))
            return it_exts_sizesMax.value();
    }
    throw std::runtime_error(QString("ERROR: The given filename %1 does not contain any of the extensions in exts_sizesMax which is impossible.").arg(file_name).toStdString());
}

bool fpath_needsToBeExcluded(const QString& fpath, const QStringList& excludes_startWith) {

    for(const auto& e : excludes_startWith) {
        if (fpath.startsWith(e, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    try {

        /////////////////////////////////////////////////////////////
        //////////////////// BEGINS: user input /////////////////////
        /////////////////////////////////////////////////////////////
        QStringList dir_sources({
                                  R"(C:\)",
								  R"(D:\)",
								  R"(E:\)",
                                });

        QString dir_dest = R"(D:\Backups)";
        QString dir_temp = R"(D:\b)";
        QStringList excludes_startWith({
										"SomeDir1ToExclude",
										"E:/AnotherDir1ToExclude"
                                       });
        QString fpath_7zip_exe = R"(C:\Program Files\7-Zip\7z.exe)";
        bool clear_oldBackedUpZippedFiles = false;
        bool clear_dirTemp_afterFinish = true;

        /////////////////////////////////////////////////////////////
        //////////////////// ENDS: user input ///////////////////////
        /////////////////////////////////////////////////////////////

        QHash<QString, qint64> exts_sizesMax;
        exts_sizesMax["h"] = 1000;
        exts_sizesMax["hpp"] = 1000;
        exts_sizesMax["cpp"] = 5000;
        exts_sizesMax["c"] = 5000;
        exts_sizesMax["py"] = 1000;
        exts_sizesMax["pyx"] = 1000;
        exts_sizesMax["java"] = 1000;
        exts_sizesMax["tex"] = 1000;
        exts_sizesMax["bib"] = 1000;
        exts_sizesMax["html"] = 1000;
        exts_sizesMax["txt"] = 100;
        exts_sizesMax["xml"] = 100;
        exts_sizesMax["fxml"] = 100;
        exts_sizesMax["json"] = 100;
        exts_sizesMax["sln"] = 1000;
        exts_sizesMax["vcxproj"] = 1000;
        exts_sizesMax["vcxproj.filters"] = 500;
        exts_sizesMax["vcxproj.user"] = 3;
        exts_sizesMax[".props"] = 100;
        exts_sizesMax["ppt"] = 10000;
        exts_sizesMax["pptx"] = 10000;
        exts_sizesMax["doc"] = 1000;
        exts_sizesMax["docx"] = 1000;
        exts_sizesMax["odt"] = 1000;
        exts_sizesMax["pdf"] = 5000;
        exts_sizesMax["cmd"] = 100;
        exts_sizesMax["iml"] = 10;
        exts_sizesMax["name"] = 2;
        exts_sizesMax["pro"] = 50;
        exts_sizesMax["pro.user"] = 50;

        if (dir_sources.isEmpty()) {
            QFileInfoList drives_in_pc = QDir::drives();
            for(const QFileInfo& drive : drives_in_pc)
                dir_sources.append(drive.path());
        }
        else {
            for(auto&& d : dir_sources)
                d = QDir::cleanPath(d);
        }

        dir_dest = QDir::cleanPath(QDir(dir_dest).canonicalPath());
        dir_temp = QDir::cleanPath(QDir(dir_temp).canonicalPath());
        fpath_7zip_exe = QDir::cleanPath(QDir(fpath_7zip_exe).canonicalPath());

        QString dir_userProfile = QProcessEnvironment::systemEnvironment().value("USERPROFILE");

        excludes_startWith.append(R"(C:\Windows)");
        excludes_startWith.append(R"(C:\Microsoft)");
        excludes_startWith.append(R"(C:\Program Files)");
        excludes_startWith.append(R"(C:\Program Files (x86))");
        excludes_startWith.append(R"(C:\ProgramData)");
        excludes_startWith.append(R"(C:\$)");
        excludes_startWith.append(R"(C:\Progs)");
        excludes_startWith.append(R"(C:\Intel)");
        excludes_startWith.append(R"(C:\cygwin)");
        excludes_startWith.append(R"(C:\Qt)");
        excludes_startWith.append(R"(C:\Qt)");
        excludes_startWith.append(dir_userProfile + "/Anaconda");
        excludes_startWith.append(dir_userProfile + "/AppData");

        for(auto&& e : excludes_startWith)
            e = QDir::cleanPath(e);

        if (!QDir(dir_dest).exists())
            throw std::runtime_error(QString("ERROR: The following dir_dest does not exist. Create it first: %1").arg(dir_dest).toStdString());

        if (!QDir(dir_temp).exists())
            throw std::runtime_error(QString("ERROR: The following dir_temp does not exist. Create it first: %1").arg(dir_temp).toStdString());

        if (!QDir(dir_temp).entryInfoList(QDir::NoDotAndDotDot|QDir::AllEntries).isEmpty())
            throw std::runtime_error(QString("ERROR: The following dir_temp must be empty: %1").arg(dir_temp).toStdString());

        excludes_startWith.append(dir_dest);
        excludes_startWith.append(dir_temp);

        // To make sure that after globbing (later on), it will include the drive letter
        // i.e. absolute path from drive letter
        for(auto&& d : dir_sources)
            d = QDir(d).canonicalPath();

        QString fname_zipfile = QDateTime::currentDateTime().toString("yyyyMMddhhmmss") + ".zip";
        QStringList fnames_zipfiles_del_later = QDir(dir_dest).entryList(QStringList() << "*.zip", QDir::Files);

        QStringList fnamesPatterns_search;
        QMutableHashIterator<QString, qint64> it_exts_sizesMax(exts_sizesMax);
        while (it_exts_sizesMax.hasNext()) {
            it_exts_sizesMax.next();
            fnamesPatterns_search.append("*." + it_exts_sizesMax.key());
            it_exts_sizesMax.value() = it_exts_sizesMax.value() * 1000; // convert kbytes to bytes
        }

        QString fpath_dest_fileCopied_p1 = dir_temp + "/"; // e.g. C:/backupTemp/

        int num_dir_sources = dir_sources.size();

        QElapsedTimer timer;
        timer.start();

        for (int i = 0; i < num_dir_sources; ++i) {

            QString dir_source = dir_sources[i];
            print_separator_line();
            qDebug() << QString("Processing source dir [%1/%2]: %3").arg(i+1).arg(num_dir_sources).arg(dir_source);
            print_separator_line();
            QDirIterator it(dir_source, fnamesPatterns_search, QDir::Files, QDirIterator::Subdirectories);
            size_t nfiles_processed = 0;
            while (it.hasNext()) {
                QString file_path = it.next();
                QFileInfo file_info(file_path);
                QString file_dirParent = file_info.path();
                QString file_name = file_info.fileName();
                qint64 file_size = file_info.size();

                if (fpath_needsToBeExcluded(file_path, excludes_startWith))
                    continue;
                if (file_size > get_max_fileSize_forExtension(exts_sizesMax, file_name) || file_size == 0)
                    continue;

                QString fpath_dest_fileCopied_p2 = file_dirParent.left(1); // drive_letter. E.g. C
                QString fpath_dest_fileCopied_p3 = file_dirParent.mid(2); // e.g. /Prog/SomeDir/SomeDir2
                QString fpath_dest_fileCopied_p4 = "/" + file_name; // e.g. /someFile.cpp

                QString dir_dest_fileCopied = fpath_dest_fileCopied_p1 + fpath_dest_fileCopied_p2 + fpath_dest_fileCopied_p3;
                QString fpath_dest_fileCopied = dir_dest_fileCopied + fpath_dest_fileCopied_p4;

                QDir dir_dest_fileCopied_qdir(dir_dest_fileCopied);
                if (!dir_dest_fileCopied_qdir.exists()) {
                    if (!dir_dest_fileCopied_qdir.mkpath("."))
                        continue;
                }
                if (!QFile::copy(file_path, fpath_dest_fileCopied))
                    continue;

                nfiles_processed++;
                if (nfiles_processed % 1000 == 0)
                    qDebug().noquote() << QString("[%1/%2] %3 \"%4\"").arg(i+1).arg(num_dir_sources).arg(nfiles_processed).arg(dir_source);
            } // end: while (it.hasNext())

        } // end: for (int i = 0; i < dir_sources.size(); ++i)

        QString cmd_str = QString("\"%1\" a \"%2/%3\" \"%4/*\"").arg(fpath_7zip_exe, dir_dest, fname_zipfile, dir_temp);
        qDebug().noquote() << "Executing the command:" << cmd_str;

        QProcess process;
        process.start(cmd_str);
        process.waitForFinished(2*24*60*60*1000); // timeout in 2 days
        process.close();

        if (clear_oldBackedUpZippedFiles) {
            for(const QString& fname_del : fnames_zipfiles_del_later) {
                qDebug() << QString("Deleting old backed-up file: %1...").arg(fname_del) ;
                QFile::remove(dir_dest + "/" + fname_del);
            }
        }

        if (clear_dirTemp_afterFinish) {
            QDir dir_temp_qdir(dir_temp);
            while (true) {
                qDebug() << "Removing dir_temp:" << dir_temp;
                if (dir_temp_qdir.removeRecursively())
                    break;
                qDebug() << "Not successful. Sleeping and trying again...";
                QThread::sleep(1);
            }
            while (true) {
                qDebug() << "Creating dir_temp:" << dir_temp;
                if (dir_temp_qdir.mkpath("."))
                    break;
                qDebug() << "Not successful. Sleeping and trying again...";
                QThread::sleep(1);
            }
        }

        qint64 msecs_taken = timer.elapsed();
        QString secs_taken = QString::number(static_cast<double>(msecs_taken) / 1000, 'f', 0);
        QString mins_taken = QString::number(static_cast<double>(msecs_taken) / (1000*60), 'f', 2);
        QString hrs_taken = QString::number(static_cast<double>(msecs_taken) / (1000*60*60), 'f', 4);
        QString days_taken = QString::number(static_cast<double>(msecs_taken) / (1000*60*60*24), 'f', 6);
        qDebug() << QString("Backup took %1 secs, or %2 mins, or %3 hrs, or %4 days").arg(secs_taken, mins_taken, hrs_taken, days_taken);

        qDebug().noquote() << QString("Backed up successfully to: \"%1/%2\"").arg(dir_dest, fname_zipfile);

    } // end: try

    catch(std::exception& e) {
        qDebug() << QString(e.what());
    }

    return a.exec();
}

