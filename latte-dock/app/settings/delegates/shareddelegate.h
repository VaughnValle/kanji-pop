/*
*  Copyright 2019 Michail Vourlakos <mvourlakos@gmail.com>
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

#ifndef SHAREDDELEGATE_H
#define SHAREDDELEGATE_H

// local
#include "../data/layoutstable.h"
#include "../controllers/layoutscontroller.h"

// Qt
#include <QStyledItemDelegate>

class QModelIndex;
class QWidget;

namespace Latte {
namespace Settings {
namespace Layout {
namespace Delegate {

class Shared : public QStyledItemDelegate
{
    Q_OBJECT
public:
    Shared(Controller::Layouts *parent);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    void paintSharedToIndicator(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void updateButtonText(QWidget *editor, const QModelIndex &index) const;

    QString joined(const Data::LayoutsTable &layouts, const QStringList &originalIds, bool formatText = true) const;

private:
    // we need it in order to send to the model the information when the SHARETO cell is edited
    Controller::Layouts *m_controller{nullptr};
};

}
}
}
}

#endif
