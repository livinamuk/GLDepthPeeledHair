#pragma once
#include <string>
#include <vector>
#include "types.h"

enum class CompressionType { DXT3, BC5, UNDEFINED };

namespace ImageTools {
    void CreateFolder(const char* path);
    void CompresssDXT3(const char* filename, unsigned char* data, int width, int height, int channelCount);
    void CompresssDXT3WithMipmaps(const char* filename, unsigned char* data, int width, int height, int channelCount);
    void CompresssBC5(const char* filename, unsigned char* data, int width, int height, int channelCount);
    CompressionType CompressionTypeFromTextureSuffix(const std::string& suffix);

    void SwizzleRGBtoBGR(unsigned char* data, int width, int height);
    unsigned char* DownscaleTexture(unsigned char* data, int width, int height, int newWidth, int newHeight, int channelCount);

    // Mipmaps
    std::vector<unsigned char*> GenerateMipmaps(unsigned char* data, int width, int height, int channelCount);
    void SaveMipmaps(const std::string& filepath, std::vector<unsigned char*>& mipmaps, int baseWidth, int baseHeight, int channelCount);

    // Texture Data
    TextureData LoadTextureData(const std::string& filepath, ImageDataType imageDataType);
}