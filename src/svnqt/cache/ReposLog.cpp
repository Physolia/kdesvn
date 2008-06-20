#include "ReposLog.hpp"

#include "LogCache.hpp"
#include "svnqt/info_entry.hpp"
#include "svnqt/svnqttypes.hpp"
#include "svnqt/client.hpp"
#include "svnqt/context_listener.hpp"
#include "svnqt/cache/DatabaseException.hpp"

#include <qsqldatabase.h>

#if QT_VERSION < 0x040000
#else
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#define Q_LLONG qlonglong
#endif

/*!
    \fn svn::cache::ReposLog::ReposLog(svn::Client*aClient,const QString&)
 */
svn::cache::ReposLog::ReposLog(svn::Client*aClient,const QString&aRepository)
    :m_Client(aClient),
#if QT_VERSION < 0x040000
              m_Database(0),
#else
              m_Database(),
#endif
              m_ReposRoot(aRepository),m_latestHead(svn::Revision::UNDEFINED)
{
    if (!aRepository.isEmpty()) {
        m_Database = LogCache::self()->reposDb(aRepository);
    }
}


/*!
    \fn svn::cache::ReposLog::latestHeadRev()
 */
svn::Revision svn::cache::ReposLog::latestHeadRev()
{
    if (!m_Client||m_ReposRoot.isEmpty()) {
        return svn::Revision::UNDEFINED;
    }
#if QT_VERSION < 0x040000
    if (!m_Database) {
#else
    if (!m_Database.isValid()) {
#endif
        m_Database = LogCache::self()->reposDb(m_ReposRoot);
#if QT_VERSION < 0x040000
        if (!m_Database) {
#else
        if (!m_Database.isValid()) {
#endif
            return svn::Revision::UNDEFINED;
        }
    }
    /// no catch - exception has go trough...
    svn::InfoEntries e = (m_Client->info(m_ReposRoot,svn::DepthEmpty,svn::Revision::HEAD,svn::Revision::HEAD));;
    if (e.count()<1||e[0].reposRoot().isEmpty()) {
        return svn::Revision::UNDEFINED;
    }
    return e[0].revision();
}


/*!
    \fn svn::cache::ReposLog::latestCachedRev()
 */
svn::Revision svn::cache::ReposLog::latestCachedRev()
{
    if (m_ReposRoot.isEmpty()) {
        return svn::Revision::UNDEFINED;
    }
#if QT_VERSION < 0x040000
    if (!m_Database) {
#else
    if (!m_Database.isValid()) {
#endif
        m_Database = LogCache::self()->reposDb(m_ReposRoot);
#if QT_VERSION < 0x040000
        if (!m_Database) {
#else
        if (!m_Database.isValid()) {
#endif
            return svn::Revision::UNDEFINED;
        }
    }
    QString q("select revision from 'logentries' order by revision DESC limit 1");
    QSqlQuery _q(QString::null, m_Database);
    if (!_q.exec(q)) {
        qDebug(_q.lastError().text().TOUTF8().data());
        return svn::Revision::UNDEFINED;
    }
    int _r;
    if (_q.isActive() && _q.next()) {
        //qDebug("Sel result: %s",_q.value(0).toString().TOUTF8().data());
        _r = _q.value(0).toInt();
    } else {
        qDebug(_q.lastError().text().TOUTF8().data());
        return svn::Revision::UNDEFINED;
    }
    return _r;
}

bool svn::cache::ReposLog::checkFill(svn::Revision&start,svn::Revision&end)
{
#if QT_VERSION < 0x040000
    if (!m_Database) {
#else
    if (!m_Database.isValid()) {
#endif
        m_Database = LogCache::self()->reposDb(m_ReposRoot);
#if QT_VERSION < 0x040000
        if (!m_Database) {
#else
        if (!m_Database.isValid()) {
#endif
            return false;
        }
    }
    ContextP cp = m_Client->getContext();
    long long icount=0;

    svn::Revision _latest=latestCachedRev();
    qDebug("Latest cached rev: %i",_latest.revnum());
    if (_latest.revnum()>=latestHeadRev().revnum()) {
        return true;
    }

    start=date2numberRev(start);
    end=date2numberRev(end);

    // both should now one of START, HEAD or NUMBER
    if (start==svn::Revision::HEAD || (end==svn::Revision::NUMBER && start==svn::Revision::NUMBER && start.revnum()>end.revnum())) {
        svn::Revision tmp = start;
        start = end;
        end = tmp;
    }
    svn::Revision _rstart=_latest.revnum()+1;
    svn::Revision _rend = end;
    if (_rend==svn::Revision::UNDEFINED) {
        _rend=svn::Revision::HEAD;
    }
    // no catch - exception should go outside.
    if (_rstart==0){
        _rstart = 1;
    }
    qDebug("Getting log %s -> %s",_rstart.toString().TOUTF8().data(),_rend.toString().TOUTF8().data());
    if (_rend==svn::Revision::HEAD) {
        _rend=latestHeadRev();
    }

    if (_rend==svn::Revision::HEAD||_rend.revnum()>_latest.revnum()) {
        LogEntriesMap _internal;
        qDebug("Retrieving from network.");
        if (!m_Client->log(m_ReposRoot,_rstart,_rend,_internal,svn::Revision::UNDEFINED,true,false)) {
            return false;
        }
        LogEntriesMap::ConstIterator it=_internal.begin();

        for (;it!=_internal.end();++it) {
            _insertLogEntry((*it));
            if (cp && cp->getListener()) {
                //cp->getListener()->contextProgress(++icount,_internal.size());
                if (cp->getListener()->contextCancel()) {
                    throw DatabaseException(QString("Could not retrieve values: User cancel."));
                }
            }
        }
    }
    return true;
}

bool svn::cache::ReposLog::fillCache(const svn::Revision&_end)
{
    svn::Revision end = _end;
    svn::Revision start = latestCachedRev().revnum()+1;
    return checkFill(start,end);
}

/*!
    \fn svn::cache::ReposLog::simpleLog(const svn::Revision&start,const svn::Revision&end,LogEntriesMap&target)
 */
bool svn::cache::ReposLog::simpleLog(LogEntriesMap&target,const svn::Revision&_start,const svn::Revision&_end,bool noNetwork)
{
    if (!m_Client||m_ReposRoot.isEmpty()) {
        return false;
    }
    target.clear();
    ContextP cp = m_Client->getContext();

    svn::Revision end = _end;
    svn::Revision start = _start;
    if (!noNetwork) {
        if (!checkFill(start,end)) {
            return false;
        }
    } else {
        end=date2numberRev(end,noNetwork);
        start=date2numberRev(start,noNetwork);
    }

    if (end==svn::Revision::HEAD) {
        end = latestCachedRev();
    }
    if (start==svn::Revision::HEAD) {
        start=latestCachedRev();
    }
    static QString sEntry("select revision,author,date,message from logentries where revision<=? and revision>=?");
    static QString sItems("select changeditem,action,copyfrom,copyfromrev from changeditems where revision=?");

    QSqlQuery bcur(QString::null,m_Database);
    bcur.prepare(sEntry);

    QSqlQuery cur(QString::null,m_Database);
    cur.prepare(sItems);

    bcur.bindValue(0,Q_LLONG(end.revnum()));
    bcur.bindValue(1,Q_LLONG(start.revnum()));

    if (!bcur.exec()) {
        qDebug(bcur.lastError().text().TOUTF8().data());
        throw svn::cache::DatabaseException(QString("Could not retrieve values: ")+bcur.lastError().text());
        return false;
    }
    Q_LLONG revision;
    while(bcur.next()) {
        revision = bcur.value(0).toLongLong();
        cur.bindValue(0,revision);
        if (!cur.exec()) {
            qDebug(cur.lastError().text().TOUTF8().data());
            throw svn::cache::DatabaseException(QString("Could not retrieve values: ")+cur.lastError().text()
                    ,cur.lastError().number());
            return false;
        }
        target[revision].revision=revision;
        target[revision].author=bcur.value(1).toString();
        target[revision].date=bcur.value(2).toLongLong();
        target[revision].message=bcur.value(3).toString();
        while(cur.next()) {
            LogChangePathEntry lcp;
            QString ac = cur.value(1).toString();
#if QT_VERSION < 0x040000
            lcp.action=ac[0].latin1();
#else
            lcp.action=ac[0].toLatin1();
#endif
            lcp.copyFromPath=cur.value(2).toString();
            lcp.path= cur.value(0).toString();
            lcp.copyFromRevision=cur.value(3).toLongLong();
            target[revision].changedPaths.push_back(lcp);
        }
        if (cp && cp->getListener()) {
            //cp->getListener()->contextProgress(++icount,bcur.size());
            if (cp->getListener()->contextCancel()) {
                throw svn::cache::DatabaseException(QString("Could not retrieve values: User cancel."));
            }
        }
    }
    return false;
}


/*!
    \fn svn::cache::ReposLog::date2numberRev(const svn::Revision&)
 */
svn::Revision svn::cache::ReposLog::date2numberRev(const svn::Revision&aRev,bool noNetwork)
{
    if (aRev!=svn::Revision::DATE) {
        return aRev;
    }
#if QT_VERSION < 0x040000
    if (!m_Database) {
#else
    if (!m_Database.isValid()) {
#endif
        return svn::Revision::UNDEFINED;
    }
    static QString _q("select revision from logentries where date<? order by revision desc");
    QSqlQuery query("select revision,date from logentries order by revision desc limit 1",m_Database);

#if QT_VERSION < 0x040000
    if (query.lastError().type()!=QSqlError::None) {
#else
    if (query.lastError().type()!=QSqlError::NoError) {
#endif
        qDebug(query.lastError().text().TOUTF8().data());
    }
    bool must_remote=!noNetwork;
    if (query.next()) {
        if (query.value(1).toLongLong()>=aRev.date()) {
            must_remote=false;
        }
    }
    if (must_remote) {
        svn::InfoEntries e = (m_Client->info(m_ReposRoot,svn::DepthEmpty,aRev,aRev));;
        if (e.count()<1||e[0].reposRoot().isEmpty()) {
            return aRev;
        }
        return e[0].revision();
    }
    query.prepare(_q);
    query.bindValue(0,Q_LLONG(aRev.date()));
    query.exec();
#if QT_VERSION < 0x040000
    if (query.lastError().type()!=QSqlError::None) {
#else
    if (query.lastError().type()!=QSqlError::NoError) {
#endif
        qDebug(query.lastError().text().TOUTF8().data());
    }
    if (query.next()) {
        return query.value(0).toInt();
    }
    // not found...
    if (noNetwork) {
        return svn::Revision::UNDEFINED;
    }
    svn::InfoEntries e = (m_Client->info(m_ReposRoot,svn::DepthEmpty,svn::Revision::HEAD,svn::Revision::HEAD));;
    if (e.count()<1||e[0].reposRoot().isEmpty()) {
        return svn::Revision::UNDEFINED;
    }
    return e[0].revision();
}


/*!
    \fn svn::cache::ReposLog::insertLogEntry(const svn::LogEntry&)
 */
bool svn::cache::ReposLog::_insertLogEntry(const svn::LogEntry&aEntry)
{
    QSqlRecord *buffer;

#if QT_VERSION < 0x040000
    m_Database->transaction();
    Q_LLONG j = aEntry.revision;
#else
    m_Database.transaction();
    qlonglong j = aEntry.revision;
#endif
    static QString qEntry("insert into logentries (revision,date,author,message) values (?,?,?,?)");
    static QString qPathes("insert into changeditems (revision,changeditem,action,copyfrom,copyfromrev) values (?,?,?,?,?)");
    QSqlQuery _q(QString::null,m_Database);
    _q.prepare(qEntry);
    _q.bindValue(0,j);
    _q.bindValue(1,aEntry.date);
    _q.bindValue(2,aEntry.author);
    _q.bindValue(3,aEntry.message);
    if (!_q.exec()) {
#if QT_VERSION < 0x040000
        m_Database->rollback();
#else
        m_Database.rollback();
#endif
        qDebug("Could not insert values: %s",_q.lastError().text().TOUTF8().data());
        qDebug(_q.lastQuery().TOUTF8().data());
        throw svn::cache::DatabaseException(QString("Could not insert values: ")+_q.lastError().text(),_q.lastError().number());
    }
    _q.prepare(qPathes);
    svn::LogChangePathEntries::ConstIterator cpit = aEntry.changedPaths.begin();
    for (;cpit!=aEntry.changedPaths.end();++cpit){
        _q.bindValue(0,j);
        _q.bindValue(1,(*cpit).path);
        _q.bindValue(2,QString(QChar((*cpit).action)));
        _q.bindValue(3,(*cpit).copyFromPath);
        _q.bindValue(4,Q_LLONG((*cpit).copyFromRevision));
        if (!_q.exec()) {
#if QT_VERSION < 0x040000
            m_Database->rollback();
#else
            m_Database.rollback();
#endif
            qDebug("Could not insert values: %s",_q.lastError().text().TOUTF8().data());
            qDebug(_q.lastQuery().TOUTF8().data());
            throw svn::cache::DatabaseException(QString("Could not insert values: ")+_q.lastError().text(),_q.lastError().number());
        }
    }
#if QT_VERSION < 0x040000
        m_Database->commit();
#else
        m_Database.commit();
#endif
    return true;
}

bool svn::cache::ReposLog::insertLogEntry(const svn::LogEntry&aEntry)
{
    return _insertLogEntry(aEntry);
}


/*!
    \fn svn::cache::ReposLog::log(const svn::Path&,const svn::Revision&start, const svn::Revision&end,const svn::Revision&peg,svn::LogEntriesMap&target, bool strictNodeHistory,int limit))
 */
bool svn::cache::ReposLog::log(const svn::Path&what,const svn::Revision&_start, const svn::Revision&_end,const svn::Revision&_peg,svn::LogEntriesMap&target, bool strictNodeHistory,int limit)
{
    static QString s_q("select logentries.revision,logentries.author,logentries.date,logentries.message from logentries where logentries.revision in (select changeditems.revision from changeditems where (changeditems.changeditem='%1' or changeditems.changeditem GLOB '%2/*') %3 GROUP BY changeditems.revision) ORDER BY logentries.revision DESC");

    static QString s_e("select changeditem,action,copyfrom,copyfromrev from changeditems where changeditems.revision='%1'");

    svn::Revision peg = date2numberRev(_peg,true);
    svn::Revision end = date2numberRev(_end,true);
    svn::Revision start = date2numberRev(_start,true);
    QString query_string = QString(s_q).arg(what.native()).arg(what.native()).arg((peg==svn::Revision::UNDEFINED?"":QString(" AND revision<=%1").arg(peg.revnum())));
    if (peg==svn::Revision::UNDEFINED) {
        peg = latestCachedRev();
    }
    if (!itemExists(peg,what)) {
        throw svn::cache::DatabaseException(QString("Entry '%1' does not exists at revision %2").arg(what.native()).arg(peg.toString()));
    }
    if (limit>0) {
        query_string+=QString(" LIMIT %1").arg(limit);
    }
    qDebug("Query-string: %s",query_string.TOUTF8().data());
    QSqlQuery _q(QString::null,m_Database);
    QSqlQuery _q2(QString::null,m_Database);
    _q.prepare(query_string);
    if (!_q.exec()) {
        qDebug("Could not select values: %s",_q.lastError().text().TOUTF8().data());
        qDebug(_q.lastQuery().TOUTF8().data());
        throw svn::cache::DatabaseException(QString("Could not select values: ")+_q.lastError().text(),_q.lastError().number());
    }
    while(_q.next()) {
        Q_LLONG revision = _q.value(0).toLongLong();
        target[revision].revision=revision;
        target[revision].author=_q.value(1).toString();
        target[revision].date=_q.value(2).toLongLong();
        target[revision].message=_q.value(3).toString();
        query_string=s_e.arg(revision);
        qDebug("Query-string: %s",query_string.TOUTF8().data());
        _q2.prepare(query_string);
        if (!_q2.exec()) {
            qDebug("Could not select values: %s",_q2.lastError().text().TOUTF8().data());
        } else {
            while (_q2.next()) {
                target[revision].changedPaths.push_back (
                        LogChangePathEntry (_q2.value(0).toString(),
                                            _q2.value(1).toString()[0],
                                            _q2.value(2).toString(),
                                            _q2.value(3).toLongLong()
                                           )
                                                        );
            }
        }

    }
    return true;
}


/*!
    \fn svn::cache::ReposLog::itemExists(const svn::Revision&,const QString&)
 */
bool svn::cache::ReposLog::itemExists(const svn::Revision&peg,const svn::Path&path)
{
    /// @todo this moment I have no idea how to check real  with all moves and deletes of parent folders without a hell of sql statements so we make it quite simple: it exists if we found it.


#if 0
    static QString _s1("select revision from changeditems where changeditem='%1' and action='A' and revision<=%2 order by revision desc limit 1");
    QSqlQuery _q(QString::null,m_Database);
    QString query_string=QString(_s1).arg(path.native()).arg(peg.revnum());
    if (!_q.exec(query_string)) {
        qDebug("Could not select values: %s",_q.lastError().text().TOUTF8().data());
        qDebug(_q.lastQuery().TOUTF8().data());
        throw svn::cache::DatabaseException(QString("Could not select values: ")+_q.lastError().text(),_q.lastError().number());
    }
    qDebug(_q.lastQuery().TOUTF8().data());


    svn::Path _p = path;
    static QString _s2("select revision from changeditem where changeditem in (%1) and action='D' and revision>%2 and revision<=%3 order by revision desc limit 1");
    QStringList p_list;
    while (_p.length()>0) {
        p_list.append(QString("'%1'").arg(_p.native()));
        _p.removeLast();
    }
    query_string=QString(_s2).arg(p_list.join(",")).arg();
#endif
    return true;
}
