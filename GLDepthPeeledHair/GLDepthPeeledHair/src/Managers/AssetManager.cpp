#pragma once
#include "AssetManager.h"
#include "../File/AssimpImporter.h"
#include "../Managers/MeshManager.hpp"
#include "../Hardcoded.hpp"
#include "../Util.hpp"
#include "../Tools/DDSHelpers.h"
#include "../Tools/ImageTools.h"
#include "../API/OpenGL/GL_backend.h"
#include <thread>

namespace AssetManager {

    std::vector<Texture> g_textures;
    std::vector<FlipbookTexture> g_flipbookTextures;
    std::vector<Material> g_materials;
    std::vector<Model> g_models;
    std::unordered_map<std::string, int> g_textureIndexMap;
    std::unordered_map<std::string, int> g_materialIndexMap;
    std::unordered_map<std::string, int> g_modelIndexMap;
    bool g_loadingComplete = false;

    bool FileInfoIsAlbedoTexture(const FileInfo& fileInfo) {
        if (fileInfo.name.size() >= 4 && fileInfo.name.substr(fileInfo.name.size() - 4) == "_ALB") {
            return true;
        }
        return false;
    }

    std::string GetMaterialNameFromFileInfo(const FileInfo& fileInfo) {
        const std::string suffix = "_ALB";
        if (fileInfo.name.size() > suffix.size() && fileInfo.name.substr(fileInfo.name.size() - suffix.size()) == suffix) {
            return fileInfo.name.substr(0, fileInfo.name.size() - suffix.size());
        }
        return "";
    }

    void BuildMaterials() {
        for (Texture& texture : g_textures) {
            if (FileInfoIsAlbedoTexture(texture.GetFileInfo())) {
                Material& material = g_materials.emplace_back(Material());
                material.m_name = GetMaterialNameFromFileInfo(texture.GetFileInfo());
                int basecolorIndex = AssetManager::GetTextureIndexByName(material.m_name + "_ALB", true);
                int normalIndex = AssetManager::GetTextureIndexByName(material.m_name + "_NRM", true);
                int rmaIndex = AssetManager::GetTextureIndexByName(material.m_name + "_RMA", true);
                int sssIndex = AssetManager::GetTextureIndexByName(material.m_name + "_SSS", true);
                material.m_basecolor = (basecolorIndex != -1) ? basecolorIndex : AssetManager::GetTextureIndexByName("Empty_NRMRMA");
                material.m_normal = (normalIndex != -1) ? normalIndex : AssetManager::GetTextureIndexByName("Empty_NRMRMA");
                material.m_rma = (rmaIndex != -1) ? rmaIndex : AssetManager::GetTextureIndexByName("Empty_NRMRMA");
                material.m_sss = (sssIndex != -1) ? sssIndex : AssetManager::GetTextureIndexByName("Black");
            }
        }
        // Build index maps
        for (int i = 0; i < g_materials.size(); i++) {
            g_materialIndexMap[g_materials[i].m_name] = i;
        }
    }

    void LoadCustomFileFormats() {

        // Scan for new obj and fbx
        for (FileInfo& fileInfo : Util::IterateDirectory("res/assets_raw/models/", { "obj", "fbx" })) {
            std::string assetPath = "res/assets/models/" + fileInfo.name + ".model";
            bool exportFile = false;
            if (Util::FileExists(assetPath)) {
                uint64_t lastModified = File::GetLastModifiedTime(fileInfo.path);
                ModelHeader modelHeader = File::ReadModelHeader(assetPath);
                // If the file timestamps don't match, trigger a re-export
                if (modelHeader.timestamp != lastModified) {
                    File::DeleteFile(assetPath);
                    exportFile = true;
                }
            }
            else {
                exportFile = true;
            }
            if (exportFile) {
                ModelData modelData = AssimpImporter::ImportFbx(fileInfo.path);
                File::ExportModel(modelData);
            }
        }

        // Import .model files
        for (FileInfo& fileInfo : Util::IterateDirectory("res/assets/models/")) {
            ModelData modelData = File::ImportModel("res/assets/models/" + fileInfo.GetFileNameWithExtension());
            CreateModelFromData(modelData);
        }
    }

    void AddItemToLoadLog(std::string text) {
        static std::mutex logMutex;
        std::lock_guard<std::mutex> lock(logMutex);
        //std::cout << text << "\n";
    }

    bool LoadingIsComplete() {
        return g_loadingComplete;
    }

#include "../API/OpenGL/GL_util.hpp"

    void LoadNextItem() {
        
        OpenGLBackend::BakeNextAwaitingTexture(g_textures);

        // Are we done yet?
        bool done = true;
        for (Texture& texture : g_textures) {
            if (texture.GetLoadingState() == LoadingState::LOADING_COMPLETE &&
                texture.GetBakingState() != BakingState::BAKE_COMPLETE) {
                done = false;
            }
        }
        if (done) {
            std::cout << "All baking complete\n";
            g_loadingComplete = true;
        }
    }

    void LoadFont() {
        // Find files
        for (FileInfo& fileInfo : Util::IterateDirectory("res/textures/font", { "png", "jpg", })) {
            Texture& texture = g_textures.emplace_back();
            texture.SetFileInfo(fileInfo);
            texture.SetImageDataType(ImageDataType::UNCOMPRESSED);
            texture.SetMipmapState(MipmapState::NO_MIPMAPS_REQUIRED);
        }
        // Async load them all
        std::vector<std::future<void>> futures;
        for (Texture& texture : g_textures) {
            if (texture.GetLoadingState() == LoadingState::AWAITING_LOADING_FROM_DISK) {
                texture.SetLoadingState(LoadingState::LOADING_FROM_DISK);
                futures.emplace_back(std::async(std::launch::async, LoadTexture, &texture));
            }
        }
        for (auto& future : futures) {
            future.get();
        }
        // Immediate bake them
        for (Texture& texture : g_textures) {
            texture.GetGLTexture().PreAllocate();
            OpenGLBackend::ImmediateBake(texture);
        }
        // Update index map
        for (int i = 0; i < g_textures.size(); i++) {
            g_textureIndexMap[g_textures[i].GetFileName()] = i;
        }
    }

    void LoadFlipbookTextures() {
        FlipbookTexture& flipbookTexture0 = g_flipbookTextures.emplace_back();
        flipbookTexture0.Load("res/textures/flipbook/WaterSplash0_Color_4x4.tga");

        FlipbookTexture& flipbookTexture1 = g_flipbookTextures.emplace_back();
        flipbookTexture1.Load("res/textures/flipbook/WaterSplash1_Color_4x4.tga");

        FlipbookTexture& flipbookTexture2 = g_flipbookTextures.emplace_back();
        flipbookTexture2.Load("res/textures/flipbook/WaterSplash2_Color_4x4.tga");
    }

    void LoadTextures() {

        // Generate mip maps
        //for (FileInfo& fileInfo : Util::IterateDirectory("res/textures/compress_me", { "png", "jpg", "tga" })) {
        //    if (fileInfo.name != "MermaidTail_ALB" && fileInfo.name != "MermaidTail_NRM" && fileInfo.name != "MermaidTail_RMA") {
        //        continue;
        //    }
        //    stbi_set_flip_vertically_on_load(false);
        //    TextureData textureData;
        //    textureData.m_data = stbi_load(fileInfo.path.data(), &textureData.m_width, &textureData.m_height, &textureData.m_numChannels, 0);
        //    std::string compressedPath = "res/textures/compressed/" + fileInfo.name + ".dds";
        //    ImageTools::CompresssDXT3WithMipmaps(compressedPath.c_str(), static_cast<unsigned char*>(textureData.m_data), textureData.m_width, textureData.m_height, textureData.m_numChannels);
        //    stbi_image_free(textureData.m_data);
        //}

        // Find file paths
        for (FileInfo& fileInfo : Util::IterateDirectory("res/textures/uncompressed", { "png", "jpg", "tga" })) {
            Texture& texture = g_textures.emplace_back();
            texture.SetFileInfo(fileInfo);
            texture.SetImageDataType(ImageDataType::UNCOMPRESSED);
            texture.SetMipmapState(MipmapState::AWAITING_MIPMAP_GENERATION);
        }
        for (FileInfo& fileInfo : Util::IterateDirectory("res/textures/ui", { "png", "jpg", "tga" })) {
            Texture& texture = g_textures.emplace_back();
            texture.SetFileInfo(fileInfo);
            texture.SetImageDataType(ImageDataType::UNCOMPRESSED);
            texture.SetMipmapState(MipmapState::NO_MIPMAPS_REQUIRED);
        }
        for (FileInfo& fileInfo : Util::IterateDirectory("res/textures/compressed", { "dds" })) {
            Texture& texture = g_textures.emplace_back();
            texture.SetFileInfo(fileInfo);
            texture.SetImageDataType(ImageDataType::COMPRESSED);
            texture.SetMipmapState(MipmapState::AWAITING_MIPMAP_GENERATION);
        }
        for (FileInfo& fileInfo : Util::IterateDirectory("res/textures/exr", { "exr" })) {
            Texture& texture = g_textures.emplace_back();
            texture.SetFileInfo(fileInfo);
            texture.SetImageDataType(ImageDataType::EXR);
            texture.SetMipmapState(MipmapState::NO_MIPMAPS_REQUIRED);
        }
        // Build index maps
        for (int i = 0; i < g_textures.size(); i++) {
            g_textureIndexMap[g_textures[i].GetFileInfo().name] = i;
        }

        // Async load them all
        std::vector<std::future<void>> futures;   
        for (Texture& texture : g_textures) {
            if (texture.GetLoadingState() == LoadingState::AWAITING_LOADING_FROM_DISK) {
                texture.SetLoadingState(LoadingState::LOADING_FROM_DISK);
                futures.emplace_back(std::async(std::launch::async, LoadTexture, &texture));
                AddItemToLoadLog("Loaded " + texture.GetFilePath());
            }
        }
        for (auto& future : futures) {
            future.get();
        }
        futures.clear();

        // Preallocate space on the gpu
        for (Texture& texture : g_textures) {
            texture.GetGLTexture().PreAllocate();
        }

        // Immediate bake exr and UI
        for (Texture& texture : g_textures) {
            if (texture.GetImageDataType() == ImageDataType::EXR) {
                //OpenGLBackend::ImmediateBake(texture);
            }
        }
    }
       
    void Init() {
        LoadCustomFileFormats();
        Hardcoded::LoadHardcodedRoomModel();
        Hardcoded::LoadHardcodedWaterModel();
        Hardcoded::LoadQuadModel();
        LoadFont();
        LoadTextures();
        BuildMaterials();
        for (int i = 0; i < g_models.size(); i++) {
            g_modelIndexMap[g_models[i].GetName()] = i;
        }
        LoadFlipbookTextures();
    }

    void LoadTexture(Texture* texture) {
        texture->Load();
    }

    Texture* GetTextureByName(const std::string& name) {
        for (Texture& texture : g_textures) {
            if (texture.GetFileInfo().name == name)
                return &texture;
        }
        std::cout << "AssetManager::GetTextureByName() failed because '" << name << "' does not exist\n";
        return nullptr;
    }

    int GetTextureIndexByName(const std::string& name, bool ignoreWarning) {
        for (int i = 0; i < g_textures.size(); i++) {
            if (g_textures[i].GetFileInfo().name == name)
                return i;
        }
        if (!ignoreWarning) {
            std::cout << "AssetManager::GetTextureIndexByName() failed because '" << name << "' does not exist\n";
        }
        return -1;
    }

    Texture* GetTextureByIndex(int index) {
        if (index != -1) {
            return &g_textures[index];
        }
        std::cout << "AssetManager::GetTextureByIndex() failed because index was -1\n";
        return nullptr;
    }

    int GetTextureCount() {
        return g_textures.size();
    }

    Model* GetModelByIndex(int index) {
        if (index != -1) {
            return &g_models[index];
        }
        std::cout << "AssetManager::GetModelByIndex() failed because index was -1\n";
        return nullptr;
    }

    Model* CreateModel(const std::string& name) {
        g_models.emplace_back();
        Model* model = &g_models[g_models.size() - 1];
        model->SetName(name);
        return model;
    }

    void CreateModelFromData(ModelData& modelData) {
        Model& model = g_models.emplace_back();
        model.SetName(modelData.name);
        model.SetAABB(modelData.aabbMin, modelData.aabbMax);
        for (MeshData& meshData : modelData.meshes) {
            int meshIndex = MeshManager::CreateMesh(meshData.name, meshData.vertices, meshData.indices, meshData.aabbMin, meshData.aabbMax);
            model.AddMeshIndex(meshIndex);
        }
    }

    int GetModelIndexByName(const std::string& name) {
        auto it = g_modelIndexMap.find(name);
        if (it != g_modelIndexMap.end()) {
            return it->second;
        }
        std::cout << "AssetManager::GetModelIndexByName() failed because name '" << name << "' was not found in _models!\n";
        return -1;
    }

    Material* GetMaterialByIndex(int index) {
        if (index >= 0 && index < g_materials.size()) {
            return &g_materials[index];
        }
        std::cout << "AssetManager::GetMaterialByIndex() failed because index '" << index << "' is out of range. Size is " << g_materials.size() << "!\n";
        return nullptr;
    }


    int GetMaterialIndex(const std::string& name) {
        auto it = g_materialIndexMap.find(name);
        if (it != g_materialIndexMap.end()) {
            return it->second;
        }
        else {
            std::cout << "AssetManager::GetMaterialIndex() failed because '" << name << "' does not exist\n";
            return -1;
        }
    }

    std::string& GetMaterialNameByIndex(int index) {
        return g_materials[index].m_name;
    }

    FlipbookTexture* GetFlipbookTextureByIndex(int index) {
        if (index >= 0 && index < g_flipbookTextures.size()) {
            return &g_flipbookTextures[index];
        }
        else {
            std::cout << "AssetManager::GetFlipbookTextureByIndex() failed because '" << index << "' was out of range of size not g_flipbookTextures.size()\n";
            return nullptr;
        }
    }

    FlipbookTexture* GetFlipbookByName(const std::string& name) {
        for (FlipbookTexture& flipbookTexture : g_flipbookTextures) {
            if (flipbookTexture.m_name == name) {
                return &flipbookTexture;
            }
        }
        std::cout << "AssetManager::GetFlipbookByName() failed because '" << name << "' was not found\n";
        return nullptr;
    }
}




    



    

