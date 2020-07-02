/*
*  Copyright 2017-2018 Michail Vourlakos <mvourlakos@gmail.com>
*
*  This file is part of Latte-Dock
*
*  Latte-Dock is free software; you can redistribute it and/or
*  modify it under the terms of the GNU General Public License as
*  published by the Free Software Foundation; either version 2 of
*  the License, or (at your option) any later version.
*
*  Latte-Dock is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "checkboxdelegate.h"

// local
#include "../models/layoutsmodel.h"
#include "../tools/settingstools.h"

// Qt
#include <QApplication>
#include <QDebug>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QStandardItemModel>

namespace Latte {
namespace Settings {
namespace Layout {
namespace Delegate {

const QChar HeavyCheckMark{0x2714};
const QChar CheckMark{0x2713};

CheckBox::CheckBox(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void CheckBox::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem adjustedOption = option;
    //! Remove the focus dotted lines
    adjustedOption.state = (adjustedOption.state & ~QStyle::State_HasFocus);
    adjustedOption.displayAlignment = Qt::AlignHCenter | Qt::AlignVCenter;

    bool isSharedCapable = index.data(Model::Layouts::ISSHAREDROLE).toBool() && index.data(Model::Layouts::INMULTIPLELAYOUTSROLE).toBool();

    if (!isSharedCapable) {
        bool originalChecked{false};
        bool currentChecked = index.data(Qt::UserRole).toBool();

        if (index.column() == Model::Layouts::MENUCOLUMN) {
            originalChecked =  index.data(Model::Layouts::ORIGINALISSHOWNINMENUROLE).toBool();
        } else if (index.column() == Model::Layouts::BORDERSCOLUMN) {
            originalChecked =  index.data(Model::Layouts::ORIGINALHASBORDERSROLE).toBool();
        } else {
            originalChecked = currentChecked;
        }

        bool isChanged = (originalChecked != currentChecked);

        if (isChanged) {
            adjustedOption.font.setPointSize(adjustedOption.font.pointSize() + 2);
            adjustedOption.font.setBold(true);
        } else {
            // normal appearance
        }

        if (currentChecked) {
            adjustedOption.text = isChanged ? HeavyCheckMark : CheckMark;
        } else {
            adjustedOption.text = "";
        }


        QStyledItemDelegate::paint(painter, adjustedOption, index);
    } else {
        // Disabled
        bool isSelected{Latte::isSelected(option)};
        QPalette::ColorRole backColorRole = isSelected ? QPalette::Highlight : QPalette::Base;
        QPalette::ColorRole textColorRole = isSelected ? QPalette::HighlightedText : QPalette::Text;

        //! draw background
        //! HIDDENTEXTCOLUMN is just needed to draw empty background rectangles properly based on states
        QStyledItemDelegate::paint(painter, option, index.model()->index(index.row(), Model::Layouts::HIDDENTEXTCOLUMN));

        // text
        QPen pen(Qt::DotLine);
        pen.setWidth(2); pen.setColor(option.palette.brush(Latte::colorGroup(option), textColorRole).color());
        int y = option.rect.y()+option.rect.height()/2;

        bool inMenu = (index.column() == Model::Layouts::MENUCOLUMN);
        int space = inMenu ? option.rect.height() / 2 : 0;

        painter->setPen(pen);

        if (qApp->layoutDirection() == Qt::LeftToRight) {
            painter->drawLine(option.rect.x() + space, y,
                              option.rect.x() + option.rect.width(), y);

            if (inMenu) {
                int xm = option.rect.x() + space;
                int thick = option.rect.height() / 2;
                int ym = option.rect.y() + ((option.rect.height() - thick) / 2);

                pen.setStyle(Qt::SolidLine);
                painter->setPen(pen);
                painter->drawLine(xm, ym, xm, ym + thick);
            }

        } else {
            painter->drawLine(option.rect.x(), y,
                              option.rect.x()+option.rect.width() - space, y);

            if (inMenu) {
                int xm = option.rect.x() + option.rect.width() - space;
                int thick = option.rect.height() / 2;
                int ym = option.rect.y() + ((option.rect.height() - thick) / 2);

                pen.setStyle(Qt::SolidLine);
                painter->setPen(pen);
                painter->drawLine(xm, ym, xm, ym + thick);
            }
        }
    }
}

bool CheckBox::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                                   const QModelIndex &index)
{
    Q_ASSERT(event);
    Q_ASSERT(model);

    bool isSharedCapable = index.data(Model::Layouts::ISSHAREDROLE).toBool() && index.data(Model::Layouts::INMULTIPLELAYOUTSROLE).toBool();

    if (isSharedCapable) {
        return false;
    }

    if (event->type() == QEvent::MouseButtonDblClick) {
        if (!option.rect.contains(static_cast<QMouseEvent *>(event)->pos()))
            return false;
    } else if (event->type() == QEvent::KeyPress) {
        if (static_cast<QKeyEvent *>(event)->key() != Qt::Key_Space && static_cast<QKeyEvent *>(event)->key() != Qt::Key_Select)
            return false;
    } else {
        return false;
    }

    const bool currentState = index.data(Qt::UserRole).toBool();
    return model->setData(index, !currentState, Qt::UserRole);
}

}
}
}
}
