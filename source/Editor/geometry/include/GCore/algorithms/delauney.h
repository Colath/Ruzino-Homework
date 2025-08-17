#pragma once

#include "GCore/GOP.h"

USTC_CG_NAMESPACE_OPEN_SCOPE

namespace geom_algorithm {

/// Create a Delaunay triangulation of the input geometry
/// @param geometry Input geometry containing a mesh to triangulate
/// @param maximum_radius Maximum triangle area constraint (use 0 for no
/// constraint)
/// @return New geometry containing the Delaunay triangulated mesh
GEOMETRY_API Geometry
delaunay(const Geometry& geometry, float maximum_radius = 0.0f);

}  // namespace geom_algorithm

USTC_CG_NAMESPACE_CLOSE_SCOPE
