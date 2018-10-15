/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/context/context.inl"
#include "runtime/os_interface/windows/d3d_sharing_functions.h"
#include "runtime/sharings/sharing_factory.h"

using namespace OCLRT;

template class D3DSharingFunctions<D3DTypesHelper::D3D9>;
const uint32_t D3DSharingFunctions<D3DTypesHelper::D3D9>::sharingId = SharingType::D3D9_SHARING;

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D9>::createQuery(D3DQuery **query) {
    D3DQUERYTYPE queryType = D3DQUERYTYPE_EVENT;
    d3dDevice->CreateQuery(queryType, query);
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D9>::updateDevice(D3DResource *resource) {
    resource->GetDevice(&d3dDevice);
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D9>::fillCreateBufferDesc(D3DBufferDesc &desc, unsigned int width) {
}

template <>
std::vector<DXGI_FORMAT> &D3DSharingFunctions<D3DTypesHelper::D3D9>::retrieveTextureFormats(cl_mem_object_type imageType) {
    return DXGINoFormats;
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D9>::fillCreateTexture2dDesc(D3DTexture2dDesc &desc, D3DTexture2dDesc *srcDesc, cl_uint subresource) {
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D9>::fillCreateTexture3dDesc(D3DTexture3dDesc &desc, D3DTexture3dDesc *srcDesc, cl_uint subresource) {
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D9>::createBuffer(D3DBufferObj **buffer, unsigned int width) {
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D9>::createTexture2d(D3DTexture2d **texture, D3DTexture2dDesc *desc, cl_uint subresource) {
    d3dDevice->CreateOffscreenPlainSurface(desc->Width, desc->Height, desc->Format, D3DPOOL_SYSTEMMEM, texture, nullptr);
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D9>::createTexture3d(D3DTexture3d **texture, D3DTexture3dDesc *desc, cl_uint subresource) {
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D9>::checkFormatSupport(DXGI_FORMAT format, UINT *pFormat) {
}

template <>
bool D3DSharingFunctions<D3DTypesHelper::D3D9>::memObjectFormatSupport(cl_mem_object_type object, UINT format) {
    return false;
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D9>::getBufferDesc(D3DBufferDesc *bufferDesc, D3DBufferObj *buffer) {
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D9>::getTexture2dDesc(D3DTexture2dDesc *textureDesc, D3DTexture2d *texture) {
    texture->GetDesc(textureDesc);
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D9>::getTexture3dDesc(D3DTexture3dDesc *textureDesc, D3DTexture3d *texture) {
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D9>::getSharedHandle(D3DResource *resource, void **handle) {
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D9>::getSharedNTHandle(D3DResource *resource, void **handle) {
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D9>::addRef(D3DResource *resource) {
    resource->AddRef();
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D9>::release(IUnknown *resource) {
    if (resource) {
        resource->Release();
    }
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D9>::lockRect(D3DTexture2d *d3dresource, D3DLOCKED_RECT *lockedRect, uint32_t flags) {
    d3dresource->LockRect(lockedRect, nullptr, flags);
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D9>::unlockRect(D3DTexture2d *d3dresource) {
    d3dresource->UnlockRect();
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D9>::getRenderTargetData(D3DTexture2d *renderTarget, D3DTexture2d *dstSurface) {
    d3dDevice->GetRenderTargetData(renderTarget, dstSurface);
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D9>::copySubresourceRegion(D3DResource *dst, cl_uint dstSubresource,
                                                                      D3DResource *src, cl_uint srcSubresource) {
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D9>::updateSurface(D3DTexture2d *src, D3DTexture2d *dst) {
    d3dDevice->UpdateSurface(src, nullptr, dst, nullptr);
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D9>::flushAndWait(D3DQuery *query) {
    query->Issue(D3DISSUE_END);
    while (query->GetData(nullptr, 0, D3DGETDATA_FLUSH) != S_OK)
        ;
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D9>::getDeviceContext(D3DQuery *query) {
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D9>::releaseDeviceContext(D3DQuery *query) {
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D9>::getDxgiDesc(DXGI_ADAPTER_DESC *dxgiDesc, IDXGIAdapter *adapter, D3DDevice *device) {
    if (!adapter) {
        IDXGIDevice *dxgiDevice = nullptr;
        device->QueryInterface(__uuidof(IDXGIDevice), (void **)&dxgiDevice);
        dxgiDevice->GetAdapter(&adapter);
        dxgiDevice->Release();
    } else {
        adapter->AddRef();
    }
    adapter->GetDesc(dxgiDesc);
    adapter->Release();
}

template D3DSharingFunctions<D3DTypesHelper::D3D9> *Context::getSharing<D3DSharingFunctions<D3DTypesHelper::D3D9>>();
