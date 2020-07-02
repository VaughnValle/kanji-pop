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

import QtQuick 2.1

import org.kde.plasma.plasmoid 2.0
import org.kde.plasma.core 2.0 as PlasmaCore

import org.kde.latte.components 1.0 as LatteComponents


LatteComponents.ComboBoxButton{
    id: custom
    checkable: true

    buttonToolTip:  custom.type === "download:" ? i18n("Download indicator styles from the internet") :
                                                  i18n("Use %0 style for your indicators").arg(buttonText)

    comboBoxTextRole: "name"
    comboBoxIconRole: "icon"
    comboBoxIconToolTipRole: "iconToolTip"
    comboBoxIconOnlyWhenHoveredRole: "iconOnlyWhenHovered"
    comboBoxBlankSpaceForEmptyIcons: true
    comboBoxForcePressed: latteView.indicator.type === type
    comboBoxPopUpAlignRight: Qt.application.layoutDirection !== Qt.RightToLeft

    property string type: ""

    Component.onCompleted: {
        reloadModel();
        updateButtonInformation();
    }

    ListModel {
        id: actionsModel
    }

    Connections{
        target: latteView.indicator
        onCustomPluginsCountChanged: {
            custom.reloadModel();
            custom.updateButtonInformation()
        }
    }

    Connections{
        target: custom.button

        onClicked: {
            if (custom.type === "download:") {
                latteView.indicator.downloadIndicator();
            } else {
                latteView.indicator.type = custom.type;
            }
        }
    }

    Connections{
        target: custom.comboBox

        onActivated: {
            if (index>=0) {
                var item = actionsModel.get(index);
                if (item.pluginId === "add:") {
                    latteView.indicator.addIndicator();
                } else if (item.pluginId === "download:") {
                    latteView.indicator.downloadIndicator();
                } else {
                    latteView.indicator.type = item.pluginId;
                }
            }

            custom.updateButtonInformation();
        }

        onIconClicked: {
            if (index>=0) {
                var item = actionsModel.get(index);
                var pluginId = item.pluginId;
                if (latteView.indicator.customLocalPluginIds.indexOf(pluginId)>=0) {
                    latteView.indicator.removeIndicator(pluginId);
                    custom.comboBox.popup.close();
                }
            }
        }
    }

    Connections{
        target: custom.comboBox.popup
        onVisibleChanged: {
            if (visible) {
                custom.selectChosenType();
            }
        }
    }

    function updateButtonInformation() {
        if (latteView.indicator.customPluginsCount === 0) {
            custom.buttonText = i18n("Download");
            custom.type = "download:";
            custom.checkable = false;
        } else {
            custom.checkable = true;

            var curCustomIndex = latteView.indicator.customPluginIds.indexOf(latteView.indicator.customType);

            if (curCustomIndex>=0) {
                custom.buttonText = actionsModel.get(curCustomIndex).name;
                custom.type = actionsModel.get(curCustomIndex).pluginId;
            } else {
                custom.buttonText = actionsModel.get(0).name;
                custom.type = actionsModel.get(0).pluginId;
            }
        }
    }

    function reloadModel() {
        actionsModel.clear();

        if (latteView.indicator.customPluginsCount > 0) {
            var pluginIds = latteView.indicator.customPluginIds;
            var pluginNames = latteView.indicator.customPluginNames;
            var localPluginIds = latteView.indicator.customLocalPluginIds;

            for(var i=0; i<pluginIds.length; ++i) {
                var canBeRemoved = localPluginIds.indexOf(pluginIds[i])>=0;
                var iconString = canBeRemoved ? 'remove' : '';
                var iconTip = canBeRemoved ? i18n("Remove indicator") : '';
                var iconOnlyForHovered = canBeRemoved ? true : false;

                var element = {
                    pluginId: pluginIds[i],
                    name: pluginNames[i],
                    icon: iconString,
                    iconToolTip: iconTip,
                    iconOnlyWhenHovered: iconOnlyForHovered
                };

                actionsModel.append(element);
            }
        }

        appendDefaults();

        comboBox.model = actionsModel;

        if (custom.type === latteView.indicator.type) {
            selectChosenType();
        } else {
            comboBox.currentIndex = -1;
        }
    }

    function selectChosenType() {
        var found = false;

        for (var i=0; i<actionsModel.count; ++i) {
            if (actionsModel.get(i).pluginId === custom.type) {
                found = true;
                custom.comboBox.currentIndex = i;
                break;
            }
        }

        if (!found) {
            custom.comboBox.currentIndex = -1;
        }
    }

    function emptyModel() {
        actionsModel.clear();
        appendDefaults();

        comboBox.model = actionsModel;
        comboBox.currentIndex = -1;
    }

    function appendDefaults() {
        //! add
        var addElement = {
            pluginId: 'add:',
            name: i18n("Add Indicator..."),
            icon: 'document-import',
            iconToolTip: '',
            iconOnlyWhenHovered: false
        };
        actionsModel.append(addElement);

        //! download
        var downloadElement = {
            pluginId: 'download:',
            name: i18n("Get New Indicators..."),
            icon: 'get-hot-new-stuff',
            iconToolTip: '',
            iconOnlyWhenHovered: false
        };
        actionsModel.append(downloadElement);
    }

}
