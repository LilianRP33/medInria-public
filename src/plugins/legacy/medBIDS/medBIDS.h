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

#include <itkImageIOBase.h>

#include <medAbstractSource.h>
#include <medStringParameter.h>
//#include <writingPolicy/medBIDSWritingPolicy.h>
#include <medGroupParameter.h>
#include <medTriggerParameter.h>
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
    QJsonObject readJsonFile(QString filePath); // Li et Extrait les données d'un fichier Json : fichiers médicaux ou dataset_description
    void getDatasetDescription(QJsonObject jsonObj, QString parentKey); // Traite les données du fichier 'dataset_description.json' pour les afficher : récursion pour objets imbriqués
    void getNiftiAttributesForMandatories(QString niftiPath); // Ajouter aux mandatoriesAttributes des "Files" et "Derivative" les clés des fichiers nifti

    QStringList getAttributesFromTsv(QString &line); // Extrait et renvoie une liste des attributs d'une ligne d'un tsv
    bool getPatientsFromTsv(); // Lit un fichier "participants.tsv" pour en extraire le contenu. Renvoie true si fichier conforme et lecture ok 
    bool getSessionsFromTsv(QString parentId); // Lit un fichier "..._sessions.tsv" pour en extraire le contenu. Renvoie true si fichier conforme et lecture ok

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
    QList<QMap<QString, QString>> getDerivativeMandatoriesAttributes(const QString &key);

    QMap<QString, QString> getNiftiValuesFromFile(QString niftiFile, int level);

    bool getSubjectAdditionalAttributes(const QString &key, medAbstractSource::datasetAttributes &po_attributes);
    bool getSessionAdditionalAttributes(const QString &key, medAbstractSource::datasetAttributes &po_attributes);
    bool getFilesAdditionalAttributes(const QString &key, medAbstractSource::datasetAttributes &po_attributes);

    void createBIDSDerivativeSubFolders(QString parentKey, QString derivativeType, QString &newPath); // Créer les sous-dossiers pour "derivatives" dans l'arborescence BIDS afin d'y copier un fichier
    void createJsonDerivativeFile(QString sourcePath, QString jsonFilePath, QString derivativeType); // Créer le fichier json pour un fichier derivative

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
    // répertoire racine de l'arborescence BIDS
    QString bids_path;

    // Corespondance nom de dossier sub ('participant_id') et attributs pour un patient : noms de colonnes/valeurs
    QMap<QString, QMap<QString, QString>> participant_tsv;

    // Corespondance nom de dossier ses ('session_id') et attributs pour une session : noms de colonnes/valeurs
    QMap<QString, QMap<QString, QString>> session_tsv;

    // Contient les attributs et valeurs du fichier 'dataset_description.json'
    QMap<QString, QString> descriptionDatasetValues;

    // Bouton sur l'interface "sources" pour l'affichage du contenu de 'dataset_description.json'
    medTriggerParameter *descriptionButton;

    // Pointeur pour lire image nifti
    itk::ImageIOBase::Pointer imageIO;

};


