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

import QtQuick 2.7

import org.kde.plasma.plasmoid 2.0
import org.kde.plasma.core 2.0 as PlasmaCore

import "../applet/indicator" as AppletIndicator

Item{
    id: managerIndicator

    readonly property QtObject configuration: latteView && latteView.indicator ? latteView.indicator.configuration : null
    readonly property QtObject resources: latteView && latteView.indicator ? latteView.indicator.resources : null

    readonly property bool isEnabled: latteView && latteView.indicator ? (latteView.indicator.enabled && latteView.indicator.pluginIsReady) : false
    readonly property real padding: Math.max(info.minLengthPadding, info.lengthPadding)
    readonly property string type: latteView && latteView.indicator ? latteView.indicator.type : "org.kde.latte.default"  

    readonly property bool infoLoaded: metricsLoader.active && metricsLoader.item

    readonly property Component plasmaStyleComponent: latteView && latteView.indicator ? latteView.indicator.plasmaComponent : null
    readonly property Component indicatorComponent: latteView && latteView.indicator ? latteView.indicator.component : null

    readonly property Item info: Item{
        readonly property bool enabledForApplets: infoLoaded && metricsLoader.item.hasOwnProperty("enabledForApplets")
                                                  && metricsLoader.item.enabledForApplets

        readonly property bool needsIconColors: infoLoaded && metricsLoader.item.hasOwnProperty("needsIconColors")
                                                && metricsLoader.item.needsIconColors

        readonly property bool needsMouseEventCoordinates: infoLoaded && metricsLoader.item.hasOwnProperty("needsMouseEventCoordinates")
                                                           && metricsLoader.item.needsMouseEventCoordinates

        readonly property bool providesFrontLayer: infoLoaded &&  metricsLoader.item.hasOwnProperty("providesFrontLayer")
                                                   && metricsLoader.item.providesFrontLayer

        readonly property bool providesHoveredAnimation: infoLoaded && metricsLoader.item.hasOwnProperty("providesHoveredAnimation")
                                                         && metricsLoader.item.providesHoveredAnimation

        readonly property bool providesClickedAnimation: infoLoaded && metricsLoader.item.hasOwnProperty("providesClickedAnimation")
                                                         && metricsLoader.item.providesClickedAnimation

        readonly property int extraMaskThickness: {
            if (infoLoaded && metricsLoader.item.hasOwnProperty("extraMaskThickness")) {
                return metricsLoader.item.extraMaskThickness;
            }

            return 0;
        }

        readonly property real minThicknessPadding: {
            if (infoLoaded && metricsLoader.item.hasOwnProperty("minThicknessPadding")) {
                return metricsLoader.item.minThicknessPadding;
            }

            return 0;
        }

        readonly property real minLengthPadding: {
            if (infoLoaded && metricsLoader.item.hasOwnProperty("minLengthPadding")) {
                return metricsLoader.item.minLengthPadding;
            }

            return 0;
        }

        readonly property real lengthPadding: {
            if (infoLoaded && metricsLoader.item.hasOwnProperty("lengthPadding")) {
                return metricsLoader.item.lengthPadding;
            }

            return 0.08;
        }

        readonly property real appletLengthPadding: {
            if (infoLoaded && metricsLoader.item.hasOwnProperty("appletLengthPadding")) {
                return metricsLoader.item.appletLengthPadding;
            }

            return -1;
        }

        readonly property variant svgPaths: infoLoaded && metricsLoader.item.hasOwnProperty("svgImagePaths") ?
                                                metricsLoader.item.svgImagePaths : []

        onSvgPathsChanged: latteView.indicator.resources.setSvgImagePaths(svgPaths);
    }


    //! Metrics and values provided from an invisible indicator
    Loader{
        id: metricsLoader
        opacity: 0
        active: managerIndicator.isEnabled

        readonly property Item level: AppletIndicator.LevelOptions {
            isBackground: true
            bridge: AppletIndicator.Bridge{
                appletIsValid: false
            }
        }

        sourceComponent: managerIndicator.indicatorComponent
    }

    //! Bindings in order to inform View::Indicator
    Binding{
        target: latteView && latteView.indicator ? latteView.indicator : null
        property:"enabledForApplets"
        when: latteView && latteView.indicator
        value: managerIndicator.info.enabledForApplets
    }

    //! Bindings in order to inform View::Indicator::Info    
    Binding{
        target: latteView && latteView.indicator ? latteView.indicator.info : null
        property:"needsIconColors"
        when: latteView && latteView.indicator
        value: managerIndicator.info.needsIconColors
    }

    Binding{
        target: latteView && latteView.indicator ? latteView.indicator.info : null
        property:"needsMouseEventCoordinates"
        when: latteView && latteView.indicator
        value: managerIndicator.info.needsMouseEventCoordinates
    }

    Binding{
        target: latteView && latteView.indicator ? latteView.indicator.info : null
        property:"providesClickedAnimation"
        when: latteView && latteView.indicator
        value: managerIndicator.info.providesClickedAnimation
    }

    Binding{
        target: latteView && latteView.indicator ? latteView.indicator.info : null
        property:"providesHoveredAnimation"
        when: latteView && latteView.indicator
        value: managerIndicator.info.providesHoveredAnimation
    }

    Binding{
        target: latteView && latteView.indicator ? latteView.indicator.info : null
        property:"providesFrontLayer"
        when: latteView && latteView.indicator
        value: managerIndicator.info.providesFrontLayer
    }

    Binding{
        target: latteView && latteView.indicator ? latteView.indicator.info : null
        property:"extraMaskThickness"
        when: latteView && latteView.indicator
        value: managerIndicator.info.extraMaskThickness
    }

    Binding{
        target: latteView && latteView.indicator ? latteView.indicator.info : null
        property:"minLengthPadding"
        when: latteView && latteView.indicator
        value: managerIndicator.info.minLengthPadding
    }

    Binding{
        target: latteView && latteView.indicator ? latteView.indicator.info : null
        property:"minThicknessPadding"
        when: latteView && latteView.indicator
        value: managerIndicator.info.minThicknessPadding
    }
}

