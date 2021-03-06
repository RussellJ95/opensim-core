/* -------------------------------------------------------------------------- *
 *                             OpenSim:  Geometry.cpp                             *
 * -------------------------------------------------------------------------- *
 * The OpenSim API is a toolkit for musculoskeletal modeling and simulation.  *
 * See http://opensim.stanford.edu and the NOTICE file for more information.  *
 * OpenSim is developed at Stanford University and supported by the US        *
 * National Institutes of Health (U54 GM072970, R24 HD065690) and by DARPA    *
 * through the Warrior Web program.                                           *
 *                                                                            *
 * Copyright (c) 2005-2015 Stanford University and the Authors                *
 * Author(s): Ayman Habib                                                     *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may    *
 * not use this file except in compliance with the License. You may obtain a  *
 * copy of the License at http://www.apache.org/licenses/LICENSE-2.0.         *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 * -------------------------------------------------------------------------- */

//=============================================================================
// INCLUDES
//=============================================================================
#include <OpenSim/Simulation/osimSimulationDLL.h>
#include <OpenSim/Common/Property.h>
#include <OpenSim/Common/Component.h>
#include "Frame.h"
#include "PhysicalFrame.h"
#include "Geometry.h"
#include "Model.h"
#include "ModelVisualizer.h"
//=============================================================================
// STATICS
//=============================================================================
using namespace std;
using namespace OpenSim;
using namespace SimTK;

OpenSim_DEFINE_CONNECTOR_FD(frame, Geometry);

Geometry::Geometry() {
    setNull();
    constructProperties();
}

void Geometry::setFrame(const Frame& frame)
{
    updConnector<Frame>("frame").setConnecteeName(frame.getRelativePathName(*this));
}

const OpenSim::Frame& Geometry::getFrame() const
{
    return getConnector<Frame>("frame").getConnectee();
}

void Geometry::extendConnect(Component& root)
{
    Super::extendConnect(root);

    bool attachedToFrame = getConnector<Frame>("frame").isConnected();
    bool hasInputTransform = getInput("transform").isConnected();
    // Being both attached to a Frame (i.e. Connector<Frame> connected) 
    // and the Input transform connected has ambiguous behavior so disallow it
    if (attachedToFrame && hasInputTransform ) {
        OPENSIM_THROW(Exception, getConcreteClassName() + " '" + getName()
            + "' cannot be attached to a Frame and have its "
                "Input `transform` set.");
    }
    else if (!attachedToFrame && !hasInputTransform) {
        OPENSIM_THROW(Exception, getConcreteClassName() + " '" + getName()
            + "' must be attached to a Frame OR have its "
                "Input `transform` set.");
    }
}

void Geometry::generateDecorations(bool fixed, 
    const ModelDisplayHints& hints,
    const SimTK::State& state,
    SimTK::Array_<SimTK::DecorativeGeometry>& appendToThis) const
{
    // serialized Geometry is assumed fixed
    // if it has a Transform input then it is not "attached" geometry
    // and fixed to a body but floating w.r.t Ground.
    if (!fixed && !getInput("transform").isConnected())
        return; 
    
    SimTK::Array_<SimTK::DecorativeGeometry> decos;
    implementCreateDecorativeGeometry(decos);
    if (decos.size() == 0) return;
    setDecorativeGeometryTransform(decos, state);
    for (unsigned i = 0; i < decos.size(); i++){
        setDecorativeGeometryAppearance(decos[i]);
        appendToThis.push_back(decos[i]);
    }
}

/*
 * Apply the Transform of the Frame the Geometry is attached to,
 * OR use the Transform supplied to the Geometry via its Input.
*/
void Geometry::setDecorativeGeometryTransform(
    SimTK::Array_<SimTK::DecorativeGeometry>& decorations, 
    const SimTK::State& state) const
{
    auto& input = getInput<SimTK::Transform>("transform");

    SimTK::Transform transformInBaseFrame;
    SimTK::MobilizedBodyIndex mbidx;

    if (input.isConnected()) {
        transformInBaseFrame = input.getValue(state);
        mbidx = SimTK::MobilizedBodyIndex(0);
    }
    else {
        const Frame& myFrame = getFrame();
        const Frame& bFrame = myFrame.findBaseFrame();
        const PhysicalFrame* bPhysicalFrame =
            dynamic_cast<const PhysicalFrame*>(&bFrame);
        if (bPhysicalFrame == nullptr) {
            // throw exception something is wrong
            throw (Exception("Frame for Geometry " + getName() +
                " is not attached to a PhysicalFrame."));
        }
        mbidx = bPhysicalFrame->getMobilizedBodyIndex();
        transformInBaseFrame = myFrame.findTransformInBaseFrame();
    }

    for (unsigned i = 0; i < decorations.size(); i++){
        decorations[i].setBodyId(mbidx);
        decorations[i].setTransform(transformInBaseFrame);
        decorations[i].setIndexOnBody(i);
    }
}
void Sphere::implementCreateDecorativeGeometry(
    SimTK::Array_<SimTK::DecorativeGeometry>& decoGeoms) const
{
    const Vec3 netScale = get_scale_factors();
    DecorativeSphere deco(get_radius());
    deco.setScaleFactors(netScale);
    decoGeoms.push_back(deco);
}

void Cylinder::implementCreateDecorativeGeometry(
    SimTK::Array_<SimTK::DecorativeGeometry>& decoGeoms) const
{
    const Vec3 netScale = get_scale_factors();
    DecorativeCylinder deco(get_radius(), get_half_height());
    deco.setScaleFactors(netScale);
    decoGeoms.push_back(deco);
}

void Cone::implementCreateDecorativeGeometry(SimTK::Array_<SimTK::DecorativeGeometry>& decoGeoms) const
{
    const Vec3 netScale = get_scale_factors();
    DecorativeCone deco(get_origin(), SimTK::UnitVec3(get_direction()), get_height(), get_base_radius());
    deco.setScaleFactors(netScale);
    decoGeoms.push_back(deco);
}

void LineGeometry::implementCreateDecorativeGeometry(SimTK::Array_<SimTK::DecorativeGeometry>& decoGeoms) const
{
    const Vec3 netScale = get_scale_factors();
    DecorativeLine deco(get_start_point(), get_end_point());
    deco.setScaleFactors(netScale);
    decoGeoms.push_back(deco);
}


void Arrow::implementCreateDecorativeGeometry(SimTK::Array_<SimTK::DecorativeGeometry>& decoGeoms) const
{
    const Vec3 netScale = get_scale_factors();
    SimTK::Vec3 endPt(get_length()*get_direction());
    DecorativeArrow deco(SimTK::Vec3(0), endPt);
    deco.setLineThickness(0.05);
    deco.setScaleFactors(netScale);
    decoGeoms.push_back(deco);
}

void Ellipsoid::implementCreateDecorativeGeometry(SimTK::Array_<SimTK::DecorativeGeometry>& decoGeoms) const
{
    const Vec3 netScale = get_scale_factors();
    DecorativeEllipsoid deco(get_radii());
    deco.setScaleFactors(netScale);
    decoGeoms.push_back(deco);
}

void Brick::implementCreateDecorativeGeometry(SimTK::Array_<SimTK::DecorativeGeometry>& decoGeoms) const
{
    const Vec3 netScale = get_scale_factors();
    DecorativeBrick deco(get_half_lengths());
    deco.setScaleFactors(netScale);
    decoGeoms.push_back(deco);
}

void FrameGeometry::implementCreateDecorativeGeometry(SimTK::Array_<SimTK::DecorativeGeometry>& decoGeoms) const
{
    const Vec3 netScale = get_scale_factors();
    DecorativeFrame deco(1.0);
    deco.setLineThickness(get_display_radius());
    deco.setScaleFactors(netScale);
    decoGeoms.push_back(deco);
}

void Mesh::extendFinalizeFromProperties() {

    if (!isObjectUpToDateWithProperties()) {
        const Component* rootModel = nullptr;
        if (!hasParent()) {
            std::clog << "Mesh " << get_mesh_file() << " not connected to model..ignoring\n";
            return;   // Orphan Mesh not part of a model yet
        }
        const Component* parent = &getParent();
        while (parent != nullptr) {
            if (dynamic_cast<const Model*>(parent) != nullptr) {
                rootModel = parent;
                break;
            }
            if (parent->hasParent())
                parent = &(parent->getParent()); // traverse up Component tree
            else
                break; // can't traverse up.
        }

        if (rootModel == nullptr) {
            std::clog << "Mesh " << get_mesh_file() << " not connected to model..ignoring\n";
            return;   // Orphan Mesh not descendent of a model
        }
        // Current interface to Visualizer calls generateDecorations on every frame.
        // On first time through, load file and create DecorativeMeshFile and cache it
        // so we don't load files from disk during live drawing/rendering.
        const std::string& file = get_mesh_file();
        bool isAbsolutePath; string directory, fileName, extension;
        SimTK::Pathname::deconstructPathname(file,
            isAbsolutePath, directory, fileName, extension);
        const string lowerExtension = SimTK::String::toLower(extension);
        if (lowerExtension != ".vtp" && lowerExtension != ".obj" && lowerExtension != ".stl") {
            std::clog << "ModelVisualizer ignoring '" << file
                << "'; only .vtp .stl and .obj files currently supported.\n";
            return;
        }

        // File is a .vtp or .obj. See if we can find it.
        Array_<string> attempts;
        const Model& model = dynamic_cast<const Model&>(*rootModel);
        bool foundIt = ModelVisualizer::findGeometryFile(model, file, isAbsolutePath, attempts);

        if (!foundIt) {
            std::clog << "ModelVisualizer couldn't find file '" << file
                << "'; tried\n";
            for (unsigned i = 0; i < attempts.size(); ++i)
                std::clog << "  " << attempts[i] << "\n";
            if (!isAbsolutePath &&
                !Pathname::environmentVariableExists("OPENSIM_HOME"))
                std::clog << "Set environment variable OPENSIM_HOME "
                << "to search $OPENSIM_HOME/Geometry.\n";
            return;
        }

        SimTK::PolygonalMesh pmesh;
        try {
            std::ifstream objFile;
            objFile.open(attempts.back().c_str());
            pmesh.loadFile(attempts.back().c_str());
            // objFile closes when destructed

        }
        catch (const std::exception& e) {
            std::clog << "Visualizer couldn't read "
                << attempts.back() << " because:\n"
                << e.what() << "\n";
            return;
        }

        cachedMesh.reset(new DecorativeMeshFile(attempts.back().c_str()));
    }
}


void Mesh::implementCreateDecorativeGeometry(SimTK::Array_<SimTK::DecorativeGeometry>& decoGeoms) const
{
    if (cachedMesh.get() != nullptr) {
        cachedMesh->setScaleFactors(get_scale_factors());
        decoGeoms.push_back(*cachedMesh);
    }
}
