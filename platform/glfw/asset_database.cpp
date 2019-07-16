#include "asset_database.hpp"
#include "mesh_loader.hpp"

#include <mbgl/storage/local_file_source.hpp>
#include <mbgl/storage/online_file_source.hpp>

#include <map>

using namespace std;

namespace mbgl {

// Private implementation

struct MeshContainer {
    bool loading;
    unique_ptr<Mesh> mesh;
    AssetDatabase::Descriptor descriptor;
};

class AssetDatabaseImpl {
public:
    AssetDatabaseImpl() {
        meshLoader = MeshLoader::createObjLoader();

        meshLoader->onMeshLoaded([this](int result, const string& url, unique_ptr<Mesh>&& mesh) {
            if (result != MeshLoader::Status_ok)
                return;
            auto it = meshes.find(url);
            assert(it != meshes.end());
            it->second.mesh = std::move(mesh);
            it->second.loading = false;
        });
    }

    void addMeshSource(const AssetDatabase::Descriptor& desc) {
        meshes.insert({ desc.uri, MeshContainer { false, nullptr, desc }});
    }

    const Mesh* getMesh(const std::string& url) const {
        auto it = meshes.find(url);
        return it != meshes.end() ? it->second.mesh.get() : nullptr;
    }

    void queryMeshes(std::vector<AssetDatabase::Descriptor>& outDescriptors) {
        for (auto& pair : meshes) {
            auto& container = pair.second;
            if (container.mesh) {
                outDescriptors.push_back(container.descriptor);
            }
            else if (!container.loading) {
                // Mesh found but not loaded yet
                FileSource& source = container.descriptor.source == AssetDatabase::Source_file
                    ? (FileSource&)localFileSource
                    : (FileSource&)onlineFileSource;
                meshLoader->loadMesh(container.descriptor.uri, source);
                container.loading = true;
            }
        }
    }

private:
    LocalFileSource localFileSource;
    OnlineFileSource onlineFileSource;
    map<string, MeshContainer> meshes;
    unique_ptr<MeshLoader> meshLoader;
};


// Public interface

AssetDatabase::AssetDatabase() {
    impl = new AssetDatabaseImpl();
}
AssetDatabase::~AssetDatabase() {
    delete impl;
}

AssetDatabase* AssetDatabase::addMeshSource(const Descriptor& desc) {
    impl->addMeshSource(desc);
    return this;
}

const Mesh* AssetDatabase::getMesh(const std::string& url) const {
    return impl->getMesh(url);
}

void AssetDatabase::queryMeshes(std::vector<Descriptor>& outDescriptors) {
    impl->queryMeshes(outDescriptors);
}
}