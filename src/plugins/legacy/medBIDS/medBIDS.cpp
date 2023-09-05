/*=========================================================================

 medInria

 Copyright (c) INRIA 2013 - 2019. All rights reserved.
 See LICENSE.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.

=========================================================================*/

#include "medBIDS.h"
#include "Worker.h"

#include <medStringParameter.h>
#include <medStringListParameter.h>
#include <medDataHub.h>

#include <medDataInfoWidget.h>

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
      /*m_ConnectionName("sqlite"),*/ m_instanceId(QString()),
      m_online(false), 
      //tree depth
      m_LevelNames({"subject", "session", "data", "files", "derivative"})
{
    // root_path contient le chemin vers un fichier 'dataset_description.json'
    root_path = new medStringParameter("Description File", this);
    // root_path->setCaption("JSON file (dataset_description.json)");
    // root_path->setDescription("Select dataset description file from BIDS repository");
    root_path->setDefaultRepresentation(3);

    // Bouton pour afficher les données du 'dataset_description.json'
    descriptionButton = new medTriggerParameter("Show file content");
    // QObject::connect(descriptionButton, &medTriggerParameter::pushed, [&](bool state){ 
    //     if(state){
    //         auto popupDataInfo = new medDataInfoWidget(descriptionDatasetValues);
    //         popupDataInfo->show();
    //     }
    // });

    // Niveaux de l'arbo et leurs attributs
    m_MandatoryKeysByLevel["Subject"] = QStringList({"id", "name", "description", "type"});
    m_MandatoryKeysByLevel["Session"] = QStringList({"id", "name", "description", "type"});
    m_MandatoryKeysByLevel["Data"] = QStringList({"id", "name", "description", "type"});
    m_MandatoryKeysByLevel["Files"] = QStringList({"id", "SeriesDescription", "description", "type"});   
    m_MandatoryKeysByLevel["Derivative"] = QStringList({"id", "SeriesDescription", "description", "type"});                                           
}

medBIDS::~medBIDS()
{
    // Termine la connexion
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
    // Ouvre le fichier
    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly)){
        return jsonObj;
    }

    // Lit le fichier pour récupérer le contenu tel quel
    QString content(file.readAll());
    file.close();

    // QByteArray en paramètre : tableau d'octets -> 'toUtf8()'
    // Le transforme en objet Json pour simplifier la récupération des valeurs (similaire à QMap -> dictionnaire)
    if(!content.isEmpty()){
        QJsonDocument jsonDoc = QJsonDocument::fromJson(content.toUtf8());
        jsonObj = jsonDoc.object();
    }

    return jsonObj;
}

void medBIDS::getDatasetDescription(QJsonObject jsonObj, QString parentKey)
{
    for(auto key : jsonObj.keys()){
        QJsonValue jsonValue = jsonObj.value(key);
        if(jsonValue.isObject()){
            if(!parentKey.isEmpty()){
                key.prepend(parentKey + ":");
            }
            // Objets imbriqués : garde la clé parent
            getDatasetDescription(jsonValue.toObject(), key);
        }
        else if(jsonValue.isArray())
        {
            QJsonArray jsonTab(jsonValue.toArray());
            // 'elements' sert de mise en forme si pas Object alors QString
            QString elements = "[";
            for(auto tabValue : jsonTab){
                if(tabValue.isObject()){
                    if(!parentKey.isEmpty()){
                        key.prepend(parentKey + ":");
                    }
                    // Objets imbriqués : garde la clé parent
                    getDatasetDescription(tabValue.toObject(), key);
                }else{
                    elements.append(tabValue.toString() + "; ");
                }
            }
            if(elements.size() > 1){
                elements.chop(2);
                elements.append("]");
                descriptionDatasetValues[key] = elements;
            }
        }
        else if(!parentKey.isEmpty())
        {
            // cas objets imbriqués : ajoute la clé parent
            descriptionDatasetValues[parentKey + ":" + key] = jsonValue.toString();
        }
        else
        {
            descriptionDatasetValues[key] = jsonValue.toString();
        }
    }
}

void medBIDS::getNiftiAttributesForMandatories(QString niftiPath, QString levelName){
    imageIO = itk::ImageIOFactory::CreateImageIO(niftiPath.toLatin1().constData(), itk::ImageIOFactory::ReadMode);
    if (!imageIO.IsNull()){
        // deux lignes nécessaires sinon ne reconnaît pas en tant que fichier nifti
        imageIO->SetFileName(niftiPath.toLatin1().constData());
        try{
            imageIO->ReadImageInformation();
        }catch(itk::ExceptionObject &e){
            qDebug() << e.GetDescription();
        }

        itk::MetaDataDictionary& metaDataDictionary = imageIO->GetMetaDataDictionary();
        std::vector<std::string> niftiKeys = metaDataDictionary.GetKeys();

        QStringList levelAttributes = m_MandatoryKeysByLevel[levelName];
        for(auto niftiKey : niftiKeys){
            levelAttributes.append(niftiKey.c_str());
        }

        // Remplacement liste mandatoriesAttributes avec les nouveaux éléments
        m_MandatoryKeysByLevel.insert(levelName, levelAttributes);
    }
}


bool medBIDS::connect(bool pi_bEnable)
{
    bool bRes = false;
    if (pi_bEnable) // establish connection
    {
        // extrait le chemin du répertoire -> racine arborescence
        if(!root_path->value().isEmpty()){
            bids_path = root_path->value().remove("dataset_description.json");
            bRes = true;
        }

        // Vérifie que la valeur (= chemin d'entrée) n'est pas vide et que le répertoire existe localement 
        QDir dirPath(bids_path);
        if (dirPath.exists() && bRes)
        {
            // Vérifie que le fichier 'data_description.json' existe et en extrait le contenu
            QString filePath(root_path->value());
            QFileInfo descriptionFile(filePath);
            if(descriptionFile.exists() && descriptionFile.isFile()){
                bRes = true;
                QJsonObject jsonObj(readJsonFile(filePath));
                if(!jsonObj.isEmpty()){
                    getDatasetDescription(jsonObj, "");
                }
            }

            // Vérifie qu'il existe des dossiers sujets ("sub-") dans le répertoire du chemin d'entrée 
            QDir dir(bids_path);
            QStringList subPrefixes({"sub-*"});
            QFileInfoList filesList = dir.entryInfoList(subPrefixes);
            if(!filesList.isEmpty() && bRes){
                // Vérifie qu'il existe au moins un fichier au format '.nii.gz' dans les sous-dossiers du répertoire
                QDirIterator it(bids_path, QDir::Files, QDirIterator::Subdirectories);
                while(it.hasNext()) {
                    QString subDirs = it.next();
                    if(subDirs.contains(".nii.gz") || subDirs.contains(".nii")){
                        getNiftiAttributesForMandatories(subDirs, QString("Files"));
                        bRes = true;
                        break;
                    }
                }
            }else{
                bRes = false;
            }
        }
    }

    m_online = bRes;
    emit connectionStatus(m_online);

    return bRes;
}

QStringList medBIDS::getAttributesFromTsv(QString &line){
    int quote = 0;

    for(int idx = 0; idx < line.size(); ++idx){
        if(line.at(idx) == "\""){
            quote = !quote;
        }
        if(line.at(idx) == " " && quote == 1){
            line.replace(idx, 1, "~");
        }
    }

    // La fonction 'split' ne prend plus en compte les espaces dans les chaînes entre " "
    QStringList attributes = line.split(QRegExp("\\s+"));
    // Remplace les '~' précédents par des espaces
    for(int i=0; i < attributes.size(); ++i){
        attributes[i].replace("~", " ");
        attributes[i].remove("\"");
    }

    return attributes;
}

bool medBIDS::getPatientsFromTsv(){
    // Ouvre le fichier "participants.tsv"
    QFile tsvFile(bids_path + "participants.tsv");
    if (!tsvFile.open(QIODevice::ReadOnly))
        return false;

    // Lit l'en-tête du fichier pour vérifier qu'il y a au moins la colonne 'participant_id' (1ère position)
    QTextStream in(&tsvFile); 
    QString header = in.readLine();
    QStringList keys = header.split(QRegExp("\\s+"));
    if(keys[0].compare("participant_id") != 0){
        return false;
    }

    QMap<QString, QString> patientValues;
    // Lit le fichier ligne par ligne
    while(!in.atEnd()){
        QString line = in.readLine();
        QStringList values = getAttributesFromTsv(line);

        if(keys.size() == values.size()){
            for(int i=0; i < keys.size(); ++i){
                patientValues[keys[i]] = values[i];
            }
        }

        // Clé = participant_id, valeur = QMap avec valeurs (value) des attributs (key) des noms de colonnes du fichier .tsv
        participant_tsv[values[0]] = patientValues;
    }

    return true;
}

bool medBIDS::getSessionsFromTsv(QString parentId){
    //Crée le chemin du répertoire propre au 'parentId' = se place dans le dossier sub
    QString subPath(bids_path + parentId + "/");
    QDir dir(subPath);

    // Chercher le fichier .tsv
    QStringList filters({"*.tsv"});
    QFileInfoList filesList = dir.entryInfoList(filters);
    // Vérifier qu'il existe, il ne doit y en avoir qu'un dans un dossier sub
    if(filesList.isEmpty() || filesList.size() != 1){
        return false;
    }

    // Essaye d'ouvrir le fichier en lecture
    QFile tsvFile(subPath + filesList[0].fileName());
    if (!tsvFile.open(QIODevice::ReadOnly))
        return false;

    // Lit l'en-tête du fichier pour vérifier qu'il y a au moins la colonne 'session_id' (1ère position)
    QTextStream in(&tsvFile); 
    QString header = in.readLine();
    QStringList keys = header.split(QRegExp("\\s+"));
    if(keys[0].compare("session_id") != 0){
        return false;
    }

    // Lit le fichier ligne par ligne
    QMap<QString, QString> sessionValues;
    while(!in.atEnd()){
        QString line = in.readLine();
        QStringList values = getAttributesFromTsv(line);

        if(keys.size() == values.size()){
            for(int i=0; i < keys.size(); ++i){
                sessionValues[keys[i]] = values[i];
            }
        }

        // Clé = sub-..._session_id, valeur = QMap avec valeurs (value) des attributs (key) des noms de colonnes du fichier .tsv
        QString id = parentId + "_" + values[0];
        session_tsv[id] = sessionValues;
    }

    return true;
}


// met à jour interface graphique 
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


// Récupéré par minimal or mandatories (obligatoires)
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

// Niveau désiré accessible en écriture
 
unsigned int medBIDS::getLevelDesiredWritable()
{
    return 4;
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


// Niveau dans lequel on peut écrire -> 3
bool  medBIDS::isLevelWritable(unsigned int pi_uiLevel)
{
    return pi_uiLevel == 4;
}

// Permet d'obtenir les attributs qui composent un niveau
QStringList medBIDS::getMandatoryAttributesKeys(unsigned int pi_uiLevel)
{
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

    // Obtiens les informations du niveau 'pi_uiLevel' pour l'affichage des menus déroulants 
    // dont le parent correspond au 'parentId'.
    // Correspond aux entrées minimales qui sont une sous-partie des attributs obligatoires
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
        default:
            break;
    }

    return entries;
}


QList<medAbstractSource::levelMinimalEntries> medBIDS::getSubjectMinimalEntries(QString &key)
{
    QList<levelMinimalEntries> subjectEntries;

    // Obtient le répertoire racine
    QDir dir(bids_path);

    // Récupère tous les dossiers du répertoire contenant le préfixe "sub-"
    QStringList prefixes({"sub-*"});
    QFileInfoList filesList = dir.entryInfoList(prefixes, QDir::Dirs);
    for(auto file : filesList){
        // Vérifie qu'il s'agit bien d'un dossier
        if(!file.isFile()){

            // Test si le dossier (= 'participant_id') de cette entité sujet appartient à la QMap (= inclus dans le fichier .tsv)
            if(!participant_tsv.contains(file.fileName())){
                continue;
            }

            // Complète les attributs
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

    // Test si le dossier subject possède un fichier "..._session.tsv" et en extrait les données le cas échéant
    if(!getSessionsFromTsv(parentId)){
        return sessionEntries;
    }

    // Reconstruit le chemin d'accès pour se positionner dans le dossier sub
    QDir dir(bids_path + parentId + "/");

    // Récupère tous les dossiers du répertoire contenant le préfixe "ses-"
    QStringList prefixes({"ses-*"});
    QFileInfoList filesList = dir.entryInfoList(prefixes, QDir::Dirs);
    for(auto file : filesList){
        // Vérifie qu'il s'agit bien d'un dossier
        if(!file.isFile()){
    
            // Test si le dossier (= 'session_id') de cette entité sujet appartient à la QMap (= inclus dans le fichier .tsv)
            if(!participant_tsv.contains(parentId + "_" + file.fileName())){
                continue;
            }

            // Complète les attributs
            levelMinimalEntries entry;
            // parentId pour mieux identifier quel patient est associé à cette session
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

    // Identifie et récupère les entités subject et session correspondantes 
    QStringList subDirs = parentId.split("_");

    // Reconstruit le chemin d'accès, à partir de ces entités, pour se positionner dans le dossier ses
    QDir dir(bids_path + subDirs[0] + "/" + subDirs[1] + "/");

    // Récupère tous les dossiers du répertoire contenant les types de données
    QFileInfoList filesList = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for(auto file : filesList){
        // Vérifie qu'il s'agit bien d'un dossier
        if(!file.isFile()){

            // Récupère le titre du dossier pour compléter les attributs
            levelMinimalEntries entry;
            // parentId pour mieux identifier de quelle session est issu ce data
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

    // Identifie et récupère les entités, subject et session, et le data correspondant
    QStringList subDirs = parentId.split("_");

    // Reconstruit le chemin d'accès pour se positionner dans le répertoire du data
    QString filePath(bids_path + subDirs[0] + "/" + subDirs[1] + "/" + subDirs[2] + "/");
    QDir dir(filePath);

    // Récupère uniqument les fichiers au format json
    QStringList prefixes({"*.nii.gz", "*.nii"});
    QFileInfoList filesList = dir.entryInfoList(prefixes, QDir::Files);
    for(auto file : filesList){
        // Vérifie qu'il s'agit bien d'un fichier
        if(file.isFile()){

            // Enlever extension
            QString fileName = file.fileName().split(".")[0];

            // Lit le fichier json
            QJsonObject jsonObj = readJsonFile(filePath + file.fileName());
            // Si cette liste n'est pas vide
            QString fileSeries = "";
            if(!jsonObj.isEmpty()){
                //Récupère la valeur du tag "seriesDescription" pour le champ .name
                fileSeries = jsonObj.value("SeriesDescription").toString();
            }

            levelMinimalEntries entry;
            // Séparation avec '~' pour simplifier le getDirectData (et aller chercher le nifti pour getDirectData): path ~ file_name
            entry.key = parentId + "~" + fileName;
            entry.name = fileSeries;
            entry.description = "";
            entry.type = entryType::dataset;
            filesEntries.append(entry);
        }
    }
    
    return filesEntries;
}

 
QList<QMap<QString, QString>> medBIDS::getMandatoryAttributes(unsigned int pi_uiLevel, QString parentId)
{
    QList< QMap<QString, QString>> res;

    // Obtiens les informations de l'élément 'key' pour l'affichage des attributs 
    // Contient les entrées minimales qui sont une sous-partie des attributs obligatoires
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

    //Test si le dossier possède un fichier .tsv de session et en extrait les données le cas échéant
    bool tsvExists = getPatientsFromTsv();

    // se place dans le répertoire racine
    QDir dir(bids_path);
    // Trouve tous les dossiers des 'subject'
    QStringList prefixes({"sub-*"});
    QFileInfoList filesList = dir.entryInfoList(prefixes, QDir::Dirs);
    for(auto file : filesList){
        // Vérifie qu'il s'agit bien d'un dossier
        if(!file.isFile()){

            if(tsvExists){
                // Récupère les valeurs de la clé (= filename) si fichier .tsv il y a
                QMap<QString, QString> attributes = participant_tsv.value(file.fileName());
                if(!attributes.isEmpty()){
                    // Test si le participant_id de cette entité appartient à la QMap et son participant_id est bien le même
                    if(attributes["participant_id"] != file.fileName()){
                        continue;
                    }
                }
            }

            // Stocke les attributs dans une QMap : minimalEntries + subjectKeys
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

    // Test si le dossier possède un fichier .tsv de session et en extrait les données le cas échéant
    bool tsvExists = getSessionsFromTsv(key);

    // Reconstruit le chemin d'accès pour se placer dans le dossier sub
    QDir dir(bids_path + key + "/");

    // Chercher tous les dossiers du répertoire contenant le préfixe "ses-"
    QStringList prefixes({"ses-*"});
    QFileInfoList filesList = dir.entryInfoList(prefixes);
    for(auto file : filesList){
        // Vérifie qu'il s'agit bien d'un dossier
        if(!file.isFile()){
            QString id = key + "_" + file.fileName();
    
            // Vérifie que c'est une session du patient, donc contenue dans le fichier .tsv
            if(tsvExists){
                // Récupère les valeurs de la clé (= filename)
                QMap<QString, QString> attributes = session_tsv.value(id);
                if(!attributes.isEmpty()){
                    // Test si le session_id de cette entité appartient à la QMap et son session_id est bien le même
                    if(attributes["session_id"] != file.fileName()){
                        continue;
                    }
                }
            }

            // Stocke les attributs dans une QMap : minimalEntries + sessionKeys
            QMap<QString, QString> sessionMap;
            // key = participant_id
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

    // Identifie et récupère les entités subject et session correspondantes 
    QStringList subDirs = key.split("_");

    // Reconstruit le chemin d'accès pour se placer dans le dossier ses
    QDir dir(bids_path + subDirs[0] + "/" + subDirs[1] + "/");

    // Récupère tous les dossiers du répertoire contenant les types de données
    QFileInfoList filesList = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for(auto file : filesList){
        // Vérifie qu'il s'agit bien d'un dossier
        if(!file.isFile()){ 

            // Récupère le titre du dossier correspondant au data pour compléter les attributs
            QMap<QString, QString> dataMap;
            // key = participant_id + session_id
            dataMap["id"] = key + "_" + file.fileName();
            dataMap["name"] = file.fileName();
            dataMap["description"] = "";
            dataMap["type"] = entryTypeToString(entryType::folder);

            dataAttributes.append(dataMap);
        }
    }

    return dataAttributes;
}

QMap<QString, QString> medBIDS::getNiftiValuesFromFile(QString niftiFile, int level)
{
    QMap<QString, QString> niftiValues;

    // Récupère les clés nifti du niveau 'level'
    QStringList filesKeys = getMandatoryAttributesKeys(level);
    
    imageIO = itk::ImageIOFactory::CreateImageIO(niftiFile.toLatin1().constData(), itk::ImageIOFactory::ReadMode);
    if (!imageIO.IsNull()){
        // deux lignes nécessaires sinon ne reconnaît pas en tant que fichier nifti
        imageIO->SetFileName(niftiFile.toLatin1().constData());
        try{
            imageIO->ReadImageInformation();
        }catch(itk::ExceptionObject &e){
            qDebug() << e.GetDescription();
        }

        itk::MetaDataDictionary& metaDataDictionary = imageIO->GetMetaDataDictionary();
        for(int i=4; i < filesKeys.size(); i++){
            std::string niftiValue;
            itk::ExposeMetaData(metaDataDictionary, filesKeys[i].toStdString(), niftiValue);
            niftiValues[filesKeys[i]] = niftiValue.c_str();
        }
    }

    return niftiValues;
}

// Version 1
QList<QMap<QString, QString>> medBIDS::getFilesMandatoriesAttributes(const QString &key)
{
    QList< QMap<QString, QString>> filesAttributes;

    // Identifie et récupère les entités, subject et session, et le data correspondant
    QStringList subDirs = key.split("_");

    // Reconstruit le chemin d'accès pour ce placer dans le répertoire data
    QString filePath(bids_path + subDirs[0] + "/" + subDirs[1] + "/" + subDirs[2] + "/");
    QDir dir(filePath);

    QStringList prefixes({"*.nii.gz", "*.nii"});
    QFileInfoList filesList = dir.entryInfoList(prefixes, QDir::Files);
    for(auto file : filesList){
        // Vérifie qu'il s'agit bien d'un fichier
        if(file.isFile()){

            // Récupère le nom du fichier pour compléter les attributs
            QMap<QString, QString> filesMap;

            // Enlever extension
            QString fileName = file.fileName().split(".")[0];
            // Séparation avec ~ pour simplifier le getDirectData (et aller chercher le nifti): path ~ file_name
            // key = participant_id + session_id + data
            filesMap["id"] = key + "~" + fileName;
            filesMap["description"] = "";
            filesMap["type"] = entryTypeToString(entryType::dataset);

            //champ "SeriesDescription" (="name") = attribut "seriesDescription" fichier .json
            QJsonObject jsonObj = readJsonFile(filePath + fileName + ".json");
            // Si cette liste n'est pas vide
            if(!jsonObj.isEmpty()){
                filesMap["SeriesDescription"] = jsonObj.value("SeriesDescription").toString();
                
                int runIndex = fileName.indexOf("run");
                if(runIndex != -1){
                    QString endName = fileName.mid(runIndex);
                    QString numRun = endName.split("_")[0];

                    filesMap["SeriesDescription"].append(" - " + numRun);
                }
            }

            // Le reste des mandatoriesAttributes sont les données des nifti
            QString niftiFile = filePath + file.fileName();
            filesMap.insert(getNiftiValuesFromFile(niftiFile, 3));

            filesAttributes.append(filesMap);
        }
    }

    return filesAttributes;
}

QList<QMap<QString, QString>> medBIDS::getDerivativeMandatoriesAttributes(const QString &key)
{
    QList< QMap<QString, QString>> derivativeAttributes;

    QStringList dir(key.split("~"));
    QString filePath(dir[0].replace("_", "/"));
    QString fileName(dir[1]);
    QString extension(".nii.gz");

    QFile niftiFile(bids_path + filePath + "/" + fileName  + ".nii.gz");
    if(!niftiFile.exists()){
        extension = ".nii";
    }

    //Récupère le nom derivative
    // QString derivativeName;
    // QJsonObject jsonObj = readJsonFile(bids_path + filePath + "/" + fileName + ".json");
    // if(!jsonObj.isEmpty()){
    //     derivativeName = jsonObj.value("SeriesDescription").toString();

    //     int runIndex = fileName.indexOf("run");
    //     if(runIndex != -1){
    //         QString endName = fileName.mid(runIndex);
    //         QString numRun = endName.split("_")[0];

    //         derivativeName.append(" - " + numRun);
    //     }
    // }

    // Vérifie qu'un fichier derivative existe
    QString derivativeSubDirs("derivatives/mask/");
    QStringList nameEntities = fileName.split("_");
    nameEntities.last() = "desc-" + nameEntities.last();
    QString derivativeFile(nameEntities.join("_"));

    QString derivativeFilePath(bids_path + derivativeSubDirs + filePath + "/" + derivativeFile + extension);
    QFileInfo file(derivativeFilePath);
    if(file.exists()){
        QMap<QString, QString> derivativeMap;

        QString derivativeName;
        QString jsonDerivativeFile(bids_path + derivativeSubDirs + filePath + "/" + derivativeFile + ".json");
        QFileInfo jsonFile(jsonDerivativeFile);
        if(jsonFile.exists()){
            QJsonObject jsonDerivativeObj = readJsonFile(jsonDerivativeFile);
            if(!jsonDerivativeObj.isEmpty()){
                derivativeName = jsonDerivativeObj.value("Name").toString();
            }
        }

        derivativeMap["id"] = key + "~" + derivativeFile;
        derivativeMap["SeriesDescription"] = derivativeName;
        derivativeMap["description"] = "";
        derivativeMap["type"] = entryTypeToString(entryType::dataset);

        getNiftiAttributesForMandatories(derivativeFilePath, QString("Derivative"));
            
        derivativeMap.insert(getNiftiValuesFromFile(derivativeFilePath, 4));
        derivativeAttributes.append(derivativeMap);
    }

    return derivativeAttributes;
}

 
bool medBIDS::getAdditionalAttributes(unsigned int pi_uiLevel, QString id, datasetAttributes &po_attributes)
{
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
            res = true;
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
    
    // S'il existe, récupère les clés et les valeurs pour le patient avec id 'key' dans le fichier .tsv
    QMap<QString, QString> attributes = participant_tsv.value(key);

    // Récupérer les clés obligatoires
    QStringList subjectKeys = getMandatoryAttributesKeys(0);

    for(auto key : attributes.keys()){
        // Si la clé n'est pas déjà affichée dans les mandatories, alors affiche dans additional
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

    // // S'il existe, récupère les clés et les valeurs pour la session avec id 'key' dans le fichier .tsv
    QMap<QString, QString> attributes = session_tsv.value(key);

    // Récupérer les clés obligatoires
    QStringList subjectKeys = getMandatoryAttributesKeys(1);

    for(auto key : attributes.keys()){
        // Si la clé n'est pas déjà affichée dans les mandatories, alors affiche dans additional
        if(!subjectKeys.contains(key)){
            po_attributes.values[key] = attributes[key];
        }
    }

    return true;
}

bool medBIDS::getFilesAdditionalAttributes(const QString &key, medAbstractSource::datasetAttributes &po_attributes)
{
    // Recrée le chemin vers le fichier .json
    QStringList dir(key.split("~"));
    QString filePath(dir[0].split("_").join("/"));
    QString fileName(dir[1] + ".json");

    QString path(bids_path + filePath + "/" + fileName);
    QJsonObject jsonObj = readJsonFile(path);
    if(!jsonObj.isEmpty()){
        // Récupère les clés des tags DICOM contenus dans le fichier
        QStringList dcmTags = jsonObj.keys();  
        for(auto tag : dcmTags){
            // Test le type de la valeur du 'tag'
            if(jsonObj.value(tag).isString())
            {
                QString tagValue = jsonObj.value(tag).toString();
                if(tagValue.contains("\r\n")){
                    tagValue = tagValue.replace("\r\n", ", ");
                }
                po_attributes.values[tag] = tagValue;
            }
            else if(jsonObj.value(tag).isArray())
            {
                // Récupère le tableau pour récupérer son contenu en QString
                QJsonArray jsonTab(jsonObj.value(tag).toArray());
                QString types = "[";
                // auto = QJsonValueRef
                for(auto type : jsonTab){
                    if(type.isString()){
                        types.append(type.toString() + ", ");
                    }else{
                        types.append(type.toVariant().toString() + ", ");
                    }
                }
                // pour affichage : supprime les deux derniers caractères
                types.chop(2);
                types.append("]");
                po_attributes.values[tag] = types;
            }
            else
            { // Si c'est un int, passe d'abord par une 'variant'
                po_attributes.values[tag] = jsonObj.value(tag).toVariant().toString();
            }
        }

        return true;
    }

    return false;
}

 
QVariant medBIDS::getDirectData(unsigned int pi_uiLevel, QString key)
{
    QVariant res;
    // Niveau des fichiers ".nii.gz"
    if(pi_uiLevel == 3 || pi_uiLevel == 4){
        // 'split' pour récupérer 'path' du 'filename' : ajoute l'extension nifti au filename
        QStringList dir(key.split("~"));
        QString filePath(dir[0].split("_").join("/"));
        QString fileName(dir[1] + ".nii.gz");
        QString extension(".nii.gz");

        QFile niftiFile(bids_path + filePath + "/" + fileName);
        if(!niftiFile.exists()){
            fileName = dir[1] + ".nii";
            extension = ".nii";
        }
    
        QString displayFile(bids_path + filePath + "/" + fileName);
        if(pi_uiLevel == 4){
            QString derivativeFile(dir[2] + extension);
            displayFile = bids_path + "derivatives/mask/" + filePath + "/" + derivativeFile;
        }

        // Indique au programme le répertoire du fichier à afficher
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
    //m_SupportedTypeAndFormats["Header"] = QStringList({".json", ".bvec", ".bval"});

    return m_SupportedTypeAndFormats;
}


void medBIDS::createBIDSDerivativeSubFolders(QString parentKey, QString derivativeType, QString &newPath)
{
    // Défini le type de derivative
    QString derivativeDescription;
    if(derivativeType == "mask"){
        derivativeDescription = "Define a polygon ROI mask";
    }

    // Crée le chemin du pipeline
    QString pipeline = bids_path + "derivatives/" + derivativeType + "/";
    QDir newDerivativeDir;
    if(newDerivativeDir.mkpath(pipeline))
    {
        // Crée le contenu pour le fichier json
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

        QFile file(pipeline + "dataset_description.json");
        if(!file.open(QIODevice::WriteOnly)){
            qDebug() << "Impossible d'ouvrir le fichier en écriture";
        }
        int writtenFile = file.write(jsonDoc.toJson());
        file.close();

        if(writtenFile == -1){
            qDebug() << "Erreur lors de l'écriture dans le fichier";
        }

        // Récupère les niveaux (sous-dossiers) parents BIDS et crée le nouveau chemin pour les derivatives
        QStringList subDirs = parentKey.split("~");
        QStringList pathDirs = subDirs[0].split("_");
        newPath = pipeline + pathDirs.join("/");
        newDerivativeDir.mkpath(newPath);
    }
}

void medBIDS::createJsonDerivativeFile(QString sourcePath, QString jsonFilePath, QString derivativeType, QString maskName){
    // Crée le fichier json associé
    QFile jsonFile(jsonFilePath);
    if(!jsonFile.open(QIODevice::WriteOnly)){
        qDebug() << "Impossible d'ouvrir le fichier en écriture";
    }

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
    jsonObject.insert("Name", QJsonValue(maskName));

    QJsonDocument jsonDoc(jsonObject);

    int writtenFile = jsonFile.write(jsonDoc.toJson());
    jsonFile.close();

    if(writtenFile == -1){
        qDebug() << "Erreur lors de l'écriture dans le fichier";
    }
}

// Ajoute un fichier dans l'arborescence du logiciel (glissé-déposé), puis le copie au même endroit dans l'arborescence locale 
bool medBIDS::addDirectData(QVariant data, levelMinimalEntries &pio_minimalEntries, unsigned int pi_uiLevel, QString parentKey)
{
    bool bRes = false;

    // std::cout << data.toString().toStdString() << std::endl;
    // std::cout << pio_minimalEntries.key.toStdString() << std::endl;
    // std::cout << pio_minimalEntries.name.toStdString() << std::endl;
    // std::cout << pio_minimalEntries.description.toStdString() << std::endl;
    // std::cout << pi_uiLevel << std::endl;
    // std::cout << parentKey.toStdString() << std::endl;

    if(pi_uiLevel == 4){
        // chemin de stockage/lecture
        QString pathIn = data.toString();

        // Récupère les formats acceptés pour les fichiers
        QStringList extensions;
        for(QStringList formats : m_SupportedTypeAndFormats.values()){
            extensions.append(formats);
        }

        // Compare avec le fichier d'entrée voir s'il est correct
        bool correctExt = false;
        QString fileExt;
        for(auto ext : extensions){
            if(pathIn.contains(ext)){
                fileExt = ext;
                correctExt = true;
                break;
            }
        }

        // Si l'extension appartient à celles possibles
        if(correctExt == true){
            QString derivativeType(pio_minimalEntries.name.split(" ")[0]);

            QString newPath;
            createBIDSDerivativeSubFolders(parentKey, derivativeType, newPath);
            QDir newDerivativeDir(newPath);
            if(newDerivativeDir.exists()){
                // Récupère le nom du fichier
                QStringList maskName = pio_minimalEntries.name.split("(");
                QString seriesName = maskName.last().chopped(1);

                // ajouter une entité 'desc' au fichier derivative pour le distinguer de raw
                QString rawFileName = parentKey.split("~")[1];
                QStringList nameEntities = rawFileName.split("_");
                nameEntities.last() = "desc-" + nameEntities.last();
                QString derivativeFileName(nameEntities.join("_"));

                //Copie le fichier pathIn vers pathOut (chemin BIDS) : true si succès, false si un fichier du même nom existe déjà
                QString pathOut = newPath + "/" + derivativeFileName + fileExt;
                if(QFile::copy(pathIn, pathOut)){
                    // Complète les minimalEntries du fichier : à l'origine ne contient que le champ .name
                    pio_minimalEntries.key = parentKey + "~" + derivativeFileName;
                    pio_minimalEntries.description = "";
                    pio_minimalEntries.type = entryType::dataset;

                    QString jsonName(derivativeFileName  + ".json");
                    QStringList keyPath = parentKey.split("~");
                    QString sourcePath = (keyPath[0].split("_")).join("/") + "/" + keyPath[1] + fileExt;
                    createJsonDerivativeFile(sourcePath, newPath + "/" + jsonName, derivativeType, pio_minimalEntries.name);
                }
            }
        }
    }

    // Test si 'key' non vide : moyen également de tester les éléments dont elle dépend comme 'parentKey'
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

