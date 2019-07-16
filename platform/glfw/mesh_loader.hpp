#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <cmath>
#include "vectors.hpp"

namespace mbgl {

class FileSource;

struct VertexPN {
    Vec4f position;
    Vec4f normal;
};

struct Tri3i { int i; int j; int k; };

struct Mesh {
    Bounds3f bounds;
    std::vector<VertexPN> vertices;
    std::vector<Tri3i> triangles;
};

class MeshLoader {
public:
    enum
    {
        Status_ok,
        Status_notFound,
        Status_invalidData,
        Status_unknownError
    };

    virtual ~MeshLoader() = default;

    virtual void loadMesh(const std::string& url, FileSource& fileSource) = 0;
    virtual void onMeshLoaded(std::function<void(int, const std::string&, std::unique_ptr<Mesh>&&)> callback) = 0;

    static std::unique_ptr<MeshLoader> createObjLoader();
};

}