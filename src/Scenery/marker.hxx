// marker.hxx - 3D Marker pins in FlightGear
// Copyright (C) 2022 Tobias Dammers
// SPDX-License-Identifier: GPL-2.0-or-later OR MIT

#pragma once

#include <simgear/structure/SGReferenced.hxx>
#include <osg/Geometry>

namespace osgText {
    class String;
    class Text;
}

namespace osg {
    class Node;
    class MatrixTransform;
}

class FGMarker : public SGReferenced
{
public:
    FGMarker();
    FGMarker(const osgText::String& label, float font_size = 32.0f, float pin_height = 500.0f, float tip_height = 0.0f);
    FGMarker(const osgText::String& label, const osg::Vec4f& color);
    FGMarker(const osgText::String& label, float font_size, float pin_height, const osg::Vec4f& color);
    FGMarker(const osgText::String& label, float font_size, const osg::Vec4f& color);
    FGMarker(const osgText::String& label, float font_size, float pin_height, float tip_height, const osg::Vec4f& color);
    virtual ~FGMarker();
    virtual void setText(const osgText::String& label);
    virtual void setFontSize(float);
    virtual void setDistance(float);
    virtual void setScaling(float);
    virtual const char* className() const { return "FGMarker"; }
    virtual osg::Node* getMasterNode() const { return masterNode; }
private:
    float font_size;
    float pin_height;
    float tip_height;
    osg::Vec4f color;
    osgText::Text* labelText;
    osgText::Text* distanceText;
    osg::Group* masterNode;
    osg::MatrixTransform* scaleTransform;
};

