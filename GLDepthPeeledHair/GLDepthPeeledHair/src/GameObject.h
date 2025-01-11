#pragma once
#include "Model.hpp"

struct GameObject {
    std::string m_name;
    Model* m_model = nullptr;
    Transform m_transform;
    std::vector<BlendingMode> m_meshBlendingModes;
    std::vector<int> m_meshMaterialIndices;

    void SetName(const std::string& name);
    void SetPosition(glm::vec3 position);
    void SetRotationY(float rotation);
    void SetModel(const std::string& name);
    void SetMeshMaterialByMeshName(std::string meshName, const char* materialName);
    void SetMeshBlendingMode(const char* meshName, BlendingMode blendingMode);
    void SetMeshBlendingModes(BlendingMode blendingMode);
    void PrintMeshNames();
};