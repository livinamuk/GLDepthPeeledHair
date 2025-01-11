#include "GameObject.h"
#include "Managers/AssetManager.h"
#include "Managers/MeshManager.hpp"

void GameObject::SetPosition(glm::vec3 position) {
    m_transform.position = position;
}

void GameObject::SetRotationY(float rotation) {
    m_transform.rotation.y = rotation;
}

void GameObject::SetModel(const std::string& name) {
    m_model = AssetManager::GetModelByIndex(AssetManager::GetModelIndexByName(name.c_str()));
    if (m_model) {
        m_meshMaterialIndices.resize(m_model->GetMeshCount());
        m_meshBlendingModes.resize(m_model->GetMeshCount());
    }
    else {
        std::cout << "Failed to set model '" << name << "', it does not exist.\n";
    }
    for (BlendingMode& blendingMode : m_meshBlendingModes) {
        blendingMode = BlendingMode::NONE;
    }
}

void GameObject::SetMeshMaterialByMeshName(std::string meshName, const char* materialName) {
    int materialIndex = AssetManager::GetMaterialIndex(materialName);
    if (m_model && materialIndex != -1) {
        for (int i = 0; i < m_model->GetMeshCount(); i++) {

            if (MeshManager::GetMeshByIndex(m_model->GetMeshIndices()[i])->GetName() == meshName) {
                //if (model->GetMeshNameByIndex(i) == meshName) {
                m_meshMaterialIndices[i] = materialIndex;
                return;
            }
        }
    }
    if (!m_model) {
        std::cout << "Tried to call SetMeshMaterialByMeshName() but this GameObject has a nullptr m_model\n";
    }
    else if (materialIndex == -1) {
        //std::cout << "Tried to call SetMeshMaterialByMeshName() but the material index was -1\n";
    }
    else {
        std::cout << "Tried to call SetMeshMaterialByMeshName() but the meshName '" << meshName << "' not found\n";
    }
}

void GameObject::SetMeshBlendingMode(const char* meshName, BlendingMode blendingMode) {
    // Range checks
    if (m_meshBlendingModes.empty()) {
        std::cout << "GameObject::SetMeshBlendingMode() failed: m_meshBlendingModes was empty!\n";
        return;
    }
    if (m_model) {
        bool found = false;
        for (int i = 0; i < m_model->GetMeshIndices().size(); i++) {
            OpenGLDetachedMesh* mesh = MeshManager::GetMeshByIndex(m_model->GetMeshIndices()[i]);
            if (mesh && mesh->GetName() == meshName) {
                m_meshBlendingModes[i] = blendingMode;
                found = true;
            }
        }
        if (!found) {
            std::cout << "GameObject::SetMeshBlendingMode() failed: " << meshName << " was not found!\n";
        }
    }
}

void GameObject::SetMeshBlendingModes(BlendingMode blendingMode) {
    if (m_model) {
        for (int i = 0; i < m_model->GetMeshIndices().size(); i++) {
            OpenGLDetachedMesh* mesh = MeshManager::GetMeshByIndex(m_model->GetMeshIndices()[i]);
            if (mesh) {
                m_meshBlendingModes[i] = blendingMode;
            }
        }
    }
}

void GameObject::SetName(const std::string& name) {
    m_name = name;
}

void GameObject::PrintMeshNames() {
    if (m_model) {
        std::cout << m_model->GetName() << "\n";
        for (uint32_t meshIndex : m_model->GetMeshIndices()) {
            OpenGLDetachedMesh* mesh = MeshManager::GetMeshByIndex(meshIndex);
            if (mesh) {
                std::cout << "-" << meshIndex << ": " << mesh->GetName() << "\n";
            }
        }
    }
}