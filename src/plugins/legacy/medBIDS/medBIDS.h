/*=========================================================================

 medInria

 Copyright (c) INRIA 2013 - 2019. All rights reserved.
 See LICENSE.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.

=========================================================================*/
#pragma once

#include "medBIDSPluginExport.h"

#include <medAbstractSource.h>
#include <medStringParameter.h>
//#include <writingPolicy/medBIDSWritingPolicy.h>
#include <medGroupParameter.h>
#include <QThread>

class Worker;
class medBIDS : public medAbstractSource
{

public:
    explicit medBIDS();
    ~medBIDS() override;

    /* ***********************************************************************/
    /* *************** Init/set/ctrl source properties ***********************/
    /* ***********************************************************************/
    bool initialization(const QString &pi_instanceId) override;

    bool setInstanceName(const QString &pi_instanceName) override;

    bool connect(bool pi_bEnable) override;

    QList<medAbstractParameter *> getAllParameters() override;

    QList<medAbstractParameter *> getCipherParameters() override;

    QList<medAbstractParameter *> getVolatilParameters() override;

    QList<medAbstractParameter*> getFilteringParameters() override;

    /* ***********************************************************************/
    /* *************** Get source properties *********************************/
    /* ***********************************************************************/
    bool isWritable() override;

    bool isLocal() override;

    bool isCached() override;

    bool isOnline() override;

    bool isFetchByMinimalEntriesOrMandatoryAttributes() override;

    /* ***********************************************************************/
    /* *************** Get source structure information **********************/
    /* ***********************************************************************/
    QString getInstanceName() override;

    QString getInstanceId() override;

    unsigned int getLevelCount() override;

    unsigned int getLevelDesiredWritable() override;

    QStringList getLevelNames() override;

    QString getLevelName(unsigned int pi_uiLevel) override;

    bool isLevelWritable(unsigned int pi_uiLevel) override;

    virtual int getIOInterface() override;
    virtual QMap<QString, QStringList> getTypeAndFormat() override;

    QStringList getMandatoryAttributesKeys(unsigned int pi_uiLevel) override;

    //QStringList getAdditionalAttributesKeys(unsigned int pi_uiLevel) override;


    /* ***********************************************************************/
    /* *************** Get elements data *************************************/
    /* ***********************************************************************/
    QList<levelMinimalEntries> getMinimalEntries(unsigned int pi_uiLevel, QString parentId) override;

    QList<QMap<QString, QString>> getMandatoryAttributes(unsigned int pi_uiLevel, QString parentId) override;

    bool getAdditionalAttributes(unsigned int pi_uiLevel, QString id, datasetAttributes &po_attributes) override;

    /* ***********************************************************************/
    /* *************** Get data          *************************************/
    /* ***********************************************************************/
    QVariant getDirectData(unsigned int pi_uiLevel, QString key) override;

    int getAssyncData(unsigned int pi_uiLevel, QString id) override;

    //    QString addData(QVariant data, QStringList parentUri, QString name) override;
    virtual bool addDirectData(QVariant data, levelMinimalEntries &pio_minimalEntries, unsigned int pi_uiLevel, QString parentKey) override;
    virtual int addAssyncData(QVariant data, levelMinimalEntries &pio_minimalEntries, unsigned int pi_uiLevel, QString parentKey) override;
    virtual bool createPath(QList<levelMinimalEntries> &pio_path, datasetAttributes const &pi_attributes, unsigned int pi_uiLevel = 0, QString parentKey = "") override;
    virtual bool createFolder(levelMinimalEntries &pio_minimalEntries, datasetAttributes const &pi_attributes, unsigned int pi_uiLevel, QString parentKey) override;
    virtual bool alterMetaData(datasetAttributes const &pi_attributes, unsigned int pi_uiLevel, QString key) override;
    virtual bool getThumbnail(QPixmap &po_thumbnail, unsigned int pi_uiLevel, QString key) override;
    virtual bool setThumbnail(QPixmap &pi_thumbnail, unsigned int pi_uiLevel, QString key) override;
    virtual bool commitData(QVariant data, levelMinimalEntries &pio_minimalEntries, unsigned int pi_uiLevel, QString parentKey) override { return false; };
    virtual QVariant getAsyncResults(int pi_iRequest) override;
    virtual int push(unsigned int pi_uiLevel, QString key) override { return -1; };

    //inline virtual medAbstractWritingPolicy *getWritingPolicy() override { return &m_writingPolicy; };
public slots:
    void abort(int pi_iRequest) override;

    void updateDatabaseName(QString const &path);



private:
    QStringList getAttributes(QString &line); // modifie la ligne d'un tsv pour en récupérer les attributs des colonnes
    // Renvoie true si le fichier a pu être ouvrir et lu, renvoie false sinon
    virtual bool getPatientsFromTsv(); // Lit un fichier "participants.tsv" pour en extraire le contenu. Renvoie un bool si fichier conforme et lecture ok 
    virtual bool getSessionsFromTsv(QString parentId); // Lit un fichier "..._sessions.tsv" pour en extraire le contenu. Renvoie un bool si fichier conforme et lecture ok

    // methods
    virtual QList<medAbstractSource::levelMinimalEntries> getSubjectMinimalEntries(QString &key);
    virtual QList<medAbstractSource::levelMinimalEntries> getSessionMinimalEntries(QString &parentId);
    virtual QList<medAbstractSource::levelMinimalEntries> getDataMinimalEntries(QString &parentId);
    virtual QList<medAbstractSource::levelMinimalEntries> getFilesMinimalEntries(QString &parentId);

    // 'const' : seulement accessible en lecture, ne doit pas être modifié pendant l'exécution
    QList<QMap<QString, QString>> getSubjectMandatoriesAttributes(const QString &key);
    QList<QMap<QString, QString>> getSessionMandatoriesAttributes(const QString &key);
    QList<QMap<QString, QString>> getDataMandatoriesAttributes(const QString &key);
    QList<QMap<QString, QString>> getFilesMandatoriesAttributes(const QString &key);

    // Lecture d'un fichier Json : extraire des valeurs de tags DICOM
    QJsonObject readJsonFile(QString file_path);

    bool getFilesAdditionalAttributes(const QString &key, medAbstractSource::datasetAttributes &po_attributes);

private:
    // members
    medStringParameter *root_path;
    const QString m_Driver;
    //const QString m_ConnectionName;
    QString m_instanceId;
    QString m_instanceName;
    bool m_online;
    QStringList m_LevelNames;
    QMap<QString, QStringList> m_MandatoryKeysByLevel;
    QMap<QString, QStringList> m_SupportedTypeAndFormats;
    //medBIDSWritingPolicy m_writingPolicy;

    QMap<QString, QString> m_PatientLevelAttributes;
    medGroupParameter *m_FilterDBSettings;

    static std::atomic<int> s_RequestId;
    QMap<int, QVariant> m_requestToDataMap;
    //QMap<int, QTimer*> m_requestToTimerMap;
    QMap<int, QTime*> m_requestToTimeMap;

    QThread m_Thread;
    Worker *m_pWorker;
//    QTimer m_timer;
//    void timeManagement();


    // Ajouts
    // Corespondance nom de dossier sub ('participant_id') et reste des attributs pour un patient
    QMap<QString, QList<QString>> patient_tsv;

    // Les sessions liées à un patient ('participant_id')
    QMap<QString, QList<QString>> sessions_from_patient;
    // Corespondance nom de dossier ses ('session_id') et reste des attributs pour une session
    QMap<QString, QList<QString>> session_tsv;

    // Potentiellement plus utilisé
    // // Dans les mandatoriesAttributes des fichiers, permet de stocker les informations des tags du json pour les afficher pour les fichiers .nii.gz
    // QMap<QString, QMap<QString, QString>> infos_nifti;
};


