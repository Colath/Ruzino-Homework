#include <cassert>
#include <iostream>
#include <vector>

#include "fem_bem/ElementBasis.hpp"
#include "fem_bem/api.h"
#include "fem_bem/fem_bem.hpp"

using namespace USTC_CG;
int main()
{
    // Create a simple triangle: (0,0), (0,1), (1,0)
    std::vector<std::vector<double>> vertices = {
        { 0.0, 0.0 },  // v0
        { 0.0, 5.0 },  // v1
        { 1.0, 0.0 }   // v2
    };

    std::cout << "Testing gradient inner product with triangle vertices:"
              << std::endl;
    std::cout << "v0: (" << vertices[0][0] << ", " << vertices[0][1] << ")"
              << std::endl;
    std::cout << "v1: (" << vertices[1][0] << ", " << vertices[1][1] << ")"
              << std::endl;
    std::cout << "v2: (" << vertices[2][0] << ", " << vertices[2][1] << ")"
              << std::endl;

    // Create 2D finite element basis
    auto basis = fem_bem::make_fem_2d();

    // Add linear shape functions
    basis->add_vertex_expression("1 - u1 - u2");  // N0 = 1 - u1 - u2
    basis->add_vertex_expression("u1");           // N1 = u1
    basis->add_vertex_expression("u2");           // N2 = u2

    auto expressions = basis->get_vertex_expressions();

    // Compute gradients in reference coordinates
    auto grad_N0 = expressions[0].gradient({ "u1", "u2" });
    auto grad_N1 = expressions[1].gradient({ "u1", "u2" });
    auto grad_N2 = expressions[2].gradient({ "u1", "u2" });

    // Compute Jacobian transformation
    // d1 = v1 - v0, d2 = v2 - v0
    auto d1_x = vertices[1][0] - vertices[0][0];  // 0 - 0 = 0
    auto d1_y = vertices[1][1] - vertices[0][1];  // 1 - 0 = 1
    auto d2_x = vertices[2][0] - vertices[0][0];  // 1 - 0 = 1
    auto d2_y = vertices[2][1] - vertices[0][1];  // 0 - 0 = 0

    std::cout << "\nJacobian vectors:" << std::endl;
    std::cout << "d1 = [" << d1_x << ", " << d1_y << "]" << std::endl;
    std::cout << "d2 = [" << d2_x << ", " << d2_y << "]" << std::endl;

    // Jacobian determinant
    auto jac_det = d1_x * d2_y - d1_y * d2_x;  // 0*0 - 1*1 = -1
    auto det_sq = jac_det * jac_det;
    std::cout << "Jacobian determinant: " << jac_det << std::endl;

    // Inverse Jacobian squared elements (按照rzfembem.cpp的方法)
    auto j00 = (d2_x * d2_x + d2_y * d2_y) / det_sq;
    auto j01 = -(d2_x * d2_x + d1_y * d1_y) / det_sq;
    auto j10 = j01;
    auto j11 = (d1_x * d1_x + d1_y * d1_y) / det_sq;

    std::cout << "\nInverse Jacobian squared elements:" << std::endl;
    std::cout << "[" << j00 << ", " << j01 << "]" << std::endl;
    std::cout << "[" << j10 << ", " << j11 << "]" << std::endl;

    // Triangle area (absolute value of Jacobian determinant / 2)
    auto triangle_area = std::abs(jac_det) / 2.0;  // |-1| / 2 = 0.5
    std::cout << "\nTriangle area: " << triangle_area << std::endl;

    // Create final expressions for gradient inner products
    fem_bem::Expression final_expressions[9];  // 3x3 matrix

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            // Get gradients based on index
            auto& grad_i = (i == 0) ? grad_N0 : (i == 1) ? grad_N1 : grad_N2;
            auto& grad_j = (j == 0) ? grad_N0 : (j == 1) ? grad_N1 : grad_N2;

            // ∇Ni · G · ∇Nj where G is the metric tensor
            final_expressions[i * 3 + j] =
                grad_i[0] * fem_bem::Expression("j00") * grad_j[0] +
                grad_i[0] * fem_bem::Expression("j01") * grad_j[1] +
                grad_i[1] * fem_bem::Expression("j10") * grad_j[0] +
                grad_i[1] * fem_bem::Expression("j11") * grad_j[1];
        }
    }

    std::cout << "\nComputing stiffness matrix entries:" << std::endl;

    // Compute and output all 3x3 entries
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            // Bind the metric tensor values
            final_expressions[i * 3 + j].bind_variables({ { "j00", j00 },
                                                          { "j01", j01 },
                                                          { "j10", j10 },
                                                          { "j11", j11 } });

            // Integrate over reference triangle
            auto integrated = fem_bem::integrate_over_simplex(
                final_expressions[i * 3 + j], { "u1", "u2" }, nullptr, 2);

            // Scale by triangle area
            auto stiffness_entry = integrated * triangle_area;

            std::cout << "K[" << i << "][" << j << "] = " << stiffness_entry
                      << std::endl;
        }
    }

    // Verify theoretical values for this specific triangle
    std::cout << "\nTheoretical verification:" << std::endl;
    std::cout << "For this triangle, the stiffness matrix should be:"
              << std::endl;
    std::cout << "K = [[ 1, -0.5, -0.5]," << std::endl;
    std::cout << "     [-0.5,  0.5,   0]," << std::endl;
    std::cout << "     [-0.5,   0,  0.5]]" << std::endl;

    return 0;
}
