#pragma once

#include "asset_database.hpp"
#include <mbgl/style/layers/custom_layer.hpp>
#include <mbgl/util/mat2.hpp>
#include <mbgl/util/mat4.hpp>
#include <mbgl/util/projection.hpp>
#include <mbgl/platform/gl_functions.hpp>
#include <mbgl/gl/defines.hpp>
#include <mbgl/util/convert.hpp>
#include <map>

namespace mbgl {

struct GLMesh {
    platform::GLuint vertexBuffer = 0;
    platform::GLuint indexBuffer = 0;
    int indexCount = 0;
};

class MeshAssetLayer : public mbgl::style::CustomLayerHost {
public:
    MeshAssetLayer(std::shared_ptr<AssetDatabase> meshDb);

    void initialize() override;
    void render(const mbgl::style::CustomLayerRenderParameters& params) override;
    void contextLost() override;
    void deinitialize() override ;

private:
    platform::GLuint program = 0;
    platform::GLuint vertexShader = 0;
    platform::GLuint fragmentShader = 0;
    platform::GLuint u_wvp = 0;
    platform::GLuint u_lodColor = 0;
    platform::GLuint a_pos = 0;
    platform::GLuint a_norm = 0;

    std::shared_ptr<AssetDatabase> meshDatabase;
    std::map<std::string, GLMesh> loadedMeshes;
};
}