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
#include "commandline.h"
#include "kdesvn_part.h"
#include "commandline_part.h"
#include <kcmdlineargs.h>
#include <kdialogbase.h>
#include <ktextbrowser.h>
#include <kapplication.h>
#include <klocale.h>
#include <qstring.h>
#include <qlayout.h>
#include <qvbox.h>

class CommandLineData
{
public:
    CommandLineData():cmd(""){};
    virtual ~CommandLineData(){};

    void displayHelp();

    QString cmd;
};

CommandLine::CommandLine(KCmdLineArgs*_args)
{
    m_args = _args;
    m_data = new CommandLineData;
}

CommandLine::~CommandLine()
{
}

int CommandLine::exec()
{
    if (!m_args||m_args->count()<1) {
        return -1;
    }
    if (m_args->count()<2) {
        m_data->cmd = "help";
    } else {
        m_data->cmd = m_args->arg(1);
    }
    if (m_data->cmd=="help") {
        m_data->displayHelp();
        return 0;
    }
    KLibFactory *factory = KLibLoader::self()->factory("libkdesvnpart");
    if (factory) {
        if (QCString(factory->className())!="cFactory") {
            kdDebug()<<"wrong factory"<<endl;
            return -1;
        }
        cFactory*cfa = static_cast<cFactory*>(factory);
        commandline_part * cpart = cfa->createCommandIf((QObject*)0,(const char*)0,m_args);
        int res = cpart->exec();
        return res;
    }
    return 0;
}

void CommandLineData::displayHelp()
{
    kapp->invokeHelp("kdesvn-commandline","kdesvn");
}
