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
#include "commandline_part.h"
#include "kdesvn_part.h"
#include "settings.h"
#include "svnfrontend/svnactions.h"
#include "svnfrontend/dummydisplay.h"
#include "svncpp/targets.hpp"
#include "helpers/sub2qt.h"

#include <kglobal.h>
#include <kstandarddirs.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <kcmdlineargs.h>

#include <qfile.h>
#include <qtextstream.h>
#include <qvaluelist.h>

class pCPart
{
public:
    pCPart();
    ~pCPart();

    QString cmd;
    QStringList url;
    bool ask_revision;
    bool rev_set;
    bool outfile_set;
    SvnActions*m_SvnWrapper;
    KCmdLineArgs *args;
    svn::Revision start,end;

    // for output
    QFile toStdout,toStderr;
    QString outfile;
    QTextStream Stdout,Stderr;
    DummyDisplay * disp;
    QMap<int,svn::Revision> extraRevisions;
};

pCPart::pCPart()
    :cmd(""),url(),ask_revision(false),rev_set(false),outfile_set(false)
{
    m_SvnWrapper = 0;
    start = svn::Revision::START;
    end = svn::Revision::HEAD;
    toStdout.open(IO_WriteOnly, stdout);
    toStderr.open(IO_WriteOnly, stderr);
    Stdout.setDevice(&toStdout);
    Stderr.setDevice(&toStderr);
    disp = new DummyDisplay();
    m_SvnWrapper = new SvnActions(disp);
}

pCPart::~pCPart()
{
    delete m_SvnWrapper;
    delete disp;
}

commandline_part::commandline_part(QObject *parent, const char *name,KCmdLineArgs *args)
 : QObject(parent, name)
{
    KGlobal::locale()->insertCatalogue("kdesvn");
    KInstance * inst = kdesvnPartFactory::instance();
    KGlobal::locale()->insertCatalogue(inst->instanceName());
    KGlobal::dirs()->addResourceType( inst->instanceName() + "data",
        KStandardDirs::kde_default("data")+ QString::fromLatin1( inst->instanceName() ) + '/' );

    m_pCPart = new pCPart;
    m_pCPart->args = args;

    connect(m_pCPart->m_SvnWrapper,SIGNAL(clientException(const QString&)),this,SLOT(clientException(const QString&)));
    connect(m_pCPart->m_SvnWrapper,SIGNAL(sendNotify(const QString&)),this,SLOT(slotNotifyMessage(const QString&)));
    m_pCPart->m_SvnWrapper->reInitClient();
}

commandline_part::~commandline_part()
{
    delete m_pCPart;
}

int commandline_part::exec()
{
    if (!m_pCPart->args) {
        return -1;
    }
    if (m_pCPart->args->count()>=2) {
        m_pCPart->cmd=m_pCPart->args->arg(1);
        m_pCPart->cmd=m_pCPart->cmd.lower();
    }
    QString slotCmd;
    if (!QString::compare(m_pCPart->cmd,"log")) {
        slotCmd=SLOT(slotCmd_log());
    } else if (!QString::compare(m_pCPart->cmd,"cat")) {
        slotCmd=SLOT(slotCmd_cat());
    } else if (!QString::compare(m_pCPart->cmd,"get")) {
        slotCmd=SLOT(slotCmd_get());
    } else if (!QString::compare(m_pCPart->cmd,"help")) {
        slotCmd=SLOT(slotCmd_help());
    } else if (!QString::compare(m_pCPart->cmd,"blame")||
               !QString::compare(m_pCPart->cmd,"annotate")) {
        slotCmd=SLOT(slotCmd_blame());
    } else if (!QString::compare(m_pCPart->cmd,"update")) {
        slotCmd=SLOT(slotCmd_update());
    } else if (!QString::compare(m_pCPart->cmd,"diff")) {
        slotCmd=SLOT(slotCmd_diff());
    } else if (!QString::compare(m_pCPart->cmd,"info")) {
        slotCmd=SLOT(slotCmd_info());
    } else if (!QString::compare(m_pCPart->cmd,"commit")||
               !QString::compare(m_pCPart->cmd,"ci")) {
        slotCmd=SLOT(slotCmd_commit());
    }

    bool found = connect(this,SIGNAL(executeMe()),this,slotCmd.ascii());
    if (!found) {
        slotCmd=i18n("Command \"%1\" not implemented or known").arg(m_pCPart->cmd);
        KMessageBox::sorry(0,slotCmd,i18n("SVN Error"));
        return -1;
    }

    QString tmp,query,proto;
    KURL tmpurl;
    for (int j = 2; j<m_pCPart->args->count();++j) {
        tmpurl = m_pCPart->args->url(j);
        query = tmpurl.query();
        if (tmpurl.protocol()=="https+svn") {
            tmpurl.setProtocol("https");
        } else if (tmpurl.protocol()=="http+svn") {
            tmpurl.setProtocol("http");
        }

        kdDebug()<<"Urlpath: " << tmpurl.path()<<endl;
        if (tmpurl.isLocalFile()) {
            if (m_pCPart->m_SvnWrapper->isLocalWorkingCopy("file://"+tmpurl.path())) {
                tmp = tmpurl.path();
            } else {
                tmp = "file://"+tmpurl.path();
            }
        } else {
            tmp = tmpurl.url();
        }
        while (tmp.endsWith("/")) {
            tmp.truncate(tmp.length()-1);
        }
        m_pCPart->url.append(tmp);
        svn::Revision re = helpers::sub2qt::urlToRev(m_pCPart->args->url(j));
        if (re != svn::Revision::UNDEFINED) {
            kdDebug()<<"Revision " << re << " gefunden. " << endl;
            m_pCPart->extraRevisions[j-2]=re;
        }
        kdDebug()<<"Uri zum testen: " << tmpurl<<endl;
    }
    if (m_pCPart->url.count()==0) {
        m_pCPart->url.append(".");
    }

    if (m_pCPart->args->isSet("R")) {
        m_pCPart->ask_revision = true;
    } else if (m_pCPart->args->isSet("r")){
        scanRevision();
    }
    if (m_pCPart->args->isSet("o")) {
        m_pCPart->outfile_set=true;
        m_pCPart->outfile = m_pCPart->args->getOption("o");
    }

    emit executeMe();
    return 0;
}



/*!
    \fn commandline_part::clientException(const QString&)
 */
void commandline_part::clientException(const QString&what)
{
    m_pCPart->Stderr<<what<<endl;
    KMessageBox::sorry(0,what,i18n("SVN Error"));
}


/*!
    \fn commandline_part::slotCmdLog
 */
void commandline_part::slotCmd_log()
{
    bool list = Settings::self()->log_always_list_changed_files();
    m_pCPart->m_SvnWrapper->makeLog(m_pCPart->start,m_pCPart->end,m_pCPart->url[0],list);
}

void commandline_part::slotCmd_blame()
{
    m_pCPart->m_SvnWrapper->makeBlame(m_pCPart->start,m_pCPart->end,m_pCPart->url[0]);
}

void commandline_part::slotCmd_cat()
{
    if (m_pCPart->extraRevisions.find(0)!=m_pCPart->extraRevisions.end()) {
        m_pCPart->rev_set=true;
        m_pCPart->start=m_pCPart->extraRevisions[0];
        kdDebug()<<"Using url revision" << endl;
    }
    m_pCPart->m_SvnWrapper->makeCat((m_pCPart->rev_set?m_pCPart->start:m_pCPart->end),m_pCPart->url[0],m_pCPart->url[0]);
}

void commandline_part::slotCmd_get()
{
    if (m_pCPart->extraRevisions.find(0)!=m_pCPart->extraRevisions.end()) {
        m_pCPart->rev_set=true;
        m_pCPart->start=m_pCPart->extraRevisions[0];
        kdDebug()<<"Using url revision" << endl;
    }
    if (!m_pCPart->outfile_set || m_pCPart->outfile.isEmpty()) {
        clientException(i18n("\"GET\" requires output file!"));
        return;
    }
    QFile of(m_pCPart->outfile);
    if (!of.open(IO_WriteOnly)) {
        clientException(i18n("Could not open %1 for writing").arg(m_pCPart->outfile));
        return;
    }
    QByteArray content = m_pCPart->m_SvnWrapper->makeGet((m_pCPart->rev_set?m_pCPart->start:m_pCPart->end),m_pCPart->url[0]);
    if (content.size()) {
        of.writeBlock(content,content.size());
    }
}

void commandline_part::slotCmd_update()
{
    m_pCPart->m_SvnWrapper->makeUpdate(m_pCPart->url,
        (m_pCPart->rev_set?m_pCPart->start:m_pCPart->end),true);
}

void commandline_part::slotCmd_diff()
{
    m_pCPart->m_SvnWrapper->makeDiff(m_pCPart->url[0],m_pCPart->start,m_pCPart->end);
}

void commandline_part::slotCmd_info()
{
    if (m_pCPart->extraRevisions.find(0)!=m_pCPart->extraRevisions.end()) {
        m_pCPart->rev_set=true;
        m_pCPart->start=m_pCPart->extraRevisions[0];
        kdDebug()<<"Using url revision" << endl;
    }
    m_pCPart->m_SvnWrapper->makeInfo(m_pCPart->url,(m_pCPart->rev_set?m_pCPart->start:m_pCPart->end),svn::Revision::UNDEFINED,false);
}

void commandline_part::slotCmd_commit()
{
    QValueList<svn::Path> targets;
    for (unsigned j=0; j<m_pCPart->url.count();++j) {
        targets.push_back(svn::Path(m_pCPart->url[j]));
    }
    m_pCPart->m_SvnWrapper->makeCommit(svn::Targets(targets));
}

/*!
    \fn commandline_part::scanRevision()
 */
bool commandline_part::scanRevision()
{
    /// @todo scan for date-ranges, too.
    QString revstring = m_pCPart->args->getOption("r");
    QStringList revl = QStringList::split(":",revstring);
    if (revl.count()==0) {
        return false;
    }
    m_pCPart->start=helpers::sub2qt::stringToRev(revl[0]);
    m_pCPart->rev_set=true;
    if (revl.count()>1) {
        m_pCPart->end=helpers::sub2qt::stringToRev(revl[1]);
    }
    return true;
}

void commandline_part::slotNotifyMessage(const QString&msg)
{
    m_pCPart->Stdout << msg << endl;
}

#include "commandline_part.moc"
