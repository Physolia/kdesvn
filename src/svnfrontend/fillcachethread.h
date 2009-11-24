/***************************************************************************
 *   Copyright (C) 2006-2009 by Rajko Albrecht                             *
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
#ifndef FILLCACHE_THREAD_H
#define FILLCACHE_THREAD_H

#include "src/svnqt/client.hpp"
#include "src/svnqt/revision.hpp"
#include "src/svnqt/status.hpp"
#include "frontendtypes.h"
#include "svnthread.h"

class QObject;

class FillCacheThread:public SvnThread
{
public:
    FillCacheThread(QObject*,const QString&aPath,bool startup);
    virtual ~FillCacheThread();
    virtual void run();

    const QString&reposRoot()const;
    const QString&Path()const;

protected:
    void fillInfo();

    QMutex mutex;
    QString m_what;
    QString m_path;
    bool m_startup;
};

#endif
