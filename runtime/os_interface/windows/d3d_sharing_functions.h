/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "runtime/api/dispatch.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/sharings/sharing.h"
#include "DXGI1_2.h"
#include <vector>

namespace OCLRT {
namespace D3DTypesHelper {
struct D3D9 {
    typedef IDirect3DDevice9 D3DDevice;
    typedef IDirect3DQuery9 D3DQuery;
    typedef D3DQUERYTYPE D3DQueryDesc;
    typedef IDirect3DResource9 D3DResource;
    typedef struct {
    } D3DBufferDesc;
    typedef void *D3DBufferObj;
    typedef D3DSURFACE_DESC D3DTexture2dDesc;
    typedef struct {
    } D3DTexture3dDesc;
    typedef IDirect3DSurface9 D3DTexture2d;
    typedef struct {
    } D3DTexture3d;
};
struct D3D10 {
    typedef ID3D10Device D3DDevice;
    typedef ID3D10Query D3DQuery;
    typedef D3D10_QUERY_DESC D3DQueryDesc;
    typedef ID3D10Resource D3DResource;
    typedef D3D10_BUFFER_DESC D3DBufferDesc;
    typedef ID3D10Buffer D3DBufferObj;
    typedef D3D10_TEXTURE2D_DESC D3DTexture2dDesc;
    typedef D3D10_TEXTURE3D_DESC D3DTexture3dDesc;
    typedef ID3D10Texture2D D3DTexture2d;
    typedef ID3D10Texture3D D3DTexture3d;
};
struct D3D11 {
    typedef ID3D11Device D3DDevice;
    typedef ID3D11Query D3DQuery;
    typedef D3D11_QUERY_DESC D3DQueryDesc;
    typedef ID3D11Resource D3DResource;
    typedef D3D11_BUFFER_DESC D3DBufferDesc;
    typedef ID3D11Buffer D3DBufferObj;
    typedef D3D11_TEXTURE2D_DESC D3DTexture2dDesc;
    typedef D3D11_TEXTURE3D_DESC D3DTexture3dDesc;
    typedef ID3D11Texture2D D3DTexture2d;
    typedef ID3D11Texture3D D3DTexture3d;
};
} // namespace D3DTypesHelper

enum D3DResourceFlags {
    USAGE_RENDERTARGET = 1,
    MISC_SHARED = 2,
    MISC_SHARED_KEYEDMUTEX = 256,
    MISC_SHARED_NTHANDLE = 2048
};

template <typename D3D>
class D3DSharingFunctions : public SharingFunctions {
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
    typedef void (*GetDxgiDescFcn)(DXGI_ADAPTER_DESC *dxgiDesc, IDXGIAdapter *adapter, D3DDevice *device);

    D3DSharingFunctions(D3DDevice *d3dDevice) : d3dDevice(d3dDevice) {
        trackedResources.reserve(128);
        getDxgiDescFcn = &this->getDxgiDesc;
    };

    uint32_t getId() const override {
        return D3DSharingFunctions<D3D>::sharingId;
    }

    D3DSharingFunctions() = delete;

    static const uint32_t sharingId;

    MOCKABLE_VIRTUAL void createQuery(D3DQuery **query);
    MOCKABLE_VIRTUAL void createBuffer(D3DBufferObj **buffer, unsigned int width);
    MOCKABLE_VIRTUAL void createTexture2d(D3DTexture2d **texture, D3DTexture2dDesc *desc, cl_uint subresource);
    MOCKABLE_VIRTUAL void createTexture3d(D3DTexture3d **texture, D3DTexture3dDesc *desc, cl_uint subresource);
    MOCKABLE_VIRTUAL void getBufferDesc(D3DBufferDesc *bufferDesc, D3DBufferObj *buffer);
    MOCKABLE_VIRTUAL void getTexture2dDesc(D3DTexture2dDesc *textureDesc, D3DTexture2d *texture);
    MOCKABLE_VIRTUAL void getTexture3dDesc(D3DTexture3dDesc *textureDesc, D3DTexture3d *texture);
    MOCKABLE_VIRTUAL void getSharedHandle(D3DResource *resource, void **handle);
    MOCKABLE_VIRTUAL void getSharedNTHandle(D3DResource *resource, void **handle);
    MOCKABLE_VIRTUAL void addRef(D3DResource *resource);
    MOCKABLE_VIRTUAL void release(IUnknown *resource);
    MOCKABLE_VIRTUAL void copySubresourceRegion(D3DResource *dst, cl_uint dstSubresource,
                                                D3DResource *src, cl_uint srcSubresource);
    MOCKABLE_VIRTUAL void flushAndWait(D3DQuery *query);
    MOCKABLE_VIRTUAL void getDeviceContext(D3DQuery *query);
    MOCKABLE_VIRTUAL void releaseDeviceContext(D3DQuery *query);
    MOCKABLE_VIRTUAL void lockRect(D3DTexture2d *d3dResource, D3DLOCKED_RECT *lockedRect, uint32_t flags);
    MOCKABLE_VIRTUAL void unlockRect(D3DTexture2d *d3dResource);
    MOCKABLE_VIRTUAL void getRenderTargetData(D3DTexture2d *renderTarget, D3DTexture2d *dstSurface);
    MOCKABLE_VIRTUAL void updateSurface(D3DTexture2d *src, D3DTexture2d *dst);
    MOCKABLE_VIRTUAL void updateDevice(D3DResource *resource);

    GetDxgiDescFcn getDxgiDescFcn = nullptr;

    bool isTracked(D3DResource *resource, cl_uint subresource) {
        return std::find(trackedResources.begin(), trackedResources.end(), std::make_pair(resource, subresource)) != trackedResources.end();
    }

    void track(D3DResource *resource, cl_uint subresource) {
        trackedResources.push_back(std::make_pair(resource, subresource));
    }

    void untrack(D3DResource *resource, cl_uint subresource) {
        auto element = std::find(trackedResources.begin(), trackedResources.end(), std::make_pair(resource, subresource));
        DEBUG_BREAK_IF(element == trackedResources.end());
        trackedResources.erase(element);
    }

    void setDevice(D3DDevice *d3dDevice) { this->d3dDevice = d3dDevice; }
    D3DDevice *getDevice() { return d3dDevice; }

    void fillCreateBufferDesc(D3DBufferDesc &desc, unsigned int width);
    void fillCreateTexture2dDesc(D3DTexture2dDesc &desc, D3DTexture2dDesc *srcDesc, cl_uint subresource);
    void fillCreateTexture3dDesc(D3DTexture3dDesc &desc, D3DTexture3dDesc *srcDesc, cl_uint subresource);

  protected:
    D3DDevice *d3dDevice = nullptr;
    ID3D11DeviceContext *d3d11DeviceContext = nullptr;

    std::vector<std::pair<D3DResource *, cl_uint>> trackedResources;
    static void getDxgiDesc(DXGI_ADAPTER_DESC *dxgiDesc, IDXGIAdapter *adapter, D3DDevice *device);
};
} // namespace OCLRT
