/*=========================================================================

 medInria

 Copyright (c) INRIA 2013 - 2019. All rights reserved.
 See LICENSE.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.

=========================================================================*/
#include <medSourcesLoader.h>
#include "medBIDSPlugin.h"
#include "medBIDS.h"

medBIDSPlugin::medBIDSPlugin(QObject *parent) : medPluginLegacy(parent)
{
}

medBIDSPlugin::~medBIDSPlugin()
{
}

bool medBIDSPlugin::initialize()
{
    return medSourcesLoader::instance()->registerSourceType(
        "medBIDS",
        "Datasource de type BIDS",
        "Ce type de datasource permet l'exploitation des anciennes base medInria 3",
        //&foo);
        []() -> medAbstractSource* {return new medBIDS(); });
}

QString medBIDSPlugin::name() const
{
    return "BIDS Strandard";
}

QString medBIDSPlugin::description() const
{
    QString description = \
            tr("BIDS local tree importation and representation.");
    return description;
}

QString medBIDSPlugin::version() const
{
    return MEDBIDSPLUGIN_VERSION;
}

QStringList medBIDSPlugin::types() const
{
    return QStringList() << "medBIDS";
}
