#include "mesh_loader.hpp"

#include <mbgl/storage/file_source.hpp>

#include <sstream>
#include <iostream>
#include <map>

namespace mbgl {

using MeshLoadCallback = std::function<void(int, const std::string&, std::unique_ptr<Mesh>&&)>;

// Meshloader implementation for obj-files
class ObjMeshLoader : public MeshLoader {
public:
    void loadMesh(const std::string& url, FileSource& fileSource) override;
    void onMeshLoaded(MeshLoadCallback callback) override;

private:
    MeshLoadCallback callback;
    std::map<int, std::unique_ptr<AsyncRequest>> requests;
    int idCounter = 0;
};

struct ParseResult {
    ParseResult()
        : status(MeshLoader::Status_ok)
        , positionCount(0)
        , normalCount(0)
        , triangleCount(0)
    { }

    ParseResult& Status(int value) {
        status = value;
        return *this;
    }

    int status;
    int positionCount;
    int normalCount;
    int triangleCount;
};

static ParseResult parseObj(const std::string& data, Mesh& outMesh) {
    ParseResult res;
    if (!data.length()) 
        return res.Status(MeshLoader::Status_invalidData);

    std::istringstream dataStream(data);
    std::string line;

    while(std::getline(dataStream, line)) {
        // Skip empty and comment lines
        if (!line.length() || line[0] == '#')
            continue;
        // Only vertex and face entries supported
        std::istringstream lineStream(line);
        char lineType;
        lineStream >> lineType;
        if (lineStream.fail())
            return res.Status(MeshLoader::Status_invalidData);

        if (lineType == 'v') {
            VertexPN v;
            lineStream >> v.position.x >> v.position.y >> v.position.z;
            v.position.w = 1.0f;
            outMesh.vertices.push_back(v);
            res.positionCount++;
        }
        else if (lineType == 'f') {
            Tri3i tri;
            lineStream >> tri.i >> tri.j >> tri.k;
            // face elements start at 1
            tri.i -=1 ; tri.j -= 1; tri.k -= 1;
            outMesh.triangles.push_back(tri);
            res.triangleCount++;
        }
        if (lineStream.fail())
            return res.Status(MeshLoader::Status_invalidData);
    }

    return res;
}

static Bounds3f computeBounds(Mesh& mesh) {
    Bounds3f bounds;
    if (!mesh.vertices.size())
        return bounds;
    bounds.min = toVec3f(mesh.vertices[0].position);
    bounds.max = toVec3f(mesh.vertices[0].position);
    for (size_t i = 1; i < mesh.vertices.size(); i++)
        bounds.inflate(toVec3f(mesh.vertices[i].position));
    return bounds;
}

static void computeMeshNormals(Mesh& mesh) {
    struct WeightedNormal {
        Vec3f normal = { 0, 0, 0 };
        int count = 0;
    };

    std::vector<WeightedNormal> normals(mesh.vertices.size());
    for (size_t i = 0; i < mesh.triangles.size(); i++) {
        const auto& tri = mesh.triangles[i];
        Vec3f v0 = toVec3f(mesh.vertices[tri.i].position);
        Vec3f v1 = toVec3f(mesh.vertices[tri.j].position);
        Vec3f v2 = toVec3f(mesh.vertices[tri.k].position);

        // Note: triangle winding should be taken into account
        Vec3f n = cross(v1 - v0, v2 - v0);

        normals[tri.i].normal = normals[tri.i].normal + n;
        normals[tri.i].count++;
        normals[tri.j].normal = normals[tri.j].normal + n;
        normals[tri.j].count++;
        normals[tri.k].normal = normals[tri.k].normal + n;
        normals[tri.k].count++;
    }
    for (size_t i = 0; i < mesh.vertices.size(); i++) {
        mesh.vertices[i].normal = toVec4f(norm(normals[i].normal / normals[i].count));
    }
}

static int toMeshError(const Response::Error& err)
{
    switch (err.reason)
    {
    case Response::Error::Reason::Success:
        return MeshLoader::Status_ok;
    case Response::Error::Reason::NotFound:
        return MeshLoader::Status_notFound;
    default:
        return MeshLoader::Status_unknownError;
    }
}

void ObjMeshLoader::loadMesh(const std::string& url, FileSource& fileSource) {
    // Don't load anything if result is not captured
    if (!callback)
        return;
    if (url.empty()) {
        callback(Status_ok, url, nullptr);
        return;
    }

    // Store the request into a map using incremental id until the request has completed
    int reqId = idCounter++;

    auto meshRequest = fileSource.request(Resource(Resource::Unknown, url), [this, url, reqId](Response res) {
        if (res.error) {
            callback(toMeshError(*res.error.get()), url, nullptr);
        }
        else if (res.notModified) {
            // TODO: handle correctly
            callback(MeshLoader::Status_unknownError, url, nullptr);
        }
        else {
            std::unique_ptr<Mesh> mesh = std::make_unique<Mesh>();
            auto parseResult = parseObj(*res.data.get(), *mesh.get());
            if (parseResult.status == Status_ok) {
                mesh->bounds = computeBounds(*mesh.get());
                if (parseResult.positionCount != parseResult.normalCount)
                    computeMeshNormals(*mesh.get());
            }

            callback(parseResult.status, url, std::move(mesh));
        }

        auto it = requests.find(reqId);
        assert(it != requests.end());
        requests.erase(it);
    });

    requests.insert({ reqId, std::move(meshRequest)});
}

void ObjMeshLoader::onMeshLoaded(MeshLoadCallback func) {
    this->callback = func;
}


std::unique_ptr<MeshLoader> MeshLoader::createObjLoader() {
    return std::make_unique<ObjMeshLoader>();
}
}