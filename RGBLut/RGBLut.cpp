/*
 OFX RGBLut plugin, a plugin that illustrates the use of the OFX Support library.

 Copyright (C) 2013 INRIA
 Author: Frederic Devernay <frederic.devernay@inria.fr>

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 Redistributions in binary form must reproduce the above copyright notice, this
 list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.

 Neither the name of the {organization} nor the names of its
 contributors may be used to endorse or promote products derived from
 this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 INRIA
 Domaine de Voluceau
 Rocquencourt - B.P. 105
 78153 Le Chesnay Cedex - France


 The skeleton for this source file is from:
 OFX Basic Example plugin, a plugin that illustrates the use of the OFX Support library.

 Copyright (C) 2004-2005 The Open Effects Association Ltd
 Author Bruno Nicoletti bruno@thefoundry.co.uk

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.
 * Neither the name The Open Effects Association Ltd, nor the names of its
 contributors may be used to endorse or promote products derived from this
 software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 The Open Effects Association Ltd
 1 Wardour St
 London W1D 6PA
 England

 */

#include "RGBLut.h"

#ifdef _WINDOWS
#include <windows.h>
#endif

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "ofxsProcessing.H"

#define kPluginName "RGBLutOFX"
#define kPluginGrouping "Color"
#define kPluginDescription "Apply a parametric lookup curve to each channel separately. The master curve is combined with the red, green and blue curves, but not with the alpha curve."
#define kPluginIdentifier "net.sf.openfx:RGBLutPlugin"
#define kPluginVersionMajor 1 // Incrementing this number means that you have broken backwards compatibility of the plug-in.
#define kPluginVersionMinor 0 // Increment this when you have fixed a bug or made it faster.

#define kLookupTableParamName "lookupTable"
#define kLookupTableParamLabel "Lookup Table"
#define kLookupTableParamHint "Colour lookup table. The master curve is combined with the red, green and blue curves, but not with the alpha curve."
#define kAddCtrlPtsParamName "addCtrlPts"
#define kAddCtrlPtsParamLabel "Add Control Points"
#define kResetCtrlPtsParamName "resetCtrlPts"
#define kResetCtrlPtsParamLabel "Reset"

#define kCurveMaster 0
#define kCurveRed 1
#define kCurveGreen 2
#define kCurveBlue 3
#define kCurveAlpha 4
#define kCurveNb 5

class RGBLutBase : public OFX::ImageProcessor {
protected:
    OFX::Image *_srcImg;

public:
    RGBLutBase(OFX::ImageEffect &instance): OFX::ImageProcessor(instance), _srcImg(0)
    {
    }
    void setSrcImg(OFX::Image *v) {_srcImg = v;}
};

static inline int
componentToCurve(int comp)
{
    switch (comp) {
        case 0:
            return kCurveRed;
        case 1:
            return kCurveGreen;
        case 2:
            return kCurveBlue;
        case 3:
            return kCurveAlpha;
        default:
            return 0;
    }
}

// template to do the RGBA processing for discrete types
template <class PIX, int nComponents, int maxValue>
class ImageRGBLutProcessor : public RGBLutBase
{
public:
    // ctor
    ImageRGBLutProcessor(OFX::ImageEffect &instance, const OFX::RenderArguments &args, OFX::ParametricParam  *lookupTable)
    : RGBLutBase(instance)
    {
        // build the LUT
        assert(lookupTable);
        for (int component = 0; component < nComponents; ++component) {
            int lutIndex = nComponents == 1 ? kCurveAlpha : componentToCurve(component); // special case for components == alpha only
            bool applyMaster = lutIndex != kCurveAlpha;
            for (PIX position = 0; position <= maxValue; ++position) {
                // position to evaluate the param at
                float parametricPos = float(position)/maxValue;

                // evaluate the parametric param
                double value = lookupTable->getValue(lutIndex, args.time, parametricPos);
                if (applyMaster) {
                    value += lookupTable->getValue(kCurveMaster, args.time, parametricPos) - parametricPos;
                }
                // set that in the lut
                _lookupTable[component][position] = std::max(PIX(0),std::min(PIX(value*maxValue+0.5), PIX(maxValue)));
            }
        }
    }

private:
    // and do some processing
    void multiThreadProcessImages(OfxRectI procWindow)
    {
        assert(_dstImg);
        for (int y = procWindow.y1; y < procWindow.y2; y++) {
            if (_effect.abort()) {
                break;
            }
            PIX *dstPix = (PIX *) _dstImg->getPixelAddress(procWindow.x1, y);
            for (int x = procWindow.x1; x < procWindow.x2; x++)  {
                PIX *srcPix = (PIX *)  (_srcImg ? _srcImg->getPixelAddress(x, y) : 0);
                if (srcPix) {
                    for (int c = 0; c < nComponents; c++) {
                        //assert(0 <= srcPix[c] && srcPix[c] <= maxValue);
                        dstPix[c] = _lookupTable[c][srcPix[c]];
                    }
                } else  {
                    // no src pixel here, be black and transparent
                    for (int c = 0; c < nComponents; c++) {
                        dstPix[c] = 0;
                    }
                }
                // increment the dst pixel
                dstPix += nComponents;
            }
        }
    }

private:
    PIX _lookupTable[nComponents][maxValue+1];
};

// template to do the RGBA processing for floating-point types
template <int nComponents, int maxValue>
class ImageRGBLutProcessorFloat : public RGBLutBase
{
public:
    // ctor
    ImageRGBLutProcessorFloat(OFX::ImageEffect &instance, const OFX::RenderArguments &args, OFX::ParametricParam  *lookupTable)
    : RGBLutBase(instance)
    {
        // build the LUT
        for (int component = 0; component < nComponents; ++component) {
            int lutIndex = nComponents == 1 ? kCurveAlpha : componentToCurve(component); // special case for components == alpha only
            bool applyMaster = lutIndex != kCurveAlpha;
            for (int position = 0; position <= maxValue; ++position) {
                // position to evaluate the param at
                double parametricPos = float(position)/maxValue;

                // evaluate the parametric param
                double value = lookupTable->getValue(lutIndex, args.time, parametricPos);
                if (applyMaster) {
                    value += lookupTable->getValue(kCurveMaster, args.time, parametricPos) - parametricPos;
                }
                //value = value * maxValue;
                //value = clamp(value, 0, maxValue);

                // set that in the lut
                _lookupTable[component][position] = std::max(PIX(0),std::min(PIX(value),PIX(maxValue)));
            }
        }
    }

private:
    // and do some processing
    void multiThreadProcessImages(OfxRectI procWindow)
    {
        assert(_dstImg);
        for (int y = procWindow.y1; y < procWindow.y2; y++) {
            if (_effect.abort()) {
                break;
            }
            PIX *dstPix = (PIX *) _dstImg->getPixelAddress(procWindow.x1, y);
            for (int x = procWindow.x1; x < procWindow.x2; x++) {
                PIX *srcPix = (PIX *)  (_srcImg ? _srcImg->getPixelAddress(x, y) : 0);
                if (srcPix) {
                    for (int c = 0; c < nComponents; c++) {
                        dstPix[c] = interpolate(c, srcPix[c]);
                    }
                } else {
                    // no src pixel here, be black and transparent
                    for (int c = 0; c < nComponents; c++) {
                        dstPix[c] = 0;
                    }
                }
                // increment the dst pixel
                dstPix += nComponents;
            }
        }
    }

    float interpolate(int component, float value) {
        if (value < 0.) {
            return _lookupTable[component][0];
        } else if (value >= 1.) {
            return _lookupTable[component][maxValue];
        } else {
            int i = (int)(value * maxValue);
            assert(i < maxValue);
            float alpha = value - (float)i / maxValue;
            return _lookupTable[component][i] * (1.-alpha) + _lookupTable[component][i+1] * alpha;
        }
    }

private:
    typedef float PIX;
    PIX _lookupTable[nComponents][maxValue+1];
};

using namespace OFX;

////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class RGBLutPlugin : public OFX::ImageEffect
{
public:
    RGBLutPlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
    , dstClip_(0)
    , srcClip_(0)
    {
        dstClip_ = fetchClip(kOfxImageEffectOutputClipName);
        assert(dstClip_ && dstClip_->getPixelComponents() == ePixelComponentAlpha || dstClip_->getPixelComponents() == ePixelComponentRGB || dstClip_->getPixelComponents() == ePixelComponentRGBA);
        srcClip_ = fetchClip(kOfxImageEffectSimpleSourceClipName);
        assert(srcClip_ && srcClip_->getPixelComponents() == ePixelComponentAlpha || srcClip_->getPixelComponents() == ePixelComponentRGB || srcClip_->getPixelComponents() == ePixelComponentRGBA);

        lookupTable_ = fetchParametricParam(kLookupTableParamName);
        assert(lookupTable_);
    }

private:
    virtual void render(const OFX::RenderArguments &args);
    void setupAndProcess(RGBLutBase &, const OFX::RenderArguments &args);
    void changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName)
    {
        if (paramName == kAddCtrlPtsParamName) {
            for (int component = 0; component < kCurveNb; ++component) {
                int n = lookupTable_->getNControlPoints(component, args.time);
                if (n <= 1) {
                    // clear all control points
                    lookupTable_->deleteControlPoint(component);
                    // add a control point at 0, value is 0
                    lookupTable_->addControlPoint(component, // curve to set
                                                 args.time,   // time, ignored in this case, as we are not adding a key
                                                 0.0,   // parametric position, zero
                                                 0.0,   // value to be, 0
                                                 false);   // don't add a key
                    // add a control point at 1, value is 1
                    lookupTable_->addControlPoint(component, args.time, 1.0, 1.0, false);
                } else {
                    std::pair<double, double> prev = lookupTable_->getNthControlPoint(component, args.time, 0);
                    for (int i = 1; i < n; ++i) {
                        // note that getNthControlPoint is buggy in Nuke 6, and always returns point 1 for nthCtl > 0
                        std::pair<double, double> next = lookupTable_->getNthControlPoint(component, args.time, i);
                        if (prev != next) { // don't create additional points if we encounter the Nuke bug
                            // create a new control point between two existing control points
                            double parametricPos = (prev.first + next.first)/2.;
                            double parametricVal = lookupTable_->getValue(component, args.time, parametricPos);
                            lookupTable_->addControlPoint(component, // curve to set
                                                         args.time,   // time, ignored in this case, as we are not adding a key
                                                         parametricPos,   // parametric position
                                                         parametricVal,   // value to be, 0
                                                         false);
                        }
                        prev = next;
                    }
                }
            }
#if 0
        } else if (paramName == kResetCtrlPtsParamName) {
            OFX::Message::MessageReplyEnum reply = sendMessage(OFX::Message::eMessageQuestion, "", "Delete all control points for all components?");
            // Nuke seems to always reply eMessageReplyOK, whatever the real answer was
            switch (reply) {
                case OFX::Message::eMessageReplyOK:
                    sendMessage(OFX::Message::eMessageMessage, "","OK");
                    break;
                case OFX::Message::eMessageReplyYes:
                    sendMessage(OFX::Message::eMessageMessage, "","Yes");
                    break;
                case OFX::Message::eMessageReplyNo:
                    sendMessage(OFX::Message::eMessageMessage, "","No");
                    break;
                case OFX::Message::eMessageReplyFailed:
                    sendMessage(OFX::Message::eMessageMessage, "","Failed");
                    break;
            }
            if (reply == OFX::Message::eMessageReplyYes) {
                for (int component = 0; component < kCurveNb; ++component) {
                    lookupTable_->deleteControlPoint(component);
                    // add a control point at 0, value is 0
                    lookupTable_->addControlPoint(component, // curve to set
                                                 args.time,   // time, ignored in this case, as we are not adding a key
                                                 0.0,   // parametric position, zero
                                                 0.0,   // value to be, 0
                                                 false);   // don't add a key
                    // add a control point at 1, value is 1
                    lookupTable->addControlPoint(component, args.time, 1.0, 1.0, false);
                }
            }
#endif
        }
    }

private:
    OFX::Clip *dstClip_;
    OFX::Clip *srcClip_;
    OFX::ParametricParam  *lookupTable_;

};


void RGBLutPlugin::setupAndProcess(RGBLutBase &processor, const OFX::RenderArguments &args)
{
    assert(dstClip_);
    std::auto_ptr<OFX::Image> dst(dstClip_->fetchImage(args.time));
    if (!dst.get()) {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    if (dst->getRenderScale().x != args.renderScale.x ||
        dst->getRenderScale().y != args.renderScale.y ||
        dst->getField() != args.fieldToRender) {
        setPersistentMessage(OFX::Message::eMessageError, "", "OFX Host gave image with wrong scale or field properties");
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    assert(srcClip_);
    std::auto_ptr<OFX::Image> src(srcClip_->fetchImage(args.time));

    if (src.get() && dst.get())
    {
        OFX::BitDepthEnum    srcBitDepth      = src->getPixelDepth();
        OFX::PixelComponentEnum srcComponents = src->getPixelComponents();
        OFX::BitDepthEnum dstBitDepth       = dst->getPixelDepth();
        OFX::PixelComponentEnum dstComponents  = dst->getPixelComponents();

        // see if they have the same depths and bytes and all
        if (srcBitDepth != dstBitDepth || srcComponents != dstComponents)
            OFX::throwSuiteStatusException(kOfxStatErrImageFormat);
    }

    processor.setDstImg(dst.get());
    processor.setSrcImg(src.get());
    processor.setRenderWindow(args.renderWindow);
    processor.process();
}


void RGBLutPlugin::render(const OFX::RenderArguments &args)
{
    OFX::BitDepthEnum       dstBitDepth    = dstClip_->getPixelDepth();
    OFX::PixelComponentEnum dstComponents  = dstClip_->getPixelComponents();

    if (dstComponents == OFX::ePixelComponentRGBA) {
        switch (dstBitDepth) {
            case OFX::eBitDepthUByte: {
                ImageRGBLutProcessor<unsigned char, 4, 255> fred(*this, args, lookupTable_);
                setupAndProcess(fred, args);
            }
                break;
            case OFX::eBitDepthUShort: {
                ImageRGBLutProcessor<unsigned short, 4, 65535> fred(*this, args, lookupTable_);
                setupAndProcess(fred, args);
            }
                break;
            case OFX::eBitDepthFloat: {
                ImageRGBLutProcessorFloat<4,1023> fred(*this, args, lookupTable_);
                setupAndProcess(fred, args);
            }
                break;
            default:
                OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
        }
    } else if (dstComponents == OFX::ePixelComponentRGB) {
        switch (dstBitDepth) {
            case OFX::eBitDepthUByte: {
                ImageRGBLutProcessor<unsigned char, 3, 255> fred(*this, args, lookupTable_);
                setupAndProcess(fred, args);
            }
                break;
            case OFX::eBitDepthUShort: {
                ImageRGBLutProcessor<unsigned short, 3, 65535> fred(*this, args, lookupTable_);
                setupAndProcess(fred, args);
            }
                break;
            case OFX::eBitDepthFloat: {
                ImageRGBLutProcessorFloat<3,1023> fred(*this, args, lookupTable_);
                setupAndProcess(fred, args);
            }
                break;
            default:
                OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
        }
    } else {
        assert(dstComponents == OFX::ePixelComponentAlpha);
        switch (dstBitDepth) {
            case OFX::eBitDepthUByte: {
                ImageRGBLutProcessor<unsigned char, 1, 255> fred(*this, args, lookupTable_);
                setupAndProcess(fred, args);
            }
                break;
            case OFX::eBitDepthUShort: {
                ImageRGBLutProcessor<unsigned short, 1, 65535> fred(*this, args, lookupTable_);
                setupAndProcess(fred, args);
            }
                break;
            case OFX::eBitDepthFloat: {
                ImageRGBLutProcessorFloat<1,1023> fred(*this, args, lookupTable_);
                setupAndProcess(fred, args);
            }
                break;
            default:
                OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
        }
    }
}


using namespace OFX;

mDeclarePluginFactory(RGBLutPluginFactory, {}, {});

void RGBLutPluginFactory::describe(OFX::ImageEffectDescriptor &desc)
{
    desc.setLabels(kPluginName, kPluginName, kPluginName);
    desc.setPluginGrouping(kPluginGrouping);
    desc.setPluginDescription(kPluginDescription);

    desc.addSupportedContext(eContextFilter);
    desc.addSupportedBitDepth(eBitDepthUByte);
    desc.addSupportedBitDepth(eBitDepthUShort);
    desc.addSupportedBitDepth(eBitDepthFloat);

    desc.setSingleInstance(false);
    desc.setHostFrameThreading(false);
    desc.setSupportsMultiResolution(true);
    desc.setSupportsTiles(true);
    desc.setTemporalClipAccess(false);
    desc.setRenderTwiceAlways(false);
    desc.setSupportsMultipleClipPARs(false);

    // returning an error here crashes Nuke
    //if (!OFX::getImageEffectHostDescription()->supportsParametricParameter) {
    //  throwHostMissingSuiteException(kOfxParametricParameterSuite);
    //}
}

void RGBLutPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context)
{
    if (!OFX::getImageEffectHostDescription()->supportsParametricParameter) {
        throwHostMissingSuiteException(kOfxParametricParameterSuite);
    }

    ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
    assert(srcClip);
    srcClip->addSupportedComponent(ePixelComponentRGB);
    srcClip->addSupportedComponent(ePixelComponentRGBA);
    srcClip->addSupportedComponent(ePixelComponentAlpha);
    srcClip->setTemporalClipAccess(false);
    srcClip->setSupportsTiles(true);
    srcClip->setIsMask(false);

    ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    assert(dstClip);
    dstClip->addSupportedComponent(ePixelComponentRGB);
    dstClip->addSupportedComponent(ePixelComponentRGBA);
    dstClip->addSupportedComponent(ePixelComponentAlpha);
    dstClip->setSupportsTiles(true);

    // make some pages and to things in
    PageParamDescriptor *page = desc.definePageParam("Controls");

    // define it
    OFX::ParametricParamDescriptor* lookupTable = desc.defineParametricParam(kLookupTableParamName);
    assert(lookupTable);
    lookupTable->setLabels(kLookupTableParamLabel, kLookupTableParamLabel, kLookupTableParamLabel);
    lookupTable->setHint(kLookupTableParamHint);

    // define it as three dimensional
    lookupTable->setDimension(kCurveNb);
    
    // label our dimensions are r/g/b
    lookupTable->setDimensionLabel("master", kCurveMaster);
    lookupTable->setDimensionLabel("red", kCurveRed);
    lookupTable->setDimensionLabel("green", kCurveGreen);
    lookupTable->setDimensionLabel("blue", kCurveBlue);
    lookupTable->setDimensionLabel("alpha", kCurveAlpha);

    // set the UI colour for each dimension
    const OfxRGBColourD master  = {0.9,0.9,0.9};
    // the following are magic colors, they all have the same luminance
    const OfxRGBColourD red   = {0.711519527404004, 0.164533420851110, 0.164533420851110};		//set red color to red curve
    const OfxRGBColourD green = {0., 0.546986106552894, 0.};		//set green color to green curve
    const OfxRGBColourD blue  = {0.288480472595996, 0.288480472595996, 0.835466579148890};		//set blue color to blue curve
    const OfxRGBColourD alpha  = {0.398979,0.398979,0.398979};
    lookupTable->setUIColour( kCurveRed, red );
    lookupTable->setUIColour( kCurveGreen, green );
    lookupTable->setUIColour( kCurveBlue, blue );
    lookupTable->setUIColour( kCurveAlpha, alpha );
    lookupTable->setUIColour( kCurveMaster, master );

    // set the min/max parametric range to 0..1
    lookupTable->setRange(0.0, 1.0);
    
    /*
     // set a default curve, this example sets identity
     for (int component = 0; component < 3; ++component) {
     // add a control point at 0, value is 0
     lookupTable->addControlPoint(component, // curve to set
     0.0,   // time, ignored in this case, as we are not adding a key
     0.0,   // parametric position, zero
     0.0,   // value to be, 0
     false);   // don't add a key
     // add a control point at 1, value is 1
     lookupTable->addControlPoint(component, 0.0, 1.0, 1.0, false);
     }
     */
    lookupTable->setIdentity();
    
    page->addChild(*lookupTable);
    
    OFX::PushButtonParamDescriptor* addCtrlPts = desc.definePushButtonParam(kAddCtrlPtsParamName);
    addCtrlPts->setLabels(kAddCtrlPtsParamLabel, kAddCtrlPtsParamLabel, kAddCtrlPtsParamLabel);
    
    page->addChild(*addCtrlPts);
    
#if 0
    OFX::PushButtonParamDescriptor* resetCtrlPts = desc.definePushButtonParam(kResetCtrlPtsParamName);
    resetCtrlPts->setLabels(kResetCtrlPtsParamLabel, kResetCtrlPtsParamLabel, kResetCtrlPtsParamLabel);
    
    page->addChild(*resetCtrlPts);
#endif
}

OFX::ImageEffect* RGBLutPluginFactory::createInstance(OfxImageEffectHandle handle, OFX::ContextEnum context)
{
    return new RGBLutPlugin(handle);
}

void getRGBLutPluginID(OFX::PluginFactoryArray &ids)
{
    static RGBLutPluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
    ids.push_back(&p);
}

