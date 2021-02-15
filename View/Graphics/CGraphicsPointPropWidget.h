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

#ifndef CGRAPHICSPOINTPROPERTYWIDGET_H
#define CGRAPHICSPOINTPROPERTYWIDGET_H

#include "Widgets/CColorPushButton.h"

struct GraphicsPointProperty;

class CGraphicsPointPropWidget: public QWidget
{
    public:

        CGraphicsPointPropWidget(QWidget *parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());

        void            setProperties(GraphicsPointProperty* pProp);

        void            propertyChanged();

    private:

        void            initLayout();
        void            initConnections();

        void            updateUI();

    private:

        CColorPushButton*       m_pPenColorBtn = nullptr;
        CColorPushButton*       m_pBrushColorBtn = nullptr;
        QSpinBox*               m_pSpinSize = nullptr;
        QSpinBox*               m_pSpinOpacity = nullptr;
        QLineEdit*              m_pEditCategory = nullptr;
        GraphicsPointProperty*  m_pProperty = nullptr;
};

#endif // CGRAPHICSPOINTPROPERTYWIDGET_H
