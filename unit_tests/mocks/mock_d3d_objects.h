/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/sharings/d3d/d3d_sharing.h"
#include "gmock/gmock.h"
using ::testing::_;
using ::testing::NiceMock;
using ::testing::SetArgPointee;

namespace OCLRT {
template <typename D3D>
class MockD3DSharingFunctions : public D3DSharingFunctions<D3D> {
    typedef typename D3D::D3DDevice D3DDevice;
    typedef typename D3D::D3DQuery D3DQuery;
    typedef typename D3D::D3DQueryDesc D3DQueryDesc;
    typedef typename D3D::D3DResource D3DResource;
    typedef typename D3D::D3DBufferDesc D3DBufferDesc;
    typedef typename D3D::D3DBufferObj D3DBufferObj;
    typedef typename D3D::D3DTexture2dDesc D3DTexture2dDesc;
    typedef typename D3D::D3DTexture3dDesc D3DTexture3dDesc;
    typedef typename D3D::D3DTexture2d D3DTexture2d;
    typedef typename D3D::D3DTexture3d D3DTexture3d;

  public:
    MockD3DSharingFunctions() : D3DSharingFunctions((D3DDevice *)1) {
        memset(&mockDxgiDesc, 0, sizeof(DXGI_ADAPTER_DESC));
        mockDxgiDesc.VendorId = INTEL_VENDOR_ID;
        getDxgiDescFcn = &this->mockGetDxgiDesc;
        getDxgiDescCalled = 0;
        getDxgiDescAdapterRequested = nullptr;
    }

    MOCK_METHOD1_T(createQuery, void(D3DQuery **query));
    MOCK_METHOD2_T(createBuffer, void(D3DBufferObj **buffer, unsigned int width));
    MOCK_METHOD3_T(createTexture2d, void(D3DTexture2d **texture, D3DTexture2dDesc *desc, cl_uint subresource));
    MOCK_METHOD3_T(createTexture3d, void(D3DTexture3d **texture, D3DTexture3dDesc *desc, cl_uint subresource));
    MOCK_METHOD2_T(getBufferDesc, void(D3DBufferDesc *bufferDesc, D3DBufferObj *buffer));
    MOCK_METHOD2_T(getTexture2dDesc, void(D3DTexture2dDesc *textureDesc, D3DTexture2d *texture));
    MOCK_METHOD2_T(getTexture3dDesc, void(D3DTexture3dDesc *textureDesc, D3DTexture3d *texture));
    MOCK_METHOD2_T(getSharedHandle, void(D3DResource *resource, void **handle));
    MOCK_METHOD2_T(getSharedNTHandle, void(D3DResource *resource, void **handle));
    MOCK_METHOD1_T(addRef, void(D3DResource *resource));
    MOCK_METHOD1_T(release, void(IUnknown *resource));
    MOCK_METHOD1_T(getDeviceContext, void(D3DQuery *query));
    MOCK_METHOD1_T(releaseDeviceContext, void(D3DQuery *query));
    MOCK_METHOD4_T(copySubresourceRegion, void(D3DResource *dst, cl_uint dstSubresource, D3DResource *src, cl_uint srcSubresource));
    MOCK_METHOD1_T(flushAndWait, void(D3DQuery *query));
    MOCK_METHOD3_T(lockRect, void(D3DTexture2d *d3dResource, D3DLOCKED_RECT *lockedRect, uint32_t flags));
    MOCK_METHOD1_T(unlockRect, void(D3DTexture2d *d3dResource));
    MOCK_METHOD2_T(getRenderTargetData, void(D3DTexture2d *renderTarget, D3DTexture2d *dstSurface));
    MOCK_METHOD2_T(updateSurface, void(D3DTexture2d *src, D3DTexture2d *dst));
    MOCK_METHOD1_T(updateDevice, void(D3DResource *resource));

    std::vector<std::pair<D3DResource *, cl_uint>> *getTrackedResourcesVector() { return &this->trackedResources; }

    D3DBufferDesc mockBufferDesc = {};
    D3DTexture2dDesc mockTexture2dDesc = {};
    D3DTexture3dDesc mockTexture3dDesc = {};

    static DXGI_ADAPTER_DESC mockDxgiDesc;
    static IDXGIAdapter *getDxgiDescAdapterRequested;
    static uint32_t getDxgiDescCalled;

    static void mockGetDxgiDesc(DXGI_ADAPTER_DESC *dxgiDesc, IDXGIAdapter *adapter, D3DDevice *device) {
        getDxgiDescCalled++;
        getDxgiDescAdapterRequested = adapter;
        *dxgiDesc = mockDxgiDesc;
    }
};
} // namespace OCLRT