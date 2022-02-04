/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/memory_manager/definitions/storage_info.h"

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
    virtual ~Gmm();
    Gmm() = delete;
    Gmm(GmmClientContext *clientContext, ImageInfo &inputOutputImgInfo, StorageInfo storageInfo, bool preferCompressed);
    Gmm(GmmClientContext *clientContext, const void *alignedPtr, size_t alignedSize, size_t alignment,
        GMM_RESOURCE_USAGE_TYPE_ENUM gmmResourceUsage, bool preferCompressed, StorageInfo storageInfo, bool allowLargePages);
    Gmm(GmmClientContext *clientContext, GMM_RESOURCE_INFO *inputGmm);

    void queryImageParams(ImageInfo &inputOutputImgInfo);

    void applyAuxFlagsForBuffer(bool preferCompression);
    void applyMemoryFlags(StorageInfo &storageInfo);
    void applyAppResource(StorageInfo &storageInfo);

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

    bool isCompressionEnabled = false;

  protected:
    void applyAuxFlagsForImage(ImageInfo &imgInfo, bool preferCompressed);
    void setupImageResourceParams(ImageInfo &imgInfo, bool preferCompressed);
    bool extraMemoryFlagsRequired();
    void applyExtraMemoryFlags(const StorageInfo &storageInfo);
    void applyDebugOverrides();
    GmmClientContext *clientContext = nullptr;
};
} // namespace NEO
