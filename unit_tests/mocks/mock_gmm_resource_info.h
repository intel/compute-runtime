/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/helpers/surface_formats.h"
#include "gmock/gmock.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

namespace OCLRT {
struct SurfaceFormatInfo;

class MockGmmResourceInfo : public GmmResourceInfo {
  public:
    virtual ~MockGmmResourceInfo();

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

    uint32_t getVAlign() override { return 4u; }

    uint32_t getMaxLod() override { return 7u; }

    uint32_t getTileModeSurfaceState() override;

    uint32_t getRenderAuxPitchTiles() override { return unifiedAuxPitch; };

    uint32_t getAuxQPitch() override { return auxQPitch; }

    GMM_RESOURCE_FORMAT getResourceFormat() override { return mockResourceCreateParams.Format; }

    GMM_SURFACESTATE_FORMAT getResourceFormatSurfaceState() override { return (GMM_SURFACESTATE_FORMAT)0; }

    GMM_RESOURCE_TYPE getResourceType() override { return mockResourceCreateParams.Type; }

    GMM_RESOURCE_FLAG *getResourceFlags() override { return &mockResourceCreateParams.Flags; }

    GMM_STATUS getOffset(GMM_REQ_OFFSET_INFO &reqOffsetInfo) override;

    MOCK_METHOD1(cpuBlt, uint8_t(GMM_RES_COPY_BLT *resCopyBlt));

    void *getSystemMemPointer(uint8_t isD3DDdiAllocation) override { return (void *)mockResourceCreateParams.pExistingSysMem; }

    MOCK_METHOD1(getUnifiedAuxSurfaceOffset, uint64_t(GMM_UNIFIED_AUX_TYPE auxType));

    GMM_RESOURCE_INFO *peekHandle() const override { return mockResourceInfoHandle; }

    GMM_RESOURCE_INFO *mockResourceInfoHandle = (GMM_RESOURCE_INFO *)this;
    GMM_RESCREATE_PARAMS mockResourceCreateParams = {};

    void overrideReturnedRenderPitch(size_t newPitch) { rowPitch = newPitch; }
    void overrideReturnedSize(size_t newSize) { size = newSize; }

    void setUnifiedAuxTranslationCapable();
    void setMultisampleControlSurface();

    void setUnifiedAuxPitchTiles(uint32_t value);
    void setAuxQPitch(uint32_t value);

    uint32_t getOffsetCalled = 0u;
    uint32_t arrayIndexPassedToGetOffset = 0;
    SurfaceFormatInfo tempSurface{};

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
};
} // namespace OCLRT

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
