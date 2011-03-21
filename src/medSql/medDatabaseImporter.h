/* medDatabaseImporter.h --- 
 * 
 * Author: Julien Wintz
 * Copyright (C) 2008 - Julien Wintz, Inria.
 * Created: Tue Jan 19 13:41:28 2010 (+0100)
 * Version: $Id$
 * Last-Updated: Wed Oct  6 15:33:33 2010 (+0200)
 *           By: Julien Wintz
 *     Update #: 10
 */

/* Commentary: 
 * 
 */

/* Change log:
 * 
 */

#ifndef MEDDATABASEIMPORTER_H
#define MEDDATABASEIMPORTER_H

#include "medSqlExport.h"

#include <medCore/medJobItem.h>
#include <QtCore>

class medDatabaseImporterPrivate;

class MEDSQL_EXPORT medDatabaseImporter : public medJobItem
{
    Q_OBJECT

public:
     medDatabaseImporter(const QString& file);
    ~medDatabaseImporter(void);

    void run(void);

private:
    medDatabaseImporterPrivate *d;

};

#endif // MEDDATABASEIMPORTER_H
