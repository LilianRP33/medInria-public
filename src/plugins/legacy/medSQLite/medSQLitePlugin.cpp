/*=========================================================================

 medInria

 Copyright (c) INRIA 2013 - 2019. All rights reserved.
 See LICENSE.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.

=========================================================================*/
#include "medSQLitePlugin.h"

medSQLitePlugin::medSQLitePlugin(QObject *parent) : medPluginLegacy(parent)
{
}

bool medSQLitePlugin::initialize()
{
     return true;
}

QString medSQLitePlugin::name() const
{
    return "SQLite Local DB";
}

QString medSQLitePlugin::description() const
{
    QString description = \
            tr("Store data in local Database based on SQLite. \
               <br><br>This plugin uses the <a href=\"https://doc.qt.io/qt-5/qsqldatabase.html/\" style=\"color: #cc0000\" >QSQLDATABASE plugin</a> from Qt library.");
    return description;
}

QString medSQLitePlugin::version() const
{
    return MEDSQLITEPLUGIN_VERSION;
}

QStringList medSQLitePlugin::types() const
{
    return QStringList() << "medSQLite";
}
