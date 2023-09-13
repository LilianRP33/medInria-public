#pragma once
/*=========================================================================

 medInria

 Copyright (c) INRIA 2013 - 2020. All rights reserved.
 See LICENSE.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.

=========================================================================*/


#include <medWidgetsExport.h>

#include <QWidget>
#include <QDialog>
#include <QMap>
#include <QMultiMap>
#include <QString>

class medSourceModelPresenter;

class medBIDSDataInfoWidget : public QDialog
{
    Q_OBJECT
public:
    medBIDSDataInfoWidget(QMap<QString, QString> dataAttributes, QMultiMap<QString, QString> optionalAttributes, QWidget *parent = nullptr);
    virtual ~medBIDSDataInfoWidget();
};
