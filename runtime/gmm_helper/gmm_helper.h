/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <cstdlib>
#include <memory>
#include "runtime/gmm_helper/gmm_lib.h"
#include "runtime/api/cl_types.h"

namespace OCLRT {
enum class OCLPlane;
struct HardwareInfo;
struct FeatureTable;
struct WorkaroundTable;
struct ImageInfo;
class GraphicsAllocation;
class Gmm;
class OsLibrary;
class GmmClientContext;

class GmmHelper {
  public:
    GmmHelper() = delete;
    GmmHelper(const HardwareInfo *hwInfo);
    MOCKABLE_VIRTUAL ~GmmHelper();

    const HardwareInfo *getHardwareInfo();
    uint32_t getMOCS(uint32_t type);
    void setSimplifiedMocsTableUsage(bool value);

    static constexpr uint32_t cacheDisabledIndex = 0;
    static constexpr uint32_t cacheEnabledIndex = 4;
    static constexpr uint32_t maxPossiblePitch = 2147483648;

    static uint64_t canonize(uint64_t address);
    static uint64_t decanonize(uint64_t address);

    static GmmClientContext *getClientContext();
    static GmmHelper *getInstance();

    static void queryImgFromBufferParams(ImageInfo &imgInfo, GraphicsAllocation *gfxAlloc);
    static GMM_CUBE_FACE_ENUM getCubeFaceIndex(uint32_t target);
    static bool allowTiling(const cl_image_desc &imageDesc);
    static uint32_t getRenderTileMode(uint32_t tileWalk);
    static uint32_t getRenderAlignment(uint32_t alignment);
    static uint32_t getRenderMultisamplesCount(uint32_t numSamples);
    static GMM_YUV_PLANE convertPlane(OCLPlane oclPlane);

    static std::unique_ptr<GmmClientContext> (*createGmmContextWrapperFunc)(GMM_CLIENT, GmmExportEntries &);

  protected:
    void loadLib();
    void initContext(const PLATFORM *pPlatform, const FeatureTable *pSkuTable, const WorkaroundTable *pWaTable, const GT_SYSTEM_INFO *pGtSysInfo);

    bool useSimplifiedMocsTable = false;
    const HardwareInfo *hwInfo = nullptr;
    std::unique_ptr<OsLibrary> gmmLib;
    std::unique_ptr<GmmClientContext> gmmClientContext;
    GmmExportEntries gmmEntries = {};
};
} // namespace OCLRT
