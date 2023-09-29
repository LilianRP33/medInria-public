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
    QJsonObject readJsonFile(QString filePath);
    QString readJsonArray(QMap<QString, QString> &jsonContent, QJsonArray jsonTab, QString parentKey, QString key);
    void getJsonContent(QMap<QString, QString> &jsonContent, QJsonObject jsonObj, QString parentKey);
    
    void existSesLevel(QString subEntitieDir);
    void getNiftiAttributesForMandatories(QString niftiPath);

    QStringList getAttributesFromTsv(QString &line);
    bool getPatientsFromTsv();
    bool getSessionsFromTsv(QString parentId);

    // methods
    virtual QList<medAbstractSource::levelMinimalEntries> getSubjectMinimalEntries(QString &key);
    virtual QList<medAbstractSource::levelMinimalEntries> getSessionMinimalEntries(QString &parentId);
    virtual QList<medAbstractSource::levelMinimalEntries> getDataMinimalEntries(QString &parentId);
    virtual QList<medAbstractSource::levelMinimalEntries> getFilesMinimalEntries(QString &parentId);
    virtual QList<medAbstractSource::levelMinimalEntries> getDerivativeMinimalEntries(QString &parentId);

    QList<QMap<QString, QString>> getSubjectMandatoriesAttributes(const QString &key);
    QList<QMap<QString, QString>> getSessionMandatoriesAttributes(const QString &key);
    QList<QMap<QString, QString>> getDataMandatoriesAttributes(const QString &key);
    QList<QMap<QString, QString>> getFilesMandatoriesAttributes(const QString &key);
    QList<QMap<QString, QString>> getDerivativeMandatoriesAttributes(const QString &key);

    QMap<QString, QString> getNiftiValuesFromFile(QString niftiFile, QString levelName);

    bool getSubjectAdditionalAttributes(const QString &key, medAbstractSource::datasetAttributes &po_attributes);
    bool getSessionAdditionalAttributes(const QString &key, medAbstractSource::datasetAttributes &po_attributes);
    bool getDataAdditionalAttributes(const QString &key, medAbstractSource::datasetAttributes &po_attributes);
    bool getFilesAdditionalAttributes(const QString &key, medAbstractSource::datasetAttributes &po_attributes);

    bool createBIDSDerivativeSubFolders(QString parentKey, QString derivativeType, QString &newPath);
    void createJsonDerivativeFile(QString sourcePath, QString jsonFilePath, QString derivativeType);

private:
    // members
    medStringParameter *root_path;
    const QString m_Driver;
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
    //QTimer m_timer;
    //void timeManagement();


    QString bids_path; // root of the BIDS tree
    QMap<QString, QMap<QString, QString>> participant_tsv; // Contains the attributes (values) of each patient (keys) in a "participants.tsv" file
    QMap<QString, QMap<QString, QString>> session_tsv; // Contains the attributes (values) of each session (keys) in a "sub-..._session.tsv" file
    QMap<QString, QString> descriptionDatasetValues; // Contains the attributes, and the corresponding values, of the 'dataset_description.json' file
    medTriggerParameter *descriptionButton; // Button on the "Sources" interface for displaying the contents of the 'dataset_description.json' file
    itk::ImageIOBase::Pointer imageIO; // Pointer to read a nifti image : functions medBIDS::getNiftiAttributesForMandatories() and medBIDS::getNiftiValuesFromFile()
    bool sesLevel = true; // true if there is a session level in BIDS structure, false otherwise 
    unsigned int levelDesiredWritable = 4; // Level in which it is possible to write -> level of derivative data files : 4 if sesLevel=true, 3 otherwise

};


/**
 * @fn QJsonObject medBIDS::readJsonFile(QString filePath)
 * @brief extract a QJsonObject containing DICOM tags from the file 
 * @param filePath [in] the path to the file
 * @return QJsonObject with metadata, empty if there is no content
*/

/**
 * @fn QString medBIDS::readJsonArray(QMap<QString, QString> &jsonContent, QJsonArray jsonTab, QString parentKey, QString key)
 * @brief recursive function : read the data contained in a QJsonArray
 * @details allow to read nested QJsonArray
 * @param jsonContent [in] [out] A reference to the Qmap in which the content of the json will be stored
 * @param jsonTab [in] the QJsonArray to be visited
 * @param parentKey [in] used to store the multiple keys on which a value may depend -> recursion
 * @param key [in] the last key in the QJsonObject corresponding to this QJsonArray
 * @return a QString of the array content
*/

/**
 * @fn void medBIDS::getJsonContent(QMap<QString, QString> &jsonContent, QJsonObject jsonObj, QString parentKey)
 * @brief recursive function : read the data contained in a QJsonObject
 * @details allow to read nested QJsonObject
 * @param jsonContent [in] [out] A reference to the Qmap in which the content of the json will be stored
 * @param jsonObj [in] the QJsonObject which is read
 * @param parentKey [in] used to store the multiple keys on which a value may depend -> recursion
*/




/**
 * @fn void medBIDS::existSesLevel(QString subEntitieDir)
 * @brief determine whether it is a 5-level or 4-level tree structure : whether there is a level of sessions entities or not
 * @details set a boolean ('sesLevel') to true if there is a 'ses' entity. 
 * This information is used to adapt the reading of a BIDS structure in a dynamic way.
 * In this version, a tree structure is considered to have the same number of levels in each subject sub-folder.
 * @param subEntitieDir [in] the directory of one of the subjects sub-folders of the tree
*/

/**
 * @fn void medBIDS::getNiftiAttributesForMandatories(QString niftiPath)
 * @brief add the nifti attributes from nifti files to the mandatories attributes of "Files" and "Derivative" levels
 * @param subEntitieDir [in] the path of a nifti file
*/




/**
 * @fn QStringList medBIDS::getAttributesFromTsv(QString &line)
 * @brief read a line of a tsv file to separate the values of each column 
 * @param line [in] the line to be decomposed
 * @return a QStringList containing all the values of the same line
*/

/**
 * @fn bool medBIDS::getPatientsFromTsv()
 * @brief try to open and read the "participants.tsv" file
 * @details completed a QMap<QString, QMap<QString, QString>> 'participant_tsv' containing the information about a subject (patient) 
 * @return true if the file is correclty opened and its content complies with the BIDS standard, false otherwise
*/

/**
 * @fn bool medBIDS::getSessionsFromTsv(QString parentId)
 * @brief try to open and read a "sub-..._sessions.tsv" file of the 'parentId' sub-folder
 * @details completed a QMap<QString, QMap<QString, QString>> 'session_tsv' containing the information about a session of a subject 
 * @param parentId [in] the id of the parent sub-folder corresponding to a subject entity
 * @return true if the file is correclty opened and its content complies with the BIDS standard, false otherwise
*/




/**
 * @fn QList<QMap<QString, QString>> medBIDS::getSubjectMandatoriesAttributes(const QString &key)
 * @brief search for sub-folders corresponding to the subject entity (patient) at the root of the tree structure and determines the value of each mandatory keys in a QMap<QString, QString> for each of them
 * @param key [in] empty QString
 * @details id = sub-..., name = sub-... -> sub-folder name
 * @return a QList containing the QMap of each sub-folder of subject 
*/

/**
 * @fn QList<QMap<QString, QString>> medBIDS::getSessionMandatoriesAttributes(const QString &key)
 * @brief search for sub-folders corresponding to the session entity (session of an exam for a subject) in the 'bids_path + key' path of the tree structure and determines the value of each mandatory keys in a QMap<QString, QString> for each of them
 * @param key [in] contains the id of the parent sub-folder (=subject entity) : key = sub-...
 * @details id = sub-..._ses-..., name = ses-... -> 'ses-...' : session sub-folder name
 * @return a QList containing the QMap of each sub-folder of session 
*/

/**
 * @fn QList<QMap<QString, QString>> medBIDS::getDataMandatoriesAttributes(const QString &key)
 * @brief search for sub-folders corresponding to the data type of the files in the 'bids_path + key' path of the tree structure and determines the value of each mandatory keys in a QMap<QString, QString> for each of them
 * @param key [in] contains the id of the parent sub-folder (=session entity): key = sub-..._ses-...
 * @details id = sub-..._ses-..._<data type>, name = <data type> -> <data type> : data sub-folder name
 * @return a QList containing the QMap of each sub-folder of data type 
*/


/**
 * @fn QMap<QString, QString> medBIDS::getNiftiValuesFromFile(QString niftiFile, QString levelName)
 * @brief read a nifti file to extract its metadata
 * @details the levelName is used to differentiate between two levels with possible different mandatoryKeys : "Files" and "Derivatives"
 * @param niftiFile [in] the path of the file being read
 * @param levelName [in] the level at which the file is located in the tree structure
 * @return a QMap<QString, QString> containing the attributes values for each key
*/

/**
 * @fn QList<QMap<QString, QString>> medBIDS::getFilesMandatoriesAttributes(const QString &key)
 * @brief search for nifti files in the 'bids_path + key' path of the tree structure and determines the value of each mandatory keys in a QMap<QString, QString> for each of them
 * @param key [in] contains the id of the parent sub-folder (=<data type>): key = sub-..._ses-..._<data type>
 * @details id = sub-..._ses-..._<data type>~<filename>
 * name = main entities contained in the file name -> possible entities (in order) : suffix (= data type), acq, run
 * the completed QMap also contains nifti attributes obtained with medBIDS::getNiftiValuesFromFile() function
 * @return a QList containing the QMap of each nifti file
*/

/**
 * @fn QList<QMap<QString, QString>> medBIDS::getDerivativeMandatoriesAttributes(const QString &key)
 * @brief search for nifti derivative data files in the 'bids_path + derivatives/' path of the tree structure and determines the value of each mandatory keys in a QMap<QString, QString> for each of them
 * @param key [in] contains the id of the parent raw file : id = sub-..._ses-..._<data type>~<filename>
 * @details id = sub-..._ses-..._<data type>~<filename>~<derivatie file path> -> <derivative file path> : contains the path from "derivatives" to the end (filename)
 * name = main entities contained in the derivative data file name -> possible entities (in order) : suffix (= data type of segmentation), desc, run
 * @return a QList containing the QMap of each nifti file 
*/




/**
 * @fn bool medBIDS::getSubjectAdditionalAttributes(const QString &key, medAbstractSource::datasetAttributes &po_attributes)
 * @brief determine the additional attributes to be displayed when sub-folders subject level information are shown
 * @details the additional attributes are determined from the fields in the "participants.tsv" file for a subject sub-folder, extracted in the QMap "participants_tsv" filled in by medBIDS::getPatientsFromTsv() function 
 * @param key [in] contains the sub-folder 'subject' id (= id created for the mandatory attributes) 
 * @param po_attributes [in] [out] stores additional values
 * @return true if the additional attributes have been retrieved, false otherwise
*/

/**
 * @fn bool medBIDS::getSessionAdditionalAttributes(const QString &key, medAbstractSource::datasetAttributes &po_attributes)
 * @brief determine the additional attributes to be displayed when sub-folders session level information are shown
 * @details the additional attributes are determined from the fields in the "*_sessions.tsv" file for a session sub-folder, extracted in the QMap "sessions_tsv" filled in by medBIDS::getSessionsFromTsv() function 
 * @param key [in] contains the sub-folder 'session' id (= id created for the mandatory attributes) 
 * @param po_attributes [in] [out] stores additional values
 * @return true if the additional attributes have been retrieved, false otherwise
*/

/**
 * @fn bool medBIDS::getDataAdditionalAttributes(const QString &key, medAbstractSource::datasetAttributes &po_attributes)
 * @brief determine the additional attributes to be displayed when sub-folders data level information are shown
 * @details the additional attributes are determined from the fields in the "*_scans.tsv" file for a data sub-folder, only the fields relating to files dependant on this data type
 * @param key [in] contains the sub-folder 'data' id (= id created for the mandatory attributes) 
 * @param po_attributes [in] [out] stores additional values 
 * @return true if the additional attributes have been retrieved, false otherwise
*/

/**
 * @fn bool medBIDS::getFilesAdditionalAttributes(const QString &key, medAbstractSource::datasetAttributes &po_attributes)
 * @brief determine the additional attributes to be displayed when files information are shown
 * @details the additional attributes are determined from the sidecar json who contains DICOM metadata on the acquisition context
 * @param key [in] contains the 'file' id (= id created for the mandatory attributes) 
 * @param po_attributes [in] [out] stores additional values 
 * @return true if the additional attributes have been retrieved, false otherwise
*/




/**
 * @fn bool medBIDS::addDirectData(QVariant data, levelMinimalEntries &pio_minimalEntries, unsigned int pi_uiLevel, QString parentKey)
 * @brief copy a derivative data file, resulting from a segmentation algorithm, from a generated path to a new path in the local BIDS tree following a path defined by the standard
 * @param data [in] path of derivative data file obtained from segmentation
 * @param pio_minimalEntries [in] [out] gives the "name" field containing the name returned by the segmentation, the other fileds are to be defined
 * @param pi_uiLevel [in] gives the current level
 * @param parentKey [in] contains the id of the parent raw file 
 * @return true if the file has been copied locally, false otherwise
*/

/**
 * @fn bool medBIDS::createBIDSDerivativeSubFolders(QString parentKey, QString derivativeType, QString &newPath)
 * @brief create pipelines sub-folders in which to store derivative data files according to the standard
 * @details create also a json file in sub-folders pipelines to define the segmentation context
 * @param parentKey [in] contains the id of the parent raw file 
 * @param derivativeType [in] the derivation type applied from the segmentation
 * @param newPath [in] [out] defines the path to copy the file according to the sub-folders created
 * @return true if sub-folders have been created correctly, false otherwise
*/

/**
 * @fn void medBIDS::createJsonDerivativeFile(QString sourcePath, QString jsonFilePath, QString derivativeType)
 * @brief create a json sidecar file to the derivative data file containing the path to the raw file used in the creation of this derivative data file
 * @details 'derivativeType' parameter allows to customise the values of the fields in the json file
 * @param sourcePath [in] path of the source raw file
 * @param jsonFilePath [in] the path where the file should be created
 * @param derivativeType [in] the derivation type applied from the segmentation
*/



