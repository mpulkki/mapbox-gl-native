#include "mesh_asset_layer.hpp"

#define VISUALIZE_FRUSTUM_CULLING 1

using namespace mbgl::platform;

namespace mbgl {

// Note that custom layers need to draw geometry with a z value of 1 to take advantage of
// depth-based fragment culling.
static const GLchar* vertexShaderSource = R"MBGL_SHADER(
attribute vec4 a_pos;
attribute vec4 a_norm;
uniform mat4 u_wvp;
varying float v_light;
void main() {
    v_light = max(dot(a_norm.xyz, vec3(-0.707, 0.0, -0.707)), 0.5);
    gl_Position = u_wvp * a_pos;
}
)MBGL_SHADER";

static const GLchar* fragmentShaderSource = R"MBGL_SHADER(
varying float v_light;
uniform vec3 u_lodColor;
void main() {
    gl_FragColor = vec4(u_lodColor * v_light, 1.0);
    gl_FragColor.w = 1.0;
}
)MBGL_SHADER";

static float clipSpaceContribution(const Bounds3f& clipBounds) {
    // return shortest extent (x or y) normalized to [0, 1]
    const auto& mn = clipBounds.min;
    const auto& mx = clipBounds.max;

    return std::min(0.5f * (mx.x - mn.x), 0.5f * (mx.y - mn.y));
}

MeshAssetLayer::MeshAssetLayer(std::shared_ptr<AssetDatabase> meshDb)
    : meshDatabase(meshDb) {
}

void MeshAssetLayer::initialize() {
    program = MBGL_CHECK_ERROR(glCreateProgram());
    vertexShader = MBGL_CHECK_ERROR(glCreateShader(GL_VERTEX_SHADER));
    fragmentShader = MBGL_CHECK_ERROR(glCreateShader(GL_FRAGMENT_SHADER));

    MBGL_CHECK_ERROR(glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr));
    MBGL_CHECK_ERROR(glCompileShader(vertexShader));
    MBGL_CHECK_ERROR(glAttachShader(program, vertexShader));
    MBGL_CHECK_ERROR(glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr));
    MBGL_CHECK_ERROR(glCompileShader(fragmentShader));
    MBGL_CHECK_ERROR(glAttachShader(program, fragmentShader));
    MBGL_CHECK_ERROR(glLinkProgram(program));
    a_pos = MBGL_CHECK_ERROR(glGetAttribLocation(program, "a_pos"));
    a_norm = MBGL_CHECK_ERROR(glGetAttribLocation(program, "a_norm"));
    u_wvp = MBGL_CHECK_ERROR(glGetUniformLocation(program, "u_wvp"));
    u_lodColor = MBGL_CHECK_ERROR(glGetUniformLocation(program, "u_lodColor"));
}

void MeshAssetLayer::render(const mbgl::style::CustomLayerRenderParameters& params) {
    static std::vector<AssetDatabase::Descriptor> meshDescriptors;

    auto unitScale = std::pow(2, params.zoom);

    // Prepare global opengl state
    MBGL_CHECK_ERROR(glUseProgram(program));
    MBGL_CHECK_ERROR(glEnable(GL_CULL_FACE));
    MBGL_CHECK_ERROR(glCullFace(GL_FRONT));
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    // Get all available meshes.
    // TODO: coarse visibility check to find only potentially visible meshes
    meshDescriptors.clear();
    meshDatabase->queryMeshes(meshDescriptors);

    for (auto& desc : meshDescriptors) {
        const Mesh* mesh = meshDatabase->getMesh(desc.uri);
        assert(mesh);

        // Project LatLng coordinates to 2d plane for rendering
        auto projectedPoint = Projection::project(desc.position, unitScale);

        mat4 translation;
        mat4 userTransform;
        mat4 modelTransform;
        mat4 wvp;

        matrix::identity(translation);
        matrix::identity(userTransform);
        matrix::identity(modelTransform);
        matrix::identity(wvp);

        matrix::translate(translation, translation, projectedPoint.x, projectedPoint.y, 0.0);
        matrix::scale(userTransform, desc.localTransform, unitScale, unitScale, unitScale);
        matrix::multiply(modelTransform, translation, userTransform);
        matrix::multiply(wvp, params.projectionMatrix, modelTransform);

        // Compute clip space bounds of the model to cull non-visible models
        Bounds3f clipBounds = projectToClipSpace(mesh->bounds, wvp);
#if VISUALIZE_FRUSTUM_CULLING
        float clipBorder = 0.75f;
#else
        float clipBorder = 1.0f;
#endif
        if (clipBounds.max.x < -clipBorder || clipBounds.min.x > clipBorder)
            continue;
        else if (clipBounds.max.y < -clipBorder || clipBounds.min.y > clipBorder)
            continue;

        // Compute conservative screen contribution of this model and determine LOD level using that value
        // 1.0 = model bounds covers the whole viewport, 0.5 = covers half of the viewport, etc...
        // This should be combined with screen dpi to further enhance the LOD level selection
        auto contrib = clipSpaceContribution(clipBounds);

        const int lodLevelCount = 6;
        const float lodLevelThresholds[lodLevelCount] = {
            0.3f,   // Max lod
            0.1f,
            0.05f,
            0.03f,
            0.01f,
            0.005f  // Min lod. Everything below this will be culled
        };

        const Vec3f lodLevelColors[lodLevelCount] = {
            { 0, 0.75, 0 },
            { 0.3, 0.75, 0 },
            { 0.6, 0.75, 0 },
            { 0.75, 0.75, 0 },
            { 0.75, 0.4, 0 },
            { 0.75, 0.2, 0 }
        };

        int lodLvl = 0;
        for (auto threshold : lodLevelThresholds) {
            if (contrib >= threshold)
                break;
            else
                lodLvl++;
        }

        if (lodLvl == lodLevelCount)
            // Culled by too small contribution
            continue;

        // NOTE: visualize lod level selection using different colors instead of actually renderind different lod meshes
        Vec3f color = lodLevelColors[lodLvl];

        // Check if the mesh should be uploaded to gpu
        GLMesh glMesh;
        auto meshIt = loadedMeshes.find(desc.uri);

        if (meshIt == loadedMeshes.end()) {
            MBGL_CHECK_ERROR(glGenBuffers(1, &glMesh.vertexBuffer));
            MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, glMesh.vertexBuffer));
            MBGL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, mesh->vertices.size() * sizeof(VertexPN), (const GLfloat*)mesh->vertices.data(), GL_STATIC_DRAW));
            MBGL_CHECK_ERROR(glGenBuffers(1, &glMesh.indexBuffer));
            MBGL_CHECK_ERROR(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glMesh.indexBuffer));
            MBGL_CHECK_ERROR(glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->triangles.size() * sizeof(Tri3i), (const float*)mesh->triangles.data(), GL_STATIC_DRAW));
            glMesh.indexCount = mesh->triangles.size() * 3;
            loadedMeshes.insert({ desc.uri, glMesh });
        }
        else
            glMesh = meshIt->second;

        // Render the mesh
        MBGL_CHECK_ERROR(glUniform3f(u_lodColor, color.x, color.y, color.z));
        MBGL_CHECK_ERROR(glUniformMatrix4fv(u_wvp, 1, false, util::convert<float>(wvp).data()));
        MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, glMesh.vertexBuffer));
        MBGL_CHECK_ERROR(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glMesh.indexBuffer));
        MBGL_CHECK_ERROR(glEnableVertexAttribArray(0));
        MBGL_CHECK_ERROR(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPN), (void*)0));
        MBGL_CHECK_ERROR(glEnableVertexAttribArray(1));
        MBGL_CHECK_ERROR(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPN), (void*)(sizeof(Vec4f))));
        MBGL_CHECK_ERROR(glDrawElements(GL_TRIANGLES, glMesh.indexCount, GL_UNSIGNED_INT, 0));
    }
}

void MeshAssetLayer::contextLost() {
}

void MeshAssetLayer::deinitialize() {
        if (program) {
        MBGL_CHECK_ERROR(glDetachShader(program, vertexShader));
        MBGL_CHECK_ERROR(glDetachShader(program, fragmentShader));
        MBGL_CHECK_ERROR(glDeleteShader(vertexShader));
        MBGL_CHECK_ERROR(glDeleteShader(fragmentShader));
        MBGL_CHECK_ERROR(glDeleteProgram(program));

        for (auto& pair : loadedMeshes) {
            auto& mesh = pair.second;
            MBGL_CHECK_ERROR(glDeleteBuffers(1, &mesh.vertexBuffer));
            MBGL_CHECK_ERROR(glDeleteBuffers(1, &mesh.indexBuffer));
        }
    }
}
}