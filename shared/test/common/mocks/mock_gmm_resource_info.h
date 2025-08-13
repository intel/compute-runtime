/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

namespace NEO {
struct SurfaceFormatInfo;

class MockGmmResourceInfo : public GmmResourceInfo {
  public:
    ~MockGmmResourceInfo() override;

    MockGmmResourceInfo(GMM_RESCREATE_PARAMS *resourceCreateParams);

    MockGmmResourceInfo(GMM_RESOURCE_INFO *inputGmmResourceInfo);

    size_t getSizeAllocation() override { return size; }

    size_t getBaseWidth() override { return static_cast<size_t>(mockResourceCreateParams.BaseWidth); }

    size_t getBaseHeight() override { return static_cast<size_t>(mockResourceCreateParams.BaseHeight); }

    size_t getBaseDepth() override { return static_cast<size_t>(mockResourceCreateParams.Depth); }

    size_t getArraySize() override { return static_cast<size_t>(mockResourceCreateParams.ArraySize); }

    size_t getRenderPitch() override { return rowPitch; }

    uint64_t getDriverProtectionBits(uint32_t overrideUsage) override {
        driverProtectionBitsUsageWasOverriden = GMM_RESOURCE_USAGE_UNKNOWN != overrideUsage;
        driverProtectionBitsUsageOverride = overrideUsage;
        return driverProtectionBits;
    }

    uint32_t getNumSamples() override { return mockResourceCreateParams.MSAA.NumSamples; }

    uint32_t getQPitch() override { return qPitch; }

    uint32_t getBitsPerPixel() override;

    uint32_t getHAlign() override { return 4u; }

    uint32_t getHAlignSurfaceState() override { return getHAlignSurfaceStateResult; }

    uint32_t getVAlignSurfaceState() override { return 1u; }

    uint32_t getMaxLod() override { return 7u; }

    uint32_t getTileModeSurfaceState() override;

    uint32_t getRenderAuxPitchTiles() override { return unifiedAuxPitch; };

    uint32_t getAuxQPitch() override { return auxQPitch; }

    uint32_t getMipTailStartLODSurfaceState() override { return mipTailStartLod; }

    GMM_RESOURCE_FORMAT getResourceFormat() override { return mockResourceCreateParams.Format; }

    GMM_SURFACESTATE_FORMAT getResourceFormatSurfaceState() override { return (GMM_SURFACESTATE_FORMAT)0; }

    GMM_RESOURCE_TYPE getResourceType() override { return mockResourceCreateParams.Type; }

    GMM_RESOURCE_USAGE_TYPE getResourceUsage() { return mockResourceCreateParams.Usage; }

    GMM_RESOURCE_FLAG *getResourceFlags() override { return &mockResourceCreateParams.Flags; }

    GMM_STATUS getOffset(GMM_REQ_OFFSET_INFO &reqOffsetInfo) override;

    uint8_t cpuBlt(GMM_RES_COPY_BLT *resCopyBlt) override {
        cpuBltCalled++;
        if (resCopyBlt) {
            requestedResCopyBlt = *resCopyBlt;
        }
        return cpuBltResult;
    };

    void *getSystemMemPointer() override { return (void *)mockResourceCreateParams.pExistingSysMem; }

    ADDMETHOD_NOBASE(getUnifiedAuxSurfaceOffset, uint64_t, 0u, (GMM_UNIFIED_AUX_TYPE auxType));

    bool is64KBPageSuitable() const override { return is64KBPageSuitableValue; }

    bool isDisplayable() const override { return isDisplayableValue; }

    GMM_RESOURCE_INFO *peekGmmResourceInfo() const override { return mockResourceInfoHandle; }

    GMM_RESOURCE_USAGE_TYPE getCachePolicyUsage() const override { return usageType; }

    void *peekHandle() const override { return mockResourceInfoHandle; }

    size_t peekHandleSize() const override { return sizeof(GMM_RESOURCE_INFO); }

    GMM_RESOURCE_INFO *mockResourceInfoHandle = (GMM_RESOURCE_INFO *)this;
    GMM_RESCREATE_PARAMS mockResourceCreateParams = {};

    void overrideReturnedRenderPitch(size_t newPitch) { rowPitch = newPitch; }
    void overrideReturnedSize(size_t newSize) { size = newSize; }

    void setUnifiedAuxTranslationCapable();
    void setMultisampleControlSurface();

    void setUnifiedAuxPitchTiles(uint32_t value);
    void setAuxQPitch(uint32_t value);
    void setMipTailStartLOD(uint32_t newMipTailStartLod) { mipTailStartLod = newMipTailStartLod; }

    void refreshHandle() override {
        refreshHandleCalled++;
        GmmResourceInfo::refreshHandle();
    };

    using GmmResourceInfo::clientContext;
    using GmmResourceInfo::createResourceInfo;
    using GmmResourceInfo::decodeResourceInfo;

    uint64_t driverProtectionBits = 0;
    bool driverProtectionBitsUsageWasOverriden = false;
    uint32_t driverProtectionBitsUsageOverride = 0u;
    uint32_t getOffsetCalled = 0u;
    uint32_t arrayIndexPassedToGetOffset = 0;
    SurfaceFormatInfo tempSurface{};
    bool is64KBPageSuitableValue = true;
    bool isDisplayableValue = false;
    GMM_RES_COPY_BLT requestedResCopyBlt = {};
    uint32_t cpuBltCalled = 0u;
    uint8_t cpuBltResult = 1u;
    static constexpr uint32_t getHAlignSurfaceStateResult = 2u;
    static constexpr uint32_t yMajorTileModeValue = 3u;
    uint32_t refreshHandleCalled = 0u;
    GMM_RESOURCE_USAGE_TYPE usageType = GMM_RESOURCE_USAGE_OCL_BUFFER;

  protected:
    MockGmmResourceInfo();

    void computeRowPitch();
    void setupDefaultActions();
    void setSurfaceFormat();
    const SurfaceFormatInfo *surfaceFormatInfo = nullptr;

    size_t size = 0;
    size_t rowPitch = 0;
    uint32_t qPitch = 0;
    uint32_t unifiedAuxPitch = 1u;
    uint32_t auxQPitch = 1u;
    uint32_t mipTailStartLod = 0;
};
} // namespace NEO
