#pragma once
#include <vector>
#include "Types.h"
#include "Managers/AssetManager.h"
#include "Managers/MeshManager.hpp"
#include "GameObject.h"
#include "Input.h"
#include "Util.hpp"

namespace Scene {

    inline std::vector<GameObject> g_gameObjects;

    inline void Init() {
        // nothing as of yet
    }

    inline void Update(float deltaTime) {
        // nothing as of yet
    }

    inline void CreateGameObject() {
        g_gameObjects.emplace_back();
    }

    inline GameObject* GetGameObjectByName(const std::string& name) {
        for (GameObject& gameObject : g_gameObjects) {
            if (gameObject.m_name == name) {
                return &gameObject;
            }
        }
        return nullptr;
    }

    inline void SetMaterials() {
        GameObject* room = GetGameObjectByName("Room");
        if (room) {
            room->SetMeshMaterialByMeshName("PlatformSide", "BathroomFloor");
            room->SetMeshMaterialByMeshName("PlatformTop", "BathroomFloor");
            room->SetMeshMaterialByMeshName("Floor", "BathroomFloor");
            room->SetMeshMaterialByMeshName("Ceiling", "Ceiling2");
            room->SetMeshMaterialByMeshName("WallZPos", "BathroomWall");
            room->SetMeshMaterialByMeshName("WallZNeg", "BathroomWall");
            room->SetMeshMaterialByMeshName("WallXPos", "BathroomWall");
            room->SetMeshMaterialByMeshName("WallXNeg", "BathroomWall");
        }

        GameObject* mermaid = GetGameObjectByName("Mermaid");
        if (mermaid) {
            mermaid->SetMeshMaterialByMeshName("Rock", "Rock");
            mermaid->SetMeshMaterialByMeshName("BoobTube", "BoobTube");
            mermaid->SetMeshMaterialByMeshName("Face", "MermaidFace");
            mermaid->SetMeshMaterialByMeshName("Body", "MermaidBody");
            mermaid->SetMeshMaterialByMeshName("Arms", "MermaidArms");
            mermaid->SetMeshMaterialByMeshName("HairInner", "MermaidHair");
            mermaid->SetMeshMaterialByMeshName("HairOutta", "MermaidHair");
            mermaid->SetMeshMaterialByMeshName("HairScalp", "MermaidScalp");
            mermaid->SetMeshMaterialByMeshName("EyeLeft", "MermaidEye");
            mermaid->SetMeshMaterialByMeshName("EyeRight", "MermaidEye");
            mermaid->SetMeshMaterialByMeshName("Tail", "MermaidTail");
            mermaid->SetMeshMaterialByMeshName("TailFin", "MermaidTail");
            mermaid->SetMeshMaterialByMeshName("EyelashUpper_HP", "MermaidLashes");
            mermaid->SetMeshMaterialByMeshName("EyelashLower_HP", "MermaidLashes");
            mermaid->SetMeshMaterialByMeshName("Nails", "Nails");
        }
    }

    inline void CreateGameObjects() {
        //CreateGameObject();
        //GameObject* room = &g_gameObjects[g_gameObjects.size() - 1];
        //room->SetModel("Room");
        //room->SetMeshMaterialByMeshName("PlatformSide", "BathroomFloor");
        //room->SetMeshMaterialByMeshName("PlatformTop", "BathroomFloor");
        //room->SetMeshMaterialByMeshName("Floor", "BathroomFloor");
        //room->SetMeshMaterialByMeshName("Ceiling", "Ceiling2");
        //room->SetMeshMaterialByMeshName("WallZPos", "BathroomWall");
        //room->SetMeshMaterialByMeshName("WallZNeg", "BathroomWall");
        //room->SetMeshMaterialByMeshName("WallXPos", "BathroomWall");
        //room->SetMeshMaterialByMeshName("WallXNeg", "BathroomWall");
        
        CreateGameObject();
        GameObject* mermaid = &g_gameObjects[g_gameObjects.size() - 1];
        mermaid->SetPosition(glm::vec3(3.5, -1.0f, 7.5f));
        mermaid->SetRotationY(3.14f * 1.7f);
        mermaid->SetModel("Mermaid");
        mermaid->SetMeshMaterialByMeshName("Rock", "Rock");
        mermaid->SetMeshMaterialByMeshName("BoobTube", "BoobTube");
        mermaid->SetMeshMaterialByMeshName("Face", "MermaidFace");
        mermaid->SetMeshMaterialByMeshName("Body", "MermaidBody");
        mermaid->SetMeshMaterialByMeshName("Arms", "MermaidArms");
        mermaid->SetMeshMaterialByMeshName("HairInner", "MermaidHair");
        mermaid->SetMeshMaterialByMeshName("HairOutta", "MermaidHair");
        mermaid->SetMeshMaterialByMeshName("HairScalp", "MermaidScalp");
        mermaid->SetMeshMaterialByMeshName("EyeLeft", "MermaidEye");
        mermaid->SetMeshMaterialByMeshName("EyeRight", "MermaidEye");
        mermaid->SetMeshMaterialByMeshName("Tail", "MermaidTail");
        mermaid->SetMeshMaterialByMeshName("TailFin", "MermaidTail");
        mermaid->SetMeshMaterialByMeshName("EyelashUpper_HP", "MermaidLashes");
        mermaid->SetMeshMaterialByMeshName("EyelashLower_HP", "MermaidLashes");
        mermaid->SetMeshMaterialByMeshName("Nails", "Nails");
        mermaid->SetMeshBlendingMode("EyelashUpper_HP", BlendingMode::BLENDED);
        mermaid->SetMeshBlendingMode("EyelashLower_HP", BlendingMode::BLENDED);
        mermaid->SetMeshBlendingMode("HairScalp", BlendingMode::BLENDED);
        mermaid->SetMeshBlendingMode("HairOutta", BlendingMode::HAIR_TOP_LAYER);
        mermaid->SetMeshBlendingMode("HairInner", BlendingMode::HAIR_UNDER_LAYER);
        mermaid->SetName("Mermaid");
    }
}