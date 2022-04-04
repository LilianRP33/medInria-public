/*=========================================================================

 medInria

 Copyright (c) INRIA 2013 - 2018. All rights reserved.
 See LICENSE.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.

=========================================================================*/

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>

#include <medCoreExport.h>
#include <medDataModel.h>


int medAbstractWritingPolicy::levelToWrite(QString pi_sourceId)
{
    int iRes = -1;

    auto * pSource = medDataModel::instance()->getSourceToWrite(pi_sourceId);
    if (pSource)
    {
        iRes = static_cast<int>(pSource->getLevelDesiredWritable());
    }

    return iRes;
}

bool medAbstractWritingPolicy::islevelWritable(QString pi_sourceId, unsigned int pi_uiLevel)
{
    bool bRes = false;

    auto * pSource = medDataModel::instance()->getSourceToWrite(pi_sourceId);
    if (pSource)
    {
        bRes = pSource->isLevelWritable(pi_uiLevel);
    }

    return bRes;
}

