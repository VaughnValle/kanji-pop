/*
*  Copyright 2016  Smith AR <audoban@openmailbox.org>
*                  Michail Vourlakos <mvourlakos@gmail.com>
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

#ifndef LATTEPACKAGE_H
#define LATTEPACKAGE_H

// Qt
#include <QObject>

// KDE
#include <KPackage/PackageStructure>

namespace Latte {
class Package : public KPackage::PackageStructure
{
    Q_OBJECT

public:
    explicit Package(QObject *parent = 0, const QVariantList &args = QVariantList());

    ~Package() override;
    void initPackage(KPackage::Package *package) override;
    void pathChanged(KPackage::Package *package) override;
};

}
#endif // LATTEPACKAGE_H
