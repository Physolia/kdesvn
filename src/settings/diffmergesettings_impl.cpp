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
#include "diffmergesettings_impl.h"
#include "src/settings/kdesvnsettings.h"

#include <qbuttongroup.h>
#include <qcheckbox.h>
#include <qradiobutton.h>
#include <klineedit.h>
#include <kdebug.h>

DiffMergeSettings_impl::DiffMergeSettings_impl(QWidget *parent, const char *name)
    :DiffMergeSettings(parent, name)
{
    kcfg_external_diff_display->setEnabled(Kdesvnsettings::use_kompare_for_diff()==2);
}

DiffMergeSettings_impl::~DiffMergeSettings_impl()
{
}

void DiffMergeSettings_impl::diffDispChanged()
{
    kcfg_external_diff_display->setEnabled(kcfg_use_kompare_for_diff->selectedId()==2);
}

#include "diffmergesettings_impl.moc"

