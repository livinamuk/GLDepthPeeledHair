#pragma once
#include "../Common/Types.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <Compressonator.h>
#include <string>
#include <memory>

struct OpenGLTexture {
public:
    OpenGLTexture() = default;
    GLuint GetHandle();
    GLuint64 GetBindlessID();
    void Bind(unsigned int slot);
    bool Load(const FileInfo& fileInfo, ImageDataType imageDataType);
    void AllocateEmptyMipmaps();
    void PreAllocate();
    void MakeBindlessTextureResident();
    void MakeBindlessTextureNonResident();
    int GetWidth();
    int GetHeight();
    int GetChannelCount();
    int GetDataSize();
    void* GetData();
    GLint GetFormat();
    GLint GetInternalFormat();
    GLint GetMipmapLevelCount();

private:
    GLuint m_handle = 0;
    GLuint64 m_bindlessID = 0;
    int m_width = 0;
    int m_height = 0;
    int m_channelCount = 0;
    GLsizei m_dataSize = 0;
    void* m_data = nullptr;
    GLint m_format = 0;
    GLint m_internalFormat = 0;
    GLint m_levels = 1;
    ImageDataType m_imageDataType;
    //TextureData m_textureData;
};
