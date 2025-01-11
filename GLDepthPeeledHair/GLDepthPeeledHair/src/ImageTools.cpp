#include "ImageTools.h"
#include <stdio.h>
#include <memory.h>
#include <iostream>
#include "DDSHelpers.h"
#include <filesystem>

void ImageTools::CompresssImageData(CompressionType type, const char* filename, unsigned char* data, int width, int height, int numChannels) { 
    uint8_t* imageData = static_cast<uint8_t*>(data);
    if (numChannels == 3) {
        const uint64_t pitch = static_cast<uint64_t>(width) * 3UL;
        for (auto r = 0; r < height; ++r) {
            uint8_t* row = imageData + r * pitch;
            for (auto c = 0UL; c < static_cast<uint64_t>(width); ++c) {
                uint8_t* pixel = row + c * 3UL;
                uint8_t  p = pixel[0];
                pixel[0] = pixel[2];
                pixel[2] = p;
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
    srcTexture.pData = imageData;

    CMP_Texture destTexture = { 0 };
    destTexture.dwSize = sizeof(CMP_Texture);
    destTexture.dwWidth = width;
    destTexture.dwHeight = height;
    destTexture.dwPitch = width;
    destTexture.format = type == CompressionType::DXT3 ? CMP_FORMAT_DXT3 : CMP_FORMAT_BC5;
    destTexture.dwDataSize = CMP_CalculateBufferSize(&destTexture);
    destTexture.pData = (CMP_BYTE*)malloc(destTexture.dwDataSize);

    CMP_CompressOptions options = { 0 };
    options.dwSize = sizeof(options);
    options.fquality = 0.88f;

    CMP_ERROR cmp_status;
    cmp_status = CMP_ConvertTexture(&srcTexture, &destTexture, &options, nullptr);
    if (cmp_status != CMP_OK) {
        std::printf("Compression returned an error %d\n", cmp_status);
        free(destTexture.pData);
        return;
    }
    else {
        std::cout << "saving compressed texture: " << filename << "\n";
        CreateFolder("res/textures/dds/");
        SaveDDSFile(filename, destTexture);
        free(destTexture.pData);
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