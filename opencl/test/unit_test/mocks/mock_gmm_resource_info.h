/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/resource_info.h"

#include "opencl/source/helpers/surface_formats.h"

#include "gmock/gmock.h"

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

    uint32_t getNumSamples() override { return mockResourceCreateParams.MSAA.NumSamples; }

    uint32_t getQPitch() override { return qPitch; }

    uint32_t getBitsPerPixel() override;

    uint32_t getHAlign() override { return 4u; }

    uint32_t getHAlignSurfaceState() override { return 1u; }

    uint32_t getVAlignSurfaceState() override { return 1u; }

    uint32_t getMaxLod() override { return 7u; }

    uint32_t getTileModeSurfaceState() override;

    uint32_t getRenderAuxPitchTiles() override { return unifiedAuxPitch; };

    uint32_t getAuxQPitch() override { return auxQPitch; }

    uint32_t getMipTailStartLodSurfaceState() override { return mipTailStartLod; }

    GMM_RESOURCE_FORMAT getResourceFormat() override { return mockResourceCreateParams.Format; }

    GMM_SURFACESTATE_FORMAT getResourceFormatSurfaceState() override { return (GMM_SURFACESTATE_FORMAT)0; }

    GMM_RESOURCE_TYPE getResourceType() override { return mockResourceCreateParams.Type; }

    GMM_RESOURCE_FLAG *getResourceFlags() override { return &mockResourceCreateParams.Flags; }

    GMM_STATUS getOffset(GMM_REQ_OFFSET_INFO &reqOffsetInfo) override;

    MOCK_METHOD(uint8_t, cpuBlt, (GMM_RES_COPY_BLT * resCopyBlt), (override));

    void *getSystemMemPointer() override { return (void *)mockResourceCreateParams.pExistingSysMem; }

    MOCK_METHOD(uint64_t, getUnifiedAuxSurfaceOffset, (GMM_UNIFIED_AUX_TYPE auxType), (override));

    bool is64KBPageSuitable() const override { return is64KBPageSuitableValue; }

    GMM_RESOURCE_INFO *peekGmmResourceInfo() const override { return mockResourceInfoHandle; }

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
    void setMipTailStartLod(uint32_t newMipTailStartLod) { mipTailStartLod = newMipTailStartLod; }

    using GmmResourceInfo::clientContext;
    using GmmResourceInfo::createResourceInfo;

    uint32_t getOffsetCalled = 0u;
    uint32_t arrayIndexPassedToGetOffset = 0;
    SurfaceFormatInfo tempSurface{};
    bool is64KBPageSuitableValue = true;

  protected:
    MockGmmResourceInfo();

    void computeRowPitch();
    void setupDefaultActions();
    void setSurfaceFormat();
    const SurfaceFormatInfo *surfaceFormatInfo = nullptr;

    size_t size = 0;
    size_t rowPitch = 0;
    uint32_t qPitch = 0;
    uint32_t unifiedAuxPitch = 0;
    uint32_t auxQPitch = 0;
    uint32_t mipTailStartLod = 0;
};
} // namespace NEO
