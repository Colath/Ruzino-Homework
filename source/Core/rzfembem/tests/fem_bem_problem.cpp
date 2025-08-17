#include <gtest/gtest.h>
#include <pxr/base/vt/array.h>

#include <fem_bem/fem_bem.hpp>

#include "GCore/Components/MeshComponent.h"
#include "GCore/algorithms/delauney.h"
#include "GCore/create_geom.h"

using namespace USTC_CG::fem_bem;
using namespace USTC_CG;

TEST(FEMBEMProblem, Laplacian)
{
    Geometry circle = create_circle(64, 1);

    auto delauneyed = geom_algorithm::delaunay(circle);

    ElementSolverDesc desc;
    desc.set_probelm_dim(2).set_element_family(ElementFamily::P_minus).set_k(0);

    auto solver = create_element_solver(desc);

    // Should be some notation from the periodic finite element table.
    solver.set_element_type();
}
