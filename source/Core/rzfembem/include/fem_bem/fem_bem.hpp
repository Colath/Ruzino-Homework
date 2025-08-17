#pragma once
#include "ElementBasis.hpp"
#include "GCore/GOP.h"
#include "api.h"

USTC_CG_NAMESPACE_OPEN_SCOPE

enum class ElementFamily {
    P_minus,
    P,
    Q_minus,
    S
};

class RZFEMBEM_API ElementSolver{
    public :

};

std::shared_ptr<ElementSolver> create_element_solver();

USTC_CG_NAMESPACE_CLOSE_SCOPE
