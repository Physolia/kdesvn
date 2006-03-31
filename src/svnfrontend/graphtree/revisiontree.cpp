/***************************************************************************
 *   Copyright (C) 2005 by Rajko Albrecht                                  *
 *   ral@alwins-world.de                                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/
#include "revisiontree.h"
#include "../stopdlg.h"
#include "svnqt/log_entry.hpp"
#include "helpers/sub2qt.h"
#include "revgraphview.h"
#include "elogentry.h"

#include <kdebug.h>
#include <kprogress.h>
#include <klocale.h>
#include <kapp.h>
#include <klistview.h>

#include <qwidget.h>
#include <qdatetime.h>
#include <qlabel.h>
#include <qfile.h>
#include <qtextstream.h>
#include <qregexp.h>

#define INTERNALCOPY 1
#define INTERNALRENAME 2

class RtreeData
{
public:
    RtreeData();
    virtual ~RtreeData();

    QMap<long,eLog_Entry> m_History;

    svn::LogEntriesMap m_OldHistory;

    long max_rev,min_rev;
    KProgressDialog*progress;
    QTime m_stopTick;

    QWidget*dlgParent;
    RevGraphView*m_TreeDisplay;

    svn::Client*m_Client;
    CContextListener* m_Listener;

    bool getLogs(const QString&);
    unsigned long m_current_node;
};

RtreeData::RtreeData()
    : max_rev(-1),min_rev(-1)
{
    progress=0;
    m_TreeDisplay = 0;
    m_Client = 0;
    dlgParent = 0;
    m_Listener = 0;
    m_current_node = 0;
}

RtreeData::~RtreeData()
{
    delete progress;
}

bool RtreeData::getLogs(const QString&reposRoot)
{
    if (!m_Listener||!m_Client) {
        return false;
    }
    try {
        StopDlg sdlg(m_Listener,dlgParent,
            0,"Logs","Getting logs - hit cancel for abort");
        m_Client->log(reposRoot,svn::Revision::HEAD,svn::Revision((long)0),m_OldHistory,true,false,0);
    } catch (svn::ClientException ce) {
        kdDebug()<<ce.msg() << endl;
        return false;
    }
    return true;
}

RevisionTree::RevisionTree(svn::Client*aClient,
    CContextListener*aListener,
    const QString& reposRoot,
    const QString&origin,
    const svn::Revision& baserevision,
    QWidget*treeParent,QWidget*parent)
    :m_InitialRevsion(0),m_Path(origin),m_Valid(false)
{
    m_Data = new RtreeData;
    m_Data->m_Client=aClient;
    m_Data->m_Listener=aListener;
    m_Data->dlgParent=parent;

    if (!m_Data->getLogs(reposRoot)) {
        return;
    }

    long possible_rev=-1;
    kdDebug()<<"Origin: "<<origin << endl;

    m_Data->progress=new KProgressDialog(
        parent,"progressdlg",i18n("Scanning logs"),i18n("Scanning the logs for %1").arg(origin),true);
    m_Data->progress->setMinimumDuration(100);
    m_Data->progress->setAllowCancel(true);
    m_Data->progress->progressBar()->setTotalSteps(m_Data->m_OldHistory.size());
    m_Data->progress->setAutoClose(false);
    bool cancel=false;
    svn::LogEntriesMap::Iterator it;
    unsigned count = 0;
    for (it=m_Data->m_OldHistory.begin();it!=m_Data->m_OldHistory.end();++it) {
        m_Data->progress->progressBar()->setProgress(count);
        kapp->processEvents();
        if (m_Data->progress->wasCancelled()) {
            cancel=true;
            break;
        }
        if (it.key()>m_Data->max_rev) {
            m_Data->max_rev=it.key();
        }
        if (it.key()<m_Data->min_rev||m_Data->min_rev==-1) {
            m_Data->min_rev=it.key();
        }
        if (baserevision.kind()==svn_opt_revision_date) {
            if (baserevision.date()<=it.data().date && possible_rev==-1||possible_rev>it.key()) {
                possible_rev=it.key();
            }
        }
        ++count;
    }
    if (baserevision.kind()==svn_opt_revision_head||baserevision.kind()==svn_opt_revision_working) {
        m_Baserevision=m_Data->max_rev;
    } else if (baserevision.kind()==svn_opt_revision_number) {
        m_Baserevision=baserevision.revnum();
    } else if (baserevision.kind()==svn_opt_revision_date) {
        m_Baserevision=possible_rev;
    }
    if (!cancel) {
        kdDebug( )<<" max revision " << m_Data->max_rev
            << " min revision " << m_Data->min_rev << endl;
        if (topDownScan()) {
            m_Data->progress->setAutoReset(true);
            m_Data->progress->progressBar()->setTotalSteps(100);
            m_Data->progress->progressBar()->setPercentageVisible(false);
            m_Data->m_stopTick.restart();
            m_Data->m_TreeDisplay=new RevGraphView(m_Data->m_Listener,m_Data->m_Client,treeParent);
            m_Data->m_current_node=0;
            if (bottomUpScan(m_InitialRevsion,0,m_Path,0)) {
                m_Valid=true;
                m_Data->m_TreeDisplay->_basePath = reposRoot;
                m_Data->m_TreeDisplay->dumpRevtree();
            } else {
                delete m_Data->m_TreeDisplay;
                m_Data->m_TreeDisplay = 0;
            }
        }
    } else {
        kdDebug()<<"Canceld"<<endl;
    }
    m_Data->progress->hide();
}

RevisionTree::~RevisionTree()
{
    delete m_Data;
}

bool RevisionTree::isDeleted(long revision,const QString&path)
{
    for (unsigned i = 0;i<m_Data->m_History[revision].changedPaths.count();++i) {
        if (isParent(m_Data->m_History[revision].changedPaths[i].path,path) && m_Data->m_History[revision].changedPaths[i].action=='D') {
            return true;
        }
    }
    return false;
}

bool RevisionTree::topDownScan()
{
    m_Data->progress->progressBar()->setTotalSteps(m_Data->max_rev-m_Data->min_rev);
    bool cancel=false;
    for (long j=m_Data->max_rev;j>=m_Data->min_rev;--j) {
        m_Data->progress->progressBar()->setProgress(m_Data->max_rev-j);
        kapp->processEvents();
        if (m_Data->progress->wasCancelled()) {
            cancel=true;
            break;
        }
        for (unsigned i = 0; i<m_Data->m_OldHistory[j].changedPaths.count();++i) {
            /* find min revision of item */
            if (m_Data->m_OldHistory[j].changedPaths[i].action=='A'&&
                isParent(m_Data->m_OldHistory[j].changedPaths[i].path,m_Path))
            {
                if (!m_Data->m_OldHistory[j].changedPaths[i].copyFromPath.isEmpty()) {
                    if (m_InitialRevsion<m_Data->m_OldHistory[j].revision) {
                        QString tmpPath = m_Path;
                        QString r = m_Path.mid(m_Data->m_OldHistory[j].changedPaths[i].path.length());
                        m_Path=m_Data->m_OldHistory[j].changedPaths[i].copyFromPath;
                        m_Path+=r;
                    }
                } else if (m_Data->m_OldHistory[j].changedPaths[i].path==m_Path){
                    // here it is added
                    m_InitialRevsion = m_Data->m_OldHistory[j].revision;
                }
            } else {
            }
        }
    }
    if (cancel==true) {
        return false;
    }
    /* find forward references and filter them out */
    for (long j=m_Data->max_rev;j>=m_Data->min_rev;--j) {
        m_Data->progress->progressBar()->setProgress(m_Data->max_rev-j);
        kapp->processEvents();
        if (m_Data->progress->wasCancelled()) {
            cancel=true;
            break;
        }
        for (unsigned i = 0; i<m_Data->m_OldHistory[j].changedPaths.count();++i) {
            if (!m_Data->m_OldHistory[j].changedPaths[i].copyFromPath.isEmpty()) {
                long r = m_Data->m_OldHistory[j].changedPaths[i].copyFromRevision;
                QString sourcepath = m_Data->m_OldHistory[j].changedPaths[i].copyFromPath;
                char a = m_Data->m_OldHistory[j].changedPaths[i].action;
                if (m_Data->m_OldHistory[j].changedPaths[i].path.isEmpty()) {
                    kdDebug()<<"Empty entry! rev " << j << " source " << sourcepath << endl;
                    continue;
                }
                if (a=='R') {
#if 0
                    m_Data->m_History[j].addCopyTo(sourcepath,m_Data->m_OldHistory[j].changedPaths[i].path,j,a,r);
#endif
                    m_Data->m_OldHistory[j].changedPaths[i].action=0;
                } else if (a=='A'){
                    a=INTERNALCOPY;
                    for (unsigned z = 0;z<m_Data->m_OldHistory[j].changedPaths.count();++z) {
                        if (m_Data->m_OldHistory[j].changedPaths[z].action=='D'
                            &&m_Data->m_OldHistory[j].changedPaths[z].path==sourcepath) {
                            a=INTERNALRENAME;
                            m_Data->m_OldHistory[j].changedPaths[z].action=0;
                            break;
                        }
                    }
                    m_Data->m_History[j].addCopyTo(sourcepath,m_Data->m_OldHistory[j].changedPaths[i].path,j,a,r);
                    m_Data->m_OldHistory[j].changedPaths[i].action=0;
                } else {
                    kdDebug()<<"Action with source path but wrong action \""<<a<<"\" found!"<<endl;
                }
            }
        }
    }
    if (cancel==true) {
        return false;
    }
    for (long j=m_Data->max_rev;j>=m_Data->min_rev;--j) {
        m_Data->progress->progressBar()->setProgress(m_Data->max_rev-j);
        kapp->processEvents();
        if (m_Data->progress->wasCancelled()) {
            cancel=true;
            break;
        }
        for (unsigned i = 0; i<m_Data->m_OldHistory[j].changedPaths.count();++i) {
            if (m_Data->m_OldHistory[j].changedPaths[i].action==0) {
                continue;
            }
            m_Data->m_History[j].addCopyTo(m_Data->m_OldHistory[j].changedPaths[i].path,QString::null,-1,m_Data->m_OldHistory[j].changedPaths[i].action);
        }
        m_Data->m_History[j].author=m_Data->m_OldHistory[j].author;
        m_Data->m_History[j].date=m_Data->m_OldHistory[j].date;
        m_Data->m_History[j].revision=m_Data->m_OldHistory[j].revision;
        m_Data->m_History[j].message=m_Data->m_OldHistory[j].message;
    }
    return !cancel;
}

bool RevisionTree::isParent(const QString&_par,const QString&tar)
{
    if (_par==tar) return true;
    QString par = _par+(_par.endsWith("/")?"":"/");
    return tar.startsWith(par);
}

bool RevisionTree::isValid()const
{
    return m_Valid;
}

static QString uniqueNodeName(long rev,const QString&path)
{
    QString res = path;
    res.replace("\"","_quot_");
    res.replace(" ","_space_");
    QString n; n.sprintf("%05ld",rev);
    res = "\""+n+QString("_%1\"").arg(res);
    return res;
}

bool RevisionTree::bottomUpScan(long startrev,unsigned recurse,const QString&_path,long _last)
{
#define REVENTRY m_Data->m_History[j]
#define FORWARDENTRY m_Data->m_History[j].changedPaths[i]

    QString path = _path;
    long lastrev = _last;
#ifdef DEBUG_PARSE
    kdDebug()<<"Searching for "<<path<< " at revision " << startrev
        << " recursion " << recurse << endl;
#endif
    bool cancel = false;
    for (long j=startrev;j<=m_Data->max_rev;++j) {
        if (m_Data->m_stopTick.elapsed()>500) {
            m_Data->progress->progressBar()->advance(1);
            kapp->processEvents();
            m_Data->m_stopTick.restart();
        }
        if (m_Data->progress->wasCancelled()) {
            cancel=true;
            break;
        }
        for (unsigned i=0;i<REVENTRY.changedPaths.count();++i) {
            if (!isParent(FORWARDENTRY.path,path)) {
                continue;
            }
            QString n1,n2;
            if (isParent(FORWARDENTRY.path,path)) {
                bool get_out = false;
                if (FORWARDENTRY.path!=path) {
#ifdef DEBUG_PARSE
                    kdDebug()<<"Parent rename? "<< FORWARDENTRY.path << " -> " << FORWARDENTRY.copyToPath << " -> " << FORWARDENTRY.copyFromPath << endl;
#endif
                }
                if (FORWARDENTRY.action==INTERNALCOPY ||
                    /* FORWARDENTRY.action=='R' || */
                    FORWARDENTRY.action==INTERNALRENAME ) {
#if 0
                    if (FORWARDENTRY.action=='R' && FORWARDENTRY.path!=path ) {
                        continue;
                    }
#endif
                    bool ren = /* FORWARDENTRY.action=='R'|| */ FORWARDENTRY.action==INTERNALRENAME;
                    QString tmpPath = path;
                    QString recPath;
                    if (FORWARDENTRY.copyToPath.length()==0) {
                        continue;
                    }
                    QString r = path.mid(FORWARDENTRY.path.length());
                    recPath= FORWARDENTRY.copyToPath;
                    recPath+=r;
                    n1 = uniqueNodeName(lastrev,tmpPath);
                    n2 = uniqueNodeName(j,recPath);
                    m_Data->m_TreeDisplay->m_Tree[n1].targets.append(RevGraphView::targetData(n2,FORWARDENTRY.action));
                    fillItem(j,i,n2,recPath);
                    if (ren) {
                        lastrev = j;
                    }
                    if (ren) {
#ifdef DEBUG_PARSE
                        kdDebug()<<"Renamed to "<< recPath << " at revison " << j << endl;
#endif
                        path=recPath;
                    }
#ifdef DEBUG_PARSE
                     else {

                        kdDebug()<<"Copy to "<< recPath << endl;
                    }
#endif
                    if (!ren) {
                        if (!bottomUpScan(FORWARDENTRY.copyToRevision,recurse+1,recPath,j)) {
                            return false;
                        }
                    }
                } else if (FORWARDENTRY.path==path) {
                    switch (FORWARDENTRY.action) {
                    case 'A':
#ifdef DEBUG_PARSE
                        kdDebug()<<"Inserting adding base item"<<endl;
#endif
                        n1 = uniqueNodeName(j,FORWARDENTRY.path);
                        m_Data->m_TreeDisplay->m_Tree[n1].Action=FORWARDENTRY.action;
                        fillItem(j,i,n1,path);
                        lastrev=j;
                    break;
                    case 'M':
#ifdef DEBUG_PARSE
                        kdDebug()<<"Item modified at revision "<< j << " recurse " << recurse << endl;
#endif
                        n1 = uniqueNodeName(j,FORWARDENTRY.path);
                        n2 = uniqueNodeName(lastrev,FORWARDENTRY.path);
                        m_Data->m_TreeDisplay->m_Tree[n2].targets.append(RevGraphView::targetData(n1,FORWARDENTRY.action));
                        fillItem(j,i,n1,path);
                        lastrev = j;
                    break;
                    case 'D':
#ifdef DEBUG_PARSE
                        kdDebug()<<"(Sloppy match) Item deleted at revision "<< j << " recurse " << recurse << endl;
#endif
                        n1 = uniqueNodeName(j,path);
                        n2 = uniqueNodeName(lastrev,path);
                        if (n1==n2) {
                            /* cvs import - copy and deletion at same revision.
                             * CVS sucks.
                             */
                            n1 = uniqueNodeName(j,"D_"+path);
                        }
                        m_Data->m_TreeDisplay->m_Tree[n2].targets.append(RevGraphView::targetData(n1,FORWARDENTRY.action));
                        fillItem(j,i,n1,path);
                        lastrev = j;
                        get_out= true;
                    break;
                    default:
                    break;
                    }
                } else {
                    switch (FORWARDENTRY.action) {
                    case 'D':
#ifdef DEBUG_PARSE
                        kdDebug()<<"(Exact match) Item deleted at revision "<< j << " recurse " << recurse << endl;
#endif
                        n1 = uniqueNodeName(j,path);
                        n2 = uniqueNodeName(lastrev,path);
                        if (n1==n2) {
                            /* cvs import - copy and deletion at same revision.
                             * CVS sucks.
                             */
                            n1 = uniqueNodeName(j,"D_"+path);
                        }
                        m_Data->m_TreeDisplay->m_Tree[n2].targets.append(RevGraphView::targetData(n1,FORWARDENTRY.action));
                        fillItem(j,i,n1,path);
                        lastrev = j;
                        get_out = true;
                    break;
                    default:
                    break;
                    }
                }
                if (get_out) {
                    return true;
                }
            }
        }
    }
    return !cancel;
}

QWidget*RevisionTree::getView()
{
    return m_Data->m_TreeDisplay;
}

void RevisionTree::fillItem(long rev,int pathIndex,const QString&nodeName,const QString&path)
{
    m_Data->m_TreeDisplay->m_Tree[nodeName].Action=m_Data->m_History[rev].changedPaths[pathIndex].action;
    m_Data->m_TreeDisplay->m_Tree[nodeName].name=path;
    m_Data->m_TreeDisplay->m_Tree[nodeName].rev = rev;
    m_Data->m_TreeDisplay->m_Tree[nodeName].Author=m_Data->m_History[rev].author;
    m_Data->m_TreeDisplay->m_Tree[nodeName].Message=m_Data->m_History[rev].message;
    m_Data->m_TreeDisplay->m_Tree[nodeName].Date=helpers::sub2qt::apr_time2qtString(m_Data->m_History[rev].date);
}