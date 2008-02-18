/***************************************************************************
 *   Copyright (C) 2005-2007 by Rajko Albrecht                             *
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

#include "svnitem.h"
#include "svnactions.h"
#include "kdesvn_part.h"
#include "src/settings/kdesvnsettings.h"
#include "src/svnqt/status.hpp"
#include "src/svnqt/smart_pointer.hpp"
#include "helpers/sub2qt.h"
#include "helpers/ktranslateurl.h"

#include <kglobal.h>
#include <klocale.h>
#include <kiconloader.h>
#include <kdebug.h>
#include <kiconeffect.h>
#include <kfileitem.h>
#include <kdebug.h>

#include <qstring.h>
#include <qfileinfo.h>
#include <qimage.h>
#include <qptrlist.h>
#include <qpainter.h>
#include <qbitmap.h>

class SvnItem_p:public svn::ref_count
{
    friend class SvnItem;
public:
    SvnItem_p();
    SvnItem_p(const svn::StatusPtr&);
    virtual ~SvnItem_p();
    KFileItem*createItem(const svn::Revision&peg);
    const KURL& kdeName(const svn::Revision&);
    KMimeType::Ptr mimeType(bool dir=false);

protected:
    svn::StatusPtr m_Stat;
    void init();
    QString m_url,m_full,m_short;
    KURL m_kdename;
    QDateTime m_fullDate;
    QString m_infoText;
    KFileItem*m_fitem;
    bool isWc;
    svn::Revision lRev;
    KMimeType::Ptr mptr;
};

SvnItem_p::SvnItem_p()
    :ref_count(),m_Stat(new svn::Status())
{
    init();
}

SvnItem_p::SvnItem_p(const svn::StatusPtr&aStat)
    :ref_count(),m_Stat(aStat)
{
    init();
}

SvnItem_p::~SvnItem_p()
{
    delete m_fitem;
}

void SvnItem_p::init()
{
    m_full = m_Stat->path();
    m_kdename="";
    mptr = 0;
    lRev=svn::Revision::UNDEFINED;
    while (m_full.endsWith("/")) {
        /* dir name possible */
        m_full.truncate(m_full.length()-1);
    }
    int p = m_full.findRev("/");
    if (p>-1) {
        ++p;
        m_short = m_full.right(m_full.length()-p);
    } else {
        m_short = m_full;
    }
    m_url = m_Stat->entry().url();
    m_fullDate = svn::DateTime(m_Stat->entry().cmtDate());
    m_infoText = QString::null;
    m_fitem = 0;
}

KMimeType::Ptr SvnItem_p::mimeType(bool dir)
{
    if (!mptr||m_kdename.isEmpty()) {
        if (m_kdename.isEmpty()) {
            kdeName(svn::Revision::UNDEFINED);
        }
        if (dir) {
            mptr = KMimeType::mimeType("inode/directory");
        } else {
            mptr = KMimeType::findByURL(m_kdename,0,isWc,!isWc);
        }
    }
    return mptr;
}

const KURL& SvnItem_p::kdeName(const svn::Revision&r)
{
    isWc = QString::compare(m_Stat->entry().url(),m_Stat->path())!=0;
    QString name;
    if (!(r==lRev)||m_kdename.isEmpty()) {
        lRev=r;
        if (!isWc) {
            m_kdename = m_Stat->entry().url();
            QString proto;
            proto = helpers::KTranslateUrl::makeKdeUrl(m_kdename.protocol());
            m_kdename.setProtocol(proto);
            QString revstr= lRev.toString();
            if (revstr.length()>0) {
                m_kdename.setQuery("?rev="+revstr);
            }
        } else {
            m_kdename = KURL::fromPathOrURL(m_Stat->path());
        }
    }
    return m_kdename;
}

KFileItem*SvnItem_p::createItem(const svn::Revision&peg)
{
    if (!m_fitem||!(peg==lRev) ) {
        delete m_fitem;
        m_fitem=0;
        m_fitem=new KFileItem(KFileItem::Unknown,KFileItem::Unknown,kdeName(peg));
    }
    return m_fitem;
}

SvnItem::SvnItem()
    : p_Item(new SvnItem_p())
{
    m_overlaycolor = false;
}

SvnItem::SvnItem(const svn::StatusPtr&aStat)
    : p_Item(new SvnItem_p(aStat))
{
    m_overlaycolor = false;
}

SvnItem::~SvnItem()
{
}

void SvnItem::setStat(const svn::StatusPtr&aStat)
{
    m_overlaycolor = false;
    p_Item = new SvnItem_p(aStat);
}

const QString&SvnItem::fullName()const
{
    return (p_Item->m_full);
}

const QString&SvnItem::shortName()const
{
    return (p_Item->m_short);
}

const QString&SvnItem::Url()const
{
    return (p_Item->m_url);
}

bool SvnItem::isDir()const
{
    if (isRemoteAdded() || p_Item->m_Stat->entry().isValid()) {
        return p_Item->m_Stat->entry().kind()==svn_node_dir;
    }
    /* must be a local file */
    QFileInfo f(fullName());
    return f.isDir();
}

const QDateTime&SvnItem::fullDate()const
{
    return (p_Item->m_fullDate);
}

QPixmap SvnItem::internalTransform(const QPixmap&first,int size)
{
    QPixmap result(size,size);
    if (result.isNull()) {
        return result;
    }
    const QBitmap * b = first.mask();
    result.fill(Qt::white);
    if (b) {
        result.setMask(*b);
    } else {
        QBitmap m(size,size,true);
        m.fill(Qt::white);
        result.setMask(m);
    }
    QPainter pa;
    pa.begin(&result);
    int w = first.width()>size?size:first.width();
    int h = first.height()>size?size:first.height();
    pa.drawPixmap(0,0,first,0,0,w,h);
    pa.end();
    return result;
}

QPixmap SvnItem::getPixmap(const QPixmap&_p,int size,bool overlay)
{
    if (!isVersioned()) {
        m_bgColor = NOTVERSIONED;
    } else if (isRealVersioned()) {
        SvnActions*wrap = getWrapper();
        bool mod = false;
        QPixmap p2 = QPixmap();
        if (p_Item->m_Stat->textStatus()==svn_wc_status_conflicted) {
            m_bgColor = CONFLICT;
            if (overlay)
                p2 = kdesvnPartFactory::instance()->iconLoader()->loadIcon("kdesvnconflicted",KIcon::Desktop,size);
        } else if (p_Item->m_Stat->textStatus ()==svn_wc_status_missing) {
            m_bgColor = MISSING;
        } else if (isLocked()||wrap->checkReposLockCache(fullName())) {
            if (overlay) p2 = kdesvnPartFactory::instance()->iconLoader()->loadIcon("kdesvnlocked",KIcon::Desktop,size);
            m_bgColor = LOCKED;
        } else if (Kdesvnsettings::check_needslock() && !isRemoteAdded() && wrap->isLockNeeded(this,svn::Revision::UNDEFINED) ) {
            if (overlay) p2 = kdesvnPartFactory::instance()->iconLoader()->loadIcon("kdesvnneedlock",KIcon::Desktop,size);
            m_bgColor = NEEDLOCK;
        } else if (wrap->isUpdated(p_Item->m_Stat->path())) {
            if (overlay) p2 = kdesvnPartFactory::instance()->iconLoader()->loadIcon("kdesvnupdates",KIcon::Desktop,size);
            m_bgColor = UPDATES;
        } else if (p_Item->m_Stat->textStatus()==svn_wc_status_deleted) {
            if (overlay) p2 = kdesvnPartFactory::instance()->iconLoader()->loadIcon("kdesvndeleted",KIcon::Desktop,size);
            m_bgColor = DELETED;
        } else if (p_Item->m_Stat->textStatus()==svn_wc_status_added ) {
            if (overlay) p2 = kdesvnPartFactory::instance()->iconLoader()->loadIcon("kdesvnadded",KIcon::Desktop,size);
            m_bgColor = ADDED;
        } else if (isModified()) {
            mod = true;
        } else if (isDir()&&wrap) {
            svn::StatusEntries dlist;
            svn::StatusEntries::const_iterator it;
            if (isRemoteAdded() || wrap->checkUpdateCache(fullName())) {
                if (overlay) p2 = kdesvnPartFactory::instance()->iconLoader()->loadIcon("kdesvnupdates",KIcon::Desktop,size);
                m_bgColor = UPDATES;
            } else if (wrap->checkConflictedCache(fullName())) {
                m_bgColor = CONFLICT;
                if (overlay)
                    p2 = kdesvnPartFactory::instance()->iconLoader()->loadIcon("kdesvnconflicted",KIcon::Desktop,size);
            } else {
                mod = wrap->checkModifiedCache(fullName());
            }
        }
        if (mod) {
            m_bgColor = MODIFIED;
            if (overlay) p2 = kdesvnPartFactory::instance()->iconLoader()->loadIcon("kdesvnmodified",KIcon::Desktop,size);
        }
        if (!p2.isNull()) {
            QPixmap p;
            if (_p.width()!=size || _p.height()!=size) {
                p = internalTransform(_p,size);
            } else {
                p = _p;
            }
            m_overlaycolor = true;
            QImage i1; i1 = p;
            QImage i2;i2 = p2;

            KIconEffect::overlay(i1,i2);
            p = i1;
            return p;
        }
    }
    return _p;
}

QPixmap SvnItem::getPixmap(int size,bool overlay)
{
    QPixmap p;
    m_overlaycolor = false;
    m_bgColor = NONE;
    /* yes - different way to "isDir" above 'cause here we try to use the
       mime-features of KDE on ALL not just unversioned entries.
     */
    if (QString::compare(p_Item->m_Stat->entry().url(),p_Item->m_Stat->path())==0) {
        /* remote access */
        p = p_Item->mimeType(isDir())->pixmap(KIcon::Desktop,size,KIcon::DefaultState);
        if (isLocked()) {
            m_bgColor = LOCKED;
            QPixmap p2;
            if (overlay) p2 = kdesvnPartFactory::instance()->iconLoader()->loadIcon("kdesvnlocked",KIcon::Desktop,size);
            if (!p2.isNull()) {
                QImage i1; i1 = p;
                QImage i2; i2 = p2;
                KIconEffect::overlay(i1,i2);
                p = i1;
            }
        }
    } else {
        if (isRemoteAdded()) {
            if (isDir()) {
                p = kdesvnPartFactory::instance()->iconLoader()->loadIcon("folder",KIcon::Desktop,size);
            } else {
                p = kdesvnPartFactory::instance()->iconLoader()->loadIcon("unknown",KIcon::Desktop,size);
            }
        } else {
            KURL uri;
            uri.setPath(fullName());
            p = KMimeType::pixmapForURL(uri,0,KIcon::Desktop,size);
            p = getPixmap(p,size,overlay);
        }
    }
    return p;
}

bool SvnItem::isVersioned()const
{
    return p_Item->m_Stat->isVersioned();
}

bool SvnItem::isValid()const
{
    if (isVersioned()) {
        return true;
    }
    QFileInfo f(fullName());
    return f.exists();
}

bool SvnItem::isRealVersioned()const
{
    return p_Item->m_Stat->isRealVersioned();
}

bool SvnItem::isIgnored()const
{
    return p_Item->m_Stat->textStatus()==svn_wc_status_ignored;
}

bool SvnItem::isRemoteAdded()const
{
    return getWrapper()->isUpdated(p_Item->m_Stat->path()) &&
            p_Item->m_Stat->validReposStatus()&&!p_Item->m_Stat->validLocalStatus();
}

QString SvnItem::infoText()const
{
    QString info_text = "";
    if (getWrapper()->isUpdated(p_Item->m_Stat->path())) {
        if (p_Item->m_Stat->validReposStatus()&&!p_Item->m_Stat->validLocalStatus()) {
            info_text = i18n("Added in repository");
        } else {
            info_text = i18n("Needs update");
        }
    } else {
    switch(p_Item->m_Stat->textStatus ()) {
    case svn_wc_status_modified:
        info_text = i18n("Locally modified");
        break;
    case svn_wc_status_added:
        info_text = i18n("Locally added");
        break;
    case svn_wc_status_missing:
        info_text = i18n("Missing");
        break;
    case svn_wc_status_deleted:
        info_text = i18n("Deleted");
        break;
    case svn_wc_status_replaced:
        info_text = i18n("Replaced");
        break;
    case svn_wc_status_ignored:
        info_text = i18n("Ignored");
        break;
    case svn_wc_status_external:
        info_text=i18n("External");
        break;
    case svn_wc_status_conflicted:
        info_text=i18n("Conflict");
        break;
    case svn_wc_status_merged:
        info_text=i18n("Merged");
        break;
    case svn_wc_status_incomplete:
        info_text=i18n("Incomplete");
        break;
    default:
        break;
    }
    if (info_text.isEmpty()) {
        switch (p_Item->m_Stat->propStatus ()) {
        case svn_wc_status_modified:
            info_text = i18n("Property modified");
            break;
        default:
            break;
        }
    }
    }
    return info_text;
}

QString SvnItem::cmtAuthor()const
{
    return p_Item->m_Stat->entry().cmtAuthor();
}

long int SvnItem::cmtRev()const
{
    return p_Item->m_Stat->entry().cmtRev();
}

bool SvnItem::isLocked()const
{
    return p_Item->m_Stat->entry().lockEntry().Locked();
}

QString SvnItem::lockOwner()const
{
    if (p_Item->m_Stat->entry().lockEntry().Locked()) {
        return p_Item->m_Stat->entry().lockEntry().Owner();
    }
    svn::SharedPointer<svn::Status> tmp;
    if (getWrapper()->checkReposLockCache(fullName(),tmp) && tmp) {
        return tmp->lockEntry().Owner();
    }
    return "";
}


/*!
    \fn SvnItem::isModified()
 */
bool SvnItem::isModified()const
{
    return p_Item->m_Stat->textStatus ()==svn_wc_status_modified||p_Item->m_Stat->propStatus()==svn_wc_status_modified
            ||p_Item->m_Stat->textStatus ()==svn_wc_status_replaced;
}

const svn::StatusPtr& SvnItem::stat()const
{
    return p_Item->m_Stat;
}


/*!
    \fn SvnItem::isNormal()const
 */
bool SvnItem::isNormal()const
{
    return p_Item->m_Stat->textStatus()==svn_wc_status_normal;
}

bool SvnItem::isMissing()const
{
    return p_Item->m_Stat->textStatus()==svn_wc_status_missing;
}

bool SvnItem::isDeleted()const
{
    return p_Item->m_Stat->textStatus()==svn_wc_status_deleted;
}

bool SvnItem::isConflicted()const
{
    return p_Item->m_Stat->textStatus()==svn_wc_status_conflicted;
}

/*!
    \fn SvnItem::getToolTipText()
 */
const QString& SvnItem::getToolTipText()
{
    if (p_Item->m_infoText.isNull()) {
        if (isRealVersioned() && !p_Item->m_Stat->entry().url().isEmpty()) {
            SvnActions*wrap = getWrapper();
            svn::Revision peg(svn_opt_revision_unspecified);
            svn::Revision rev(svn_opt_revision_unspecified);
            if (QString::compare(p_Item->m_Stat->entry().url(),p_Item->m_Stat->path())==0) {
                /* remote */
                rev = p_Item->m_Stat->entry().revision();
                peg = correctPeg();
            } else {
                /* local */
            }
            if (wrap) {
                QPtrList<SvnItem> lst; lst.append(this);
                p_Item->m_infoText = wrap->getInfo(lst,rev,peg,false,false);
                if (p_Item->m_fitem) p_Item->m_infoText+=p_Item->m_fitem->getToolTipText(0);
            }
        } else if (p_Item->m_fitem){
            p_Item->m_infoText=p_Item->m_fitem->getToolTipText(6);
        }
    }
    return p_Item->m_infoText;
}

KFileItem*SvnItem::fileItem()
{
    return p_Item->createItem(correctPeg());
}

const KURL&SvnItem::kdeName(const svn::Revision&r)
{
    return p_Item->kdeName(r);
}

KMimeType::Ptr SvnItem::mimeType()
{
    return p_Item->mimeType(isDir());
}
