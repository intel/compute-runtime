/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/test_macros/mock_method_macros.h"

#include "opencl/source/sharings/d3d/d3d_sharing.h"

namespace NEO {
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

    void createQuery(D3DQuery **query) override {
        createQueryCalled++;
        if (createQuerySetParams) {
            *query = createQueryParamsSet.query;
        }
    }

    struct CreateQueryParams {
        D3DQuery *query{};
    };

    uint32_t createQueryCalled = 0u;
    CreateQueryParams createQueryParamsSet{};
    bool createQuerySetParams = false;

    void createBuffer(D3DBufferObj **buffer, unsigned int width) override {
        createBufferCalled++;
        if (createBufferSetParams) {
            *buffer = createBufferParamsSet.buffer;
        }
    }

    struct CreateBufferParams {
        D3DBufferObj *buffer{};
        unsigned int width{};
    };

    uint32_t createBufferCalled = 0u;
    CreateBufferParams createBufferParamsSet{};
    bool createBufferSetParams = false;

    void createTexture2d(D3DTexture2d **texture, D3DTexture2dDesc *desc, cl_uint subresource) override {
        createTexture2dCalled++;
        if (createTexture2dSetParams) {
            *texture = createTexture2dParamsSet.texture;
        }
    }

    struct CreateTexture2dParams {
        D3DTexture2d *texture{};
        D3DTexture2dDesc *desc{};
        cl_uint subresource{};
    };

    uint32_t createTexture2dCalled = 0u;
    CreateTexture2dParams createTexture2dParamsSet{};
    bool createTexture2dSetParams = false;

    void createTexture3d(D3DTexture3d **texture, D3DTexture3dDesc *desc, cl_uint subresource) override {
        createTexture3dCalled++;
        if (createTexture3dSetParams) {
            *texture = createTexture3dParamsSet.texture;
        }
    }

    struct CreateTexture3dParams {
        D3DTexture3d *texture{};
        D3DTexture3dDesc *desc{};
        cl_uint subresource{};
    };

    uint32_t createTexture3dCalled = 0u;
    CreateTexture3dParams createTexture3dParamsSet{};
    bool createTexture3dSetParams = false;

    void getBufferDesc(D3DBufferDesc *bufferDesc, D3DBufferObj *buffer) override {
        getBufferDescCalled++;
        if (getBufferDescSetParams) {
            *bufferDesc = getBufferDescParamsSet.bufferDesc;
        }
    }

    struct GetBufferDescParams {
        D3DBufferDesc bufferDesc{};
        D3DBufferObj *buffer{};
    };

    uint32_t getBufferDescCalled = 0u;
    GetBufferDescParams getBufferDescParamsSet{};
    bool getBufferDescSetParams = false;

    void getTexture2dDesc(D3DTexture2dDesc *textureDesc, D3DTexture2d *texture) override {
        getTexture2dDescCalled++;
        if (getTexture2dDescSetParams) {
            *textureDesc = getTexture2dDescParamsSet.textureDesc;
        }
    }

    struct GetTexture2dDescParams {
        D3DTexture2dDesc textureDesc{};
        D3DTexture2d *texture{};
    };

    uint32_t getTexture2dDescCalled = 0u;
    GetTexture2dDescParams getTexture2dDescParamsSet{};
    bool getTexture2dDescSetParams = false;

    void getTexture3dDesc(D3DTexture3dDesc *textureDesc, D3DTexture3d *texture) override {
        getTexture3dDescCalled++;
        if (getTexture3dDescSetParams) {
            *textureDesc = getTexture3dDescParamsSet.textureDesc;
        }
    }

    struct GetTexture3dDescParams {
        D3DTexture3dDesc textureDesc{};
        D3DTexture3d *texture{};
    };

    uint32_t getTexture3dDescCalled = 0u;
    GetTexture3dDescParams getTexture3dDescParamsSet{};
    bool getTexture3dDescSetParams = false;

    void getSharedHandle(D3DResource *resource, void **handle) override {
        getSharedHandleCalled++;
        getSharedHandleParamsPassed.push_back({resource, handle});
    }

    struct GetSharedHandleParams {
        D3DResource *resource{};
        void **handle{};
    };

    uint32_t getSharedHandleCalled = 0u;
    StackVec<GetSharedHandleParams, 1> getSharedHandleParamsPassed{};

    void addRef(D3DResource *resource) override {
        addRefCalled++;
        addRefParamsPassed.push_back({resource});
    }

    struct AddRefParams {
        D3DResource *resource{};
    };

    uint32_t addRefCalled = 0u;
    StackVec<AddRefParams, 1> addRefParamsPassed{};

    void release(IUnknown *resource) override {
        releaseCalled++;
        releaseParamsPassed.push_back({resource});
    }

    struct ReleaseParams {
        IUnknown *resource{};
    };

    uint32_t releaseCalled = 0u;
    StackVec<ReleaseParams, 3> releaseParamsPassed{};

    void copySubresourceRegion(D3DResource *dst, cl_uint dstSubresource, D3DResource *src, cl_uint srcSubresource) override {
        copySubresourceRegionCalled++;
        copySubresourceRegionParamsPassed.push_back({dst, dstSubresource, src, srcSubresource});
    }

    struct CopySubresourceRegionParams {
        D3DResource *dst{};
        cl_uint dstSubresource{};
        D3DResource *src{};
        cl_uint srcSubresource{};
    };

    uint32_t copySubresourceRegionCalled = 0u;
    StackVec<CopySubresourceRegionParams, 2> copySubresourceRegionParamsPassed{};

    void lockRect(D3DTexture2d *d3dResource, D3DLOCKED_RECT *lockedRect, uint32_t flags) override {
        lockRectCalled++;
        if (lockRectSetParams) {
            *lockedRect = lockRectParamsSet.lockedRect;
        }
        lockRectParamsPassed.push_back({d3dResource, *lockedRect, flags});
    }

    struct LockRectParams {
        D3DTexture2d *d3dResource{};
        D3DLOCKED_RECT lockedRect{};
        uint32_t flags{};
    };

    uint32_t lockRectCalled = 0u;
    LockRectParams lockRectParamsSet{};
    bool lockRectSetParams = false;
    StackVec<LockRectParams, 2> lockRectParamsPassed{};

    void unlockRect(D3DTexture2d *d3dResource) override {
        unlockRectCalled++;
        unlockRectParamsPassed.push_back({d3dResource});
    }

    struct UnlockRectParams {
        D3DTexture2d *d3dResource{};
    };

    uint32_t unlockRectCalled = 0u;
    StackVec<UnlockRectParams, 2> unlockRectParamsPassed{};

    void getRenderTargetData(D3DTexture2d *renderTarget, D3DTexture2d *dstSurface) override {
        getRenderTargetDataCalled++;
        getRenderTargetDataParamsPassed.push_back({renderTarget, dstSurface});
    }

    struct GetRenderTargetDataParams {
        D3DTexture2d *renderTarget{};
        D3DTexture2d *dstSurface{};
    };

    uint32_t getRenderTargetDataCalled = 0u;
    StackVec<GetRenderTargetDataParams, 1> getRenderTargetDataParamsPassed{};

    void updateSurface(D3DTexture2d *renderTarget, D3DTexture2d *dstSurface) override {
        updateSurfaceCalled++;
        updateSurfaceParamsPassed.push_back({renderTarget, dstSurface});
    }

    struct UpdateSurfaceParams {
        D3DTexture2d *renderTarget{};
        D3DTexture2d *dstSurface{};
    };

    uint32_t updateSurfaceCalled = 0u;
    StackVec<UpdateSurfaceParams, 1> updateSurfaceParamsPassed{};

    void updateDevice(D3DResource *resource) override {
        updateDeviceCalled++;
        updateDeviceParamsPassed.push_back({resource});
    }

    struct UpdateDeviceParams {
        D3DResource *resource{};
    };

    uint32_t updateDeviceCalled = 0u;
    StackVec<UpdateDeviceParams, 1> updateDeviceParamsPassed{};

    bool checkFormatSupport(DXGI_FORMAT format, UINT *pFormat) override {
        checkFormatSupportCalled++;
        if (nullptr == pFormat) {
            return false;
        }
        if (checkFormatSupportSetParam0) {
            format = checkFormatSupportParamsSet.format;
        }
        if (checkFormatSupportSetParam1) {
            *pFormat = checkFormatSupportParamsSet.pFormat;
        }
        if (checkUnsupportedDXGIformats) {
            auto iter = std::find(unsupportedDXGIformats.begin(), unsupportedDXGIformats.end(), format);
            if (iter != unsupportedDXGIformats.end()) {
                *pFormat = {};
                return false;
            }
            return true;
        }
        return checkFormatSupportResult;
    }

    struct CheckFormatSupportParams {
        DXGI_FORMAT format{};
        UINT pFormat{};
    };

    uint32_t checkFormatSupportCalled = 0u;
    CheckFormatSupportParams checkFormatSupportParamsSet{};
    bool checkFormatSupportResult = true;
    bool checkFormatSupportSetParam0 = false;
    bool checkFormatSupportSetParam1 = false;
    bool checkUnsupportedDXGIformats = false;
    std::vector<DXGI_FORMAT> unsupportedDXGIformats{};

    cl_int validateFormatSupport(DXGI_FORMAT format, cl_mem_object_type type) override {
        validateFormatSupportCalled++;
        if (callBaseValidateFormatSupport) {
            validateFormatSupportResult = validateFormatSupportBase(format, type);
        }
        return validateFormatSupportResult;
    }

    uint32_t validateFormatSupportCalled = 0u;
    cl_int validateFormatSupportResult = CL_SUCCESS;
    bool callBaseValidateFormatSupport = false;

    cl_int validateFormatSupportBase(DXGI_FORMAT format, cl_mem_object_type type) {
        return D3DSharingFunctions<D3D>::validateFormatSupport(format, type);
    }
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

    ADDMETHOD_NOBASE(memObjectFormatSupport, bool, true, (cl_mem_object_type object, UINT format));
    ADDMETHOD_NOBASE_VOIDRETURN(getSharedNTHandle, (D3DResource * resource, void **handle));
    ADDMETHOD_NOBASE_VOIDRETURN(getDeviceContext, (D3DQuery * query));
    ADDMETHOD_NOBASE_VOIDRETURN(releaseDeviceContext, (D3DQuery * query));
    ADDMETHOD_NOBASE_VOIDRETURN(flushAndWait, (D3DQuery * query));
};
} // namespace NEO
