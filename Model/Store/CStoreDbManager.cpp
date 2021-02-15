// Copyright (C) 2021 Ikomia SAS
// Contact: https://www.ikomia.com
//
// This file is part of the IkomiaStudio software.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "CStoreDbManager.h"
#include <QSqlError>
#include "CException.h"
#include "UtilsTools.hpp"
#include "Main/AppTools.hpp"

CStoreDbManager::CStoreDbManager()
{
#if defined(Q_OS_LINUX)
    m_currentOS = CProcessInfo::LINUX;
#elif defined(Q_OS_MACOS)
    m_currentOS = CProcessInfo::OSX;
#elif defined(Q_OS_WIN64)
    m_currentOS = CProcessInfo::WIN;
#endif
}

void CStoreDbManager::initDb()
{
    createServerPluginsDb();
}

QSqlDatabase CStoreDbManager::getServerPluginsDatabase() const
{
    auto db = Utils::Database::connect(m_name, m_serverConnectionName);
    if(db.isValid() == false)
        throw CException(DatabaseExCode::INVALID_DB_CONNECTION, db.lastError().text().toStdString(), __func__, __FILE__, __LINE__);

    return db;
}

QSqlDatabase CStoreDbManager::getLocalPluginsDatabase() const
{
    auto db = Utils::Database::connect(m_name, Utils::Database::getProcessConnectionName());
    if(db.isValid() == false)
        throw CException(DatabaseExCode::INVALID_DB_CONNECTION, db.lastError().text().toStdString(), __func__, __FILE__, __LINE__);

    return db;
}

QString CStoreDbManager::getAllServerPluginsQuery() const
{
    return QString("SELECT * FROM serverPlugins;");
}

QString CStoreDbManager::getAllLocalPluginsQuery() const
{
    return QString("SELECT * FROM process WHERE isInternal=False;");
}

QString CStoreDbManager::getLocalSearchQuery(const QString &searchText) const
{
    QString searchKey = Utils::Database::getFTSKeywords(searchText);
    QString query = QString("SELECT p.* FROM process p INNER JOIN processFTS pFts ON p.id = pFts.id "
                            "WHERE p.isInternal=False AND processFTS MATCH '%1';")
                        .arg(searchKey);
    return query;
}

QString CStoreDbManager::getServerSearchQuery(const QString &searchText) const
{
    QString searchKey = Utils::Database::getFTSKeywords(searchText);
    QString query = QString("SELECT sp.* FROM serverPlugins sp INNER JOIN serverPluginsFTS spFts ON sp.id = spFts.id "
                            "WHERE serverPluginsFTS MATCH '%1';")
                        .arg(searchKey);
    return query;
}

void CStoreDbManager::setLocalPluginServerInfo(int pluginId, const QString name, int serverId, const CUser &user)
{
    auto dbMemory = Utils::Database::connect(m_name, Utils::Database::getProcessConnectionName());
    if(dbMemory.isValid() == false)
        throw CException(DatabaseExCode::INVALID_DB_CONNECTION, dbMemory.lastError().text().toStdString(), __func__, __FILE__, __LINE__);

    if(pluginId == -1)
        pluginId = getLocalIdFromServerId(dbMemory, serverId);

    if(pluginId == -1)
        throw CException(DatabaseExCode::INVALID_ID, "Plugin ID not found", __func__, __FILE__, __LINE__);

    QString userFullName;
    if(user.m_firstName.isEmpty() && user.m_lastName.isEmpty())
        userFullName = user.m_name;
    else
        userFullName = user.m_firstName + " " + user.m_lastName;

    //Update memory database
    QSqlQuery q1(dbMemory);
    if(!q1.exec(QString("UPDATE process SET user='%1', serverId=%2, userId=%3 WHERE id=%4")
                .arg(userFullName)
                .arg(serverId)
                .arg(user.m_id)
                .arg(pluginId)))
    {
        throw CException(DatabaseExCode::INVALID_QUERY, q1.lastError().text().toStdString(), __func__, __FILE__, __LINE__);
    }

    //Update file database
    auto dbFile = Utils::Database::connect(Utils::Database::getMainPath(), Utils::Database::getMainConnectionName());
    if(dbFile.isValid() == false)
        throw CException(DatabaseExCode::INVALID_DB_CONNECTION, dbFile.lastError().text().toStdString(), __func__, __FILE__, __LINE__);

    QSqlQuery q2(dbFile);
    if(!q2.exec(QString("INSERT INTO process "
                        "(name, user, serverId, userId) "
                        "VALUES ('%1', '%2', %3, %4) "
                        "ON CONFLICT(name) DO UPDATE SET "
                        "user = COALESCE(excluded.user, user), "
                        "serverId = excluded.serverId, "
                        "userId = excluded.userId;")
                 .arg(name)
                 .arg(userFullName)
                 .arg(serverId)
                 .arg(user.m_id)))
    {
        throw CException(DatabaseExCode::INVALID_QUERY, q2.lastError().text().toStdString(), __func__, __FILE__, __LINE__);
    }
}

void CStoreDbManager::insertPlugins(const QJsonArray &plugins)
{
    auto db = Utils::Database::connect(m_name, m_serverConnectionName);
    if(db.isValid() == false)
        throw CException(DatabaseExCode::INVALID_DB_CONNECTION, db.lastError().text().toStdString(), __func__, __FILE__, __LINE__);

    //Retrieve plugins information from JSON
    QVariantList ids, names, shortDescriptions, descriptions, keywords, userNames, authors, articles,
            journals, years, docLinks, createdDates, modifiedDates, versions, ikomiaVersions, languages,
            licenses, repositories, os, iconPaths, certifications, userIds, userReputations, votes;

    for(int i=0; i<plugins.size(); ++i)
    {
        QJsonObject plugin = plugins[i].toObject();

        // Check OS compatibility
        int pluginOS = plugin["os"].toInt();
        if(pluginOS != CProcessInfo::ALL && m_currentOS != pluginOS)
            continue;

        ids << plugin["id"].toInt();
        names << plugin["name"].toString();
        shortDescriptions << plugin["short_description"].toString();
        descriptions << plugin["description"].toString();
        keywords << plugin["keywords"].toString();
        authors << plugin["authors"].toString();
        articles << plugin["article"].toString();
        journals << plugin["journal"].toString();
        years << plugin["year"].toInt();
        docLinks << plugin["docLink"].toString();
        createdDates << plugin["createdDate"].toString();
        modifiedDates << plugin["modifiedDate"].toString();
        versions << plugin["version"].toString();
        ikomiaVersions << plugin["ikomiaVersion"].toString();;
        languages << plugin["language"].toInt();
        licenses << plugin["license"].toString();
        repositories << plugin["repository"].toString();
        os << pluginOS;
        iconPaths << plugin["iconPath"].toString();
        certifications << plugin["certification"].toInt();
        votes << plugin["votes_count"].toInt();

        QJsonObject user = plugin["user"].toObject();
        userIds << user["pk"].toInt();
        userReputations << user["reputation"].toInt();

        if(user["first_name"].toString().isEmpty() && user["last_name"].toString().isEmpty())
            userNames << user["username"].toString();
        else
            userNames << user["first_name"].toString() + " " + user["last_name"].toString();
    }

    //Insert to serverPlugins table
    QSqlQuery q(db);
    if(!q.prepare(QString("INSERT INTO serverPlugins ("
                          "id, name, shortDescription, description, keywords, user, authors, article, journal, "
                          "year, docLink, createdDate, modifiedDate, version, ikomiaVersion, language, license, "
                          "repository, os, iconPath, certification, votes, userId, userReputation) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);")))
    {
        throw CException(DatabaseExCode::INVALID_QUERY, q.lastError().text().toStdString(), __func__, __FILE__, __LINE__);
    }

    q.addBindValue(ids);
    q.addBindValue(names);
    q.addBindValue(shortDescriptions);
    q.addBindValue(descriptions);
    q.addBindValue(keywords);
    q.addBindValue(userNames);
    q.addBindValue(authors);
    q.addBindValue(articles);
    q.addBindValue(journals);
    q.addBindValue(years);
    q.addBindValue(docLinks);
    q.addBindValue(createdDates);
    q.addBindValue(modifiedDates);
    q.addBindValue(versions);
    q.addBindValue(ikomiaVersions);
    q.addBindValue(languages);
    q.addBindValue(licenses);
    q.addBindValue(repositories);
    q.addBindValue(os);
    q.addBindValue(iconPaths);
    q.addBindValue(certifications);
    q.addBindValue(votes);
    q.addBindValue(userIds);
    q.addBindValue(userReputations);

    if(!q.execBatch())
        throw CException(DatabaseExCode::INVALID_QUERY, q.lastError().text().toStdString(), __func__, __FILE__, __LINE__);

    //Insert to FTS table
    QSqlQuery qFts(db);
    if(!qFts.prepare(QString("INSERT INTO serverPluginsFTS "
                             "(id, name, shortDescription, description, keywords, user, authors, article, journal) "
                             "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);")))
    {
        throw CException(DatabaseExCode::INVALID_QUERY, qFts.lastError().text().toStdString(), __func__, __FILE__, __LINE__);
    }

    qFts.addBindValue(ids);
    qFts.addBindValue(names);
    qFts.addBindValue(shortDescriptions);
    qFts.addBindValue(descriptions);
    qFts.addBindValue(keywords);
    qFts.addBindValue(userNames);
    qFts.addBindValue(authors);
    qFts.addBindValue(articles);
    qFts.addBindValue(journals);

    if(!qFts.execBatch())
        throw CException(DatabaseExCode::INVALID_QUERY, qFts.lastError().text().toStdString(), __func__, __FILE__, __LINE__);
}

void CStoreDbManager::insertPlugin(int serverId, const CProcessInfo &procInfo, const CUser &user)
{
    if(procInfo.m_os != CProcessInfo::ALL && m_currentOS != procInfo.m_os)
        throw CException(CoreExCode::INVALID_PARAMETER, "This plugin ("+ procInfo.m_name +") is not build for your platform.",  __func__, __FILE__, __LINE__);

    auto db = Utils::Database::connect(Utils::Database::getMainPath(), Utils::Database::getMainConnectionName());
    if(db.isValid() == false)
        throw CException(DatabaseExCode::INVALID_DB_CONNECTION, db.lastError().text().toStdString(), __func__, __FILE__, __LINE__);

    QSqlQuery q(db);
    auto strQuery = QString("INSERT INTO process "
                            "(name, shortDescription, description, keywords, user, authors, article, journal, year, docLink, "
                            "createdDate, modifiedDate, version, ikomiaVersion, language, license, repository, os, isInternal, iconPath, serverId, userId) "
                            "VALUES ('%1', '%2', '%3', '%4', '%5', '%6', '%7', '%8', %9, '%10', '%11', '%12', '%13', '%14', %15, '%16', '%17', %18, %19, '%20', %21, %22) "
                            "ON CONFLICT(name) DO UPDATE SET "
                            "shortDescription = excluded.shortDescription, "
                            "description = excluded.description, "
                            "keywords = excluded.keywords, "
                            "user = COALESCE(excluded.user, user), "
                            "authors = excluded.authors, "
                            "article = excluded.article, "
                            "journal = excluded.journal, "
                            "year = excluded.year, "
                            "docLink = excluded.docLink, "
                            "modifiedDate = excluded.modifiedDate, "
                            "version = excluded.version, "
                            "ikomiaVersion = excluded.ikomiaVersion, "
                            "license = excluded.license, "
                            "repository = excluded.repository, "
                            "iconPath = excluded.iconPath;")
            .arg(QString::fromStdString(Utils::String::dbFormat(procInfo.m_name)))
            .arg(QString::fromStdString(Utils::String::dbFormat(procInfo.m_shortDescription)))
            .arg(QString::fromStdString(Utils::String::dbFormat(procInfo.m_description)))
            .arg(QString::fromStdString(Utils::String::dbFormat(procInfo.m_keywords)))
            .arg(QString::fromStdString(Utils::String::dbFormat(user.m_firstName.toStdString())))
            .arg(QString::fromStdString(Utils::String::dbFormat(procInfo.m_authors)))
            .arg(QString::fromStdString(Utils::String::dbFormat(procInfo.m_article)))
            .arg(QString::fromStdString(Utils::String::dbFormat(procInfo.m_journal)))
            .arg(procInfo.m_year)
            .arg(QString::fromStdString(Utils::String::dbFormat(procInfo.m_docLink)))
            .arg(QString::fromStdString(Utils::String::dbFormat(procInfo.m_createdDate)))
            .arg(QString::fromStdString(Utils::String::dbFormat(procInfo.m_modifiedDate)))
            .arg(QString::fromStdString(Utils::String::dbFormat(procInfo.m_version)))
            .arg(QString::fromStdString(Utils::String::dbFormat(procInfo.m_ikomiaVersion)))
            .arg(procInfo.m_language)
            .arg(QString::fromStdString(Utils::String::dbFormat(procInfo.m_license)))
            .arg(QString::fromStdString(Utils::String::dbFormat(procInfo.m_repo)))
            .arg(procInfo.m_os)
            .arg(false)
            .arg(QString::fromStdString(Utils::String::dbFormat(procInfo.m_iconPath)))
            .arg(serverId)
            .arg(user.m_id);

    if(!q.exec(strQuery))
        throw CException(DatabaseExCode::INVALID_QUERY, q.lastError().text().toStdString(), __func__, __FILE__, __LINE__);
}

void CStoreDbManager::removeRemotePlugin(const QString& pluginName)
{
    auto db = getServerPluginsDatabase();
    if(db.isValid() == false)
        throw CException(DatabaseExCode::INVALID_DB_CONNECTION, db.lastError().text().toStdString(), __func__, __FILE__, __LINE__);

    // Delete plugin from server database
    QSqlQuery q1(db);
    if(!q1.exec(QString("DELETE FROM serverPlugins WHERE name='%1'").arg(pluginName)))
    {
        throw CException(DatabaseExCode::INVALID_QUERY, q1.lastError().text().toStdString(), __func__, __FILE__, __LINE__);
    }

}

void CStoreDbManager::removeLocalPlugin(const QString& pluginName)
{
    auto dbMemory = Utils::Database::connect(m_name, Utils::Database::getProcessConnectionName());
    if(dbMemory.isValid() == false)
        throw CException(DatabaseExCode::INVALID_DB_CONNECTION, dbMemory.lastError().text().toStdString(), __func__, __FILE__, __LINE__);

    //Update plugin in memory database with serverId -1 -> allows display in storeDlg
    QSqlQuery q1(dbMemory);
    if(!q1.exec(QString("UPDATE process SET serverId=-1 WHERE name='%1'").arg(pluginName)))
    {
        throw CException(DatabaseExCode::INVALID_QUERY, q1.lastError().text().toStdString(), __func__, __FILE__, __LINE__);
    }

    //Delete plugin in file database
    auto dbFile = Utils::Database::connect(Utils::Database::getMainPath(), Utils::Database::getMainConnectionName());
    if(dbFile.isValid() == false)
        throw CException(DatabaseExCode::INVALID_DB_CONNECTION, dbFile.lastError().text().toStdString(), __func__, __FILE__, __LINE__);

    QSqlQuery q2(dbFile);
    if(!q2.exec(QString("DELETE FROM process WHERE name='%1'").arg(pluginName)))
    {
        throw CException(DatabaseExCode::INVALID_QUERY, q2.lastError().text().toStdString(), __func__, __FILE__, __LINE__);
    }
}

void CStoreDbManager::updateLocalPluginModifiedDate(int pluginId)
{
    QString modifiedDate = QDateTime::currentDateTime().toString(Qt::ISODate);
    QString queryStr = QString("UPDATE process SET modifiedDate='%1' WHERE id=%2")
            .arg(modifiedDate)
            .arg(pluginId);

    //Update memory database
    auto dbMemory = Utils::Database::connect(m_name, Utils::Database::getProcessConnectionName());
    if(dbMemory.isValid() == false)
        throw CException(DatabaseExCode::INVALID_DB_CONNECTION, dbMemory.lastError().text().toStdString(), __func__, __FILE__, __LINE__);

    QSqlQuery q1(dbMemory);
    if(!q1.exec(queryStr))
        throw CException(DatabaseExCode::INVALID_QUERY, q1.lastError().text().toStdString(), __func__, __FILE__, __LINE__);

    //Update file database
    auto dbFile = Utils::Database::connect(Utils::Database::getMainPath(), Utils::Database::getMainConnectionName());
    if(dbFile.isValid() == false)
        throw CException(DatabaseExCode::INVALID_DB_CONNECTION, dbFile.lastError().text().toStdString(), __func__, __FILE__, __LINE__);

    QSqlQuery q2(dbFile);
    if(!q2.exec(queryStr))
        throw CException(DatabaseExCode::INVALID_QUERY, q2.lastError().text().toStdString(), __func__, __FILE__, __LINE__);
}

void CStoreDbManager::updateMemoryLocalPluginsInfo()
{
    //Get server certification, votes and user reputation for all plugins
    auto dbServer = Utils::Database::connect(m_name, m_serverConnectionName);

    QSqlQuery q1(dbServer);
    if(!q1.exec("SELECT id, certification, votes, userReputation FROM serverPlugins;"))
        throw CException(DatabaseExCode::INVALID_QUERY, q1.lastError().text().toStdString(), __func__, __FILE__, __LINE__);

    QVariantList serverIds, serverCertifications, serverVotes, userReputations;
    while(q1.next())
    {
        serverIds << q1.value("id");
        serverCertifications << q1.value("certification");
        serverVotes << q1.value("votes");
        userReputations << q1.value("userReputation");
    }

    //Update certification for installed plugins
    auto dbLocal = Utils::Database::connect(m_name, Utils::Database::getProcessConnectionName());

    QSqlQuery q2(dbLocal);
    q2.prepare(QString("UPDATE process SET "
                       "certification = IFNULL(?, certification), "
                       "votes = IFNULL(?, votes), "
                       "userReputation = IFNULL(?, userReputation) "
                       "WHERE serverId = ?;"));

    q2.addBindValue(serverCertifications);
    q2.addBindValue(serverVotes);
    q2.addBindValue(userReputations);
    q2.addBindValue(serverIds);

    if(!q2.execBatch())
        throw CException(DatabaseExCode::INVALID_QUERY, q2.lastError().text().toStdString(), __func__, __FILE__, __LINE__);
}

void CStoreDbManager::clearServerRecords()
{
    auto db = Utils::Database::connect(m_name, m_serverConnectionName);
    if(db.isValid() == false)
        throw CException(DatabaseExCode::INVALID_DB_CONNECTION, db.lastError().text().toStdString(), __func__, __FILE__, __LINE__);

    QSqlQuery q(db);
    if(!q.exec("DELETE FROM serverPlugins"))
        throw CException(DatabaseExCode::INVALID_QUERY, q.lastError().text().toStdString(), __func__, __FILE__, __LINE__);

    if(!q.exec("DELETE FROM serverPluginsFTS"))
        throw CException(DatabaseExCode::INVALID_QUERY, q.lastError().text().toStdString(), __func__, __FILE__, __LINE__);
}

void CStoreDbManager::createServerPluginsDb()
{
    QSqlDatabase db = QSqlDatabase::addDatabase(m_type, m_serverConnectionName);
    if(!db.isValid())
        throw CException(DatabaseExCode::INVALID_DB_CONNECTION, db.lastError().text().toStdString(), __func__, __FILE__, __LINE__);

    db.setDatabaseName(m_name);

    if(!db.open())
        throw CException(DatabaseExCode::INVALID_DB_CONNECTION, db.lastError().text().toStdString(), __func__, __FILE__, __LINE__);

    QSqlQuery q(db);
    if(!q.exec("CREATE TABLE serverPlugins ("
               "id INTEGER PRIMARY KEY, name TEXT UNIQUE NOT NULL, shortDescription TEXT, description TEXT, keywords TEXT, "
               "user TEXT, authors TEXT, article TEXT, journal TEXT, year INTEGER, docLink TEXT, createdDate TEXT, "
               "modifiedDate TEXT, version TEXT, ikomiaVersion TEXT, license TEXT, repository TEXT, language INTEGER, "
               "os INTEGER, iconPath TEXT, certification INTEGER, votes INTEGER, userId INTEGER, userReputation INTEGER);"))
    {
        throw CException(DatabaseExCode::INVALID_QUERY, q.lastError().text().toStdString(), __func__, __FILE__, __LINE__);
    }

    if(!q.exec("CREATE VIRTUAL TABLE serverPluginsFTS USING fts5(id, name, shortDescription, description, keywords, user, authors, article, journal);"))
        throw CException(DatabaseExCode::INVALID_QUERY, q.lastError().text().toStdString(), __func__, __FILE__, __LINE__);
}

int CStoreDbManager::getLocalIdFromServerId(const QSqlDatabase &db, int serverId) const
{
    QSqlQuery q(db);
    if(!q.exec(QString("SELECT id FROM process WHERE serverId=%1").arg(serverId)))
        return -1;

    if(q.first())
        return q.value(0).toInt();
    else
        return -1;
}
