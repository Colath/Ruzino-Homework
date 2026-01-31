#pragma once
#include <pxr/imaging/hdMtlx/hdMtlx.h>

#include <filesystem>

#include "MaterialX/SlangShaderGenerator.h"
#include "MaterialXGenShader/Shader.h"
#include "material.h"

RUZINO_NAMESPACE_OPEN_SCOPE
using namespace MaterialX;

/// @struct ParameterMapping
/// Maps a shader parameter to its location in the material data buffer
/// Note: parameter name is stored as map key, not in struct
struct ParameterMapping {
    unsigned int dataLocation;
    MaterialX::TypeDesc parameterType;
    size_t parameterSize;  // Size in bytes
};

/// Shared pointer to a BindlessContext
using BindlessContextPtr = std::shared_ptr<class BindlessContext>;

/// @class BindlessContext
/// Class representing a resource binding for Slang shader resources.
class BindlessContext : public HwResourceBindingContext {
   public:
    BindlessContext(
        size_t uniformBindingLocation,
        size_t samplerBindingLocation);

    static BindlessContextPtr create(
        size_t uniformBindingLocation = 0,
        size_t samplerBindingLocation = 0)
    {
        return std::make_shared<BindlessContext>(
            uniformBindingLocation, samplerBindingLocation);
    }

    void initialize() override
    {
        fetch_data = "\n VertexData vd; \n";
        data_location = 0;
    }

    void emitDirectives(GenContext& context, ShaderStage& stage) override
    {
        const ShaderGenerator& generator = context.getShaderGenerator();
        generator.emitLine("import Scene.BindlessMaterial", stage);
        generator.emitLine("import Scene.VertexInfo", stage);
    }

    // Emit uniforms with binding information
    void emitResourceBindings(
        GenContext& context,
        const VariableBlock& resources,
        ShaderStage& stage) override;

    // Emit structured uniforms with binding information and align members where
    // possible
    void emitStructuredResourceBindings(
        GenContext& context,
        const VariableBlock& uniforms,
        ShaderStage& stage,
        const std::string& structInstanceName,
        const std::string& arraySuffix) override;

    std::string get_data_code()
    {
        return fetch_data;
    }

    MaterialDataBlob& get_material_data()
    {
        return material_data;
    }

    // Get the mapping of texture names to their data locations
    const std::unordered_map<std::string, unsigned int>&
    get_texture_id_locations() const
    {
        return texture_id_locations;
    }

    // Get all parameter mappings for fast parameter updates (O(1) lookup)
    const std::unordered_map<std::string, ParameterMapping>&
    get_parameter_mappings() const
    {
        return parameter_mappings;
    }

    // Record a parameter mapping for later quick updates
    void record_parameter_mapping(
        const std::string& paramName,
        unsigned int location,
        MaterialX::TypeDesc type,
        size_t size)
    {
        parameter_mappings[paramName] = { location, type, size };
    }

    // Clear parameter mappings (call this at the start of new shader
    // generation)
    void clear_parameter_mappings()
    {
        parameter_mappings.clear();
    }

   private:
    std::string fetch_data = "";
    unsigned int data_location = 0;
    MaterialDataBlob material_data;
    // Maps texture variable name to its data location for storing texture ID
    std::unordered_map<std::string, unsigned int> texture_id_locations;
    // Map of parameter names to mappings for O(1) incremental updates
    std::unordered_map<std::string, ParameterMapping> parameter_mappings;
};

RUZINO_NAMESPACE_CLOSE_SCOPE
