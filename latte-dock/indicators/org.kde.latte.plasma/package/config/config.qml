/*
*  Copyright 2018  Michail Vourlakos <mvourlakos@gmail.com>
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
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.3

import org.kde.plasma.plasmoid 2.0
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 2.0 as PlasmaComponents

import org.kde.latte.components 1.0 as LatteComponents

ColumnLayout {
    id: root
    Layout.fillWidth: true

    LatteComponents.SubHeader {
        text: i18n("Padding")
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: units.smallSpacing

        PlasmaComponents.Label {
            text: i18n("Length")
            horizontalAlignment: Text.AlignLeft
        }

        LatteComponents.Slider {
            id: lengthIntMarginSlider
            Layout.fillWidth: true

            value: Math.round(indicator.configuration.lengthPadding * 100)
            from: 0
            to: maxMargin
            stepSize: 1
            wheelEnabled: false

            readonly property int maxMargin: 80

            onPressedChanged: {
                if (!pressed) {
                    indicator.configuration.lengthPadding = value / 100;
                }
            }
        }

        PlasmaComponents.Label {
            text: i18nc("number in percentage, e.g. 85 %","%0 %").arg(currentValue)
            horizontalAlignment: Text.AlignRight
            Layout.minimumWidth: theme.mSize(theme.defaultFont).width * 4
            Layout.maximumWidth: theme.mSize(theme.defaultFont).width * 4

            readonly property int currentValue: lengthIntMarginSlider.value
        }
    }

    LatteComponents.SubHeader {
        text: i18n("Options")
    }

    LatteComponents.CheckBoxesColumn {
        Layout.topMargin: 1.5 * units.smallSpacing

        LatteComponents.CheckBox {
            Layout.maximumWidth: dialog.optionsWidth
            text: i18n("Reverse indicator style")
            checked: indicator.configuration.reversed

            onClicked: {
                indicator.configuration.reversed = !indicator.configuration.reversed;
            }
        }

        LatteComponents.CheckBox {
            Layout.maximumWidth: dialog.optionsWidth
            text: i18n("Growing circle animation when clicked")
            checked: indicator.configuration.clickedAnimationEnabled

            onClicked: {
                indicator.configuration.clickedAnimationEnabled = !indicator.configuration.clickedAnimationEnabled;
            }
        }

        LatteComponents.CheckBox {
            Layout.maximumWidth: dialog.optionsWidth
            text: i18n("Show indicators for applets")
            checked: indicator.configuration.enabledForApplets
            tooltip: i18n("Indicators are shown for applets")

            onClicked: {
                indicator.configuration.enabledForApplets = !indicator.configuration.enabledForApplets;
            }
        }
    }
}
