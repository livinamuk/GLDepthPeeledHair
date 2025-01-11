#pragma once
#include "../Common/Types.h"
#include "../File/File.h"
#include "../Model.hpp"
#include "../Texture.h"
#include <string>

namespace AssetManager {
    void Init();

    bool LoadingIsComplete();
    void LoadNextItem();

    void LoadTexture(Texture* texture);
    Texture* GetTextureByName(const std::string& name);
    int GetTextureIndexByName(const std::string& name, bool ignoreWarning = true);
    Texture* GetTextureByIndex(int index);
    int GetTextureCount();

    void CreateModelFromData(ModelData& modelData);
    int GetModelIndexByName(const std::string& name);
    Model* CreateModel(const std::string& name);
    Model* GetModelByIndex(int index);

    Material* GetMaterialByIndex(int index);
    int GetMaterialIndex(const std::string& name);
    std::string& GetMaterialNameByIndex(int index);
}