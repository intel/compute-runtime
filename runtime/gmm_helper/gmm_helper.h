/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/memory_manager/memory_constants.h"
#include "runtime/gmm_helper/gmm_lib.h"

#include <memory>

namespace NEO {
enum class OCLPlane;
class GmmClientContext;
class GraphicsAllocation;
class OsLibrary;
struct HardwareInfo;
struct FeatureTable;
struct WorkaroundTable;
struct ImageInfo;

class GmmHelper {
  public:
    GmmHelper() = delete;
    GmmHelper(const HardwareInfo *hwInfo);
    MOCKABLE_VIRTUAL ~GmmHelper();

    const HardwareInfo *getHardwareInfo();
    uint32_t getMOCS(uint32_t type);

    static constexpr uint64_t maxPossiblePitch = 2147483648;

    template <uint8_t addressWidth = 48>
    static uint64_t canonize(uint64_t address) {
        return ((int64_t)(address << (64 - addressWidth))) >> (64 - addressWidth);
    }

    template <uint8_t addressWidth = 48>
    static uint64_t decanonize(uint64_t address) {
        return (address & maxNBitValue<addressWidth>);
    }

    static GmmClientContext *getClientContext();
    static GmmHelper *getInstance();

    static void queryImgFromBufferParams(ImageInfo &imgInfo, GraphicsAllocation *gfxAlloc);
    static GMM_CUBE_FACE_ENUM getCubeFaceIndex(uint32_t target);
    static uint32_t getRenderMultisamplesCount(uint32_t numSamples);
    static GMM_YUV_PLANE convertPlane(OCLPlane oclPlane);

    static std::unique_ptr<GmmClientContext> (*createGmmContextWrapperFunc)(GMM_CLIENT, GmmExportEntries &);

  protected:
    void loadLib();
    void initContext(const PLATFORM *platform, const FeatureTable *featureTable, const WorkaroundTable *workaroundTable, const GT_SYSTEM_INFO *pGtSysInfo);

    const HardwareInfo *hwInfo = nullptr;
    std::unique_ptr<OsLibrary> gmmLib;
    std::unique_ptr<GmmClientContext> gmmClientContext;
    GmmExportEntries gmmEntries = {};
};
} // namespace NEO
