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

import QtQuick 2.1
import QtQuick.Window 2.2

import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.plasmoid 2.0

import org.kde.latte.core 0.2 as LatteCore
import org.kde.latte.private.containment 0.1 as LatteContainment

Item{
    id: manager

    anchors.fill: parent

    property QtObject window

    property bool debugManager: Qt.application.arguments.indexOf("--mask") >= 0

    property bool blockUpdateMask: false
    property bool inForceHiding: false //is used when the docks are forced in hiding e.g. when changing layouts
    property bool normalState : false  // this is being set from updateMaskArea
    property bool previousNormalState : false // this is only for debugging purposes
    property bool panelIsBiggerFromIconSize: root.useThemePanel && (background.totals.visualThickness >= (metrics.iconSize + metrics.margin.thickness))

    property bool maskIsFloating: !root.behaveAsPlasmaPanel
                                  && !root.editMode
                                  && screenEdgeMarginEnabled
                                  && !plasmoid.configuration.fittsLawIsRequested
                                  && !inSlidingIn
                                  && !inSlidingOut

    property int maskFloatedGap: maskIsFloating ? Math.max(0, metrics.margin.screenEdge - background.shadows.headThickness) : 0

    property int animationSpeed: LatteCore.WindowSystem.compositingActive ?
                                     (editModeVisual.inEditMode ? editModeVisual.speed * 0.8 : animations.speedFactor.normal * 1.62 * animations.duration.large) : 0

    property bool inLocationAnimation: latteView && latteView.positioner && latteView.positioner.inLocationAnimation
    property bool inSlidingIn: false //necessary because of its init structure
    property alias inSlidingOut: slidingAnimationAutoHiddenOut.running
    property bool inTempHiding: false
    property bool inScreenEdgeInternalWindowSliding: root.behaveAsDockWithMask && hideThickScreenGap

    property int length: root.isVertical ?  Screen.height : Screen.width   //screenGeometry.height : screenGeometry.width

    property int slidingOutToPos: {
        if (root.behaveAsPlasmaPanel) {
            var edgeMargin = screenEdgeMarginEnabled ? plasmoid.configuration.screenEdgeMargin : 0

            root.isHorizontal ? root.height + edgeMargin - 1 : root.width + edgeMargin - 1;
        } else {
            var topOrLeftEdge = ((plasmoid.location===PlasmaCore.Types.LeftEdge)||(plasmoid.location===PlasmaCore.Types.TopEdge));
            return (topOrLeftEdge ? -thicknessNormal : thicknessNormal);
        }
    }

    property int finalScreenEdgeMargin: {
        //! is used for window geometry calculations
        if (!screenEdgeMarginEnabled
                || (hideThickScreenGap && metrics.margin.screenEdge === 0)) {
            /*window geometry is updated after the local screen margin animation was zeroed*/
            return 0;
        }

        return plasmoid.configuration.screenEdgeMargin;
    }

    property int thicknessAutoHidden: LatteCore.WindowSystem.compositingActive ?  2 : 1
    property int thicknessMid: finalScreenEdgeMargin + (1 + (0.65 * (parabolic.factor.maxZoom-1)))*(metrics.totals.thickness+extraZoomThickMask) //needed in some animations
    property int thicknessNormal: finalScreenEdgeMargin + Math.max(metrics.totals.thickness + extraNormalThickMask, background.thickness + background.shadows.headThickness)

    property int thicknessZoom: finalScreenEdgeMargin + ((metrics.totals.thickness+extraZoomThickMask) * parabolic.factor.maxZoom) + 2
    //it is used to keep thickness solid e.g. when iconSize changes from auto functions
    property int thicknessMidOriginal: finalScreenEdgeMargin + Math.max(thicknessNormalOriginal,extraNormalThickMask + (1 + (0.65 * (parabolic.factor.maxZoom-1)))*(metrics.maxIconSize+metrics.margin.maxThickness)) //needed in some animations
    property int thicknessNormalOriginal: finalScreenEdgeMargin + metrics.maxIconSize + (metrics.margin.maxThickness * 2) //this way we always have the same thickness published at all states
    /*property int thicknessNormalOriginal: !root.behaveAsPlasmaPanel || root.editMode ?
                                               thicknessNormalOriginalValue : background.thickness + background.shadows.headThickness*/

    property int thicknessNormalOriginalValue: finalScreenEdgeMargin + metrics.maxIconSize + (metrics.margin.maxThickness * 2) + extraNormalThickMask
    property int thicknessZoomOriginal: finalScreenEdgeMargin + Math.max( ((metrics.maxIconSize+(metrics.margin.maxThickness * 2)) * parabolic.factor.maxZoom) + extraZoomThickMask,
                                                                         background.thickness + background.shadows.headThickness,
                                                                         (LatteCore.WindowSystem.compositingActive ? thicknessEditMode + root.editShadow : thicknessEditMode))

    //! is used from Panel in edit mode in order to provide correct masking
    property int thicknessEditMode: thicknessNormalOriginalValue + editModeVisual.settingsThickness
    //! when Latte behaves as Plasma panel
    property int thicknessAsPanel: metrics.totals.thickness

    //! is used to increase the mask thickness
    readonly property int marginBetweenContentsAndRuler: 10
    property int extraNormalThickMask: Math.max(indicatorsExtraThickMask, shadowsExtraThickMask)
    property int extraZoomThickMask: marginBetweenContentsAndRuler + Math.max(indicatorsExtraThickMask, shadowsExtraThickMask)
    //! this is set from indicators when they need extra thickness mask size
    readonly property int indicatorsExtraThickMask: indicators.info.extraMaskThickness
    property int shadowsExtraThickMask: {
        if (LatteCore.WindowSystem.isPlatformWayland) {
            return 0;
        }

        //! 45% of max shadow size in px.
        var shadowMaxNeededMargin = 0.45 * root.appShadowSizeOriginal;
        var shadowOpacity = (plasmoid.configuration.shadowOpacity) / 100;
        //! +40% of shadow opacity in percentage
        shadowOpacity = shadowOpacity + shadowOpacity*0.4;

        //! This way we are trying to calculate how many pixels are needed in order for the shadow
        //! to be drawn correctly without being cut of from View::mask() under X11
        shadowMaxNeededMargin = (shadowMaxNeededMargin * shadowOpacity);

        //! give some more space when items shadows are enabled and extremely big
        if (root.enableShadows && metrics.margin.maxThickness < shadowMaxNeededMargin) {
            return shadowMaxNeededMargin - metrics.margin.maxThickness;
        }

        return 0;
    }

    property Item applets: null

    Binding{
        target: latteView
        property:"maxThickness"
        //! prevents updating window geometry during closing window in wayland and such fixes a crash
        when: latteView && !inTempHiding && !inForceHiding && !inScreenEdgeInternalWindowSliding
        value: root.behaveAsPlasmaPanel && !root.editMode ? thicknessAsPanel : thicknessZoomOriginal
    }

    property bool validIconSize: (metrics.iconSize===metrics.maxIconSize || metrics.iconSize === autosize.iconSize)
    property bool inPublishingState: validIconSize && !inSlidingIn && !inSlidingOut && !inTempHiding && !inForceHiding

    Binding{
        target: latteView
        property:"normalThickness"
        when: latteView && inPublishingState
        value: root.behaveAsPlasmaPanel && !root.editMode ? thicknessAsPanel : thicknessNormalOriginal
    }

    Binding{
        target: latteView
        property:"editThickness"
        when: latteView
        value: thicknessEditMode
    }

    Binding {
        target: latteView
        property: "headThicknessGap"
        when: latteView && !root.editMode && !editModeVisual.inEditMode && !inTempHiding && !inForceHiding && !inScreenEdgeInternalWindowSliding && inPublishingState
        value: {
            if (root.behaveAsPlasmaPanel || root.viewType === LatteCore.Types.PanelView || latteView.byPassWM) {
                return 0;
            }

            return thicknessZoomOriginal - thicknessNormalOriginalValue + extraNormalThickMask;
        }
    }

    Binding{
        target: latteView
        property: "type"
        when: latteView
        value: root.viewType
    }

    Binding{
        target: latteView
        property: "behaveAsPlasmaPanel"
        when: latteView
        value: root.editMode ? false : root.behaveAsPlasmaPanel
    }

    Binding{
        target: latteView
        property: "fontPixelSize"
        when: theme
        value: theme.defaultFont.pixelSize
    }

    Binding{
        target: latteView
        property:"inEditMode"
        when: latteView
        value: root.editMode
    }

    Binding{
        target: latteView
        property: "maxLength"
        when: latteView
        value: maxLengthPerCentage/100
    }

    Binding{
        target: latteView
        property: "offset"
        when: latteView
        value: plasmoid.configuration.offset/100
    }

    Binding{
        target: latteView
        property: "screenEdgeMargin"
        when: latteView
        value: Math.max(0, plasmoid.configuration.screenEdgeMargin)
    }

    Binding{
        target: latteView
        property: "screenEdgeMarginEnabled"
        when: latteView
        value: root.screenEdgeMarginEnabled && !root.hideThickScreenGap
    }

    Binding{
        target: latteView
        property: "alignment"
        when: latteView
        value: root.panelAlignment
    }

    Binding{
        target: latteView
        property: "isTouchingTopViewAndIsBusy"
        when: root.viewIsAvailable
        value: {
            if (!root.viewIsAvailable) {
                return false;
            }

            var isTouchingTopScreenEdge = (latteView.y === latteView.screenGeometry.y);
            var hasTopBorder = ((latteView.effects && (latteView.effects.enabledBorders & PlasmaCore.FrameSvg.TopBorder)) > 0);

            return root.isVertical && !latteView.visibility.isHidden && !isTouchingTopScreenEdge && !hasTopBorder && background.isShown;
        }
    }

    Binding{
        target: latteView
        property: "isTouchingBottomViewAndIsBusy"
        when: latteView
        value: {
            if (!root.viewIsAvailable) {
                return false;
            }

            var latteBottom = latteView.y + latteView.height;
            var screenBottom = latteView.screenGeometry.y + latteView.screenGeometry.height;
            var isTouchingBottomScreenEdge = (latteBottom === screenBottom);

            var hasBottomBorder = ((latteView.effects && (latteView.effects.enabledBorders & PlasmaCore.FrameSvg.BottomBorder)) > 0);

            return root.isVertical && !latteView.visibility.isHidden && !isTouchingBottomScreenEdge && !hasBottomBorder && background.isShown;
        }
    }

    //! View::Effects bindings
    Binding{
        target: latteView && latteView.effects ? latteView.effects : null
        property: "backgroundOpacity"
        when: latteView && latteView.effects
        value: background.currentOpacity
    }

    Binding{
        target: latteView && latteView.effects ? latteView.effects : null
        property: "drawEffects"
        when: latteView && latteView.effects
        value: LatteCore.WindowSystem.compositingActive
               && !root.inConfigureAppletsMode
               && (((root.blurEnabled && root.useThemePanel)
                    || (root.blurEnabled && root.forceSolidPanel && LatteCore.WindowSystem.compositingActive))
                   && (!root.inStartup || inForceHiding || inTempHiding))
    }

    Binding{
        target: latteView && latteView.effects ? latteView.effects : null
        property: "drawShadows"
        when: latteView && latteView.effects
        value: root.drawShadowsExternal && (!root.inStartup || inForceHiding || inTempHiding) && !(latteView && latteView.visibility.isHidden)
    }

    Binding{
        target: latteView && latteView.effects ? latteView.effects : null
        property:"editShadow"
        when: latteView && latteView.effects
        value: root.editShadow
    }

    Binding{
        target: latteView && latteView.effects ? latteView.effects : null
        property:"innerShadow"
        when: latteView && latteView.effects
        value: {
            if (editModeVisual.editAnimationEnded && !root.behaveAsPlasmaPanel) {
                return root.editShadow;
            } else {
                return background.shadows.headThickness;
            }
        }
    }

    //! View::Positioner bindings
    Binding{
        target: latteView && latteView.positioner ? latteView.positioner : null
        property: "isStickedOnTopEdge"
        when: latteView && latteView.positioner
        value: plasmoid.configuration.isStickedOnTopEdge
    }

    Binding{
        target: latteView && latteView.positioner ? latteView.positioner : null
        property: "isStickedOnBottomEdge"
        when: latteView && latteView.positioner
        value: plasmoid.configuration.isStickedOnBottomEdge
    }

    //! View::WindowsTracker bindings
    Binding{
        target: latteView && latteView.windowsTracker ? latteView.windowsTracker : null
        property: "enabled"
        when: latteView && latteView.windowsTracker && latteView.visibility
        value: (latteView && latteView.visibility
                && !(latteView.visibility.mode === LatteCore.Types.AlwaysVisible /* Visibility */
                     || latteView.visibility.mode === LatteCore.Types.WindowsGoBelow
                     || latteView.visibility.mode === LatteCore.Types.AutoHide))
               || applets.require.windowsTrackingCount > 0                   /*Applets Need Windows Tracking */
               || root.dragActiveWindowEnabled                               /*Dragging Active Window(Empty Areas)*/
               || ((root.backgroundOnlyOnMaximized                           /*Dynamic Background */
                    || plasmoid.configuration.solidBackgroundForMaximized
                    || root.disablePanelShadowMaximized
                    || root.windowColors !== LatteContainment.Types.NoneWindowColors))
               || (root.screenEdgeMarginsEnabled                             /*Dynamic Screen Edge Margin*/
                   && plasmoid.configuration.hideScreenGapForMaximized)
    }

    Connections{
        target:root      
        onEditModeChanged: {
            if (root.editMode) {
                visibilityManager.updateMaskArea();
            }
        }
    }

    Connections{
        target: background.totals
        onVisualLengthChanged: updateMaskArea();
        onVisualThicknessChanged: updateMaskArea();
    }

    Connections{
        target: background.shadows
        onHeadThicknessChanged: updateMaskArea();
    }

    Connections{
        target: latteView ? latteView : null
        onXChanged: updateMaskArea();
        onYChanged: updateMaskArea()
        onWidthChanged: updateMaskArea();
        onHeightChanged: updateMaskArea();
    }

    Connections{
        target: animations.needBothAxis
        onCountChanged: updateMaskArea();
    }

    Connections{
        target: animations.needLength
        onCountChanged: updateMaskArea();
    }

    Connections{
        target: animations.needThickness
        onCountChanged: updateMaskArea();
    }

    Connections{
        target: layoutsManager
        onCurrentLayoutIsSwitching: {
            if (LatteCore.WindowSystem.compositingActive && latteView && latteView.layout && latteView.layout.name === layoutName) {
                manager.inTempHiding = true;
                manager.inForceHiding = true;
                parabolic.sglClearZoom();
                manager.slotMustBeHide();
            }
        }
    }

    Connections{
        target: themeExtended ? themeExtended : null
        onRoundnessChanged: latteView.effects.forceMaskRedraw();
        onThemeChanged: latteView.effects.forceMaskRedraw();
    }

    onMaskIsFloatingChanged: updateMaskArea();

    onNormalStateChanged: {
        if (normalState) {
            autosize.updateIconSize();
            layouter.updateSizeForAppletsInFill();
        }
    }

    onThicknessZoomOriginalChanged: updateMaskArea();

    function slotContainsMouseChanged() {
        if(latteView.visibility.containsMouse && latteView.visibility.mode !== LatteCore.Types.SideBar) {
            updateMaskArea();

            if (slidingAnimationAutoHiddenOut.running && !inTempHiding && !inForceHiding) {
                slotMustBeShown();
            }
        }
    }

    function slotMustBeShown() {
        //! WindowsCanCover case
        if (latteView && latteView.visibility.mode === LatteCore.Types.WindowsCanCover) {
            latteView.visibility.setViewOnFrontLayer();
            return;
        }

        //! Normal Dodge/AutoHide case
        if (!slidingAnimationAutoHiddenIn.running && !inTempHiding && !inForceHiding){
            slidingAnimationAutoHiddenIn.init();
        }
    }

    function slotMustBeHide() {
        if (latteView && latteView.visibility.mode === LatteCore.Types.WindowsCanCover) {
            latteView.visibility.setViewOnBackLayer();
            return;
        }

        //! prevent sliding-in on startup if the dodge modes have sent a hide signal
        if (inStartupTimer.running && root.inStartup) {
            root.inStartup = false;
        }

        //! Normal Dodge/AutoHide case
        if((!slidingAnimationAutoHiddenOut.running
            && !latteView.visibility.blockHiding
            && (!latteView.visibility.containsMouse || latteView.visibility.mode === LatteCore.Types.SideBar))
                || inForceHiding) {
            slidingAnimationAutoHiddenOut.init();
        }
    }

    //! functions used for sliding out/in during location/screen changes
    function slotHideDockDuringLocationChange() {
        inTempHiding = true;
        blockUpdateMask = true;

        if(!slidingAnimationAutoHiddenOut.running) {
            slidingAnimationAutoHiddenOut.init();
        }
    }

    function slotShowDockAfterLocationChange() {
        slidingAnimationAutoHiddenIn.init();
    }

    function sendHideDockDuringLocationChangeFinished(){
        blockUpdateMask = false;
        latteView.positioner.hideDockDuringLocationChangeFinished();
    }

    function sendSlidingOutAnimationEnded() {
        latteView.visibility.hide();
        latteView.visibility.isHidden = true;

        if (visibilityManager.debugManager) {
            console.log("hiding animation ended...");
        }

        sendHideDockDuringLocationChangeFinished();
    }

    ///test maskArea
    function updateMaskArea() {
        if (!latteView || !root.viewIsAvailable || blockUpdateMask) {
            return;
        }

        var localX = 0;
        var localY = 0;

        normalState = ((animations.needBothAxis.count === 0) && (animations.needLength.count === 0))
                || (latteView && latteView.visibility.isHidden && !latteView.visibility.containsMouse && animations.needThickness.count === 0);


        // debug maskArea criteria
        if (debugManager) {
            console.log(animations.needBothAxis.count + ", " + animations.needLength.count + ", " +
                        animations.needThickness.count + ", " + latteView.visibility.isHidden);

            if (previousNormalState !== normalState) {
                console.log("normal state changed to:" + normalState);
                previousNormalState = normalState;
            }
        }

        var tempLength = root.isHorizontal ? width : height;
        var tempThickness = root.isHorizontal ? height : width;

        var noCompositingEdit = !LatteCore.WindowSystem.compositingActive && root.editMode;

        if (LatteCore.WindowSystem.compositingActive || noCompositingEdit) {
            if (normalState) {
                //console.log("entered normal state...");
                //count panel length


                //used when !compositing and in editMode
                if (noCompositingEdit) {
                    tempLength = root.isHorizontal ? root.width : root.height;
                } else {
                    tempLength = background.totals.visualLength;
                }

                tempThickness = thicknessNormal;

                if (animations.needThickness.count > 0) {
                    tempThickness = LatteCore.WindowSystem.compositingActive ? thicknessZoom : thicknessNormal;
                }

                if (maskIsFloating) {
                    tempThickness = tempThickness - maskFloatedGap;
                }

                if (latteView.visibility.isHidden && !slidingAnimationAutoHiddenOut.running ) {
                    tempThickness = thicknessAutoHidden;
                }

                //configure x,y based on plasmoid position and root.panelAlignment(Alignment)
                if ((plasmoid.location === PlasmaCore.Types.BottomEdge) || (plasmoid.location === PlasmaCore.Types.TopEdge)) {
                    if (plasmoid.location === PlasmaCore.Types.BottomEdge) {
                        if (latteView.visibility.isHidden && latteView.visibility.supportsKWinEdges) {
                            localY = latteView.height + tempThickness;
                        } else if (maskIsFloating && !latteView.visibility.isHidden) {
                            localY = latteView.height - tempThickness - maskFloatedGap;
                        } else {
                            localY = latteView.height - tempThickness;
                        }
                    } else if (plasmoid.location === PlasmaCore.Types.TopEdge) {
                        if (latteView.visibility.isHidden && latteView.visibility.supportsKWinEdges) {
                            localY = -tempThickness;
                        } else if (maskIsFloating && !latteView.visibility.isHidden) {
                            localY = maskFloatedGap;
                        } else {
                            localY = 0;
                        }
                    }

                    if (noCompositingEdit) {
                        localX = 0;
                    } else if (plasmoid.configuration.alignment === LatteCore.Types.Justify) {
                        localX = (latteView.width/2) - tempLength/2 + background.offset;
                    } else if (root.panelAlignment === LatteCore.Types.Left) {
                        localX = background.offset;
                    } else if (root.panelAlignment === LatteCore.Types.Center) {
                        localX = (latteView.width/2) - tempLength/2 + background.offset;
                    } else if (root.panelAlignment === LatteCore.Types.Right) {
                        localX = latteView.width - tempLength - background.offset;
                    }
                } else if ((plasmoid.location === PlasmaCore.Types.LeftEdge) || (plasmoid.location === PlasmaCore.Types.RightEdge)){
                    if (plasmoid.location === PlasmaCore.Types.LeftEdge) {
                        if (latteView.visibility.isHidden && latteView.visibility.supportsKWinEdges) {
                            localX = -tempThickness;
                        } else if (maskIsFloating && !latteView.visibility.isHidden) {
                            localX = maskFloatedGap;
                        } else {
                            localX = 0;
                        }
                    } else if (plasmoid.location === PlasmaCore.Types.RightEdge) {
                        if (latteView.visibility.isHidden && latteView.visibility.supportsKWinEdges) {
                            localX = latteView.width + tempThickness;
                        } else if (maskIsFloating && !latteView.visibility.isHidden) {
                            localX = latteView.width - tempThickness - maskFloatedGap;
                        } else {
                            localX = latteView.width - tempThickness;
                        }
                    }

                    if (noCompositingEdit) {
                        localY = 0;
                    } else if (plasmoid.configuration.alignment === LatteCore.Types.Justify) {
                        localY = (latteView.height/2) - tempLength/2 + background.offset;
                    } else if (root.panelAlignment === LatteCore.Types.Top) {
                        localY = background.offset;
                    } else if (root.panelAlignment === LatteCore.Types.Center) {
                        localY = (latteView.height/2) - tempLength/2 + background.offset;
                    } else if (root.panelAlignment === LatteCore.Types.Bottom) {
                        localY = latteView.height - tempLength - background.offset;
                    }
                }

                if (latteView.visibility.isHidden && latteView && latteView.visibility.mode === LatteCore.Types.SideBar) {
                    //!hide completely
                    localX = -1;
                    localY = -1;
                    tempThickness = 1;
                    tempLength = 1;
                }
            } else {
                // !inNormalState

                if(root.isHorizontal)
                    tempLength = Screen.width; //screenGeometry.width;
                else
                    tempLength = Screen.height; //screenGeometry.height;

                //grow only on length and not thickness
                var onlyLengthAnimation = (animations.needLength.count>0 && animations.needBothAxis.count === 0);

                if(onlyLengthAnimation) {

                    //this is used to fix a bug with shadow showing when the animation of edit mode
                    //is triggered
                    tempThickness = editModeVisual.editAnimationEnded ? thicknessEditMode + root.editShadow : thicknessEditMode

                    if (latteView.visibility.isHidden && !slidingAnimationAutoHiddenOut.running ) {
                        tempThickness = thicknessAutoHidden;
                    } else if (animations.needThickness.count > 0) {
                        tempThickness = thicknessZoomOriginal;
                    }
                } else{
                    //use all thickness space
                    if (latteView.visibility.isHidden && !slidingAnimationAutoHiddenOut.running ) {
                        tempThickness = LatteCore.WindowSystem.compositingActive ? thicknessAutoHidden : thicknessNormalOriginal;
                    } else {
                        tempThickness = !maskIsFloating ? thicknessZoomOriginal : thicknessZoomOriginal - maskFloatedGap;
                    }
                }

                //configure the x,y position based on thickness
                if(plasmoid.location === PlasmaCore.Types.RightEdge) {
                    localX = !maskIsFloating ? latteView.width - tempThickness : latteView.width - tempThickness - maskFloatedGap;

                    if (localX < 0) {
                        tempThickness = tempThickness + localX;
                        localX = 0;
                    }
                } else if (plasmoid.location === PlasmaCore.Types.BottomEdge) {
                    localY = !maskIsFloating ? latteView.height - tempThickness : latteView.height - tempThickness - maskFloatedGap;

                    if (localY < 0) {
                        tempThickness = tempThickness + localY;
                        localY = 0;
                    }
                } else if (plasmoid.location === PlasmaCore.Types.TopEdge) {
                    localY = !maskIsFloating ? 0 : maskFloatedGap;
                } else if (plasmoid.location === PlasmaCore.Types.LeftEdge) {
                    localX = !maskIsFloating ? 0 : maskFloatedGap;
                }
            }
        } // end of compositing calculations

        var maskArea = latteView.effects.mask;

        if (LatteCore.WindowSystem.compositingActive) {
            var maskLength = maskArea.width; //in Horizontal
            if (root.isVertical) {
                maskLength = maskArea.height;
            }

            var maskThickness = maskArea.height; //in Horizontal
            if (root.isVertical) {
                maskThickness = maskArea.width;
            }
        } else if (!noCompositingEdit){
            //! no compositing case
            var overridesHidden = latteView.visibility.isHidden && !latteView.visibility.supportsKWinEdges;

            if (!overridesHidden) {
                localX = latteView.effects.rect.x;
                localY = latteView.effects.rect.y;
            } else {
                if (plasmoid.location === PlasmaCore.Types.BottomEdge) {
                    localX = latteView.effects.rect.x;
                    localY = root.height - thicknessAutoHidden;
                } else if (plasmoid.location === PlasmaCore.Types.TopEdge) {
                    localX = latteView.effects.rect.x;
                    localY = 0;
                } else if (plasmoid.location === PlasmaCore.Types.LeftEdge) {
                    localX = 0;
                    localY = latteView.effects.rect.y;
                } else if (plasmoid.location === PlasmaCore.Types.RightEdge) {
                    localX = root.width - thicknessAutoHidden;
                    localY = latteView.effects.rect.y;
                }
            }

            if (root.isHorizontal) {
                tempThickness = overridesHidden ? thicknessAutoHidden : latteView.effects.rect.height;
                tempLength = latteView.effects.rect.width;
            } else {
                tempThickness = overridesHidden ? thicknessAutoHidden : latteView.effects.rect.width;
                tempLength = latteView.effects.rect.height;
            }
        }



        //  console.log("Not updating mask...");
        if( maskArea.x !== localX || maskArea.y !== localY
                || maskLength !== tempLength || maskThickness !== tempThickness) {

            // console.log("Updating mask...");
            var newMaskArea = Qt.rect(-1,-1,0,0);
            newMaskArea.x = localX;
            newMaskArea.y = localY;

            if (isHorizontal) {
                newMaskArea.width = tempLength;
                newMaskArea.height = tempThickness;
            } else {
                newMaskArea.width = tempThickness;
                newMaskArea.height = tempLength;
            }

            if (!LatteCore.WindowSystem.compositingActive) {
                latteView.effects.mask = newMaskArea;
            } else {
                if (latteView.behaveAsPlasmaPanel && !root.editMode) {
                    latteView.effects.mask = Qt.rect(0,0,root.width,root.height);
                } else {
                    latteView.effects.mask = newMaskArea;
                }
            }
        }

        var validIconSize = (metrics.iconSize===metrics.maxIconSize || metrics.iconSize === autosize.iconSize);

        //console.log("reached updating geometry ::: "+dock.maskArea);

        if(inPublishingState && !latteView.visibility.isHidden && (normalState || root.editMode)) {
            //! Important: Local Geometry must not be updated when view ISHIDDEN
            //! because it breaks Dodge(s) modes in such case

            var localGeometry = Qt.rect(0, 0, root.width, root.height);

            //the shadows size must be removed from the maskArea
            //before updating the localDockGeometry
            if (!latteView.behaveAsPlasmaPanel || root.editMode) {
                var cleanThickness = metrics.totals.thickness;
                var edgeMargin = finalScreenEdgeMargin;

                if (plasmoid.location === PlasmaCore.Types.TopEdge) {
                    localGeometry.x = latteView.effects.rect.x; // from effects area
                    localGeometry.width = latteView.effects.rect.width; // from effects area

                    localGeometry.y = edgeMargin;
                    localGeometry.height = cleanThickness ;
                } else if (plasmoid.location === PlasmaCore.Types.BottomEdge) {
                    localGeometry.x = latteView.effects.rect.x; // from effects area
                    localGeometry.width = latteView.effects.rect.width; // from effects area

                    localGeometry.y = root.height - cleanThickness - edgeMargin;
                    localGeometry.height = cleanThickness;
                } else if (plasmoid.location === PlasmaCore.Types.LeftEdge) {
                    localGeometry.y = latteView.effects.rect.y; // from effects area
                    localGeometry.height = latteView.effects.rect.height; // from effects area

                    localGeometry.x = edgeMargin;
                    localGeometry.width = cleanThickness;
                } else if (plasmoid.location === PlasmaCore.Types.RightEdge) {
                    localGeometry.y = latteView.effects.rect.y; // from effects area
                    localGeometry.height = latteView.effects.rect.height; // from effects area

                    localGeometry.x = root.width - cleanThickness - edgeMargin;
                    localGeometry.width = cleanThickness;
                }

                //set the boundaries for latteView local geometry
                //qBound = qMax(min, qMin(value, max)).

                localGeometry.x = Math.max(0, Math.min(localGeometry.x, latteView.width));
                localGeometry.y = Math.max(0, Math.min(localGeometry.y, latteView.height));
                localGeometry.width = Math.min(localGeometry.width, latteView.width);
                localGeometry.height = Math.min(localGeometry.height, latteView.height);
            }

            //console.log("update geometry ::: "+localGeometry);
            latteView.localGeometry = localGeometry;
        }
    }

    Loader{
        anchors.fill: parent
        active: root.debugMode

        sourceComponent: Item{
            anchors.fill:parent

            Rectangle{
                id: windowBackground
                anchors.fill: parent
                border.color: "red"
                border.width: 1
                color: "transparent"
            }

            Rectangle{
                x: latteView ? latteView.effects.mask.x : -1
                y: latteView ? latteView.effects.mask.y : -1
                height: latteView ? latteView.effects.mask.height : 0
                width: latteView ? latteView.effects.mask.width : 0

                border.color: "green"
                border.width: 1
                color: "transparent"
            }
        }
    }

    /***Hiding/Showing Animations*****/

    //////////////// Animations - Slide In - Out
    SequentialAnimation{
        id: slidingAnimationAutoHiddenOut

        ScriptAction{
            script: {
                root.isHalfShown = true;
            }
        }

        PropertyAnimation {
            target: !root.behaveAsPlasmaPanel ? layoutsContainer : latteView.positioner
            property: !root.behaveAsPlasmaPanel ? (root.isVertical ? "x" : "y") : "slideOffset"
            to: {
                if (root.behaveAsPlasmaPanel) {
                    return slidingOutToPos;
                }

                if (LatteCore.WindowSystem.compositingActive) {
                    return slidingOutToPos;
                } else {
                    if ((plasmoid.location===PlasmaCore.Types.LeftEdge)||(plasmoid.location===PlasmaCore.Types.TopEdge)) {
                        return slidingOutToPos + 1;
                    } else {
                        return slidingOutToPos - 1;
                    }
                }
            }
            duration: manager.animationSpeed
            easing.type: Easing.InQuad
        }

        ScriptAction{
            script: {
                latteView.visibility.isHidden = true;

                if (root.behaveAsPlasmaPanel && latteView.positioner.slideOffset !== 0) {
                    //! hide real panels when they slide-out
                    latteView.visibility.hide();
                }
            }
        }

        onStarted: {
            if (manager.debugManager) {
                console.log("hiding animation started...");
            }
        }

        onStopped: {
            //! Trying to move the ending part of the signals at the end of editing animation
            if (!manager.inTempHiding) {
                manager.updateMaskArea();
            } else {
                if (!editModeVisual.inEditMode) {
                    manager.sendSlidingOutAnimationEnded();
                }
            }

            latteView.visibility.slideOutFinished();
        }

        function init() {
            if (manager.inLocationAnimation || !latteView.visibility.blockHiding) {
                start();
            }
        }
    }

    SequentialAnimation{
        id: slidingAnimationAutoHiddenIn

        PauseAnimation{
            duration: manager.inTempHiding && animations.active ? 500 : 0
        }

        PropertyAnimation {
            target: !root.behaveAsPlasmaPanel ? layoutsContainer : latteView.positioner
            property: !root.behaveAsPlasmaPanel ? (root.isVertical ? "x" : "y") : "slideOffset"
            to: 0
            duration: manager.animationSpeed
            easing.type: Easing.OutQuad
        }

        ScriptAction{
            script: {
                root.isHalfShown = false;
                root.inStartup = false;
            }
        }

        onStarted: {
            latteView.visibility.show();

            if (manager.debugManager) {
                console.log("showing animation started...");
            }
        }

        onStopped: {
            inSlidingIn = false;

            if (manager.inTempHiding) {
                manager.inTempHiding = false;
                autosize.updateIconSize();
            }

            manager.inTempHiding = false;
            autosize.updateIconSize();

            if (manager.debugManager) {
                console.log("showing animation ended...");
            }

            latteView.visibility.slideInFinished();

            //! this is needed in order to update dock absolute geometry correctly in the end AND
            //! when a floating dock is sliding-in through masking techniques
            updateMaskArea();
        }

        function init() {
            if (!root.viewIsAvailable) {
                return;
            }

            inSlidingIn = true;

            if (slidingAnimationAutoHiddenOut.running) {
                slidingAnimationAutoHiddenOut.stop();
            }

            latteView.visibility.isHidden = false;
            updateMaskArea();

            start();
        }
    }

    //! Slides Animations for FLOATING+BEHAVEASPLASMAPANEL when
    //! HIDETHICKSCREENCAP dynamically is enabled/disabled
    SequentialAnimation{
        id: slidingInRealFloating

        PropertyAnimation {
            target: latteView ? latteView.positioner : null
            property: "slideOffset"
            to: 0
            duration: manager.animationSpeed
            easing.type: Easing.OutQuad
        }
    }

    SequentialAnimation{
        id: slidingOutRealFloating

        PropertyAnimation {
            target: latteView ? latteView.positioner : null
            property: "slideOffset"
            to: plasmoid.configuration.screenEdgeMargin
            duration: manager.animationSpeed
            easing.type: Easing.InQuad
        }
    }

    Connections {
        target: root
        onHideThickScreenGapChanged: {
            if (!latteView || !root.viewIsAvailable) {
                return;
            }

            if (root.behaveAsPlasmaPanel && !latteView.visibility.isHidden && !inSlidingIn && !inSlidingOut && !inStartup) {
                if (hideThickScreenGap) {
                    latteView.positioner.inSlideAnimation = true;
                    slidingInRealFloating.stop();
                    slidingOutRealFloating.start();
                } else {
                    slidingOutRealFloating.stop();
                    slidingInRealFloating.start();
                    latteView.positioner.inSlideAnimation = false;
                }
            }
        }
    }

}
