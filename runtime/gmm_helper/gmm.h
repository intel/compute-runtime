/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/gmm_helper/gmm_lib.h"
#include "runtime/api/cl_types.h"

#include "storage_info.h"

#include <cstdint>
#include <cstdlib>
#include <memory>

namespace NEO {
enum class OCLPlane;
struct HardwareInfo;
struct ImageInfo;
class GmmResourceInfo;

class Gmm {
  public:
    virtual ~Gmm() = default;
    Gmm() = delete;
    Gmm(ImageInfo &inputOutputImgInfo, StorageInfo storageInfo);
    Gmm(const void *alignedPtr, size_t alignedSize, bool uncacheable);
    Gmm(const void *alignedPtr, size_t alignedSize, bool uncacheable, bool preferRenderCompressed, bool systemMemoryPool, StorageInfo storageInfo);
    Gmm(GMM_RESOURCE_INFO *inputGmm);

    void queryImageParams(ImageInfo &inputOutputImgInfo);

    void applyAuxFlagsForBuffer(bool preferRenderCompression);
    void applyMemoryFlags(bool systemMemoryPool, StorageInfo &storageInfo);

    bool unifiedAuxTranslationCapable() const;
    bool hasMultisampleControlSurface() const;

    uint32_t queryQPitch(GMM_RESOURCE_TYPE resType);
    void updateImgInfoAndDesc(ImageInfo &imgInfo, cl_image_desc &imgDesc, cl_uint arrayIndex);
    void updateOffsetsInImgInfo(ImageInfo &imgInfo, cl_uint arrayIndex);
    uint8_t resourceCopyBlt(void *sys, void *gpu, uint32_t pitch, uint32_t height, unsigned char upload, OCLPlane plane);

    uint32_t getUnifiedAuxPitchTiles();
    uint32_t getAuxQPitch();

    GMM_RESCREATE_PARAMS resourceParams = {};
    std::unique_ptr<GmmResourceInfo> gmmResourceInfo;

    bool isRenderCompressed = false;
    bool useSystemMemoryPool = true;

  protected:
    void applyAuxFlagsForImage(ImageInfo &imgInfo);
    void setupImageResourceParams(ImageInfo &imgInfo);
};
} // namespace NEO
