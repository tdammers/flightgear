// AIManager.cxx  Based on David Luff's AIMgr:
// - a global management type for AI objects
//
// Written by David Culp, started October 2003.
// - davidculp2@comcast.net 
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include <cstring>
#include <algorithm>

#include <simgear/sg_inlines.h>
#include <simgear/math/sg_geodesy.hxx>
#include <simgear/props/props_io.hxx>
#include <simgear/structure/exception.hxx>
#include <boost/mem_fn.hpp>
#include <boost/foreach.hpp>

#include <Main/globals.hxx>
#include <Airports/simple.hxx>

#include "AIManager.hxx"
#include "AIAircraft.hxx"
#include "AIShip.hxx"
#include "AIBallistic.hxx"
#include "AIStorm.hxx"
#include "AIThermal.hxx"
#include "AICarrier.hxx"
#include "AIStatic.hxx"
#include "AIMultiplayer.hxx"
#include "AITanker.hxx"
#include "AIWingman.hxx"
#include "AIGroundVehicle.hxx"
#include "AIEscort.hxx"

FGAIManager::FGAIManager() :
    cb_ai_bare(SGPropertyChangeCallback<FGAIManager>(this,&FGAIManager::updateLOD,
               fgGetNode("/sim/rendering/static-lod/ai-bare", true))),
    cb_ai_detailed(SGPropertyChangeCallback<FGAIManager>(this,&FGAIManager::updateLOD,
                   fgGetNode("/sim/rendering/static-lod/ai-detailed", true)))
{

}

FGAIManager::~FGAIManager()
{
    std::for_each(ai_list.begin(), ai_list.end(), boost::mem_fn(&FGAIBase::unbind));
}

void
FGAIManager::init() {
    root = fgGetNode("sim/ai", true);

    enabled = root->getNode("enabled", true);

    thermal_lift_node = fgGetNode("/environment/thermal-lift-fps", true);
    wind_from_east_node  = fgGetNode("/environment/wind-from-east-fps",true);
    wind_from_north_node = fgGetNode("/environment/wind-from-north-fps",true);

    user_latitude_node  = fgGetNode("/position/latitude-deg", true);
    user_longitude_node = fgGetNode("/position/longitude-deg", true);
    user_altitude_node  = fgGetNode("/position/altitude-ft", true);
    user_altitude_agl_node  = fgGetNode("/position/altitude-agl-ft", true);
    user_heading_node   = fgGetNode("/orientation/heading-deg", true);
    user_pitch_node     = fgGetNode("/orientation/pitch-deg", true);
    user_yaw_node       = fgGetNode("/orientation/side-slip-deg", true);
    user_roll_node      = fgGetNode("/orientation/roll-deg", true);
    user_speed_node     = fgGetNode("/velocities/uBody-fps", true);
}

void
FGAIManager::postinit() {
    // postinit, so that it can access the Nasal subsystem

    if (!root->getBoolValue("scenarios-enabled", true))
        return;

    // scenarios enabled, AI subsystem required
    if (!enabled->getBoolValue())
        enabled->setBoolValue(true);

    // process all scenarios
    std::map<string, bool> scenarios;
    for (int i = 0 ; i < root->nChildren() ; i++) {
        SGPropertyNode *n = root->getChild(i);
        if (strcmp(n->getName(), "scenario"))
            continue;

        const string& name = n->getStringValue();
        if (name.empty())
            continue;

        if (scenarios.find(name) != scenarios.end()) {
            SG_LOG(SG_AI, SG_DEBUG, "won't load scenario '" << name << "' twice");
            continue;
        }

        SG_LOG(SG_AI, SG_ALERT, "loading scenario '" << name << '\'');
        processScenario(name);
        scenarios[name] = true;
    }
}

void
FGAIManager::reinit()
{
    update(0.0);
    std::for_each(ai_list.begin(), ai_list.end(), boost::mem_fn(&FGAIBase::reinit));
}

void
FGAIManager::bind() {
    root = globals->get_props()->getNode("ai/models", true);
    root->tie("count", SGRawValueMethods<FGAIManager, int>(*this,
        &FGAIManager::getNumAiObjects));
}

void
FGAIManager::unbind() {
    root->untie("count");
}

void FGAIManager::removeDeadItem(FGAIBase* base)
{
    SGPropertyNode *props = base->_getProps();
    
    props->setBoolValue("valid", false);
    base->unbind();
    
    // for backward compatibility reset properties, so that aircraft,
    // which don't know the <valid> property, keep working
    // TODO: remove after a while
    props->setIntValue("id", -1);
    props->setBoolValue("radar/in-range", false);
    props->setIntValue("refuel/tanker", false);
}

void
FGAIManager::update(double dt) {
    // initialize these for finding nearest thermals
    range_nearest = 10000.0;
    strength = 0.0;

    if (!enabled->getBoolValue())
        return;

    fetchUserState();

    // partition the list into dead followed by alive
    ai_list_iterator firstAlive =
      std::stable_partition(ai_list.begin(), ai_list.end(), boost::mem_fn(&FGAIBase::getDie));
    
    // clean up each item and finally remove from the container
    for (ai_list_iterator it=ai_list.begin(); it != firstAlive; ++it) {
        removeDeadItem(*it);
    }
  
    ai_list.erase(ai_list.begin(), firstAlive);
  
    // every remaining item is alive
    BOOST_FOREACH(FGAIBase* base, ai_list) {
        if (base->isa(FGAIBase::otThermal)) {
            processThermal(dt, (FGAIThermal*)base);
        } else {
            base->update(dt);
        }
    } // of live AI objects iteration

    thermal_lift_node->setDoubleValue( strength );  // for thermals
}

/** update LOD settings of all AI/MP models */
void
FGAIManager::updateLOD(SGPropertyNode* node)
{
    SG_UNUSED(node);
    std::for_each(ai_list.begin(), ai_list.end(), boost::mem_fn(&FGAIBase::updateLOD));
}

void
FGAIManager::attach(FGAIBase *model)
{
    const char* typeString = model->getTypeString();
    SGPropertyNode* root = globals->get_props()->getNode("ai/models", true);
    SGPropertyNode* p;
    int i;

    // find free index in the property tree, if we have
    // more than 10000 mp-aircrafts in the property tree we should optimize the mp-server
    for (i = 0; i < 10000; i++) {
        p = root->getNode(typeString, i, false);

        if (!p || !p->getBoolValue("valid", false))
            break;

        if (p->getIntValue("id",-1)==model->getID()) {
            p->setStringValue("callsign","***invalid node***"); //debug only, should never set!
        }
    }

    p = root->getNode(typeString, i, true);
    model->setManager(this, p);
    ai_list.push_back(model);

    model->init(model->getType()==FGAIBase::otAircraft
        || model->getType()==FGAIBase::otMultiplayer
        || model->getType()==FGAIBase::otStatic);
    model->bind();
    p->setBoolValue("valid", true);
}

int
FGAIManager::getNumAiObjects(void) const
{
    return ai_list.size();
}

void
FGAIManager::fetchUserState( void ) {

    user_latitude  = user_latitude_node->getDoubleValue();
    user_longitude = user_longitude_node->getDoubleValue();
    user_altitude  = user_altitude_node->getDoubleValue();
    user_heading   = user_heading_node->getDoubleValue();
    user_pitch     = user_pitch_node->getDoubleValue();
    user_yaw       = user_yaw_node->getDoubleValue();
    user_speed     = user_speed_node->getDoubleValue() * 0.592484;
    user_roll      = user_roll_node->getDoubleValue();
    wind_from_east = wind_from_east_node->getDoubleValue();
    wind_from_north   = wind_from_north_node->getDoubleValue();
    user_altitude_agl = user_altitude_agl_node->getDoubleValue();

}

// only keep the results from the nearest thermal
void
FGAIManager::processThermal( double dt, FGAIThermal* thermal ) {
    thermal->update(dt);

    if ( thermal->_getRange() < range_nearest ) {
        range_nearest = thermal->_getRange();
        strength = thermal->getStrength();
    }

}



void
FGAIManager::processScenario( const string &filename ) {

    SGPropertyNode_ptr scenarioTop = loadScenarioFile(filename);

    if (!scenarioTop)
        return;

    SGPropertyNode* scenarios = scenarioTop->getChild("scenario");

    if (!scenarios)
        return;

    for (int i = 0; i < scenarios->nChildren(); i++) {
        SGPropertyNode* scEntry = scenarios->getChild(i);

        if (strcmp(scEntry->getName(), "entry"))
            continue;
        const std::string& type = scEntry->getStringValue("type", "aircraft");

        if (type == "tanker") { // refueling scenarios
            FGAITanker* tanker = new FGAITanker;
            tanker->readFromScenario(scEntry);
            attach(tanker);

        } else if (type == "wingman") {
            FGAIWingman* wingman = new FGAIWingman;
            wingman->readFromScenario(scEntry);
            attach(wingman);

        } else if (type == "aircraft") {
            FGAIAircraft* aircraft = new FGAIAircraft;
            aircraft->readFromScenario(scEntry);
            attach(aircraft);

        } else if (type == "ship") {
            FGAIShip* ship = new FGAIShip;
            ship->readFromScenario(scEntry);
            attach(ship);

        } else if (type == "carrier") {
            FGAICarrier* carrier = new FGAICarrier;
            carrier->readFromScenario(scEntry);
            attach(carrier);

        } else if (type == "groundvehicle") {
            FGAIGroundVehicle* groundvehicle = new FGAIGroundVehicle;
            groundvehicle->readFromScenario(scEntry);
            attach(groundvehicle);

        } else if (type == "escort") {
            FGAIEscort* escort = new FGAIEscort;
            escort->readFromScenario(scEntry);
            attach(escort);

        } else if (type == "thunderstorm") {
            FGAIStorm* storm = new FGAIStorm;
            storm->readFromScenario(scEntry);
            attach(storm);

        } else if (type == "thermal") {
            FGAIThermal* thermal = new FGAIThermal;
            thermal->readFromScenario(scEntry);
            attach(thermal);

        } else if (type == "ballistic") {
            FGAIBallistic* ballistic = new FGAIBallistic;
            ballistic->readFromScenario(scEntry);
            attach(ballistic);

        } else if (type == "static") {
            FGAIStatic* aistatic = new FGAIStatic;
            aistatic->readFromScenario(scEntry);
            attach(aistatic);
        }

    }

}

SGPropertyNode_ptr
FGAIManager::loadScenarioFile(const std::string& filename)
{
    SGPath path(globals->get_fg_root());
    path.append("AI/" + filename + ".xml");
    try {
        SGPropertyNode_ptr root = new SGPropertyNode;
        readProperties(path.str(), root);
        return root;
    } catch (const sg_exception &t) {
        SG_LOG(SG_AI, SG_ALERT, "Failed to load scenario '"
            << path.str() << "': " << t.getFormattedMessage());
    }
    return 0;
}

bool
FGAIManager::getStartPosition(const string& id, const string& pid,
                              SGGeod& geodPos, double& hdng, SGVec3d& uvw)
{
    bool found = false;
    SGPropertyNode* root = fgGetNode("sim/ai", true);
    if (!root->getNode("enabled", true)->getBoolValue())
        return found;

    for (int i = 0 ; (!found) && i < root->nChildren() ; i++) {
        SGPropertyNode *aiEntry = root->getChild( i );
        if ( !strcmp( aiEntry->getName(), "scenario" ) ) {
            const string& filename = aiEntry->getStringValue();
            SGPropertyNode_ptr scenarioTop = loadScenarioFile(filename);
            if (scenarioTop) {
                SGPropertyNode* scenarios = scenarioTop->getChild("scenario");
                if (scenarios) {
                    for (int i = 0; i < scenarios->nChildren(); i++) {
                        SGPropertyNode* scEntry = scenarios->getChild(i);
                        const std::string& type = scEntry->getStringValue("type");
                        const std::string& pnumber = scEntry->getStringValue("pennant-number");
                        const std::string& name = scEntry->getStringValue("name");
                        if (type == "carrier" && (pnumber == id || name == id)) {
                            SGSharedPtr<FGAICarrier> carrier = new FGAICarrier;
                            carrier->readFromScenario(scEntry);

                            if (carrier->getParkPosition(pid, geodPos, hdng, uvw)) {
                                found = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    return found;
}

const FGAIBase *
FGAIManager::calcCollision(double alt, double lat, double lon, double fuse_range)
{
    // we specify tgt extent (ft) according to the AIObject type
    double tgt_ht[]     = {0,  50, 100, 250, 0, 100, 0, 0,  50,  50, 20, 100,  50};
    double tgt_length[] = {0, 100, 200, 750, 0,  50, 0, 0, 200, 100, 40, 200, 100};
    ai_list_iterator ai_list_itr = ai_list.begin();
    ai_list_iterator end = ai_list.end();

    SGGeod pos(SGGeod::fromDegFt(lon, lat, alt));
    SGVec3d cartPos(SGVec3d::fromGeod(pos));
    
    while (ai_list_itr != end) {
        double tgt_alt = (*ai_list_itr)->_getAltitude();
        int type       = (*ai_list_itr)->getType();
        tgt_ht[type] += fuse_range;

        if (fabs(tgt_alt - alt) > tgt_ht[type] || type == FGAIBase::otBallistic
            || type == FGAIBase::otStorm || type == FGAIBase::otThermal ) {
                //SG_LOG(SG_AI, SG_DEBUG, "AIManager: skipping "
                //    << fabs(tgt_alt - alt)
                //    << " "
                //    << type
                //    );
                ++ai_list_itr;
                continue;
        }

        int id         = (*ai_list_itr)->getID();

        double range = calcRange(cartPos, (*ai_list_itr));

        //SG_LOG(SG_AI, SG_DEBUG, "AIManager:  AI list size "
        //    << ai_list.size()
        //    << " type " << type
        //    << " ID " << id
        //    << " range " << range
        //    //<< " bearing " << bearing
        //    << " alt " << tgt_alt
        //    );

        tgt_length[type] += fuse_range;

        if (range < tgt_length[type]){
            SG_LOG(SG_AI, SG_DEBUG, "AIManager: HIT! "
                << " type " << type
                << " ID " << id
                << " range " << range
                << " alt " << tgt_alt
                );
            return (*ai_list_itr).get();
        }
        ++ai_list_itr;
    }
    return 0;
}

double
FGAIManager::calcRange(const SGVec3d& aCartPos, FGAIBase* aObject) const
{
    double distM = dist(aCartPos, aObject->getCartPos());
    return distM * SG_METER_TO_FEET;
}

//end AIManager.cxx
