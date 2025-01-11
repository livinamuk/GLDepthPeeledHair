#include "Texture.h"
#include "../Util.hpp"

void Texture::Load() {
    m_glTexture.Load(m_fileInfo, m_imageDataType);
    m_width = m_glTexture.GetWidth();
    m_height = m_glTexture.GetHeight();
    m_channelCount = m_glTexture.GetChannelCount();
    m_loadingState = LoadingState::LOADING_COMPLETE;
}

//void Texture::Bake() {
//    if (m_bakingState == BakingState::AWAITING_BAKE) {
//        m_glTexture.Bake();
//    }
//    m_bakingState = BakingState::BAKE_COMPLETE;
//}

//void Texture::BakeCMPData(CMP_Texture* cmpTexture) {
//    if (m_bakingState == BakingState::AWAITING_BAKE) {
//        m_glTexture.BakeCMPData(cmpTexture);
//    }
//    m_bakingState = BakingState::BAKE_COMPLETE;
//}

const int Texture::GetWidth() {
    return m_width;
}

const int Texture::GetHeight() {
    return m_height;
}

const std::string& Texture::GetFileName() {
    return m_fileInfo.name;
}

const std::string& Texture::GetFilePath() {
    return m_fileInfo.path;
}

OpenGLTexture& Texture::GetGLTexture() {
    return m_glTexture;
}

void Texture::SetLoadingState(LoadingState state) {
    m_loadingState = state;
}

void Texture::SetBakingState(BakingState state) {
    m_bakingState = state;
}

void Texture::SetMipmapState(MipmapState state) {
    m_mipmapState = state;
}

void Texture::SetFileInfo(FileInfo fileInfo) {
    m_fileInfo = fileInfo;
}

void Texture::SetImageDataType(ImageDataType imageDataType) {
    m_imageDataType = imageDataType;
}

const LoadingState Texture::GetLoadingState() {
    return m_loadingState;
}

const BakingState Texture::GetBakingState() {
    return m_bakingState;
}

const MipmapState Texture::GetMipmapState() {
    return m_mipmapState;
}

const FileInfo Texture::GetFileInfo() {
    return m_fileInfo;
}

const ImageDataType Texture::GetImageDataType() {
    return m_imageDataType;
}