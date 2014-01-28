//  Natron
//
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 *Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 *contact: immarespond at gmail dot com
 *
 */
#include "OfxOverlayInteract.h"


#include "Engine/OfxImageEffectInstance.h"
#include "Engine/OfxEffectInstance.h"
#include "Engine/Format.h"

#include "Gui/CurveWidget.h"
#include "Gui/ViewerGL.h"

using namespace Natron;

OfxOverlayInteract::OfxOverlayInteract(OfxImageEffectInstance &v, int bitDepthPerComponent, bool hasAlpha,CurveWidget* curveWidget):
OFX::Host::ImageEffect::OverlayInteract(v,bitDepthPerComponent,hasAlpha)
, _curveWidget(curveWidget)
, _currentViewer(NULL)
{
}

void OfxOverlayInteract::setCallingViewer(ViewerGL* viewer) {
    _currentViewer = viewer;
}

OfxStatus OfxOverlayInteract::swapBuffers(){
    if(_curveWidget){
        _curveWidget->swapBuffers();
    }else{
        assert(_currentViewer);
        _currentViewer->swapBuffers();
    }
    return kOfxStatOK;
}

OfxStatus OfxOverlayInteract::redraw(){
    if(_curveWidget){
        _curveWidget->update();
    }else{
        assert(_currentViewer);
        _currentViewer->update();
    }
    return kOfxStatOK;
}

void OfxOverlayInteract::getViewportSize(double &width, double &height) const{
    if(_curveWidget){
        width = _curveWidget->width();
        height = _curveWidget->height();
    }else{
        assert(_currentViewer);
        const Format& f = _currentViewer->getDisplayWindow();
        width = f.width();
        height = f.height();
    }
}

void OfxOverlayInteract::getPixelScale(double& xScale, double& yScale) const{
    if(_curveWidget){
        double ap = _curveWidget->getPixelAspectRatio();
        xScale = 1. / ap;
        yScale = ap;
    }else{
        assert(_currentViewer);
        xScale = _currentViewer->getDisplayWindow().getPixelAspect();
        yScale = 2. - xScale;
    }
}

void OfxOverlayInteract::getBackgroundColour(double &r, double &g, double &b) const{
    if(_curveWidget){
        _curveWidget->getBackgroundColor(&r, &g, &b);
    }else{
        assert(_currentViewer);
        _currentViewer->backgroundColor(r,g,b);
    }
}

#ifdef OFX_EXTENSIONS_NUKE
void OfxOverlayInteract::getOverlayColour(double &r, double &g, double &b) const{
    r = g = b = 1.;
}
#endif
