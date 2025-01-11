#pragma once

enum class LoadingState {
    AWAITING_LOADING_FROM_DISK,
    LOADING_FROM_DISK,
    LOADING_COMPLETE
};

enum class BakingState {
    AWAITING_BAKE,
    BAKING_IN_PROGRESS,
    BAKE_COMPLETE
};

enum class BlendingMode { 
    NONE, 
    BLENDED,
    ALPHA_DISCARDED,
    HAIR_UNDER_LAYER,
    HAIR_TOP_LAYER,
    DO_NOT_RENDER 
};

enum class MipmapState {
    NO_MIPMAPS_REQUIRED,
    AWAITING_MIPMAP_GENERATION,
    MIPMAPS_GENERATED
};

enum class ImageDataType {
    UNCOMPRESSED,
    COMPRESSED,
    EXR,
    UNDEFINED
};