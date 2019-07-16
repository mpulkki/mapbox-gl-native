#pragma once

#include "mesh_loader.hpp"
#include <mbgl/util/geo.hpp>
#include <mbgl/util/mat4.hpp>
#include <memory>

namespace mbgl {

class AssetDatabase {
public:
    enum Source {
        Source_file,
        Source_web
    };

    struct Descriptor {
        LatLng position;
        mat4 localTransform;
        Source source;
        std::string uri;

        inline static Descriptor FromFile(const LatLng& position, const std::string& uri, const mat4& transform) {
            return { position, transform, Source_file, uri };
        }

        inline static Descriptor FromWeb(const LatLng& position, const std::string& uri, const mat4& transform) {
            return { position, transform, Source_web, uri };
        }
    };

public:
    AssetDatabase();
    ~AssetDatabase();

    // Registers a new mesh to the database
    AssetDatabase* addMeshSource(const Descriptor& desc);

    const Mesh* getMesh(const std::string& url) const;

    void queryMeshes(std::vector<Descriptor>& outDescriptors);

private:
    class AssetDatabaseImpl* impl;
};

}