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
#include "displaysettings_impl.h"

#include <qbuttongroup.h>
#include <qcheckbox.h>
#include <klineedit.h>

DisplaySettings_impl::DisplaySettings_impl(QWidget *parent, const char *name)
    :DisplaySettings(parent, name)
{
    diffDispChanged();
    kcfg_display_previews_in_file_tips->setEnabled(kcfg_display_file_tips->isChecked());
}

DisplaySettings_impl::~DisplaySettings_impl()
{
}

void DisplaySettings_impl::diffDispChanged()
{
    kcfg_external_diff_display->setEnabled(kcfg_use_kompare_for_diff->selectedId()==2);
}

void DisplaySettings_impl::dispFileInfotoggled(bool how)
{
    kcfg_display_previews_in_file_tips->setEnabled(how);
}

#include "displaysettings_impl.moc"