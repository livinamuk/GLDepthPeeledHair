#include "ImageTools.h"
#include <stdio.h>
#include <memory.h>
#include <iostream>
#include "DDSHelpers.h"
#include <filesystem>
#include <mutex>
#pragma warning(push)
#pragma warning(disable : 4996)
#include "stb_image_write.h"
#pragma warning(pop)
#include "tinyexr.h"
#include "../API/OpenGL/GL_util.hpp" // Remove me when you can
#include "DDS/DDS_Helpers.h"
#include "cmp_compressonatorlib/compressonator.h"

namespace ImageTools {
    std::mutex g_consoleMutex;

    TextureData LoadUncompressedTextureData(const std::string& filepath);
    TextureData LoadCompressedTextureData(const std::string& filepath);
    TextureData LoadEXRData(const std::string& filepath);

    TextureData LoadUncompressedTextureData(const std::string& filepath) {
        stbi_set_flip_vertically_on_load(false);
        TextureData textureData;
        textureData.m_imadeDataType = ImageDataType::UNCOMPRESSED;
        textureData.m_data = stbi_load(filepath.data(), &textureData.m_width, &textureData.m_height, &textureData.m_channelCount, 0);
        textureData.m_dataSize = textureData.m_width * textureData.m_height * textureData.m_channelCount;
        textureData.m_format = OpenGLUtil::GetFormatFromChannelCount(textureData.m_channelCount);
        textureData.m_internalFormat = OpenGLUtil::GetInternalFormatFromChannelCount(textureData.m_channelCount);
        return textureData;
    }

    TextureData LoadCompressedTextureData(const std::string& filepath) {
        CMP_Texture cmpTexture;
        LoadDDSFile(filepath.c_str(), cmpTexture);
        TextureData textureData;
        textureData.m_data = cmpTexture.pData;
        textureData.m_width = cmpTexture.dwWidth;
        textureData.m_height = cmpTexture.dwHeight;
        textureData.m_dataSize = cmpTexture.dwDataSize;
        textureData.m_internalFormat = OpenGLUtil::CMPFormatToGLInternalFromat(cmpTexture.format);
        textureData.m_format = OpenGLUtil::CMPFormatToGLFormat(cmpTexture.format);
        textureData.m_channelCount = OpenGLUtil::GetChannelCountFromFormat(textureData.m_format);
        return textureData;
    }

    TextureData LoadEXRData(const std::string& filepath) {
        TextureData textureData;
        textureData.m_imadeDataType = ImageDataType::EXR;
        const char* err = nullptr;
        const char** layer_names = nullptr;
        int num_layers = 0;
        bool status = EXRLayers(filepath.c_str(), &layer_names, &num_layers, &err);
        free(layer_names);
        const char* layername = NULL;
        float* floatPtr = nullptr;
        status = LoadEXRWithLayer(&floatPtr, &textureData.m_width, &textureData.m_height, filepath.c_str(), layername, &err);
        textureData.m_data = floatPtr;
        textureData.m_channelCount = -1; // TODO
        textureData.m_dataSize = -1; // TODO
        textureData.m_format = -1; // TODO
        textureData.m_internalFormat = -1; // TODO
        return textureData;
    }
}

void SaveAsBitmap(const char* filename, unsigned char* data, int width, int height, int numChannels) {
    unsigned char* flippedData = (unsigned char*)malloc(width * height * numChannels);
    if (!flippedData) {
        std::cerr << "[ERROR] Failed to allocate memory for flipped data\n";
        return;
    }
    for (int y = 0; y < height; ++y) {
        std::memcpy(flippedData + (height - y - 1) * width * numChannels,
            data + y * width * numChannels,
            width * numChannels);
    }
    if (stbi_write_bmp(filename, width, height, numChannels, flippedData)) {
        std::cout << "Saved bitmap: " << filename << "\n";
    }
    else {
        std::cerr << "Failed to save bitmap: " << filename << "\n";
    }
    free(flippedData);
}

void ImageTools::CompresssDXT3(const char* filename, unsigned char* data, int width, int height, int numChannels) {

    if (numChannels == 3) {
        const uint64_t pitch = static_cast<uint64_t>(width) * 3UL;
        for (auto r = 0; r < height; ++r) {
            uint8_t* row = data + r * pitch;
            for (auto c = 0UL; c < static_cast<uint64_t>(width); ++c) {
                uint8_t* pixel = row + c * 3UL;
                std::swap(pixel[0], pixel[2]);  // Swap Red and Blue channels using std::swap for clarity
            }
        }
    }
    CMP_Texture srcTexture = { 0 };
    srcTexture.dwSize = sizeof(CMP_Texture);
    srcTexture.dwWidth = width;
    srcTexture.dwHeight = height;
    srcTexture.dwPitch = numChannels == 4 ? width * 4 : width * 3;
    srcTexture.format = numChannels == 4 ? CMP_FORMAT_RGBA_8888 : CMP_FORMAT_RGB_888;
    srcTexture.dwDataSize = srcTexture.dwHeight * srcTexture.dwPitch;
    srcTexture.pData = data;

    CMP_Texture destTexture = { 0 };
    destTexture.dwSize = sizeof(CMP_Texture);
    destTexture.dwWidth = width;
    destTexture.dwHeight = height;
    destTexture.dwPitch = width;
    destTexture.format = CMP_FORMAT_DXT3;
    destTexture.dwDataSize = CMP_CalculateBufferSize(&destTexture);
    destTexture.pData = (CMP_BYTE*)malloc(destTexture.dwDataSize);

    CMP_CompressOptions options = { 0 };
    options.dwSize = sizeof(options);
    options.fquality = 0.88f;

    CMP_ERROR cmp_status = CMP_ConvertTexture(&srcTexture, &destTexture, &options, nullptr);
    if (cmp_status != CMP_OK) {
        free(destTexture.pData);
        std::lock_guard<std::mutex> lock(g_consoleMutex);
        std::cerr << "Compression failed for " << filename << " with error code: " << cmp_status << "\n";
        return;
    }
    else {
        CreateFolder("res/textures/dds/");
        SaveDDSFile(filename, destTexture);
        free(destTexture.pData);
        std::lock_guard<std::mutex> lock(g_consoleMutex);
        std::cout << "Saving compressed texture: " << filename << "\n";
    }
}

unsigned char* ImageTools::DownscaleTexture(unsigned char* data, int width, int height, int newWidth, int newHeight, int numChannels) {
    unsigned char* downscaledData = (unsigned char*)malloc(newWidth * newHeight * numChannels);
    if (!downscaledData) {
        std::cerr << "Failed to allocate memory for downscaled texture\n";
        return nullptr;
    }
    float xRatio = static_cast<float>(width) / newWidth;
    float yRatio = static_cast<float>(height) / newHeight;
    for (int y = 0; y < newHeight; ++y) {
        for (int x = 0; x < newWidth; ++x) {
            int srcXStart = static_cast<int>(x * xRatio);
            int srcYStart = static_cast<int>(y * yRatio);
            int srcXEnd = static_cast<int>((x + 1) * xRatio);
            int srcYEnd = static_cast<int>((y + 1) * yRatio);
            int area = (srcXEnd - srcXStart) * (srcYEnd - srcYStart);
            for (int c = 0; c < numChannels; ++c) {
                int sum = 0;
                for (int srcY = srcYStart; srcY < srcYEnd; ++srcY) {
                    for (int srcX = srcXStart; srcX < srcXEnd; ++srcX) {
                        int srcIndex = (srcY * width + srcX) * numChannels + c;
                        sum += data[srcIndex];
                    }
                }
                downscaledData[(y * newWidth + x) * numChannels + c] = static_cast<unsigned char>(sum / area);
            }
        }
    }
    return downscaledData;
}

void ImageTools::SwizzleRGBtoBGR(unsigned char* data, int width, int height) {
    const uint64_t pitch = static_cast<uint64_t>(width) * 3UL;
    for (auto r = 0; r < height; ++r) {
        uint8_t* row = data + r * pitch;
        for (auto c = 0UL; c < static_cast<uint64_t>(width); ++c) {
            uint8_t* pixel = row + c * 3UL;
            std::swap(pixel[0], pixel[2]);  // Swap Red and Blue channels using std::swap for clarity
        }
    }
}

std::vector<unsigned char*> ImageTools::GenerateMipmaps(unsigned char* data, int width, int height, int numChannels) {
    std::vector<unsigned char*> mipmaps;
    unsigned char* currentData = data;
    int currentWidth = width;
    int currentHeight = height;
    mipmaps.push_back(currentData);
    while (currentWidth > 1 || currentHeight > 1) {
        int nextWidth = std::max(1, currentWidth / 2);
        int nextHeight = std::max(1, currentHeight / 2);
        unsigned char* nextData = DownscaleTexture(currentData, currentWidth, currentHeight, nextWidth, nextHeight, numChannels);
        if (!nextData) {
            std::cerr << "Failed to create mipmap level\n";
            break;
        }
        mipmaps.push_back(nextData);
        currentWidth = nextWidth;
        currentHeight = nextHeight;
        currentData = nextData;
    }
    return mipmaps;
}

void ImageTools::SaveMipmaps(const std::string& filepath, std::vector<unsigned char*>& mipmaps, int baseWidth, int baseHeight, int channelCount) {
    for (size_t i = 0; i < mipmaps.size(); ++i) {
        int mipWidth = std::max(1, baseWidth >> i);
        int mipHeight = std::max(1, baseHeight >> i);
        std::string mipPath = filepath;
        mipPath += "_mip" + std::to_string(i) + ".bmp";
        SaveAsBitmap(mipPath.c_str(), mipmaps[i], mipWidth, mipHeight, channelCount);
    }
}

TextureData ImageTools::LoadTextureData(const std::string& filepath, ImageDataType imageDataType) {
    if (imageDataType == ImageDataType::UNCOMPRESSED) {
        return LoadUncompressedTextureData(filepath);
    }
    if (imageDataType == ImageDataType::COMPRESSED) {
        return LoadCompressedTextureData(filepath);
    }
    if (imageDataType == ImageDataType::EXR) {
        return LoadEXRData(filepath);
    }
}

int CalculateMipLevels(int width, int height) {
    int levels = 1; // Base level (original resolution)
    while (width > 1 || height > 1) {
        width = std::max(1, width / 2);
        height = std::max(1, height / 2);
        levels++;
    }
    return levels;
}

void CleanupMipSet(CMP_MipSet& mipSet) {
    if (mipSet.m_pMipLevelTable) {
        for (int i = 0; i < mipSet.m_nDepth; ++i) {
            if (mipSet.m_pMipLevelTable[i]) {
                delete[] mipSet.m_pMipLevelTable[i];
            }
        }
        delete[] mipSet.m_pMipLevelTable;
    }
}





void ImageTools::CompresssDXT3WithMipmaps(const char* filename, unsigned char* data, int width, int height, int numChannels) {  
    //// Swizzle RGB to BGR for compatibility with compression tools 
    //if (numChannels == 3) {
    //    SwizzleRGBtoBGR(data, width, height);
    //}
    //// Generate mipmaps
    //std::vector<unsigned char*> mipmaps = GenerateMipmaps(data, width, height, numChannels);
    //
    //CMP_MipSet compressedMipSet;
    //memset(&compressedMipSet, 0, sizeof(CMP_MipSet));
    //compressedMipSet.m_nWidth = width;
    //compressedMipSet.m_nHeight = height;
    //compressedMipSet.m_nMipLevels = mipmaps.size();
    //compressedMipSet.m_nDepth = 1; // Single 2D texture
    //compressedMipSet.m_pMipLevelTable = new CMP_MipLevel * [compressedMipSet.m_nDepth];
    //compressedMipSet.m_pMipLevelTable[0] = new CMP_MipLevel[compressedMipSet.m_nMipLevels];
    //
    //for (int i = 0; i < compressedMipSet.m_nMipLevels; ++i) {
    //    memset(&compressedMipSet.m_pMipLevelTable[0][i], 0, sizeof(CMP_MipLevel));
    //}
    //
    //for (size_t i = 0; i < mipmaps.size(); ++i) {
    //    CMP_Texture srcTexture = {};
    //    srcTexture.dwSize = sizeof(CMP_Texture);
    //    srcTexture.dwWidth = std::max(1, width >> i);
    //    srcTexture.dwHeight = std::max(1, height >> i);
    //    srcTexture.dwPitch = srcTexture.dwWidth * numChannels;
    //    srcTexture.format = numChannels == 4 ? CMP_FORMAT_RGBA_8888 : CMP_FORMAT_RGB_888;
    //    srcTexture.dwDataSize = srcTexture.dwHeight * srcTexture.dwPitch;
    //    srcTexture.pData = mipmaps[i];
    //
    //    CMP_Texture destTexture = {};
    //    destTexture.dwSize = sizeof(CMP_Texture);
    //    destTexture.dwWidth = srcTexture.dwWidth;
    //    destTexture.dwHeight = srcTexture.dwHeight;
    //    destTexture.format = CMP_FORMAT_DXT3;
    //    destTexture.dwDataSize = CMP_CalculateBufferSize(&destTexture);
    //    destTexture.pData = (CMP_BYTE*)malloc(destTexture.dwDataSize);
    //
    //    CMP_CompressOptions options = {};
    //    options.dwSize = sizeof(options);
    //    options.fquality = 0.88f;
    //
    //    CMP_ERROR cmp_status = CMP_ConvertTexture(&srcTexture, &destTexture, &options, nullptr);
    //    if (cmp_status != CMP_OK) {
    //        std::cerr << "Compression failed for mip level " << i << " with error code: " << cmp_status << "\n";
    //        free(destTexture.pData);
    //        continue;
    //    }
    //
    //    CMP_MipLevel* mipLevel = GetMipLevel(&compressedMipSet, 0, i);
    //    mipLevel->m_pbData = destTexture.pData;
    //    mipLevel->m_nWidth = destTexture.dwWidth;
    //    mipLevel->m_nHeight = destTexture.dwHeight;
    //    mipLevel->m_dwLinearSize = destTexture.dwDataSize;
    //}
    //
    //// SaveDDSFileWithMipmaps(filename, compressedMipSet);
    //
    //for (size_t i = 1; i < mipmaps.size(); ++i) {
    //    free(mipmaps[i]);
    //}
    //
    //std::cout << "Compression completed successfully for " << filename << "\n";
    //
    //
}

void ImageTools::CompresssBC5(const char* filename, unsigned char* data, int width, int height, int numChannels) {
    // Copy RGB data and add Alpha channel
    uint8_t* rgbaData = (uint8_t*)malloc(width * height * 4);
    for (int i = 0; i < width * height; ++i) {
        rgbaData[i * 4 + 0] = data[i * 3 + 0];  // Copy Red channel
        rgbaData[i * 4 + 1] = data[i * 3 + 1];  // Copy Green channel
        rgbaData[i * 4 + 2] = data[i * 3 + 2];  // Copy Blue channel
        rgbaData[i * 4 + 3] = 255;              // Set Alpha to 255
    }
    numChannels = 4;

    CMP_Texture srcTexture = { 0 };
    srcTexture.dwSize = sizeof(CMP_Texture);
    srcTexture.dwWidth = width;
    srcTexture.dwHeight = height;
    srcTexture.dwPitch = width * numChannels; 
    srcTexture.format = CMP_FORMAT_RGBA_8888;
    srcTexture.dwDataSize = srcTexture.dwHeight * srcTexture.dwPitch;
    srcTexture.pData = rgbaData;

    CMP_Texture destTexture = { 0 };
    destTexture.dwSize = sizeof(CMP_Texture);
    destTexture.dwWidth = width;
    destTexture.dwHeight = height;
    destTexture.dwPitch = width;
    destTexture.format = CMP_FORMAT_BC5;
    destTexture.dwDataSize = CMP_CalculateBufferSize(&destTexture);
    destTexture.pData = (CMP_BYTE*)malloc(destTexture.dwDataSize);

    if (destTexture.pData == nullptr) {
        std::lock_guard<std::mutex> lock(g_consoleMutex);
        std::cerr << "Failed to allocate memory for destination texture!\n";
        free(rgbaData);
        return;
    }
    CMP_CompressOptions options = { 0 };
    options.dwSize = sizeof(options);
    options.fquality = 1.00f;

    CMP_ERROR cmp_status = CMP_ConvertTexture(&srcTexture, &destTexture, &options, nullptr);

    if (cmp_status != CMP_OK) {
        std::lock_guard<std::mutex> lock(g_consoleMutex);
        std::cerr << "Compression failed with error code: " << cmp_status << "\n";
        free(destTexture.pData);
        free(rgbaData);
        return;
    } 
    else {
        std::lock_guard<std::mutex> lock(g_consoleMutex);
        std::cout << "Saving compressed texture: " << filename << "\n";
        SaveDDSFile(filename, destTexture);
        free(destTexture.pData);
        free(rgbaData);
    } 
}

void ImageTools::CreateFolder(const char* path) {
    std::filesystem::path dir(path);
    if (!std::filesystem::exists(dir)) {
        if (!std::filesystem::create_directories(dir) && !std::filesystem::exists(dir)) {
            std::cout << "Failed to create directory: " << path << "\n";
        }
    }
}

CompressionType ImageTools::CompressionTypeFromTextureSuffix(const std::string& suffix) {
    if (suffix == "NRM") {
        return CompressionType::BC5;
    }
    else {
        return CompressionType::DXT3;
    }
}