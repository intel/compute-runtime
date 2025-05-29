/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm_lib.h"

#include <cstdint>
#include <memory>
#include <string>

namespace NEO {
enum class ImagePlane;
struct HardwareInfo;
struct ImageInfo;
struct StorageInfo;
class GmmResourceInfo;
class GmmHelper;

template <typename OverriddenType>
struct Overrider {
    bool enableOverride{false};
    OverriddenType value;

    void doOverride(OverriddenType &variable) const {
        if (enableOverride) {
            variable = value;
        }
    }
};

struct GmmRequirements {
    bool preferCompressed;
    bool allowLargePages;
    Overrider<bool> overriderCacheable;
    Overrider<bool> overriderPreferNoCpuAccess;
};

class Gmm {
  public:
    virtual ~Gmm();
    Gmm() = delete;
    Gmm(GmmHelper *gmmHelper, ImageInfo &inputOutputImgInfo, const StorageInfo &storageInfo, bool preferCompressed);
    Gmm(GmmHelper *gmmHelper, const void *alignedPtr, size_t alignedSize, size_t alignment,
        GMM_RESOURCE_USAGE_TYPE_ENUM gmmResourceUsage, const StorageInfo &storageInfo, const GmmRequirements &gmmRequirements);
    Gmm(GmmHelper *gmmHelper, GMM_RESOURCE_INFO *inputGmm);
    Gmm(GmmHelper *gmmHelper, GMM_RESOURCE_INFO *inputGmm, bool openingHandle);

    void queryImageParams(ImageInfo &inputOutputImgInfo);

    void applyAuxFlagsForBuffer(bool preferCompression);
    void applyExtraAuxInitFlag();
    void applyMemoryFlags(const StorageInfo &storageInfo);
    void applyAppResource(const StorageInfo &storageInfo);

    bool unifiedAuxTranslationCapable() const;
    bool hasMultisampleControlSurface() const;

    GmmHelper *getGmmHelper() const;

    uint32_t queryQPitch(GMM_RESOURCE_TYPE resType);
    void updateImgInfoAndDesc(ImageInfo &imgInfo, uint32_t arrayIndex, ImagePlane yuvPlaneType);
    void updateOffsetsInImgInfo(ImageInfo &imgInfo, uint32_t arrayIndex);
    uint8_t resourceCopyBlt(void *sys, void *gpu, uint32_t pitch, uint32_t height, unsigned char upload, ImagePlane plane);

    uint32_t getUnifiedAuxPitchTiles();
    uint32_t getAuxQPitch();
    bool getPreferNoCpuAccess() const { return preferNoCpuAccess; }

    GMM_RESCREATE_PARAMS resourceParams = {};
    std::unique_ptr<GmmResourceInfo> gmmResourceInfo;

    std::string getUsageTypeString();
    void setCompressionEnabled(bool compresionEnabled) { this->compressionEnabled = compresionEnabled; }
    bool isCompressionEnabled() const { return compressionEnabled; }

  protected:
    void applyAuxFlagsForImage(ImageInfo &imgInfo, bool preferCompressed);
    void setupImageResourceParams(ImageInfo &imgInfo, bool preferCompressed);
    MOCKABLE_VIRTUAL bool extraMemoryFlagsRequired();
    void applyExtraMemoryFlags(const StorageInfo &storageInfo);
    void applyDebugOverrides();
    GmmHelper *gmmHelper = nullptr;

    bool compressionEnabled = false;
    bool preferNoCpuAccess = false;
};
} // namespace NEO
