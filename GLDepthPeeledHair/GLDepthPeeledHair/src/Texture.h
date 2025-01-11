#pragma once
#include <Compressonator.h>
#include <string>
#include <memory>
#include "../API/OpenGL/Types/GL_texture.h"
//#include "../../API/Vulkan/Types/VK_texture.h"
#include "../Common/Enums.h"

struct Texture {
public:
    Texture() = default;
    void Load();
    //void Bake();
    //void BakeCMPData(CMP_Texture* cmpTexture);
    void SetLoadingState(LoadingState loadingState);
    void SetBakingState(BakingState state);
    void SetMipmapState(MipmapState state);
    void SetFileInfo(FileInfo fileInfo);
    void SetImageDataType(ImageDataType imageDataType);
    const int GetWidth();
    const int GetHeight();
    const std::string& GetFileName();
    const std::string& GetFilePath();
    const LoadingState GetLoadingState();
    const BakingState GetBakingState();
    const MipmapState GetMipmapState();
    const FileInfo GetFileInfo();
    const ImageDataType GetImageDataType();
    OpenGLTexture& GetGLTexture();
    //VulkanTexture& GetVKTexture();

private:
    OpenGLTexture m_glTexture;
    //VulkanTexture vkTexture;
    int m_width = 0;
    int m_height = 0;
    int m_channelCount = 0;
    LoadingState m_loadingState = LoadingState::AWAITING_LOADING_FROM_DISK;
    BakingState m_bakingState = BakingState::AWAITING_BAKE;
    MipmapState m_mipmapState = MipmapState::NO_MIPMAPS_REQUIRED;
    ImageDataType m_imageDataType = ImageDataType::UNDEFINED;
    FileInfo m_fileInfo;
};
