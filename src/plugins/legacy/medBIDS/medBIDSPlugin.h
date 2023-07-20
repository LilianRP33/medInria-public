#pragma once
/*=========================================================================

medInria

Copyright (c) INRIA 2013 - 2020. All rights reserved.
See LICENSE.txt for details.

This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.

=========================================================================*/

#include <medPluginLegacy.h>
#include "medBIDSPluginExport.h"

class MEDBIDSPLUGIN_EXPORT medBIDSPlugin : public medPluginLegacy
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "fr.inria.medBIDSPlugin" FILE "medBIDSPlugin.json")
    Q_INTERFACES(dtkPlugin)

public:
    medBIDSPlugin(QObject *parent = nullptr);
    ~medBIDSPlugin();
    virtual bool initialize();
    
    virtual QString name() const;
    virtual QString description() const;
    virtual QString version () const;
    virtual QStringList types() const;
};
