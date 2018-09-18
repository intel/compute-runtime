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

template class D3DSharingFunctions<D3DTypesHelper::D3D10>;
template class D3DSharingFunctions<D3DTypesHelper::D3D11>;

const uint32_t D3DSharingFunctions<D3DTypesHelper::D3D10>::sharingId = SharingType::D3D10_SHARING;
const uint32_t D3DSharingFunctions<D3DTypesHelper::D3D11>::sharingId = SharingType::D3D11_SHARING;

template <typename D3D>
void D3DSharingFunctions<D3D>::createQuery(D3DQuery **query) {
    D3DQueryDesc desc = {};
    d3dDevice->CreateQuery(&desc, query);
}

template <typename D3D>
void D3DSharingFunctions<D3D>::updateDevice(D3DResource *resource) {
    resource->GetDevice(&d3dDevice);
}

template <typename D3D>
void D3DSharingFunctions<D3D>::fillCreateBufferDesc(D3DBufferDesc &desc, unsigned int width) {
    desc.ByteWidth = width;
    desc.MiscFlags = D3DResourceFlags::MISC_SHARED;
}

template <typename D3D>
void D3DSharingFunctions<D3D>::fillCreateTexture2dDesc(D3DTexture2dDesc &desc, D3DTexture2dDesc *srcDesc, cl_uint subresource) {
    desc.Width = srcDesc->Width;
    desc.Height = srcDesc->Height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = srcDesc->Format;
    desc.MiscFlags = D3DResourceFlags::MISC_SHARED;
    desc.SampleDesc.Count = srcDesc->SampleDesc.Count;
    desc.SampleDesc.Quality = srcDesc->SampleDesc.Quality;

    for (uint32_t i = 0u; i < (subresource % srcDesc->MipLevels); i++) {
        desc.Width /= 2;
        desc.Height /= 2;
    }
}

template <typename D3D>
void D3DSharingFunctions<D3D>::fillCreateTexture3dDesc(D3DTexture3dDesc &desc, D3DTexture3dDesc *srcDesc, cl_uint subresource) {
    desc.Width = srcDesc->Width;
    desc.Height = srcDesc->Height;
    desc.Depth = srcDesc->Depth;
    desc.MipLevels = 1;
    desc.Format = srcDesc->Format;
    desc.MiscFlags = D3DResourceFlags::MISC_SHARED;

    for (uint32_t i = 0u; i < (subresource % srcDesc->MipLevels); i++) {
        desc.Width /= 2;
        desc.Height /= 2;
        desc.Depth /= 2;
    }
}

template <typename D3D>
void D3DSharingFunctions<D3D>::createBuffer(D3DBufferObj **buffer, unsigned int width) {
    D3DBufferDesc stagingDesc = {};
    fillCreateBufferDesc(stagingDesc, width);
    d3dDevice->CreateBuffer(&stagingDesc, nullptr, buffer);
}

template <typename D3D>
void D3DSharingFunctions<D3D>::createTexture2d(D3DTexture2d **texture, D3DTexture2dDesc *desc, cl_uint subresource) {
    D3DTexture2dDesc stagingDesc = {};
    fillCreateTexture2dDesc(stagingDesc, desc, subresource);
    d3dDevice->CreateTexture2D(&stagingDesc, nullptr, texture);
}

template <typename D3D>
void D3DSharingFunctions<D3D>::createTexture3d(D3DTexture3d **texture, D3DTexture3dDesc *desc, cl_uint subresource) {
    D3DTexture3dDesc stagingDesc = {};
    fillCreateTexture3dDesc(stagingDesc, desc, subresource);
    d3dDevice->CreateTexture3D(&stagingDesc, nullptr, texture);
}

template <typename D3D>
void D3DSharingFunctions<D3D>::getBufferDesc(D3DBufferDesc *bufferDesc, D3DBufferObj *buffer) {
    buffer->GetDesc(bufferDesc);
}

template <typename D3D>
void D3DSharingFunctions<D3D>::getTexture2dDesc(D3DTexture2dDesc *textureDesc, D3DTexture2d *texture) {
    texture->GetDesc(textureDesc);
}

template <typename D3D>
void D3DSharingFunctions<D3D>::getTexture3dDesc(D3DTexture3dDesc *textureDesc, D3DTexture3d *texture) {
    texture->GetDesc(textureDesc);
}

template <typename D3D>
void D3DSharingFunctions<D3D>::getSharedHandle(D3DResource *resource, void **handle) {
    IDXGIResource *dxgiResource = nullptr;
    resource->QueryInterface(__uuidof(IDXGIResource), (void **)&dxgiResource);
    dxgiResource->GetSharedHandle(handle);
    dxgiResource->Release();
}

template <typename D3D>
void D3DSharingFunctions<D3D>::getSharedNTHandle(D3DResource *resource, void **handle) {
    IDXGIResource *dxgiResource = nullptr;
    IDXGIResource1 *dxgiResource1 = nullptr;
    resource->QueryInterface(__uuidof(IDXGIResource), (void **)&dxgiResource);
    dxgiResource->QueryInterface(__uuidof(IDXGIResource1), (void **)&dxgiResource1);
    dxgiResource1->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr, handle);
    dxgiResource1->Release();
    dxgiResource->Release();
}

template <typename D3D>
void D3DSharingFunctions<D3D>::addRef(D3DResource *resource) {
    resource->AddRef();
}

template <typename D3D>
void D3DSharingFunctions<D3D>::release(IUnknown *resource) {
    resource->Release();
}

template <typename D3D>
void D3DSharingFunctions<D3D>::lockRect(D3DTexture2d *resource, D3DLOCKED_RECT *lockedRect, uint32_t flags) {
}

template <typename D3D>
void D3DSharingFunctions<D3D>::unlockRect(D3DTexture2d *resource) {
}
template <typename D3D>
void D3DSharingFunctions<D3D>::updateSurface(D3DTexture2d *src, D3DTexture2d *dst) {
}

template <typename D3D>
void D3DSharingFunctions<D3D>::getRenderTargetData(D3DTexture2d *renderTarget, D3DTexture2d *dstSurface) {
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D10>::copySubresourceRegion(D3DResource *dst, cl_uint dstSubresource,
                                                                       D3DResource *src, cl_uint srcSubresource) {
    d3dDevice->CopySubresourceRegion(dst, dstSubresource, 0, 0, 0, src, srcSubresource, nullptr);
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D11>::copySubresourceRegion(D3DResource *dst, cl_uint dstSubresource,
                                                                       D3DResource *src, cl_uint srcSubresource) {
    d3d11DeviceContext->CopySubresourceRegion(dst, dstSubresource, 0, 0, 0, src, srcSubresource, nullptr);
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D10>::flushAndWait(D3DQuery *query) {
    query->End();
    d3dDevice->Flush();
    while (query->GetData(nullptr, 0, 0) != S_OK)
        ;
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D11>::flushAndWait(D3DQuery *query) {
    d3d11DeviceContext->End(query);
    d3d11DeviceContext->Flush();
    while (d3d11DeviceContext->GetData(query, nullptr, 0, 0) != S_OK)
        ;
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D10>::getDeviceContext(D3DQuery *query) {
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D11>::getDeviceContext(D3DQuery *query) {
    d3dDevice->GetImmediateContext(&d3d11DeviceContext);
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D10>::releaseDeviceContext(D3DQuery *query) {
}

template <>
void D3DSharingFunctions<D3DTypesHelper::D3D11>::releaseDeviceContext(D3DQuery *query) {
    d3d11DeviceContext->Release();
    d3d11DeviceContext = nullptr;
}
template <typename D3D>
void D3DSharingFunctions<D3D>::getDxgiDesc(DXGI_ADAPTER_DESC *dxgiDesc, IDXGIAdapter *adapter, D3DDevice *device) {
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

template D3DSharingFunctions<D3DTypesHelper::D3D10> *Context::getSharing<D3DSharingFunctions<D3DTypesHelper::D3D10>>();
template D3DSharingFunctions<D3DTypesHelper::D3D11> *Context::getSharing<D3DSharingFunctions<D3DTypesHelper::D3D11>>();
