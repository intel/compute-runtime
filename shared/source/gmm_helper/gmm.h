/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm_lib.h"

#include "storage_info.h"

#include <cstdint>
#include <cstdlib>
#include <memory>

namespace NEO {
enum class ImagePlane;
struct HardwareInfo;
struct ImageInfo;
class GmmResourceInfo;
class GmmClientContext;

class Gmm {
  public:
    virtual ~Gmm() = default;
    Gmm() = delete;
    Gmm(GmmClientContext *clientContext, ImageInfo &inputOutputImgInfo, StorageInfo storageInfo);
    Gmm(GmmClientContext *clientContext, const void *alignedPtr, size_t alignedSize, bool uncacheable);
    Gmm(GmmClientContext *clientContext, const void *alignedPtr, size_t alignedSize, bool uncacheable, bool preferRenderCompressed, bool systemMemoryPool, StorageInfo storageInfo);
    Gmm(GmmClientContext *clientContext, GMM_RESOURCE_INFO *inputGmm);

    void queryImageParams(ImageInfo &inputOutputImgInfo);

    void applyAuxFlagsForBuffer(bool preferRenderCompression);
    void applyMemoryFlags(bool systemMemoryPool, StorageInfo &storageInfo);

    bool unifiedAuxTranslationCapable() const;
    bool hasMultisampleControlSurface() const;

    uint32_t queryQPitch(GMM_RESOURCE_TYPE resType);
    void updateImgInfoAndDesc(ImageInfo &imgInfo, uint32_t arrayIndex);
    void updateOffsetsInImgInfo(ImageInfo &imgInfo, uint32_t arrayIndex);
    uint8_t resourceCopyBlt(void *sys, void *gpu, uint32_t pitch, uint32_t height, unsigned char upload, ImagePlane plane);

    uint32_t getUnifiedAuxPitchTiles();
    uint32_t getAuxQPitch();

    GMM_RESCREATE_PARAMS resourceParams = {};
    std::unique_ptr<GmmResourceInfo> gmmResourceInfo;

    bool isRenderCompressed = false;
    bool useSystemMemoryPool = true;

  protected:
    void applyAuxFlagsForImage(ImageInfo &imgInfo);
    void setupImageResourceParams(ImageInfo &imgInfo);
    GmmClientContext *clientContext = nullptr;
};
} // namespace NEO
