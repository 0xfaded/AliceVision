// This file is part of the AliceVision project.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <aliceVision/mesh/Mesh.hpp>
#include <aliceVision/structures/StaticVector.hpp>

namespace aliceVision {
namespace mesh {

using PointVisibility = StaticVector<int>;
using PointsVisibility = StaticVector<PointVisibility*>;

/**
 * @brief Retrieve the nearest neighbor vertex in @p refMesh for each vertex in @p mesh.
 * @param[in] refMesh input reference mesh
 * @param[in] mesh input target mesh
 * @param[out] out_nearestVertex index of the nearest vertex in @p refMesh for each vertex in @p mesh
 * @return the nearest vertex in @p refMesh for each vertex in @p mesh
 */
int getNearestVertices(const Mesh& refMesh, const Mesh& mesh, StaticVector<int>& out_nearestVertex);

/**
 * @brief Transfer the visibility per vertex from one mesh to another.
 * For each vertex of the @p mesh, we search the nearest neighbor vertex in the @p refMesh and copy its visibility information.
 * @note The visibility information is a list of camera IDs which are seeing the vertex.
 *
 * @param[in] refMesh input reference mesh
 * @param[in] refPtsVisibilities visibility array per vertex of @p refMesh
 * @param[in] mesh input target mesh
 * @param[out] out_ptsVisibilities visibility array per vertex of @p mesh
 */
void remapMeshVisibilities(
    const Mesh& refMesh, const PointsVisibility& refPtsVisibilities,
    const Mesh& mesh, PointsVisibility& out_ptsVisibilities);

} // namespace mesh
} // namespace aliceVision
