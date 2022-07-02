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
    FGMarker(const osgText::String& label, float _fontSize = 32.0f, float _pinHeight = 500.0f, float _tipHeight = 0.0f);
    FGMarker(const osgText::String& label, const osg::Vec4f& color);
    FGMarker(const osgText::String& label, float _fontSize, float _pinHeight, const osg::Vec4f& color);
    FGMarker(const osgText::String& label, float _fontSize, const osg::Vec4f& color);
    FGMarker(const osgText::String& label, float _fontSize, float _pinHeight, float _tipHeight, const osg::Vec4f& color);
    virtual ~FGMarker();
    void setText(const osgText::String& label);
    void setFontSize(float);
    void setDistance(float);
    void setScaling(float);
    virtual osg::ref_ptr<osg::Node> getMasterNode() const { return _masterNode; }
private:
    osgText::Text* _labelText;
    osgText::Text* _distanceText;
    osg::ref_ptr<osg::Group> _masterNode;
    osg::MatrixTransform* _scaleTransform;
};

