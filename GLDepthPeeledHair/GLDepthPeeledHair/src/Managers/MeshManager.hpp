#pragma once
#include "../API/OpenGL/Types/GL_detachedMesh.hpp"
#include "../Util.hpp"

namespace MeshManager {

    inline std::vector<OpenGLDetachedMesh> gMeshes;

    inline int CreateMesh(const std::string& name, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, glm::vec3 aabbMin, glm::vec3 aabbMax) {
        OpenGLDetachedMesh& mesh = gMeshes.emplace_back();
        mesh.SetName(name);
        mesh.UpdateVertexBuffer(vertices, indices);
        mesh.aabbMin = aabbMin;
        mesh.aabbMax = aabbMax;
        return gMeshes.size() - 1;
    }

    inline int CreateMesh(const std::string& name, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
        glm::vec3 aabbMin = glm::vec3(std::numeric_limits<float>::max());
        glm::vec3 aabbMax = glm::vec3(-std::numeric_limits<float>::max());
        for (int i = 0; i < indices.size(); i += 3) {
            Vertex* vert0 = &vertices[indices[i]];
            Vertex* vert1 = &vertices[indices[i + 1]];
            Vertex* vert2 = &vertices[indices[i + 2]];
            glm::vec3 deltaPos1 = vert1->position - vert0->position;
            glm::vec3 deltaPos2 = vert2->position - vert0->position;
            glm::vec2 deltaUV1 = vert1->uv - vert0->uv;
            glm::vec2 deltaUV2 = vert2->uv - vert0->uv;
            float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
            glm::vec3 tangent = (deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * r;
            glm::vec3 bitangent = (deltaPos2 * deltaUV1.x - deltaPos1 * deltaUV2.x) * r;
            vert0->tangent = tangent;
            vert1->tangent = tangent;
            vert2->tangent = tangent;
            aabbMin = Util::Vec3Min(vert0->position, aabbMin);
            aabbMax = Util::Vec3Max(vert0->position, aabbMax);
            aabbMin = Util::Vec3Min(vert1->position, aabbMin);
            aabbMax = Util::Vec3Max(vert1->position, aabbMax);
            aabbMin = Util::Vec3Min(vert2->position, aabbMin);
            aabbMax = Util::Vec3Max(vert2->position, aabbMax);
        }
        OpenGLDetachedMesh& mesh = gMeshes.emplace_back();
        mesh.SetName(name);
        mesh.UpdateVertexBuffer(vertices, indices);
        mesh.aabbMin = aabbMin;
        mesh.aabbMax = aabbMax;
        return gMeshes.size() - 1;
    }
    
    inline int GetMeshIndexByName(const std::string& name) {
        for (int i = 0; i < gMeshes.size(); i++) {
            if (gMeshes[i].GetName() == name)
                return i;
        }
        std::cout << "AssetManager::GetMeshIndexByName() failed because '" << name << "' does not exist\n";
        return -1;
    }

    inline OpenGLDetachedMesh* GetDetachedMeshByName(const std::string& name) {
        for (int i = 0; i < gMeshes.size(); i++) {
            if (gMeshes[i].GetName() == name)
                return &gMeshes[i];
        }
        std::cout << "AssetManager::GetDetachedMeshByName() failed because '" << name << "' does not exist\n";
        return nullptr;
    }

    inline OpenGLDetachedMesh* GetMeshByIndex(int index) {
        if (index >= 0 && index < gMeshes.size()) {
            return &gMeshes[index];
        }
        else {
            std::cout << "AssetManager::GetMeshByIndex() failed because index '" << index << "' is out of range. Size is " << gMeshes.size() << "!\n";
            return nullptr;
        }
    }

}