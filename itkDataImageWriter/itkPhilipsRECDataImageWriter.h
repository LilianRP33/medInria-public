#ifndef ITKPHILIPSRECDATAIMAGEWRITER_H
#define ITKPHILIPSRECDATAIMAGEWRITER_H

#include <itkDataImageBase/itkDataImageWriterBase.h>
#include <itkDataImageWriterPluginExport.h>

class ITKDATAIMAGEWRITERPLUGIN_EXPORT itkPhilipsRECDataImageWriter: public itkDataImageWriterBase {
public:
    itkPhilipsRECDataImageWriter();
    virtual ~itkPhilipsRECDataImageWriter();

    virtual QString identifier()  const;
    virtual QString description() const;

    QStringList handled() const;

    static QStringList s_handled ();

    static bool registered();	

private:

    static const char ID[];
    static dtkAbstractDataWriter* create();
};

#endif  //  ! ITKPHILIPSRECDATAIMAGEWRITER_H
