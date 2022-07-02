// marker.cxx - 3D Marker pins in FlightGear
// Copyright (C) 2022 Tobias Dammers
// SPDX-License-Identifier: GPL-2.0-or-later OR MIT

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "Scenery/marker.hxx"
#include <simgear/scene/util/SGNodeMasks.hxx>
#include <simgear/scene/util/SGReaderWriterOptions.hxx>
#include <simgear/scene/material/Effect.hxx>
#include <simgear/scene/material/EffectGeode.hxx>
#include <osg/Geometry>
#include <osg/Material>
#include <osg/Node>
#include <osg/Billboard>
#include <osg/MatrixTransform>
#include <osgText/Text>
#include <osgText/String>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgDB/Registry>
#include "Model/modelmgr.hxx"

class FGMarkerCallback : public osg::NodeCallback
{
public:
    FGMarkerCallback(FGMarker* marker): _marker(marker) {}
    virtual void operator()(osg::Node* node, osg::NodeVisitor* nv) {
        assert(node == _marker->getMasterNode());
        const float distance = nv->getDistanceToEyePoint(osg::Vec3f{0,0,0}, false);
        _marker->setScaling(distance);
        traverse(node, nv);
    }
private:
    // Non-owning, hence plain pointer
    FGMarker* _marker;
};

FGMarker::FGMarker():
    FGMarker(osgText::String()) {}

FGMarker::FGMarker(const osgText::String& label, float _fontSize, float _pinHeight, float tipHeight):
    FGMarker(label, _fontSize, _pinHeight, tipHeight, osg::Vec4f{1, 1, 1, 1}) {}

FGMarker::FGMarker(const osgText::String& label, const osg::Vec4f& color):
    FGMarker(label, 32.0f, 500.0f, 0.0f, color) {}

FGMarker::FGMarker(const osgText::String& label, float _fontSize, float _pinHeight, const osg::Vec4f& color):
    FGMarker(label, _fontSize, _pinHeight, 0.0f, color) {}

FGMarker::FGMarker(const osgText::String& label, float _fontSize, const osg::Vec4f& color):
    FGMarker(label, _fontSize, 500.0f, 0.0f, color) {}

FGMarker::~FGMarker()
{
}

void FGMarker::setText(const osgText::String& label) { _labelText->setText(label); }

void FGMarker::setFontSize(float _fontSize)
{
    _labelText->setCharacterSize(_fontSize, 1.0f);
    _labelText->setFontResolution(std::max(32.0f, _fontSize), std::max(32.0f, _fontSize));
    _distanceText->setCharacterSize(_fontSize * 0.75f, 1.0f);
    _distanceText->setFontResolution(std::max(32.0f, _fontSize * 0.75f), std::max(32.0f, _fontSize * 0.75f));
}

void FGMarker::setScaling(float distance)
{
    osg::Matrix mtx;
    const float gamma = 0.9f;
    const float f = std::pow(std::max(distance, 1.0f) / 10000.0f, gamma);
    mtx.makeScale(f, f, f);
    _scaleTransform->setMatrix(mtx);
}

void FGMarker::setDistance(float distance)
{
    std::stringstream s;
    s << std::setprecision(1) << std::fixed << distance << "nm";
    _distanceText->setText(s.str());
}

FGMarker::FGMarker(const osgText::String& label, float fontSize, float pinHeight, float tipHeight, const osg::Vec4f& color)
{
    _masterNode = new osg::Group;

    _scaleTransform = new osg::MatrixTransform; 
    _masterNode->addChild(_scaleTransform);

    auto textNode = new osg::Billboard;
    _scaleTransform->addChild(textNode);

    textNode->setMode(osg::Billboard::AXIAL_ROT);

    _labelText = new osgText::Text;
    _labelText->setText(label);
    _labelText->setAlignment(osgText::Text::CENTER_BOTTOM);
    _labelText->setAxisAlignment(osgText::Text::XZ_PLANE);
    _labelText->setFont("Fonts/LiberationFonts/LiberationSans-Regular.ttf");
    // _labelText->setCharacterSizeMode(osgText::Text::OBJECT_COORDS_WITH_MAXIMUM_SCREEN_SIZE_CAPPED_BY_FONT_HEIGHT);
    _labelText->setColor(color);
    _labelText->setPosition(osg::Vec3(0, 0, pinHeight));
    _labelText->setBackdropType(osgText::Text::OUTLINE);
    _labelText->setBackdropColor(osg::Vec4(0, 0, 0, 0.75));
    _labelText->setBackdropOffset(0.04f);
    _labelText->setPosition(osg::Vec3(0, 0, pinHeight + fontSize));

    _distanceText = new osgText::Text;
    _distanceText->setText("--.- nm");
    _distanceText->setAlignment(osgText::Text::CENTER_BOTTOM);
    _distanceText->setAxisAlignment(osgText::Text::XZ_PLANE);
    _distanceText->setFont("Fonts/LiberationFonts/LiberationSans-Regular.ttf");
    // _distanceText->setCharacterSizeMode(osgText::Text::OBJECT_COORDS_WITH_MAXIMUM_SCREEN_SIZE_CAPPED_BY_FONT_HEIGHT);
    _distanceText->setColor(color);
    _distanceText->setPosition(osg::Vec3(0, 0, pinHeight));
    _distanceText->setBackdropType(osgText::Text::OUTLINE);
    _distanceText->setBackdropColor(osg::Vec4(0, 0, 0, 0.75));
    _distanceText->setBackdropOffset(0.06f);
    _distanceText->setPosition(osg::Vec3(0, 0, pinHeight));

    setFontSize(fontSize);

    textNode->addDrawable(_labelText);
    textNode->addDrawable(_distanceText);

    float top_spacing = fontSize * 0.25;

    if (pinHeight - top_spacing > tipHeight) {
        osg::Vec4f solid = color;
        osg::Vec4f transparent = color;
        osg::Vec3f nvec(0, 1, 0);

        solid[3] = 1.0f;
        transparent[3] = 0.0f;

        auto geoNode = new simgear::EffectGeode;
        _scaleTransform->addChild(geoNode);
        auto pinGeo = new osg::Geometry;
        osg::Vec3Array* vtx = new osg::Vec3Array;
        osg::Vec3Array* nor = new osg::Vec3Array;
        osg::Vec4Array* rgb = new osg::Vec4Array;

        nor->push_back(nvec);

        vtx->push_back(osg::Vec3f(0, 0, tipHeight));
        rgb->push_back(solid);

        vtx->push_back(osg::Vec3f(-fontSize * 0.125, 0, pinHeight - top_spacing));
        rgb->push_back(transparent);

        vtx->push_back(osg::Vec3f(0, 0, tipHeight));
        rgb->push_back(solid);

        vtx->push_back(osg::Vec3f(0, fontSize * 0.125, pinHeight - top_spacing));
        rgb->push_back(transparent);

        vtx->push_back(osg::Vec3f(0, 0, tipHeight));
        rgb->push_back(solid);

        vtx->push_back(osg::Vec3f(fontSize * 0.125, 0, pinHeight - top_spacing));
        rgb->push_back(transparent);

        vtx->push_back(osg::Vec3f(0, 0, tipHeight));
        rgb->push_back(solid);

        vtx->push_back(osg::Vec3f(0, -fontSize * 0.125, pinHeight - top_spacing));
        rgb->push_back(transparent);

        vtx->push_back(osg::Vec3f(0, 0, tipHeight));
        rgb->push_back(solid);

        vtx->push_back(osg::Vec3f(-fontSize * 0.125, 0, pinHeight - top_spacing));
        rgb->push_back(transparent);

        pinGeo->setVertexArray(vtx);
        pinGeo->setColorArray(rgb, osg::Array::BIND_PER_VERTEX);
        pinGeo->setNormalArray(nor, osg::Array::BIND_OVERALL);
        pinGeo->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::QUAD_STRIP, 0, vtx->size()));
        geoNode->addDrawable(pinGeo);

        auto stateSet = geoNode->getOrCreateStateSet();

        stateSet->setMode(GL_FOG, osg::StateAttribute::OFF);
        stateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
        stateSet->setMode(GL_BLEND, osg::StateAttribute::OFF);
        stateSet->setMode(GL_ALPHA_TEST, osg::StateAttribute::ON);
        stateSet->setMode(GL_DEPTH_TEST, osg::StateAttribute::ON);

        osg::ref_ptr<simgear::SGReaderWriterOptions> opt;
        opt = simgear::SGReaderWriterOptions::copyOrCreate(osgDB::Registry::instance()->getOptions());
        simgear::Effect* effect = simgear::makeEffect("Effects/marker-pin", true, opt);
        if (effect)
            geoNode->setEffect(effect);
    }

    _masterNode->setNodeMask(~simgear::CASTSHADOW_BIT);
    _masterNode->setCullCallback(new FGMarkerCallback(this));
}
