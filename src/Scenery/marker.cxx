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
    FGMarkerCallback(FGMarker* _marker): marker(_marker) {}
    virtual void operator()(osg::Node* node, osg::NodeVisitor* nv) {
        assert(node == marker->getMasterNode());
        const float distance = nv->getDistanceToEyePoint(osg::Vec3f{0,0,0}, false);
        marker->setScaling(distance);
        traverse(node, nv);
    }
private:
    FGMarker* marker;
};

FGMarker::FGMarker():
    FGMarker(osgText::String()) {}

FGMarker::FGMarker(const osgText::String& label, float font_size, float pin_height, float tip_height):
    FGMarker(label, font_size, pin_height, tip_height, osg::Vec4f{1, 1, 1, 1}) {}

FGMarker::FGMarker(const osgText::String& label, const osg::Vec4f& color):
    FGMarker(label, 32.0f, 500.0f, 0.0f, color) {}

FGMarker::FGMarker(const osgText::String& label, float font_size, float pin_height, const osg::Vec4f& color):
    FGMarker(label, font_size, pin_height, 0.0f, color) {}

FGMarker::FGMarker(const osgText::String& label, float font_size, const osg::Vec4f& color):
    FGMarker(label, font_size, 500.0f, 0.0f, color) {}

FGMarker::~FGMarker()
{
}

void FGMarker::setText(const osgText::String& label) { labelText->setText(label); }

void FGMarker::setFontSize(float font_size)
{
    labelText->setCharacterSize(font_size, 1.0f);
    labelText->setFontResolution(std::max(32.0f, font_size), std::max(32.0f, font_size));
    distanceText->setCharacterSize(font_size * 0.75f, 1.0f);
    distanceText->setFontResolution(std::max(32.0f, font_size * 0.75f), std::max(32.0f, font_size * 0.75f));
}

void FGMarker::setScaling(float distance)
{
    osg::Matrix mtx;
    const float gamma = 0.9f;
    const float f = std::pow(std::max(distance, 1.0f) / 10000.0f, gamma);
    mtx.makeScale(f, f, f);
    scaleTransform->setMatrix(mtx);
}

void FGMarker::setDistance(float distance)
{
    std::stringstream s;
    s << std::setprecision(1) << std::fixed << distance << "nm";
    distanceText->setText(s.str());
}

FGMarker::FGMarker(const osgText::String& label, float font_size, float pin_height, float tip_height, const osg::Vec4f& color)
{
    masterNode = new osg::Group;

    scaleTransform = new osg::MatrixTransform;
    masterNode->addChild(scaleTransform);

    auto textNode = new osg::Billboard;
    scaleTransform->addChild(textNode);

    textNode->setMode(osg::Billboard::AXIAL_ROT);

    labelText = new osgText::Text;
    labelText->setText(label);
    labelText->setAlignment(osgText::Text::CENTER_BOTTOM);
    labelText->setAxisAlignment(osgText::Text::XZ_PLANE);
    labelText->setFont("Fonts/LiberationFonts/LiberationSans-Regular.ttf");
    // labelText->setCharacterSizeMode(osgText::Text::OBJECT_COORDS_WITH_MAXIMUM_SCREEN_SIZE_CAPPED_BY_FONT_HEIGHT);
    labelText->setColor(color);
    labelText->setPosition(osg::Vec3(0, 0, pin_height));
    labelText->setBackdropType(osgText::Text::OUTLINE);
    labelText->setBackdropColor(osg::Vec4(0, 0, 0, 0.75));
    labelText->setBackdropOffset(0.04f);
    labelText->setPosition(osg::Vec3(0, 0, pin_height + font_size));

    distanceText = new osgText::Text;
    distanceText->setText("--.- nm");
    distanceText->setAlignment(osgText::Text::CENTER_BOTTOM);
    distanceText->setAxisAlignment(osgText::Text::XZ_PLANE);
    distanceText->setFont("Fonts/LiberationFonts/LiberationSans-Regular.ttf");
    // distanceText->setCharacterSizeMode(osgText::Text::OBJECT_COORDS_WITH_MAXIMUM_SCREEN_SIZE_CAPPED_BY_FONT_HEIGHT);
    distanceText->setColor(color);
    distanceText->setPosition(osg::Vec3(0, 0, pin_height));
    distanceText->setBackdropType(osgText::Text::OUTLINE);
    distanceText->setBackdropColor(osg::Vec4(0, 0, 0, 0.75));
    distanceText->setBackdropOffset(0.06f);
    distanceText->setPosition(osg::Vec3(0, 0, pin_height));

    setFontSize(font_size);

    textNode->addDrawable(labelText);
    textNode->addDrawable(distanceText);

    float top_spacing = font_size * 0.25;

    if (pin_height - top_spacing > tip_height) {
        osg::Vec4f solid = color;
        osg::Vec4f transparent = color;
        osg::Vec3f nvec(0, 1, 0);

        solid[3] = 1.0f;
        transparent[3] = 0.0f;

        auto geoNode = new simgear::EffectGeode;
        scaleTransform->addChild(geoNode);
        auto pinGeo = new osg::Geometry;
        osg::Vec3Array* vtx = new osg::Vec3Array;
        osg::Vec3Array* nor = new osg::Vec3Array;
        osg::Vec4Array* rgb = new osg::Vec4Array;

        nor->push_back(nvec);

        vtx->push_back(osg::Vec3f(0, 0, tip_height));
        rgb->push_back(solid);

        vtx->push_back(osg::Vec3f(-font_size * 0.125, 0, pin_height - top_spacing));
        rgb->push_back(transparent);

        vtx->push_back(osg::Vec3f(0, 0, tip_height));
        rgb->push_back(solid);

        vtx->push_back(osg::Vec3f(0, font_size * 0.125, pin_height - top_spacing));
        rgb->push_back(transparent);

        vtx->push_back(osg::Vec3f(0, 0, tip_height));
        rgb->push_back(solid);

        vtx->push_back(osg::Vec3f(font_size * 0.125, 0, pin_height - top_spacing));
        rgb->push_back(transparent);

        vtx->push_back(osg::Vec3f(0, 0, tip_height));
        rgb->push_back(solid);

        vtx->push_back(osg::Vec3f(0, -font_size * 0.125, pin_height - top_spacing));
        rgb->push_back(transparent);

        vtx->push_back(osg::Vec3f(0, 0, tip_height));
        rgb->push_back(solid);

        vtx->push_back(osg::Vec3f(-font_size * 0.125, 0, pin_height - top_spacing));
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

    masterNode->setNodeMask(~simgear::CASTSHADOW_BIT);
    masterNode->setCullCallback(new FGMarkerCallback(this));
}
