#pragma once

enum class CompressionType { DXT3, BC5 };

namespace ImageTools {
    void CreateFolder(const char* path);
    void CompresssImageData(CompressionType type, const char* filename, unsigned char* data, int width, int height, int numChannels);
}