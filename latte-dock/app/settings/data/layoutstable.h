/*
 * Copyright 2020  Michail Vourlakos <mvourlakos@gmail.com>
 *
 * This file is part of Latte-Dock
 *
 * Latte-Dock is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * Latte-Dock is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef SETTINGSDATALAYOUTSTABLE_H
#define SETTINGSDATALAYOUTSTABLE_H

// local
#include "layoutdata.h"
#include "../../layouts/synchronizer.h"

// Qt
#include <QList>

namespace Latte {
namespace Settings {
namespace Data {

class LayoutsTable
{

public:
    LayoutsTable();
    LayoutsTable(LayoutsTable &&o);
    LayoutsTable(const LayoutsTable &o);

    //! Operators
    LayoutsTable &operator=(const LayoutsTable &rhs);
    LayoutsTable &operator=(LayoutsTable &&rhs);
    LayoutsTable &operator<<(const Layout &rhs);
    bool operator==(const LayoutsTable &rhs) const;
    bool operator!=(const LayoutsTable &rhs) const;
    Layout &operator[](const QString &id);
    const Layout operator[](const QString &id) const;
    Layout &operator[](const uint &index);
    const Layout operator[](const uint &index) const;

    LayoutsTable subtracted(const LayoutsTable &rhs) const;

    QStringList allSharesIds() const;
    QStringList allSharesNames() const;
    Latte::Layouts::SharesMap sharesMap() const;

    bool containsId(const QString &id) const;
    bool containsName(const QString &name) const;
    bool rowExists(const int &row) const;

    int indexOf(const QString &id) const;
    int rowCount() const;

    QString idForName(const QString &name) const;

    void clear();
    void remove(const int &row);
    void removeLayout(const QString &id);
    void setLayoutForFreeActivities(const QString &id);

protected:
    //! #id, layout_record
    QList<Layout> m_layouts;

};

}
}
}

Q_DECLARE_METATYPE(Latte::Settings::Data::LayoutsTable)

#endif
