/*=========================================================================

 medInria

 Copyright (c) INRIA 2013. All rights reserved.

 See LICENSE.txt for details in the root of the sources or:
 https://github.com/medInria/medInria-public/blob/master/LICENSE.txt

 This software is distributed WITHOUT ANY WARRANTY; without even
 the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 PURPOSE.

=========================================================================*/

#include <medDataLoadThread.h>

#include <medDataManager.h>
#include <medAbstractData.h>

medDataLoadThread::medDataLoadThread(medDataIndex const & index, medViewContainer * parent) : QObject(parent), /*QThread(parent),*/ m_index(index), m_parent(parent)
{
    auto toto = connect(this, SIGNAL(dataReady(medAbstractData*)), m_parent, SLOT(addData(medAbstractData *)) );
    toto = toto;
}

medDataLoadThread::~medDataLoadThread()
{
    int i = 10;
}

void medDataLoadThread::process()
{
    m_pAbsData = medDataManager::instance()->retrieveData(m_index);
    if (m_pAbsData)
    {
        emit dataReady(m_pAbsData);
    }
    emit finished();
    deleteLater();
}

