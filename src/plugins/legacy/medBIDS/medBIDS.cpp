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

#include <QDebug>

#include <QDir>
#include <QFile>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <iostream>

#include <QUuid>
#include <QThread>

std::atomic<int> medBIDS::s_RequestId = 0;

medBIDS::medBIDS()
    : medAbstractSource(), m_Driver("BIDS"),
      /*m_ConnectionName("sqlite"),*/ m_instanceId(QString()),
      m_online(false), 
      //tree depth
      m_LevelNames({"subject", "session", "data", "files"})
{
    root_path = new medStringParameter("RootPath", this);
    // Permettre l'affichage correspondant dans la partie "Sources" de l'interface
    root_path->setDefaultRepresentation(3);
    // QObject::connect(root_path, &medStringParameter::valueChanged, [&](QString const &path){
    //     if(!path.isEmpty()){
    //         root_path->setValue(path);
    //     }
    // });

    // Niveaux de l'arbo et leurs attributs : un attribut mentionnant le parent pour chaque niveau
    m_MandatoryKeysByLevel["Subject"] = QStringList({"id", "name", "subjectSourceId", "protocol", "center", "description", "type"});
    m_MandatoryKeysByLevel["Session"] = QStringList({"id", "name", "sessionSourceId", "description", "type"});
    m_MandatoryKeysByLevel["Data"] = QStringList({"id", "name", "description", "type"});
    m_MandatoryKeysByLevel["Files"] = QStringList({"id", "SeriesDescription", "description", "type", 
                                                    "Modality", "SeriesDescription", "ProtocolName", "SeriesNumber", "ImageType"});
                                                


    // m_pWorker = new Worker();
    // m_pWorker->moveToThread(&m_Thread);
    // // Creates connections from the signal (2) in the sender object (1) to the method (4) in the receiver object (3)
    // bool c1 = QObject::connect(m_pWorker, &Worker::signalWithDelay, m_pWorker, &Worker::sendSignalWithDelay);
    // bool c2 = QObject::connect(m_pWorker, &Worker::sendProgress, this, [&](int requestId, int status)
    // {
    //     emit progress(requestId, (medAbstractSource::eRequestStatus)status);
    // });
    // m_Thread.start();
}


medBIDS::~medBIDS()
{
    // Indique à la boucle d'évènements de se terminer, attend qu'une condition se termine (exécution terminée ou délai atteint)
    // m_Thread.quit();
    // m_Thread.wait();
    // Termine la connexion
    connect(false);
}

// Les deux fonctions suivantes initialisent puis donnent un id et un nom, tout deux passés en paramètres

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


bool medBIDS::connect(bool pi_bEnable)
{
    bool bRes = false;
    if (pi_bEnable) // establish connection
    {
        // Vérifie que la valeur (= chemin d'entrée) n'est pas vide et existe localement 
        QDir path_dir(root_path->value());
        if (!root_path->value().isEmpty() && path_dir.exists())
        {
            // Normalisation : si le chemin ne finit pas par "/", en ajoute un
            if(!(root_path->value()).endsWith("/")){
                root_path->setValue(root_path->value() + "/");
            }

            // Vérifie qu'il existe des dossiers sujets ("sub-") dans le répertoire du chemin d'entrée 
            QDir dir(root_path->value());
            QStringList sub_prefixes({"sub-*"});
            QFileInfoList files_list = dir.entryInfoList(sub_prefixes);
            if(!files_list.isEmpty()){
                // Vérifie qu'il existe au moins un fichier au format '.nii.gz' dans les sous-dossiers du répertoire
                QDirIterator it(root_path->value(), QDirIterator::Subdirectories);
                bool exist = false;
                while(it.hasNext()) {
                    QString sub_dirs = it.next();
                    if(sub_dirs.contains(".nii.gz")){
                        exist = true;
                        break;
                    }
                }
                if(exist){
                    // Vérifie qu'il existe un fichier 'participants.tsv' à la racine du répertoire
                    QFileInfo tsv_info(root_path->value() + "participants.tsv");
                    if(tsv_info.exists() && tsv_info.isFile()){
                        // Appel une fonction qui ouvre le fichier pour en extraire le contenu dans une QMap
                        bRes = getPatientsFromTsv();
                    }
                }
            }
        }
        m_online = bRes;
    }

    emit connectionStatus(m_online);

    return bRes;
}

QStringList medBIDS::getAttributes(QString &line){
    int quote = 0;

    for(int idx = 0; idx < line.size(); ++idx){
        if(line.at(idx) == "\""){
            quote = !quote;
        }
        if(line.at(idx) == " " && quote == 1){
            line.replace(idx, 1, "~");
        }
    }

    // Le split ne prend plus en compte les espaces dans les chaînes entre " "
    QStringList attributes = line.split(" ");
    // Remplace les ~ précédents par des espaces
    for(int i=0; i < attributes.size(); ++i){
        attributes[i].replace("~", " ");
        attributes[i].remove("\"");
    }

    return attributes;
}

bool medBIDS::getPatientsFromTsv(){
    // Ouvre le fichier "participants.tsv"
    QFile tsv_file(root_path->value() + "participants.tsv");
    if (!tsv_file.open(QIODevice::ReadOnly))
        return false;

    // Lit l'en-tête du fichier pour vérifier qu'il y a au moins la colonne 'participant_id' (1ère position)
    QTextStream in(&tsv_file); 
    QString line = in.readLine();
    QStringList l = line.split(" ");
    if(l[0].compare("participant_id") != 0){
        return false;
    }

    // Lit le fichier ligne par ligne
    while(!in.atEnd()){
        QString line = in.readLine();
        QStringList l = getAttributes(line);

        // Clé = participant_id, valeur = liste d'attributs de la ligne correspondante dans le fichier .tsv
        patient_tsv[l[0]] = l;
    }

    return true;
}

bool medBIDS::getSessionsFromTsv(QString parentId){
    // Crée le chemin du répertoire propre au 'parentId' = se place dans le dossier sub
    QString sub_path(root_path->value() + parentId + "/");
    QDir dir(sub_path);

    // Chercher le fichier .tsv
    QStringList filters({"*.tsv"});
    QFileInfoList files_list = dir.entryInfoList(filters);
    // Vérifier qu'il existe, il ne doit y en avoir qu'un dans un dossier sub
    if(files_list.isEmpty() || files_list.size() != 1){
        return false;
    }
    
    // Essaye d'ouvrir le fichier en lecture
    QFile tsv_file(sub_path + files_list[0].fileName());
    if (!tsv_file.open(QIODevice::ReadOnly))
        return false;

    // Lit l'en-tête du fichier pour vérifier qu'il y a au moins la colonne 'session_id' (1ère position)
    QTextStream in(&tsv_file); 
    QString line = in.readLine();
    QStringList l = line.split(" ");
    if(l[0].compare("session_id") != 0){
        return false;
    }

    // Lit le fichier ligne par ligne
    QList<QString> ses;
    while(!in.atEnd()){
        QString line = in.readLine();
        QStringList l = getAttributes(line);

        // Clé = session_id, valeur = liste d'attributs de la ligne correspondante dans le fichier .tsv
        session_tsv[l[0]] = l;
        // Crée une liste des sessions qu'a effectué le patient
        ses.append(l[0]);
    }
    // Clé = participant_id, valeur = la liste des sessions qui lui sont associées
    sessions_from_patient[parentId] = ses;

    return true;
}

// met à jour interface graphique 
QList<medAbstractParameter *> medBIDS::getAllParameters()
{
    QList<medAbstractParameter *> paramListRes;

    paramListRes.push_back(root_path);

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
    return 3;
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
    return pi_uiLevel == 3;
}


// Permet d'obtenir les attributs qui composent un niveau
QStringList medBIDS::getMandatoryAttributesKeys(unsigned int pi_uiLevel)
{
    switch (pi_uiLevel)
    {
        case 0:
            return m_MandatoryKeysByLevel["Subject"];//return {"id", "name", "subjectSourceId", "protocol", "center"};
        case 1:
            return m_MandatoryKeysByLevel["Session"];//return {"id", "name", "sessionSourceId"};
        case 2:
            return m_MandatoryKeysByLevel["Data"];//return {"id", "name"};
        case 3:
            return m_MandatoryKeysByLevel["Files"];//return {"id", "name", "Modality", "SeriesDescription", "ProtocolName", "BodyPartExamined"};
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

    // Obtient le répertoire du chemin d'accès
    QDir dir(root_path->value());

    // Récupère tous les dossiers du répertoire contenant le préfixe "sub-"
    QStringList prefixes({"sub-*"});
    QFileInfoList files_list = dir.entryInfoList(prefixes, QDir::Dirs);
    for(auto file : files_list){
        // Vérifie qu'il s'agit bien d'un dossier
        if(!file.isFile()){

            // Test si le dossier (= 'participant_id') de cette entité sujet appartient à la QMap (= inclus dans le fichier .tsv)
            if(!patient_tsv.contains(file.fileName())){
                continue;
            }
            // Récupère les valeurs de la clé de l'entité subject ('participant_id')
            QStringList attributes = patient_tsv.value(file.fileName());

            // Complète les attributs
            levelMinimalEntries entry;
            entry.key = attributes[0];
            entry.name = attributes[0];
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

    // Récupère la liste des sessions du patient 
    QList<QString> ses = sessions_from_patient.value(parentId);

    // Reconstruit le chemin d'accès pour se positionner dans le dossier sub
    QDir dir(root_path->value() + parentId + "/");

    // Récupère tous les dossiers du répertoire contenant le préfixe "ses-"
    QStringList prefixes({"ses-*"});
    QFileInfoList files_list = dir.entryInfoList(prefixes, QDir::Dirs);
    for(auto file : files_list){
        // Vérifie qu'il s'agit bien d'un dossier
        if(!file.isFile()){
    
            // Vérifie que cette session appartient à celles effctuées par ce patient (= mentionné dans le fichier .tsv)
            if(!ses.contains(file.fileName())){
                continue;
            }

            // Récupère les valeurs correspondant à la cette session
            QStringList attributes = session_tsv.value(file.fileName());

            // Complète les attributs
            levelMinimalEntries entry;
            // parentId pour mieux identifier quel patient est associé à cette session
            entry.key = parentId + "_" + attributes[0];
            entry.name = attributes[0];
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
    QStringList sub_dirs = parentId.split("_");

    // Reconstruit le chemin d'accès, à partir de ces entités, pour se positionner dans le dossier ses
    QDir dir(root_path->value() + sub_dirs[0] + "/" + sub_dirs[1] + "/");

    // Récupère tous les dossiers du répertoire contenant les types de données
    QFileInfoList files_list = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for(auto file : files_list){
        // Vérifie qu'il s'agit bien d'un dossier
        if(!file.isFile()){ // && !file.fileName().contains(".")){

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
    QStringList sub_dirs = parentId.split("_");

    // Reconstruit le chemin d'accès pour se positionner dans le répertoire du data
    QString file_path(root_path->value() + sub_dirs[0] + "/" + sub_dirs[1] + "/" + sub_dirs[2] + "/");
    QDir dir(file_path);

    // Récupère uniqument les fichiers au format json
    QStringList prefixes({"*.nii.gz"});
    QFileInfoList files_list = dir.entryInfoList(prefixes, QDir::Files);
    for(auto file : files_list){
        // Vérifie qu'il s'agit bien d'un fichier
        if(file.isFile()){

            // Séparation avec ~ pour simplifier le getDirectData (et aller chercher le nifti): path ~ file_name
            // Enlever extension
            QString file_name = file.fileName().split(".")[0];

            // Lit le fichier json
            QJsonObject json_obj = readJsonFile(file_path + file.fileName());
            // Si cette liste n'est pas vide
            QString file_series = "";
            if(!json_obj.isEmpty()){
                //Récupère la valeur du tag "seriesDescription" pour le champ .name
                file_series = json_obj.value("SeriesDescription").toString();
            }

            levelMinimalEntries entry;
            entry.key = parentId + "~" + file_name;
            entry.name = file_series;
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
        default:
            break;
    }

    return res;
}


QList<QMap<QString, QString>> medBIDS::getSubjectMandatoriesAttributes(const QString &key)
{
    QList< QMap<QString, QString>> subjectAttributes;

    // Récupère les clés du niveau entité 'subject'
    QStringList subjectKeys = getMandatoryAttributesKeys(0); //Subject

    // se place dans le répertoire
    QDir dir(root_path->value());
    // Trouve tous les dossiers des subject
    QStringList prefixes({"sub-*"});
    QFileInfoList files_list = dir.entryInfoList(prefixes, QDir::Dirs);
    for(auto file : files_list){
        // Vérifie qu'il s'agit bien d'un dossier
        if(!file.isFile()){

            // Test si le participant_id de cette entité appartient à la QMap
            if(!patient_tsv.contains(file.fileName())){
                continue;
            }

            // Récupère les valeurs de la clé 'participant_id' (= filename)
            QStringList attributes = patient_tsv.value(file.fileName());

            // Stocke les attributs dans une QMap : minimalEntries + subjectKeys
            QMap<QString, QString> subjectMap;
            subjectMap["id"] = attributes[0];
            subjectMap["description"] = "";
            subjectMap["type"] = entryTypeToString(entryType::folder);

            // Associe aux subjectKeys les attributes issus du fichier tsv
            for(int i=1; i <= attributes.size(); ++i){
                subjectMap[subjectKeys[i]] = attributes[i-1];
            }
            subjectAttributes.append(subjectMap);
        }
    }

    return subjectAttributes;
}


QList<QMap<QString, QString>> medBIDS::getSessionMandatoriesAttributes(const QString &key)
{
    QList< QMap<QString, QString>> sessionAttributes;

    // Récupère les clés du niveau entité 'session'
    QStringList sessionKeys = getMandatoryAttributesKeys(1); //Session

    // Test si le dossier possède un fichier .tsv de session et en extrait les données le cas échéant
    if(!getSessionsFromTsv(key)){
        return sessionAttributes;
    }

    // Récupère la liste des sessions du patient 
    QList<QString> ses = sessions_from_patient.value(key);

    // Reconstruit le chemin d'accès pour se placer dans le dossier sub
    QDir dir(root_path->value() + key + "/");

    // Chercher tous les dossiers du répertoire contenant le préfixe "ses-"
    QStringList prefixes({"ses-*"});
    QFileInfoList files_list = dir.entryInfoList(prefixes);
    for(auto file : files_list){
        // Vérifie qu'il s'agit bien d'un dossier
        if(!file.isFile()){
    
            // Vérifie que cette session appartient bien à celles effectuées par ce patient
            if(!ses.contains(file.fileName())){
                continue;
            }

            // Récupère les valeurs de la clé 'session_id' (= filename)
            QStringList attributes = session_tsv.value(file.fileName());

            // Stocke les attributs dans une QMap : minimalEntries + sessionKeys
            QMap<QString, QString> sessionMap;
            // key = participant_id
            sessionMap["id"] = key + "_" + attributes[0];
            sessionMap["description"] = "";
            sessionMap["type"] = entryTypeToString(entryType::folder);

            // Associe aux sessionKeys les attributes issus du fichier tsv
            for(int i=1; i <= attributes.size(); ++i){
                sessionMap[sessionKeys[i]] = attributes[i-1];
            }
            sessionAttributes.append(sessionMap);
        }
    }

    // juste un petit code pour afficher contenu
    // for(auto attr : sessionAttributes){
    //     for(auto key : attr.keys()){
    //         std::cout << key.toStdString() << " : " << attr.value(key).toStdString() << std::endl;
    //     }
    // }

    return sessionAttributes;
}


QList<QMap<QString, QString>> medBIDS::getDataMandatoriesAttributes(const QString &key)
{
    QList< QMap<QString, QString>> dataAttributes;

    // Identifie et récupère les entités subject et session correspondantes 
    QStringList sub_dirs = key.split("_");

    // Reconstruit le chemin d'accès pour se placer dans le dossier ses
    QDir dir(root_path->value() + sub_dirs[0] + "/" + sub_dirs[1] + "/");

    // Récupère tous les dossiers du répertoire contenant les types de données
    QFileInfoList files_list = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for(auto file : files_list){
        // Vérifie qu'il s'agit bien d'un dossier
        if(!file.isFile() ){ 

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

    // juste un petit code pour afficher contenu
    // for(auto attr : dataAttributes){
    //     for(auto key : attr.keys()){
    //         std::cout << key.toStdString() << " : " << attr.value(key).toStdString() << std::endl;
    //     }
    // }

    return dataAttributes;
}


// Renvoie un objet qui contient le contenu du fichier json
QJsonObject medBIDS::readJsonFile(QString file_path){
    QJsonObject json_obj;
    // Ouvre le fichier
    QFile file(file_path);
    if(!file.open(QIODevice::ReadOnly)){
        return json_obj;
    }

    // Lit le fichier pour récupérer le contenu tel quel
    QString content(file.readAll());
    file.close();

    // QByteArray en paramètre : tableau d'octets
    // Le transforme en objet Json pour simplifier la récupération des valeurs (similaire à QMap -> dictionnaire)
    if(!content.isEmpty()){
        QJsonDocument json_doc = QJsonDocument::fromJson(content.toUtf8());
        json_obj = json_doc.object();
    }

    return json_obj;
}


// Version 1
QList<QMap<QString, QString>> medBIDS::getFilesMandatoriesAttributes(const QString &key)
{
    QList< QMap<QString, QString>> filesAttributes;

    // Récupère les clés du niveau 'files'
    QStringList filesKeys = getMandatoryAttributesKeys(3); //Files

    // Identifie et récupère les entités, subject et session, et le data correspondant
    QStringList sub_dirs = key.split("_");

    // Reconstruit le chemin d'accès pour ce placer dans le répertoire data
    QString file_path(root_path->value() + sub_dirs[0] + "/" + sub_dirs[1] + "/" + sub_dirs[2] + "/");
    QDir dir(file_path);

    // Récupère uniqument les fichiers au format json
    QStringList prefixes({"*.nii.gz"});
    QFileInfoList files_list = dir.entryInfoList(prefixes, QDir::Files);
    for(auto file : files_list){
        // Vérifie qu'il s'agit bien d'un fichier
        if(file.isFile()){

            // Récupère le nom du fichier pour compléter les attributs
            QMap<QString, QString> filesMap;

            // Séparation avec ~ pour simplifier le getDirectData (et aller chercher le nifti): path ~ file_name
            // Enlever extension
            QString file_name = file.fileName().split(".")[0];
            // key = participant_id + session_id + data
            filesMap["id"] = key + "~" + file_name;
            filesMap["description"] = "";
            filesMap["type"] = entryTypeToString(entryType::dataset);

            // Lit le fichier .json correspondant pour récupérer les clés/valeurs des tags DICOM
            QJsonObject json_obj = readJsonFile(file_path + file_name + ".json");
            // Si cette liste n'est pas vide
            if(!json_obj.isEmpty()){
                filesMap["SeriesDescription"] = json_obj.value("SeriesDescription").toString();
                // Récupère les valeurs des tags des MandatoryAttributesKeys
                for(int i=4; i < filesKeys.size(); ++i){
                    // Convertis la valeur en QString
                    if(json_obj.value(filesKeys[i]).isString())
                    {
                        filesMap[filesKeys[i]] = json_obj.value(filesKeys[i]).toString();
                    }
                    else if(json_obj.value(filesKeys[i]).isArray()) //Si c'est un array, le transforme en array pour ensuite le parcourir
                    {
                        // Récupère le tableau pour récupérer son contenu en QString
                        QJsonArray json_tab(json_obj.value(filesKeys[i]).toArray());
                        QString types = "[";
                        // auto = QJsonValueRef
                        for(auto type : json_tab){
                            if(type.isString()){
                                types.append(type.toString() + ", ");
                            }else{
                                types.append(type.toVariant().toString() + ", ");
                            }
                        }
                        // supprime les deux derniers caractères
                        types.chop(2);
                        types.append("]");
                        filesMap[filesKeys[i]] = types;
                    }
                    else
                    { // Si c'est un int, passe d'abord par une 'variant'
                        filesMap[filesKeys[i]] = json_obj.value(filesKeys[i]).toVariant().toString();
                    }
                }
            }else{
                // Si pas de json associé mais le nom du fichier pour le champ .name
                filesMap["SeriesDescription"] = file_name;
            }

            filesAttributes.append(filesMap);
        }
    }

    return filesAttributes;
}



 
bool medBIDS::getAdditionalAttributes(unsigned int pi_uiLevel, QString id, datasetAttributes &po_attributes)
{
    std::cout << "Passe ici ?" << std::endl;
    bool res = false;
    switch (pi_uiLevel)
    {
        case 0:
            res = true;
            break;
        case 1:
            res = true;
            break;
        case 2:
            res = true;
            break;
        case 3:
            res = getFilesAdditionalAttributes(id, po_attributes);
            break;
        default:
            break;
    }

    return res;
}


bool medBIDS::getFilesAdditionalAttributes(const QString &key, medAbstractSource::datasetAttributes &po_attributes){
    std::cout << "Test de l'affichage de la key :" << key.toStdString() << std::endl;

    // Recrée le chemin vers le fichier
    QStringList dir(key.split("~"));
    QString file_path(dir[0].split("_").join("/"));
    QString file_name(dir[1] + ".json");

    // Récupérer les tags DICOM déjà utilisés pour les mandatoryAttributes du fichier
    QStringList filesKeys = getMandatoryAttributesKeys(3);

    QString path(root_path->value() + file_path + "/" + file_name);
    // à modifier selon le paramètre 'key'
    QJsonObject json_obj = readJsonFile(key);
    if(!json_obj.isEmpty()){
        // Récupère les clés des tags DICOM contenus dans le fichier
        QStringList dcm_tags = json_obj.keys();  
        for(auto tag : dcm_tags){
            // Si ce tag n'est pas une mandatoryKey des fichiers, alors il est additional
            if(!filesKeys.contains(tag)){
                if(json_obj.value(tag).isString())
                {
                    QString tag_value = json_obj.value(tag).toString();
                    if(tag_value.contains("\r\n")){
                        tag_value = tag_value.replace("\r\n", ", ");
                    }
                    po_attributes.values[tag] = tag_value;
                }
                else if(json_obj.value(tag).isArray()) //Si c'est un array, le transforme en array pour ensuite le parcourir
                {
                    // Récupère le tableau pour récupérer son contenu en QString
                    QJsonArray json_tab(json_obj.value(tag).toArray());
                    QString types = "[";
                    // auto = QJsonValueRef
                    for(auto type : json_tab){
                        if(type.isString()){
                            types.append(type.toString() + ", ");
                        }else{
                            types.append(type.toVariant().toString() + ", ");
                        }
                    }
                    // supprime les deux derniers caractères
                    types.chop(2);
                    types.append("]");
                    po_attributes.values[tag] = types;
                }
                else
                { // Si c'est un int, passe d'abord par une 'variant'
                    po_attributes.values[tag] = json_obj.value(tag).toVariant().toString();
                }
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
    if(pi_uiLevel == 3){
        // Split pour récupérer 'path' du 'filename' : ajoute l'extension nifti au filename
        QStringList dir(key.split("~"));
        QString file_path(dir[0].split("_").join("/"));
        QString file_name(dir[1] + ".nii.gz");
        
        // Indique au programme le répertoire du fichier
        res = QVariant(root_path->value() + file_path + "/" + file_name);
    }
    return res;
}


int medBIDS::getAssyncData(unsigned int pi_uiLevel, QString id)
{
    std::cout << "Passe par getAssyncData" << std::endl;
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
    m_SupportedTypeAndFormats["Image"] = QStringList({".nii.gz"});
    //m_SupportedTypeAndFormats["Header"] = QStringList({".json", ".bvec", ".bval"});

    return m_SupportedTypeAndFormats;
}


// Ajoute un fichier dans l'arborescence du logiciel (glissé-déposé), puis le copie au même endroit dans l'arborescence locale 
bool medBIDS::addDirectData(QVariant data, levelMinimalEntries &pio_minimalEntries, unsigned int pi_uiLevel, QString parentKey)
{
    bool bRes = false;

    if(pi_uiLevel == 3){
        // chemin de stockage/lecture
        QString path_in = data.toString();
        // Récupère les formats acceptés pour les fichiers : ajouter .bvec et .bval ? 
        QStringList extensions;
        for(QStringList formats : m_SupportedTypeAndFormats.values()){
            extensions.append(formats);
        }

        // Compare avec le fichier d'entrée voir s'il est correct
        bool correct_ext = false;
        QString file_ext;
        for(auto ext : extensions){
            if(path_in.contains(ext)){
                file_ext = ext;
                correct_ext = true;
                break;
            }
        }

        // Si l'extension appartient à celles possibles
        if(correct_ext == true){
            // Crée le chemin dans l'arborescence BIDS : parentKey contient l'identifiant créé pour un niveau fichier, donc les sous-dossiers sub, ses et data
            QStringList sub_dirs(parentKey.split("_"));
            // nom du fichier contenu dans .name sans l'extension donc la rajoute
            QString file(pio_minimalEntries.name + file_ext);
            QString path_out_copy = root_path->value() + sub_dirs.join("/") + "/" + file;

            // Copie le fichier path_in vers path_out_copy (chemin BIDS) : true si succès
            if(QFile::copy(path_in, path_out_copy)){
                // Complète les minimalEntries du fichier : à l'origine ne contient que le champ .name
                pio_minimalEntries.key = parentKey + "~" + pio_minimalEntries.name;
                pio_minimalEntries.description = "";
                pio_minimalEntries.type = entryType::dataset;
            }
            bRes = true;
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
    std::cout << "Appel à la classe createFolder" << std::endl;
    if(pi_uiLevel == 0){
        std::cout << "Appel \"createFolder\" avec les paramètres suivants" << std::endl;
        std::cout << "pi_uiLevel = " << pi_uiLevel << std::endl;
        std::cout << "parentKey = " << parentKey.toStdString() << std::endl;
        std::cout << "pio_minimalEntries : " << std::endl;
        std::cout << pio_minimalEntries.key.toStdString() << std::endl;
        std::cout << pio_minimalEntries.name.toStdString() << std::endl;
        std::cout << pio_minimalEntries.description.toStdString() << std::endl;
    }
    
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

