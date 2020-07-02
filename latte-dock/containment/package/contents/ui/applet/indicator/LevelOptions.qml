/*
*  Copyright 2019  Michail Vourlakos <mvourlakos@gmail.com>
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

import QtQuick 2.7

Item {
    id: level

    signal mousePressed(int x, int y, int button);
    signal mouseReleased(int x, int y, int button);

    property bool isBackground: true
    property bool isForeground: false

    readonly property Item requested: Item{
        property int iconOffsetX: 0
        property int iconOffsetY: 0
    }

    property Item bridge

    onIsBackgroundChanged: {
        isForeground = !isBackground;
    }

    onIsForegroundChanged: {
        isBackground = !isForeground;
    }
}
