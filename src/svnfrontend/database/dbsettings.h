/***************************************************************************
 *   Copyright (C) 2005-2009 by Rajko Albrecht  ral@alwins-world.de        *
 *   http://kdesvn.alwins-world.de/                                        *
 *                                                                         *
 * This program is free software; you can redistribute it and/or           *
 * modify it under the terms of the GNU General Public              *
 * License as published by the Free Software Foundation; either            *
 * version 2.1 of the License, or (at your option) any later version.      *
 *                                                                         *
 * This program is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU       *
 * General Public License for more details.                         *
 *                                                                         *
 * You should have received a copy of the GNU General Public        *
 * License along with this program (in the file GPL.txt); if not,         *
 * write to the Free Software Foundation, Inc., 51 Franklin St,            *
 * Fifth Floor, Boston, MA  02110-1301  USA                                *
 *                                                                         *
 * This software consists of voluntary contributions made by many          *
 * individuals.  For exact contribution history, see the revision          *
 * history and logs, available at http://kdesvn.alwins-world.de.           *
 ***************************************************************************/
#pragma once

#include <QDialog>

namespace Ui
{
class DbSettings;
}
class KEditListBox;

class DbSettings: public QDialog
{
    Q_OBJECT
public:
    static void showSettings(const QString &repository, QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *e) override final;
protected Q_SLOTS:
    void accept() override final;
private:
    void init();
    explicit DbSettings(const QString &repository, QWidget *parent = nullptr);
    virtual ~DbSettings();

    void store_list(KEditListBox *, const QString &);
private:
    QString m_repository;
    Ui::DbSettings *m_ui;
};
