// Copyright: INRIA

#include <dtkComposer>

#include "medCore.h"

#include "medToMedImageNode.h"

#include "medDataReaderWriter.h"
#include "medAbstractData.h"
#include "medAbstractImageData.h"
#include "medAbstractMeshData.h"




class medToMedImageNodePrivate
{
public:
    dtkComposerTransmitterReceiver< dtkImage* >                        dataRecv;
    dtkComposerTransmitterEmitter< medAbstractImageData* >               imgEmt;
};

medToMedImageNode::medToMedImageNode(void) : d(new medToMedImageNodePrivate())
{
    this->setFactory(medCore::converter::pluginFactory());

    this->appendReceiver(&d->dataRecv);
    this->appendEmitter(&d->imgEmt);
}

medToMedImageNode::~medToMedImageNode(void)
{
    delete d;
}

void medToMedImageNode::run(void)
{
    if ( d->dataRecv.isEmpty())
    {
        qDebug() << Q_FUNC_INFO << "The input is not set. Aborting.";
        return;
    }
    else
    {
        if(!d->dataRecv.data())
        {
            qWarning()<<Q_FUNC_INFO<<"no data to convert";
        }

        medAbstractConverter* converter= this->object();
        if(converter)
        {
            d->imgEmt.setData(converter->toMedImage(d->dataRecv.data()));
        }
    }
}

