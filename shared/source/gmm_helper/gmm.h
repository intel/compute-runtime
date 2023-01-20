/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm_lib.h"

#include <cstdint>
#include <memory>

namespace NEO {
enum class ImagePlane;
struct HardwareInfo;
struct ImageInfo;
struct StorageInfo;
class GmmResourceInfo;
class GmmHelper;

class Gmm {
  public:
    virtual ~Gmm();
    Gmm() = delete;
    Gmm(GmmHelper *gmmHelper, ImageInfo &inputOutputImgInfo, const StorageInfo &storageInfo, bool preferCompressed);
    Gmm(GmmHelper *gmmHelper, const void *alignedPtr, size_t alignedSize, size_t alignment,
        GMM_RESOURCE_USAGE_TYPE_ENUM gmmResourceUsage, bool preferCompressed, const StorageInfo &storageInfo, bool allowLargePages);
    Gmm(GmmHelper *gmmHelper, GMM_RESOURCE_INFO *inputGmm);
    Gmm(GmmHelper *gmmHelper, GMM_RESOURCE_INFO *inputGmm, bool openingHandle);

    void queryImageParams(ImageInfo &inputOutputImgInfo);

    void applyAuxFlagsForBuffer(bool preferCompression);
    void applyMemoryFlags(const StorageInfo &storageInfo);
    void applyAppResource(const StorageInfo &storageInfo);

    bool unifiedAuxTranslationCapable() const;
    bool hasMultisampleControlSurface() const;

    GmmHelper *getGmmHelper() const;

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
    GmmHelper *gmmHelper = nullptr;
};
} // namespace NEO
