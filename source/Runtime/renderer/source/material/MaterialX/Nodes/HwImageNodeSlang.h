//
// Copyright Contributors to the MaterialX Project
// SPDX-License-Identifier: Apache-2.0
//

#pragma once
#include <MaterialXGenShader/Nodes/SourceCodeNode.h>

#include "../SlangShaderGenerator.h"

MATERIALX_NAMESPACE_BEGIN

/// Extending the SourceCodeNode with requirements for image nodes.
class HD_USTC_CG_API HwImageNodeSlang : public SourceCodeNode {
   public:
    static ShaderNodeImplPtr create();

    void addInputs(ShaderNode& node, GenContext& context) const override;
    void setValues(
        const Node& node,
        ShaderNode& shaderNode,
        GenContext& context) const override;

    void emitFunctionCall(
        const ShaderNode& node,
        GenContext& context,
        ShaderStage& stage) const;
};

MATERIALX_NAMESPACE_END
