/*=========================================================================

 medInria

 Copyright (c) INRIA 2013 - 2019. All rights reserved.
 See LICENSE.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.

=========================================================================*/

#include "medBIDS.h"
#include "medBIDSDataInfoWidget.h"
#include "Worker.h"

#include <medStringParameter.h>
#include <medStringListParameter.h>

#include <itkImageIOFactory.h>
#include <itkMetaDataObject.h>

#include <QDir>
#include <QFile>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QRegExp>
#include <iostream>
#include <QDebug>




std::atomic<int> medBIDS::s_RequestId = 0;

medBIDS::medBIDS()
    : medAbstractSource(), m_Driver("BIDS"),
      m_instanceId(QString()),
      m_online(false), 
      //tree depth
      m_LevelNames({"subject", "session", "data", "files", "derivative"})
{
    root_path = new medStringParameter("Description File", this);
    // root_path->setCaption("JSON file (dataset_description.json)");
    // root_path->setDescription("Select dataset description file from BIDS repository");
    root_path->setDefaultRepresentation(5);

    // Display content of 'dataset_description.json' file
    descriptionButton = new medTriggerParameter("Show file content");
    QObject::connect(descriptionButton, &medTriggerParameter::pushed, [&](bool state){ 
        if(state){
            QMultiMap<QString, QString> additionalInfoWidget;
            auto popupDataInfo = new medBIDSDataInfoWidget(descriptionDatasetValues, additionalInfoWidget);
            popupDataInfo->show();
        }
    });

    m_MandatoryKeysByLevel["Subject"] = QStringList({"id", "name", "description", "type"});
    m_MandatoryKeysByLevel["Session"] = QStringList({"id", "name", "description", "type"});
    m_MandatoryKeysByLevel["Data"] = QStringList({"id", "name", "description", "type"});
    m_MandatoryKeysByLevel["Files"] = QStringList({"id", "SeriesDescription", "description", "type"});   
    m_MandatoryKeysByLevel["Derivative"] = QStringList({"id", "SeriesDescription", "description", "type"});                                           
}

medBIDS::~medBIDS()
{
    connect(false);
}


bool medBIDS::initialization(const QString &pi_instanceId)
{
    bool bRes = !pi_instanceId.isEmpty();

    if(bRes){
        m_instanceId = pi_instanceId;
    }

    return bRes;
}

bool medBIDS::setInstanceName(const QString &pi_instanceName)
{
    bool bRes = !pi_instanceName.isEmpty();

    if (bRes){
        m_instanceName = pi_instanceName;
    }

    return bRes;
}


QJsonObject medBIDS::readJsonFile(QString filePath){
    QJsonObject jsonObj;
    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly)){
        qDebug() << "Cannot read file" << file.fileName();
        return jsonObj;
    }

    QString content(file.readAll());
    file.close();

    // QJsonObject -> simplify the retrieval of values
    if(!content.isEmpty()){
        QJsonDocument jsonDoc = QJsonDocument::fromJson(content.toUtf8());
        jsonObj = jsonDoc.object();
    }

    return jsonObj;
}


QString medBIDS::readJsonArray(QMap<QString, QString> &jsonContent, QJsonArray jsonTab, QString parentKey, QString key)
{
    QString elements = "[";
    for(auto tabValue : jsonTab){
        if(tabValue.isObject()){
            if(!parentKey.isEmpty()){
                key.prepend(parentKey + ":");
            }
            // Nested objects : keep the parent key
            getJsonContent(jsonContent, tabValue.toObject(), key);
        }else if(tabValue.isArray()){
            // Nested arrays
            elements.append(readJsonArray(jsonContent, tabValue.toArray(), parentKey, key) + "; ");
        }else{
            elements.append(tabValue.toVariant().toString() + ", ");
        }
    }
    // formatting for tables
    if(elements.size() > 1){
        elements.chop(2);
        elements.append("]");
    }

    return elements;
}

void medBIDS::getJsonContent(QMap<QString, QString> &jsonContent, QJsonObject jsonObj, QString parentKey)
{
    for(auto key : jsonObj.keys()){
        QJsonValue jsonValue = jsonObj.value(key);
        if(jsonValue.isObject()){
            if(!parentKey.isEmpty()){
                key.prepend(parentKey + ":");
            }
            // Nested objects : keep the parent key 
            getJsonContent(jsonContent, jsonValue.toObject(), key);
        }
        else if(jsonValue.isArray())
        {
            // Read the QJsonArray
            QJsonArray jsonTab(jsonValue.toArray());
            QString arrayContent = readJsonArray(jsonContent, jsonTab, parentKey, key);
            if(!parentKey.isEmpty()){
                key.prepend(parentKey + ":");
            }
            if(arrayContent.size() > 1){
                jsonContent[key] = arrayContent;
            }
        }
        else
        {
            if(!parentKey.isEmpty())
            {
                // nested object : add the parent key to the last key
                key = key.prepend(parentKey + ":");
            }

            QString tagValue = jsonValue.toVariant().toString();
            if(tagValue.contains("\r\n")){
                tagValue = tagValue.replace("\r\n", ", ");
            }
            jsonContent[key] = tagValue;
        }
    }
}

void medBIDS::existSesLevel(QString subEntitieDir){
    // Uniform case -> check one of the 'sub-' sub-folders to see if there is a 'ses-' sub-folder or not
    QDir subDir(subEntitieDir);
    QStringList prefixes({"ses-*"});
    QFileInfoList filesList = subDir.entryInfoList(prefixes, QDir::Dirs);
    // update important data for reading tree structure of different levels
    if(filesList.isEmpty() && getLevelDesiredWritable() == 4){
        // decrement write level and deletion of session level data
        sesLevel = false;
        levelDesiredWritable --;

        m_LevelNames.removeAt(1);
        m_MandatoryKeysByLevel.remove("Session");

    }else if(!filesList.isEmpty() && getLevelDesiredWritable() == 3){
        // increment write level and addition of session level data
        sesLevel = true;
        levelDesiredWritable ++;

        m_LevelNames.insert(1, "session");
        QMap<QString, QStringList>::const_iterator pos = m_MandatoryKeysByLevel.find("Subject");
        m_MandatoryKeysByLevel.insert(++pos, "Session", QStringList({"id", "name", "description", "type"}));
    }
}

void medBIDS::getNiftiAttributesForMandatories(QString niftiPath){
    // Create the pointer from the image
    imageIO = itk::ImageIOFactory::CreateImageIO(niftiPath.toLatin1().constData(), itk::ImageIOFactory::ReadMode);
    if (!imageIO.IsNull()){
        // Required to read the nifti file
        imageIO->SetFileName(niftiPath.toLatin1().constData());
        try{
            // Read the content
            imageIO->ReadImageInformation();
        }catch(itk::ExceptionObject &e){
            qDebug() << e.GetDescription();
        }

        // Get keys of the nifti attributes
        itk::MetaDataDictionary& metaDataDictionary = imageIO->GetMetaDataDictionary();
        std::vector<std::string> niftiKeys = metaDataDictionary.GetKeys();

        QStringList levelAttributes = QStringList({"id", "SeriesDescription", "description", "type"});
        for(auto niftiKey : niftiKeys){
            levelAttributes.append(niftiKey.c_str());
        }

        // Add attributes contained in nifti files in the mandatoryKeys
        m_MandatoryKeysByLevel.insert("Files", levelAttributes);
        m_MandatoryKeysByLevel.insert("Derivative", levelAttributes);
    }
}


bool medBIDS::connect(bool pi_bEnable)
{
    bool bRes = false;
    if (pi_bEnable) // establish connection
    {
        // Extract the root of BIDS structure
        if(!root_path->value().isEmpty()){
            bids_path = root_path->value().remove("dataset_description.json");
            bRes = true;
        }

        // Check that the value (= entry path) is not empty and that the root structure exists locally
        QDir dirPath(bids_path);
        if (dirPath.exists() && bRes)
        {
            // Check that the 'data_description.json' file exists and extracts its content
            QString filePath(root_path->value());
            QFileInfo descriptionFile(filePath);
            if(descriptionFile.exists() && descriptionFile.isFile()){
                bRes = true;
                QJsonObject jsonObj(readJsonFile(descriptionFile.absoluteFilePath()));
                if(!jsonObj.isEmpty()){
                    descriptionDatasetValues.clear();
                    getJsonContent(descriptionDatasetValues, jsonObj, "");
                }
            }else{
                bRes = false;
                qDebug() << "Dataset description file does not exist or is not located at the root of the BIDS structure";
            }

            if(bRes){
                // Checks if there are sub-folders 'sub-' in the root structure
                QDir dir(bids_path);
                QStringList subPrefixes({"sub-*"});
                QFileInfoList filesList = dir.entryInfoList(subPrefixes);
                if(!filesList.isEmpty()){
                    // Find out if there are any session level in the structure
                    existSesLevel(filesList.first().absoluteFilePath());
                    // Check if there is at least one nifti file in the sub-folders of the structure
                    QDirIterator it(bids_path, QDir::Files, QDirIterator::Subdirectories);
                    bool niftiFile = false;
                    while(it.hasNext()) {
                        QString subDirs = it.next();
                        if(subDirs.contains(".nii.gz") || subDirs.contains(".nii")){
                            getNiftiAttributesForMandatories(subDirs);
                            niftiFile = true;
                            break;
                        } 
                    }

                    if(niftiFile){
                        bRes = true;
                    }else{
                        bRes = false;
                        qDebug() << "There are no nifti files in this tree";
                    }

                }else{
                    bRes = false;
                    qDebug() << "There are no sub-folders for subject entities at the root of the BIDS structure";
                }
            }
        }else{
            bRes = false;
            qDebug() << "No dataset description file selected or the root of the BIDS structure does not exist locally";
        }
    }

    m_online = bRes;
    emit connectionStatus(m_online);

    return bRes;
}

QStringList medBIDS::getAttributesFromTsv(QString &line){
    int quote = 0;

    // Modify spaces between quotes to '~'
    for(int idx = 0; idx < line.size(); ++idx){
        if(line.at(idx) == "\""){
            quote = !quote;
        }
        if(line.at(idx) == " " && quote == 1){
            line.replace(idx, 1, "~");
        }
    }

    // Previous changes simplify the separation of values for each column
    QStringList attributes = line.split(QRegExp("\\s+"));
    // Replace '~' by " ", and delete quotes
    for(int i=0; i < attributes.size(); ++i){
        attributes[i].replace("~", " ");
        attributes[i].remove("\"");
    }

    return attributes;
}

bool medBIDS::getPatientsFromTsv(){
    QFile tsvFile(bids_path + "participants.tsv");
    if (!tsvFile.open(QIODevice::ReadOnly)){
        qDebug() << "Cannot read file" << tsvFile.fileName();
        return false;
    }

    // Read the file header to check if there is at least the 'participant_id' column (1st position)
    QTextStream in(&tsvFile); 
    QString header = in.readLine();
    QStringList keys = header.split(QRegExp("\\s+"));
    if(keys[0].compare("participant_id") != 0){
        qDebug() << "non-compliant file";
        return false;
    }

    QMap<QString, QString> patientValues;
    // Read the file line by line
    while(!in.atEnd()){
        QString line = in.readLine();
        QStringList values = getAttributesFromTsv(line);

        if(keys.size() == values.size()){
            for(int i=0; i < keys.size(); ++i){
                patientValues[keys[i]] = values[i];
            }
        }

        // Key = "participant_id"; valeur = QMap with value (value) of each column (key)
        participant_tsv[values[0]] = patientValues;
    }

    return true;
}

bool medBIDS::getSessionsFromTsv(QString parentId){
    // Create the path to be placed in the sub-folder subject of the 'parentId'
    QString subPath(bids_path + parentId + "/");
    QDir dir(subPath);

    QStringList filters({"*.tsv"});
    QFileInfoList filesList = dir.entryInfoList(filters);
    if(filesList.isEmpty() || filesList.size() != 1){
        qDebug() << "There is no sessions file";
        return false;
    }

    QFile tsvFile(subPath + filesList[0].fileName());
    if (!tsvFile.open(QIODevice::ReadOnly)){
        qDebug() << "Cannot read file" << tsvFile.fileName();
        return false;
    }

    // Read the file header to check if there is at least the 'session_id' column (1st position)
    QTextStream in(&tsvFile); 
    QString header = in.readLine();
    QStringList keys = header.split(QRegExp("\\s+"));
    if(keys[0].compare("session_id") != 0){
        qDebug() << "non-compliant file";
        return false;
    }

    // Read the file line by line
    QMap<QString, QString> sessionValues;
    while(!in.atEnd()){
        QString line = in.readLine();
        QStringList values = getAttributesFromTsv(line);

        if(keys.size() == values.size()){
            for(int i=0; i < keys.size(); ++i){
                sessionValues[keys[i]] = values[i];
            }
        }

        // Key = sub-..._"session_id", valeur = QMap with value (value) of each column (key)
        QString id = parentId + "_" + values[0];
        session_tsv[id] = sessionValues;
    }

    return true;
}


QList<medAbstractParameter *> medBIDS::getAllParameters()
{
    QList<medAbstractParameter *> paramListRes;

    paramListRes.push_back(root_path);
    paramListRes.push_back(descriptionButton);

    return paramListRes;
}


QList<medAbstractParameter *> medBIDS::getCipherParameters()
{
    return QList<medAbstractParameter *>();
}


QList<medAbstractParameter *> medBIDS::getVolatilParameters()
{
    return QList<medAbstractParameter *>();
}


QList<medAbstractParameter *> medBIDS::getFilteringParameters()
{
    return {};
}

bool medBIDS::isWritable()
{
    return false;
}

 
bool medBIDS::isLocal()
{
    return true;
}

 
bool medBIDS::isCached()
{
    return false;
}

 
bool medBIDS::isOnline()
{
    return m_online;
}


bool medBIDS::isFetchByMinimalEntriesOrMandatoryAttributes()
{
    return false;
}


QString medBIDS::getInstanceName()
{
    return m_instanceName;
}


QString medBIDS::getInstanceId()
{
    return m_instanceId;
}

 
unsigned int medBIDS::getLevelCount()
{
    return m_LevelNames.size();
}

 
unsigned int medBIDS::getLevelDesiredWritable()
{
    return levelDesiredWritable;
}

 
QStringList medBIDS::getLevelNames()
{
    return m_LevelNames;
}

 
QString medBIDS::getLevelName(unsigned int pi_uiLevel)
{
    QString retVal; 
    if (pi_uiLevel>0 && pi_uiLevel < static_cast<unsigned int>(m_LevelNames.size()))
    {
        retVal = m_LevelNames.value((int)pi_uiLevel);
    }
    return retVal;
}


bool  medBIDS::isLevelWritable(unsigned int pi_uiLevel)
{
    return pi_uiLevel == levelDesiredWritable;
}

QStringList medBIDS::getMandatoryAttributesKeys(unsigned int pi_uiLevel)
{
    // If there are no sessions in the tree structure, skip the level 1
    if(pi_uiLevel >= 1 && !sesLevel){
        pi_uiLevel ++;
    }

    switch (pi_uiLevel)
    {
        case 0:
            return m_MandatoryKeysByLevel["Subject"];//return {"id", "name", "description", "type"};
        case 1:
            return m_MandatoryKeysByLevel["Session"];//return {"id", "name", "description", "type"};
        case 2:
            return m_MandatoryKeysByLevel["Data"];//return {"id", "name", "description", "type"};
        case 3:
            return m_MandatoryKeysByLevel["Files"];//return {"id", "SeriesDescription", "description", "type"};
        case 4:
            return m_MandatoryKeysByLevel["Derivative"];//return {"id", "SeriesDescription", "description", "type"};
        default:
            return QStringList();
    }
}

 
QList<medAbstractSource::levelMinimalEntries> medBIDS::getMinimalEntries(unsigned int pi_uiLevel, QString parentId)
{
    QList<levelMinimalEntries> entries;

    // If there are no sessions in the tree structure, skip the level 1
    if(pi_uiLevel >= 1 && !sesLevel){
        pi_uiLevel ++;
    }

    switch (pi_uiLevel)
    {
        case 0:
            entries = getSubjectMinimalEntries(parentId);
            break;
        case 1:
            entries = getSessionMinimalEntries(parentId);
            break;
        case 2:
            entries = getDataMinimalEntries(parentId);
            break;
        case 3:
            entries = getFilesMinimalEntries(parentId);
            break;
        case 4:
            entries = getDerivativeMinimalEntries(parentId);
            break;
        default:
            break;
    }

    return entries;
}


QList<medAbstractSource::levelMinimalEntries> medBIDS::getSubjectMinimalEntries(QString &key)
{
    QList<levelMinimalEntries> subjectEntries;

    // Check whether the structure root has a "participants.tsv" file and extracts data from it
    bool tsvExists = getPatientsFromTsv();

    // Is placed at the root of the structure to retrieve all sub-folders containing the prefix "sub-"
    QDir dir(bids_path);
    QStringList prefixes({"sub-*"});
    QFileInfoList filesList = dir.entryInfoList(prefixes, QDir::Dirs);
    for(auto file : filesList){
        // Check that it is a folder
        if(!file.isFile()){

            if(tsvExists){
                // Retrieve values associated to the patient 'file.fileName()' 
                if(!participant_tsv.contains(file.fileName())){
                    qDebug() << "This subject sub-folder is not mentioned in the corresponding tsv file";
                }
            }

            levelMinimalEntries entry;
            entry.key = file.fileName();
            entry.name = file.fileName();
            entry.description = "";
            entry.type = entryType::folder;

            subjectEntries.append(entry);
        }
    }
    
    return subjectEntries;
}


QList<medAbstractSource::levelMinimalEntries> medBIDS::getSessionMinimalEntries(QString &parentId)
{
    QList<levelMinimalEntries> sessionEntries;

    // Check whether the subject sub-folder has a "..._sessions.tsv" file and extracts data from it
    bool tsvExists = getSessionsFromTsv(parentId);

    // Is placed in the parent 'subject' sub-folder to retrieve all sub-folders containing the prefix "ses-"
    QDir dir(bids_path + parentId + "/");
    QStringList prefixes({"ses-*"});
    QFileInfoList filesList = dir.entryInfoList(prefixes);
    for(auto file : filesList){
        // Check that it is a folder
        if(!file.isFile()){
            QString sessionId = parentId + "_" + file.fileName();
    
            if(tsvExists){
                // Retrieve values associated to the session 'sessionId' 
                if(!session_tsv.contains(sessionId)){
                    qDebug() << "This session sub-folder is not mentioned in the corresponding tsv file";
                }
            }

            levelMinimalEntries entry;
            // parentId = 'sub-...'
            entry.key = parentId + "_" + file.fileName();
            entry.name = file.fileName();
            entry.description = "";
            entry.type = entryType::folder;

            sessionEntries.append(entry);
        }
    }
    
    return sessionEntries;
}


QList<medAbstractSource::levelMinimalEntries> medBIDS::getDataMinimalEntries(QString &parentId)
{
    QList<levelMinimalEntries> dataEntries;

    // Is placed in the parent 'session' sub-folder to retrieve all data (data type) sub-folders
    QString subDirs = parentId.split("_").join("/");
    QDir dir(bids_path + subDirs);
    QFileInfoList filesList = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for(auto file : filesList){
        // Check that it is a folder
        if(!file.isFile()){

            levelMinimalEntries entry;
            // parentId = 'sub-..._ses-...'
            entry.key = parentId + "_" + file.fileName();
            entry.name = file.fileName();
            entry.description = "";
            entry.type = entryType::folder;

            dataEntries.append(entry);
        }
    }
    
    return dataEntries;
}


QList<medAbstractSource::levelMinimalEntries> medBIDS::getFilesMinimalEntries(QString &parentId)
{
    QList<levelMinimalEntries> filesEntries;

    // Is placed in the parent 'data' sub-folder to retrieve all nifti files
    QString subDirs = parentId.split("_").join("/");
    QString filePath(bids_path + subDirs + "/");
    QDir dir(filePath);
    QStringList prefixes({"*.nii.gz", "*.nii"});
    QFileInfoList filesList = dir.entryInfoList(prefixes, QDir::Files);
    for(auto file : filesList){
        // Check that it is a file
        if(file.isFile()){

            QString fileName = file.fileName().split(".")[0];

            // Retains some of the entities to make it easier to identify files in medInria
            QString fileNameSuffixType = fileName.split("_").last();
            int acqIndex = fileName.indexOf("acq");
            if(acqIndex != -1){
                QString endName = fileName.mid(acqIndex);
                QString acqEntitie = endName.split("_")[0];
                QString acqName = acqEntitie.split("-")[1];

                fileNameSuffixType.append(" - " + acqName);
            }

            int runIndex = fileName.indexOf("run");
            if(runIndex != -1){
                QString endName = fileName.mid(runIndex);
                QString numRun = endName.split("_")[0];

                fileNameSuffixType.append(" - " + numRun);
            }

            levelMinimalEntries entry;
            // key = 'sub-..._ses-..._<data type>'
            // Separation with '~' to simplify image retrieval in medBIDS::getDirectData() function
            entry.key = parentId + "~" + fileName;
            entry.name = fileNameSuffixType;
            entry.description = "";
            entry.type = entryType::dataset;

            filesEntries.append(entry);
        }
    }
    
    return filesEntries;
}

QList<medAbstractSource::levelMinimalEntries> medBIDS::getDerivativeMinimalEntries(QString &parentId)
{
    
    QList<levelMinimalEntries> derivativeEntries;
    
    QStringList dir(parentId.split("~"));
    QString subEntitiesPath(dir[0].replace("_", "/"));
    QString fileName(dir[1]);
    QString extension(".nii.gz");

    // Know the extension of the parent file
    QFile niftiParentFile(bids_path + subEntitiesPath + "/" + fileName + extension);
    if(!niftiParentFile.exists()){
        extension = ".nii";
    }
    QString parentFile(subEntitiesPath + "/" + fileName + extension);

    QStringList derivativeFiles;
    // Browse the pipelines of the "derivatives" sub-folder
    QDir derivativesDir(bids_path + "derivatives/");
    QFileInfoList derivativeFoldersList = derivativesDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for(auto folder : derivativeFoldersList){
        // Check that it is a folder
        if(!folder.isFile())
        {
            QDir derivativesFilesDir(derivativesDir.absolutePath() + '/' + folder.fileName());
            if(derivativesFilesDir.exists())
            {
                // Is placed in each pipeline to retrieve all .json files
                QDirIterator it(derivativesFilesDir.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
                while(it.hasNext()){
                    QString subFilePath = it.next();
                    QString file(subFilePath.split("/").last());
                    if(file.contains(".json"))
                    {
                        // Find in each sidecar json file which nifti derivative file is built from the parent raw file
                        QJsonObject jsonObj = readJsonFile(subFilePath);
                        if(!jsonObj.isEmpty())
                        {
                            // Retrieve "Sources" ou "RawSources" fields in the json file to compare with the parent raw file name
                            if(jsonObj.contains("Sources"))
                            {
                                QJsonArray sourcesFile = jsonObj.value("Sources").toArray();
                                for(auto source : sourcesFile){
                                    QString derivativeSourceFile = source.toString().split(":").last();
                                    if(derivativeSourceFile.compare(parentFile) == 0){
                                        derivativeFiles.append(subFilePath);
                                    }
                                }
                            }
                            else if(jsonObj.contains("RawSources"))
                            {
                                QJsonArray sourcesFile = jsonObj.value("RawSources").toArray();
                                for(auto source : sourcesFile){
                                    QString derivativeSourceFile = source.toString().split(":").last();
                                    if(derivativeSourceFile.compare(parentFile) == 0){
                                        derivativeFiles.append(subFilePath);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if(!derivativeFiles.isEmpty()){
        for(auto devFile : derivativeFiles)
        {
            // Extract the subpath from "/derivatives" to the end to complete the 'id' field for minimalEntries
            int derivativeIndex = devFile.indexOf("derivatives");
            QString derivativeFilePath;
            if(derivativeIndex != -1){
                devFile.chop(5);
                derivativeFilePath = devFile.mid(derivativeIndex);
            }

            // Extract the label of the 'desc' entity to complete the 'name' field of minimalEntries
            int descriptionIndex = devFile.indexOf("desc");
            QString devDescriptionName;
            if(descriptionIndex != -1){
                QString endName = devFile.mid(descriptionIndex);
                QString descName = endName.split("_")[0];
                devDescriptionName = descName.split("-")[1];
            }

            // Extract the 'run' entity to maintain file differentiation : only the 'run' added after 'desc' entity
            int runIndex = devFile.lastIndexOf("run");
            if(runIndex != -1 && (runIndex > descriptionIndex && descriptionIndex != -1)){
                QString endName = devFile.mid(runIndex);
                QString numRun = endName.split("_")[0];
                devDescriptionName.append(" - " + numRun);
            }

            levelMinimalEntries entry;
            // parentId = 'sub-..._ses-..._<data type>~<parent file name>'
            entry.key = parentId + "_" + derivativeFilePath;
            entry.name = devDescriptionName;
            entry.description = "";
            entry.type = entryType::dataset;

            derivativeEntries.append(entry);
        }
    }
    else
    {
        qDebug() << "No correspondance found in the derived datasets";
    }

    return derivativeEntries;
}

 
QList<QMap<QString, QString>> medBIDS::getMandatoryAttributes(unsigned int pi_uiLevel, QString parentId)
{
    QList< QMap<QString, QString>> res;

    // If there are no sessions in the tree structure, skip level 1
    if(pi_uiLevel >= 1 && !sesLevel){
        pi_uiLevel ++;
    }

    switch (pi_uiLevel)
    {
        case 0:
            res = getSubjectMandatoriesAttributes(parentId);
            break;
        case 1:
            res = getSessionMandatoriesAttributes(parentId);
            break;
        case 2:
            res = getDataMandatoriesAttributes(parentId);
            break;
        case 3:
            res = getFilesMandatoriesAttributes(parentId);
            break;
        case 4:
            res = getDerivativeMandatoriesAttributes(parentId);
            break;
        default:
            break;
    }

    return res;
}


QList<QMap<QString, QString>> medBIDS::getSubjectMandatoriesAttributes(const QString &key)
{
    QList< QMap<QString, QString>> subjectAttributes;

    // Check whether the structure root has a "participants.tsv" file and extracts data from it
    bool tsvExists = getPatientsFromTsv();

    // Is placed at the root of the structure to retrieve all sub-folders containing the prefix "sub-"
    QDir dir(bids_path);
    QStringList prefixes({"sub-*"});
    QFileInfoList filesList = dir.entryInfoList(prefixes, QDir::Dirs);
    for(auto file : filesList){
        // Check that it is a folder
        if(!file.isFile()){

            if(tsvExists){
                // Retrieve values associated to the patient 'file.fileName()' 
                if(!participant_tsv.contains(file.fileName())){
                    qDebug() << "This subject sub-folder is not mentioned in the corresponding tsv file";
                }
            }

            QMap<QString, QString> subjectMap;
            subjectMap["id"] = file.fileName();
            subjectMap["name"] = file.fileName();
            subjectMap["description"] = "";
            subjectMap["type"] = entryTypeToString(entryType::folder);

            subjectAttributes.append(subjectMap);
        }
    }

    return subjectAttributes;
}


QList<QMap<QString, QString>> medBIDS::getSessionMandatoriesAttributes(const QString &key)
{
    QList<QMap<QString, QString>> sessionAttributes;

    // Check whether the subject sub-folder has a "..._sessions.tsv" file and extracts data from it
    bool tsvExists = getSessionsFromTsv(key);

    // Is placed in the 'subject' sub-folder to retrieve all sub-folders containing the prefix "ses-"
    QDir dir(bids_path + key + "/");
    QStringList prefixes({"ses-*"});
    QFileInfoList filesList = dir.entryInfoList(prefixes);
    for(auto file : filesList){
        // Check that it is a folder
        if(!file.isFile()){
            QString sessionId = key + "_" + file.fileName();
    
            if(tsvExists){
                // Retrieve values associated to the session 'sessionId' 
                if(!session_tsv.contains(sessionId)){
                    qDebug() << "This session sub-folder is not mentioned in the corresponding tsv file";
                }
            }

            QMap<QString, QString> sessionMap;
            // key = 'sub-...'
            sessionMap["id"] = key + "_" + file.fileName();
            sessionMap["name"] = file.fileName();
            sessionMap["description"] = "";
            sessionMap["type"] = entryTypeToString(entryType::folder);

            sessionAttributes.append(sessionMap);
        }
    }

    return sessionAttributes;
}


QList<QMap<QString, QString>> medBIDS::getDataMandatoriesAttributes(const QString &key)
{
    QList< QMap<QString, QString>> dataAttributes;

    // Is placed in the 'session' sub-folder to retrieve all data (data type) sub-folders
    QString subDirs = key.split("_").join("/");
    QDir dir(bids_path + subDirs);
    QFileInfoList filesList = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for(auto file : filesList){
        // Check that it is a folder
        if(!file.isFile()){ 

            QMap<QString, QString> dataMap;
            // key = 'sub-..._ses-...'
            dataMap["id"] = key + "_" + file.fileName();
            dataMap["name"] = file.fileName();
            dataMap["description"] = "";
            dataMap["type"] = entryTypeToString(entryType::folder);

            dataAttributes.append(dataMap);
        }
    }

    return dataAttributes;
}

QMap<QString, QString> medBIDS::getNiftiValuesFromFile(QString niftiFile, QString levelName)
{
    QMap<QString, QString> niftiValues;

    // Retrieves all the keys from the LevelName, whose nifti keys
    QStringList levelKeys =  m_MandatoryKeysByLevel[levelName];
    
    // Create the pointer from the image
    imageIO = itk::ImageIOFactory::CreateImageIO(niftiFile.toLatin1().constData(), itk::ImageIOFactory::ReadMode);
    if (!imageIO.IsNull()){
        // Required to read the nifti file
        imageIO->SetFileName(niftiFile.toLatin1().constData());
        try{
            // Read the content
            imageIO->ReadImageInformation();
        }catch(itk::ExceptionObject &e){
            qDebug() << e.GetDescription();
        }

        // Get the values for each nifti key
        itk::MetaDataDictionary& metaDataDictionary = imageIO->GetMetaDataDictionary();
        for(int i=4; i < levelKeys.size(); i++){
            std::string niftiValue;
            itk::ExposeMetaData(metaDataDictionary, levelKeys[i].toStdString(), niftiValue);
            niftiValues[levelKeys[i]] = niftiValue.c_str();
        }
    }

    return niftiValues;
}

QList<QMap<QString, QString>> medBIDS::getFilesMandatoriesAttributes(const QString &key)
{
    QList< QMap<QString, QString>> filesAttributes;

    // Is placed in the 'data' (data type) sub-folder to retrieve all nifti files
    QString subDirs = key.split("_").join("/");
    QString filePath(bids_path + subDirs + "/");
    QDir dir(filePath);
    QStringList prefixes({"*.nii.gz", "*.nii"});
    QFileInfoList filesList = dir.entryInfoList(prefixes, QDir::Files);
    for(auto file : filesList){
        // Check that it is a file
        if(file.isFile()){

            QMap<QString, QString> filesMap;
            QString fileName = file.fileName().split(".")[0];
            // key = 'sub-..._ses-..._<data type>'
            // Separation with '~' to simplify image retrieval in medBIDS::getDirectData() function
            filesMap["id"] = key + "~" + fileName;
            filesMap["description"] = "";
            filesMap["type"] = entryTypeToString(entryType::dataset);

            // Retains some of the entities to make it easier to identify files in medInria
            QString fileNameSuffixType = fileName.split("_").last();
            int acqIndex = fileName.indexOf("acq");
            if(acqIndex != -1){
                QString endName = fileName.mid(acqIndex);
                QString acqEntitie = endName.split("_")[0];
                QString acqName = acqEntitie.split("-")[1];

                fileNameSuffixType.append(" - " + acqName);
            }

            int runIndex = fileName.indexOf("run");
            if(runIndex != -1){
                QString endName = fileName.mid(runIndex);
                QString numRun = endName.split("_")[0];

                fileNameSuffixType.append(" - " + numRun);
            }

            filesMap["SeriesDescription"] = fileNameSuffixType;

            // Retrieve nifti metadata to complete the mandatoriesAttributes
            QString niftiFile = filePath + file.fileName();
            filesMap.insert(getNiftiValuesFromFile(niftiFile, "Files"));

            filesAttributes.append(filesMap);
        }
    }

    return filesAttributes;
}

QList<QMap<QString, QString>> medBIDS::getDerivativeMandatoriesAttributes(const QString &key)
{
    QList< QMap<QString, QString>> derivativeAttributes;

    QStringList dir(key.split("~"));
    QString subEntitiesPath(dir[0].replace("_", "/"));
    QString fileName(dir[1]);
    QString extension(".nii.gz");

    // Know the extension of the parent raw file
    QFile niftiParentFile(bids_path + subEntitiesPath + "/" + fileName + extension);
    if(!niftiParentFile.exists()){
        extension = ".nii";
    }
    QString parentFile(subEntitiesPath + "/" + fileName + extension);

    QStringList derivativeFiles;
    // Browse the pipelines of the "derivatives" sub-folder
    QDir derivativesDir(bids_path + "derivatives/");
    QFileInfoList derivativeFoldersList = derivativesDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for(auto folder : derivativeFoldersList){
        // Check that it is a folder
        if(!folder.isFile())
        {
            QDir derivativesFilesDir(derivativesDir.absolutePath() + '/' + folder.fileName());
            if(derivativesFilesDir.exists())
            {
                // Is placed in each pipeline to retrieve all .json files
                std::cout << derivativesFilesDir.absolutePath().toStdString() << std::endl;
                QDirIterator it(derivativesFilesDir.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
                while(it.hasNext()){
                    QString subFilePath = it.next();
                    QString file(subFilePath.split("/").last());
                    if(file.contains(".json"))
                    {
                        // Find in each sidecar json file which nifti derivative file is built from the parent raw file
                        QJsonObject jsonObj = readJsonFile(subFilePath);
                        if(!jsonObj.isEmpty())
                        {
                            // Retrieve "Sources" ou "RawSources" fields in the json file to compare with the parent raw file name
                            if(jsonObj.contains("Sources"))
                            {
                                QJsonArray sourcesFile = jsonObj.value("Sources").toArray();
                                for(auto source : sourcesFile){
                                    QString derivativeSourceFile = source.toString().split(":").last();
                                    if(derivativeSourceFile.compare(parentFile) == 0){
                                        derivativeFiles.append(subFilePath);
                                    }
                                }
                            }
                            else if(jsonObj.contains("RawSources"))
                            {
                                QJsonArray sourcesFile = jsonObj.value("RawSources").toArray();
                                for(auto source : sourcesFile){
                                    QString derivativeSourceFile = source.toString().split(":").last();
                                    if(derivativeSourceFile.compare(parentFile) == 0){
                                        derivativeFiles.append(subFilePath);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if(!derivativeFiles.isEmpty()){
        for(auto devFile : derivativeFiles)
        {
            QMap<QString, QString> derivativeMap;

            // Extract the subpath from "/derivatives" to the end to complete the 'id' field for minimalEntries
            int derivativeIndex = devFile.indexOf("derivatives");
            QString derivativeFilePath;
            if(derivativeIndex != -1){
                devFile.chop(5);
                derivativeFilePath = devFile.mid(derivativeIndex);
            }

            // Extract the label of the 'desc' entity to complete the 'name' field of minimalEntries
            int descriptionIndex = devFile.indexOf("desc");
            QString devDescriptionName;
            if(descriptionIndex != -1){
                QString endName = devFile.mid(descriptionIndex);
                QString descName = endName.split("_")[0];
                devDescriptionName = descName.split("-")[1];
            }

            // Extract the 'run' entity to maintain file differentiation : only the 'run' added after 'desc' entity
            int runIndex = devFile.lastIndexOf("run");
            if(runIndex != -1 && (runIndex > descriptionIndex && descriptionIndex != -1)){
                QString endName = devFile.mid(runIndex);
                QString numRun = endName.split("_")[0];
                devDescriptionName.append(" - " + numRun);
            }

            // key = 'sub-..._ses-..._<data type>~<parent file name>'
            derivativeMap["id"] = key + "~" + derivativeFilePath;
            derivativeMap["SeriesDescription"] = devDescriptionName;
            derivativeMap["description"] = "";
            derivativeMap["type"] = entryTypeToString(entryType::dataset);

            // Retrieve nifti metadata to complete the mandatoriesAttributes
            derivativeMap.insert(getNiftiValuesFromFile(devFile + ".nii.gz", "Derivative"));
            derivativeAttributes.append(derivativeMap);
        }
    }
    else
    {
        qDebug() << "No correspondance found in the derived datasets";
    }

    return derivativeAttributes;
}

 
bool medBIDS::getAdditionalAttributes(unsigned int pi_uiLevel, QString id, datasetAttributes &po_attributes)
{
    // If there are no sessions in the tree structure, skip level 1
    if(pi_uiLevel >= 1 && !sesLevel){
        pi_uiLevel ++;
    }

    bool res = false;
    switch (pi_uiLevel)
    {
        case 0:
            res = getSubjectAdditionalAttributes(id, po_attributes);
            break;
        case 1:
            res = getSessionAdditionalAttributes(id, po_attributes);
            break;
        case 2:
            res = getDataAdditionalAttributes(id, po_attributes);
            break;
        case 3:
            res = getFilesAdditionalAttributes(id, po_attributes);
            break;
        case 4:
            res = true;
            break;
        default:
            break;
    }

    return res;
}

bool medBIDS::getSubjectAdditionalAttributes(const QString &key, medAbstractSource::datasetAttributes &po_attributes)
{
    if(!participant_tsv.contains(key)){
        return false;
    }
    
    // If it exists, retrieve the attributes and values for the patient with id 'key' in the .tsv file
    QMap<QString, QString> attributes = participant_tsv.value(key);

    // Get the mandatoryKeys
    QStringList subjectKeys = getMandatoryAttributesKeys(0);

    for(auto key : attributes.keys()){
        // if the key is not already displayed in the mandatories, then display in additional
        if(!subjectKeys.contains(key)){
            po_attributes.values[key] = attributes[key];
        }
    }

    return true;
}

bool medBIDS::getSessionAdditionalAttributes(const QString &key, medAbstractSource::datasetAttributes &po_attributes)
{
    if(!session_tsv.contains(key)){
        return false;
    }

    // If it exists, retrieve the attributes and values for the session with id 'key' in the .tsv file
    QMap<QString, QString> attributes = session_tsv.value(key);

    // Get the mandatoryKeys
    QStringList sessionKeys = getMandatoryAttributesKeys(1);

    for(auto key : attributes.keys()){
        // if the key is not already displayed in the mandatories, then display in additional
        if(!sessionKeys.contains(key)){
            po_attributes.values[key] = attributes[key];
        }
    }

    return true;
}

bool medBIDS::getDataAdditionalAttributes(const QString &key, medAbstractSource::datasetAttributes &po_attributes)
{
    // Isolate the data type
	QStringList entities = key.split("_");
	QString folderDataType = entities.last();
	entities.removeLast();
	
    // Search for a possible "*_scans.tsv" file
	QString parentDir = entities.join("/");
	QDir dir(bids_path + parentDir);
	QStringList prefixes({"*_scans.tsv"});
    QFileInfoList filesList = dir.entryInfoList(prefixes);
    if(filesList.isEmpty() || filesList.size() != 1){
    	qDebug() << "There is no scans file";
        return false;
    }

    QFileInfo scansFile = filesList.first();
    QFile tsvFile(scansFile.absoluteFilePath());
    if (!tsvFile.open(QIODevice::ReadOnly)){
        qDebug() << "Cannot read file" << tsvFile.fileName();
        return false;
    }
    
    // Get the header
    QTextStream in(&tsvFile); 
    QString header = in.readLine();
    QStringList keys = header.split(QRegExp("\\s+"));
    if(keys[0].compare("filename") != 0){
    	qDebug() << "non-compliant file";
        return false;
    }
    
    // Search for lines containing this data type in filename field
    QStringList values;
    int numOccur = 1;
    while(!in.atEnd()){
        QString line = in.readLine();
        values = getAttributesFromTsv(line);
        QString filename = values.first();
        QString fileDataType = filename.split("/").first();

        QFile filePath(bids_path + parentDir + '/' + filename);
        // Search for all files dependant on the parent data type
        if(fileDataType.compare(folderDataType) == 0){
            // Check that the file exists locally
            if(filePath.exists()){
                QString tsvKey, tsvValue;
                for(int i=0; i < keys.size(); i++){
                    tsvKey = keys[i];
                    tsvKey.append("_" + QString::number(numOccur));
                    po_attributes.values[tsvKey] = values[i];
                    
                }
                numOccur ++;
            }else{
                qDebug() << "The" << filename << "file mentioned does not exist in the" << folderDataType << "data type folder";
            }
        }
    }
    
    return true;
}

bool medBIDS::getFilesAdditionalAttributes(const QString &key, medAbstractSource::datasetAttributes &po_attributes)
{
    // Recreate the path to the sidecar .json file
    QStringList dir(key.split("~"));
    QString filePath(dir[0].split("_").join("/"));
    QString fileName(dir[1] + ".json");

    // Read the content to add it to the additional attributes
    QString path(bids_path + filePath + "/" + fileName);
    QJsonObject jsonObj = readJsonFile(path);
    if(!jsonObj.isEmpty()){
        QMap<QString, QString> jsonContent;
        getJsonContent(jsonContent, jsonObj, "");
        for(auto key : jsonContent.keys()){
            po_attributes.values[key] = jsonContent.value(key);
        }

        return true;
    }

    return false;
}

 
QVariant medBIDS::getDirectData(unsigned int pi_uiLevel, QString key)
{
    QVariant res;
    // Get images at levels "Files" and "Derivatve" : level of recovery adapted to the size of the tree
    if(pi_uiLevel == getLevelDesiredWritable()-1 || pi_uiLevel == getLevelDesiredWritable()){
        // '.split("~")' to retrieve 'filePath' and 'fileName' : add the nifti extension to fileName
        QStringList dir(key.split("~"));
        QString filePath(dir[0].split("_").join("/"));
        QString fileName(dir[1] + ".nii.gz");
        //QString extension(".nii.gz");

        QFile niftiFile(bids_path + filePath + "/" + fileName);
        if(!niftiFile.exists()){
            fileName = dir[1] + ".nii";
            //extension = ".nii";
        }
    
        // If last level, display derivative file
        QString displayFile(bids_path + filePath + "/" + fileName);
        if(isLevelWritable(pi_uiLevel)){
            //QString derivativeFile(dir[2] + extension);
            QString derivativeFile(dir[2] + ".nii.gz");
            displayFile = bids_path + "/" + derivativeFile;
        }

        // Tell the program the path of the file to be displayed
        res = QVariant(displayFile);
    }
    return res;
}


int medBIDS::getAssyncData(unsigned int pi_uiLevel, QString id)
{
    int iRequestId = ++s_RequestId; 
    return iRequestId;
}


void medBIDS::abort(int pi_iRequest)
{
}


int medBIDS::getIOInterface()
{
    return IO_FILE;
}

 
QMap<QString, QStringList> medBIDS::getTypeAndFormat()
{
    m_SupportedTypeAndFormats["Image"] = QStringList({".nii.gz", ".nii"});

    return m_SupportedTypeAndFormats;
}


bool medBIDS::createBIDSDerivativeSubFolders(QString parentKey, QString derivativeType, QString &newPath)
{
    // Define the derivative type according to the segmentation
    QString derivativeDescription;
    if(derivativeType == "mask"){
        derivativeDescription = "Define a polygon ROI mask";
    }

    // Create the pipeline path
    QString pipeline = bids_path + "derivatives/" + derivativeType + "/";
    QDir newDerivativeDir;
    if(newDerivativeDir.mkpath(pipeline))
    {
        // Create the json sidecar file content to describe the pipeline content
        QJsonObject jsonObject;
        jsonObject.insert("Name", QJsonValue("Segmentation outputs"));
        jsonObject.insert("BIDSVersion", QJsonValue("2.1.7"));
        jsonObject.insert("DatasetType", QJsonValue("derivative"));

        QJsonObject jsonObj1;
        jsonObj1.insert("Name", QJsonValue(derivativeType));

        QJsonObject jsonObj2;
        jsonObj2.insert("Name", QJsonValue("Manual"));
        jsonObj2.insert("Description", QJsonValue(derivativeDescription));

        QJsonArray jsonArray;
        jsonArray.append(jsonObj1);
        jsonArray.append(jsonObj2);

        jsonObject.insert("GeneratedBy", jsonArray);

        QJsonDocument jsonDoc(jsonObject);

        // Write the content into the file
        QFile file(pipeline + "dataset_description.json");
        if(!file.open(QIODevice::WriteOnly)){
            qDebug() << "Unable to open file" << file.fileName() << "in write mode";
        }

        int writtenFile = file.write(jsonDoc.toJson());
        file.close();

        if(writtenFile == -1){
            qDebug() << "Writing error to the file" << file.fileName();
        }

        // Retrieves the parent BIDS levels (sub-folders) from 'parentKey' and creates the same path for the derivatives in correspondant pipeline
        QStringList subDirs = parentKey.split("~");
        QStringList pathDirs = subDirs[0].split("_");
        newPath = pipeline + pathDirs.join("/");
        return newDerivativeDir.mkpath(newPath);
    }

    return false;
}

void medBIDS::createJsonDerivativeFile(QString sourcePath, QString jsonFilePath, QString derivativeType){
    // Create the content of the json sidecar file to the derivative file
    QFile jsonFile(jsonFilePath);
    if(!jsonFile.open(QIODevice::WriteOnly)){
        qDebug() << "Unable to open file" << jsonFile.fileName() << "in write mode";
    }

    // Define a description according to the segmentation
    QString derivativeDescription;
    if(derivativeType == "mask"){
        derivativeDescription = "The output nifti file is obtained using a polygon ROI segmentation";
    }

    QJsonObject jsonObject;
    jsonObject.insert("Description", QJsonValue(derivativeDescription));

    QJsonArray jsonArray;
    jsonArray.append(QJsonValue("bids:raw:" + sourcePath));
    jsonObject.insert("Sources", jsonArray);
    jsonObject.insert("RawSources", jsonArray);

    jsonObject.insert("Manual", QJsonValue("true"));

    QJsonDocument jsonDoc(jsonObject);

    int writtenFile = jsonFile.write(jsonDoc.toJson());
    jsonFile.close();

    if(writtenFile == -1){
        qDebug() << "Writing error to the file" << jsonFile.fileName();
    }
}

bool medBIDS::addDirectData(QVariant data, levelMinimalEntries &pio_minimalEntries, unsigned int pi_uiLevel, QString parentKey)
{
    bool bRes = false;

    // Only possible at leaf level of tree structure
    if(isLevelWritable(pi_uiLevel)){
        // Storage/read path
        QString pathIn = data.toString();

        // Retrieves accepted file formats
        QStringList extensions;
        for(QStringList formats : m_SupportedTypeAndFormats.values()){
            extensions.append(formats);
        }

        // Compare with the input file to see if it is in the correct format
        bool correctExt = false;
        QString fileExt;
        for(auto ext : extensions){
            if(pathIn.contains(ext)){
                fileExt = ext;
                correctExt = true;
                break;
            }
        }

        if(correctExt == true){
            QString derivativeType(pio_minimalEntries.name.split(" ")[0]);

            QString newPath;

            // Create the directory to which the file is to be copied
            bool createdSubDirs = createBIDSDerivativeSubFolders(parentKey, derivativeType, newPath);
            if(createdSubDirs){
                // Retrieve the segmentation label returned by medInria segmentation tool
                QString Label = pio_minimalEntries.name.split("(")[0];
                QString segmentationLabel = Label.replace(" ", "");

                // Keep the entities by adding the appropriate 'desc' entity to segmentation -> to differentiate from the source file
                QStringList key = parentKey.split("~");
                QStringList parentFileEntities = key.last().split("_");
                parentFileEntities.removeLast();
                QString newEntities(parentFileEntities.join("_") + "_desc-" + segmentationLabel);
                QString newFileName(newEntities + "_" + derivativeType);

                // Create the writing path
                QString pathOut(newPath + "/" + newFileName + fileExt);  
                // If file with same name already exists, add run entity after 'desc' entity
                QFile filePathOut(pathOut);
                int numOccur = 1;
                while(filePathOut.exists()){
                    qDebug() << pathOut + "-> file with the same name already exists";
                    newFileName = newEntities + "_run-" + QString::number(numOccur) + "_" + derivativeType;
                    pathOut = newPath + "/" + newFileName + fileExt;
                    filePathOut.setFileName(pathOut);
                    numOccur ++;
                }

                // Copy the 'pathIn' file to 'pathOut' (BIDS path) : true if successful, false if a file with the same name already exists
                if(QFile::copy(pathIn, pathOut)){

                    // parentKey = 'sub-..._ses-..._<data type>~<parent file name>' 
                    pio_minimalEntries.key = parentKey + "~" + newFileName;
                    pio_minimalEntries.description = "";
                    pio_minimalEntries.type = entryType::dataset;

                    QString jsonName(newFileName  + ".json");
                    QString sourcePath = (key[0].split("_")).join("/") + "/" + key[1] + fileExt;
                    // Create the json sidecar file to the derivative file
                    createJsonDerivativeFile(sourcePath, newPath + "/" + jsonName, derivativeType);
                }
            }
        }
    }

    bRes = !pio_minimalEntries.key.isEmpty();

    return bRes;
}

 
int medBIDS::addAssyncData(QVariant data, levelMinimalEntries &pio_minimalEntries, unsigned int pi_uiLevel, QString parentKey)
{
    int rqstRes = 0;

    return rqstRes;
}

 
bool medBIDS::createPath(QList<levelMinimalEntries> &pio_path, datasetAttributes const &pi_attributes, unsigned int pi_uiLevel, QString parentKey)
{
    // TODO
    return false;
}


bool medBIDS::createFolder(levelMinimalEntries &pio_minimalEntries, datasetAttributes const &pi_attributes, unsigned int pi_uiLevel, QString parentKey)
{
    bool bRes = false;
    
    return bRes;
}

 
bool medBIDS::alterMetaData(datasetAttributes const &pi_attributes, unsigned int pi_uiLevel, QString key)
{
    // TODO
    return false;
}

 
bool medBIDS::getThumbnail(QPixmap &po_thumbnail, unsigned int pi_uiLevel, QString key)
{
    // TODO
    return false;
}

 
bool medBIDS::setThumbnail(QPixmap &pi_thumbnail, unsigned int pi_uiLevel, QString key)
{
    // TODO
    return false;
}

 
QVariant medBIDS::getAsyncResults(int pi_iRequest)
{
    return m_requestToDataMap[pi_iRequest];
}

