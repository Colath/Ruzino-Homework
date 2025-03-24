//
// Copyright Contributors to the MaterialX Project
// SPDX-License-Identifier: Apache-2.0
//

#include "HwImageNodeSlang.h"

#include <MaterialXGenShader/Util.h>

#include <iostream>

MATERIALX_NAMESPACE_BEGIN

namespace {

const string INLINE_VARIABLE_PREFIX("{{");
const string INLINE_VARIABLE_SUFFIX("}}");

}  // anonymous namespace

// Additional implementaton arguments for image nodes
const string UV_SCALE = "uv_scale";
const string UV_OFFSET = "uv_offset";

ShaderNodeImplPtr HwImageNodeSlang::create()
{
    return std::make_shared<HwImageNodeSlang>();
}

void HwImageNodeSlang::addInputs(ShaderNode& node, GenContext&) const
{
    // Add additional scale and offset inputs to match implementation arguments
    ShaderInput* input = node.addInput(UV_SCALE, Type::VECTOR2);
    input->setValue(Value::createValue<Vector2>(Vector2(1.0f, 1.0f)));
    input = node.addInput(UV_OFFSET, Type::VECTOR2);
    input->setValue(Value::createValue<Vector2>(Vector2(0.0f, 0.0f)));
}

void HwImageNodeSlang::setValues(
    const Node& node,
    ShaderNode& shaderNode,
    GenContext& context) const
{
    // Remap uvs to normalized 0..1 space if the original UDIMs in a UDIM set
    // have been mapped to a single texture atlas which must be accessed in 0..1
    // space.
    if (context.getOptions().hwNormalizeUdimTexCoords) {
        InputPtr file = node.getInput("file");
        if (file) {
            // set the uv scale and offset properly.
            const string& fileName = file->getValueString();
            if (fileName.find(UDIM_TOKEN) != string::npos) {
                ValuePtr udimSetValue =
                    node.getDocument()->getGeomPropValue(UDIM_SET_PROPERTY);
                if (udimSetValue && udimSetValue->isA<StringVec>()) {
                    const StringVec& udimIdentifiers =
                        udimSetValue->asA<StringVec>();
                    vector<Vector2> udimCoordinates{ getUdimCoordinates(
                        udimIdentifiers) };

                    Vector2 scaleUV{ 1.0f, 1.0f };
                    Vector2 offsetUV{ 0.0f, 0.0f };
                    getUdimScaleAndOffset(udimCoordinates, scaleUV, offsetUV);

                    ShaderInput* input = shaderNode.getInput(UV_SCALE);
                    if (input) {
                        input->setValue(Value::createValue<Vector2>(scaleUV));
                    }
                    input = shaderNode.getInput(UV_OFFSET);
                    if (input) {
                        input->setValue(Value::createValue<Vector2>(offsetUV));
                    }
                }
            }
        }
    }
}

void HwImageNodeSlang::emitFunctionCall(
    const ShaderNode& node,
    GenContext& context,
    ShaderStage& stage) const
{
    DEFINE_SHADER_STAGE(stage, Stage::PIXEL)
    {
        const ShaderGenerator& shadergen = context.getShaderGenerator();
        if (_inlined) {
            // An inline function call

            size_t pos = 0;
            size_t i = _functionSource.find(INLINE_VARIABLE_PREFIX);
            StringSet variableNames;
            StringVec code;
            while (i != string::npos) {
                code.push_back(_functionSource.substr(pos, i - pos));
                size_t j = _functionSource.find(INLINE_VARIABLE_SUFFIX, i + 2);
                if (j == string::npos) {
                    throw ExceptionShaderGenError(
                        "Malformed inline expression in implementation for "
                        "node " +
                        node.getName());
                }

                const string variable =
                    _functionSource.substr(i + 2, j - i - 2);
                const ShaderInput* input = node.getInput(variable);
                if (!input) {
                    throw ExceptionShaderGenError(
                        "Could not find an input named '" + variable +
                        "' on node '" + node.getName() + "'");
                }

                if (input->getConnection()) {
                    code.push_back(shadergen.getUpstreamResult(input, context));
                }
                else {
                    string variableName =
                        node.getName() + "_" + input->getName() + "_tmp";
                    if (!variableNames.count(variableName)) {
                        ShaderPort v(
                            nullptr,
                            input->getType(),
                            variableName,
                            input->getValue());
                        shadergen.emitLineBegin(stage);
                        const Syntax& syntax = shadergen.getSyntax();
                        const string valueStr =
                            (v.getValue()
                                 ? syntax.getValue(v.getType(), *v.getValue())
                                 : syntax.getDefaultValue(v.getType()));
                        const string& qualifier = syntax.getConstantQualifier();
                        string str =
                            qualifier.empty() ? EMPTY_STRING : qualifier + " ";
                        str += syntax.getTypeName(v.getType()) + " " +
                               v.getVariable();
                        str +=
                            valueStr.empty() ? EMPTY_STRING : " = " + valueStr;
                        shadergen.emitString(str, stage);
                        shadergen.emitLineEnd(stage);
                        variableNames.insert(variableName);
                    }
                    code.push_back(variableName);
                }

                pos = j + 2;
                i = _functionSource.find(INLINE_VARIABLE_PREFIX, pos);
            }
            code.push_back(_functionSource.substr(pos));

            shadergen.emitLineBegin(stage);
            shadergen.emitOutput(node.getOutput(), true, false, context, stage);
            shadergen.emitString(" = ", stage);
            for (const string& c : code) {
                shadergen.emitString(c, stage);
            }
            shadergen.emitLineEnd(stage);
        }
        else {
            // An ordinary source code function call

            // Declare the output variables.
            emitOutputVariables(node, context, stage);

            shadergen.emitLineBegin(stage);
            string delim = "";

            // Emit function name.
            shadergen.emitString(_functionName + "(", stage);

            shadergen.emitString("sampler, ", stage);

            // Emit all inputs on the node.
            for (ShaderInput* input : node.getInputs()) {
                shadergen.emitString(delim, stage);
                shadergen.emitInput(input, context, stage);
                delim = ", ";
            }

            // Emit node outputs.
            for (size_t i = 0; i < node.numOutputs(); ++i) {
                shadergen.emitString(delim, stage);
                shadergen.emitOutput(
                    node.getOutput(i), false, false, context, stage);
                delim = ", ";
            }

            // End function call
            shadergen.emitString(")", stage);
            shadergen.emitLineEnd(stage);
        }
    }
}

MATERIALX_NAMESPACE_END
