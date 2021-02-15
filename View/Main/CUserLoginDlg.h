/*
 * Copyright (C) 2021 Ikomia SAS
 * Contact: https://www.ikomia.com
 *
 * This file is part of the IkomiaStudio software.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CUSERLOGINDLG_H
#define CUSERLOGINDLG_H

#include <QDialog>
#include "View/Common/CDialog.h"

class CUserLoginDlg : public CDialog
{
    Q_OBJECT

    public:

        CUserLoginDlg(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

        void    setCurrentUser(const QString& userName);

    signals:

        void    doConnectUser(const QString& login, const QString& pwd, bool bRememberMe);
        void    doDisconnectUser();
        void    doHide();

    private:

        void    initLayout();
        void    initConnections();
        void    hideEvent(QHideEvent* event) override;

    private:

        QLineEdit*      m_pEditLogin = nullptr;
        QLineEdit*      m_pEditPwd = nullptr;
        QCheckBox*      m_pCheckRememberMe = nullptr;
        QPushButton*    m_pBtnConnect = nullptr;
        QString         m_userName = "";
};

#endif // CUSERLOGINDLG_H
