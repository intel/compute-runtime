/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifdef WIN32

#include "opencl/source/sharings/d3d/enable_d3d.h"

#include "opencl/source/api/api.h"
#include "opencl/source/context/context.h"
#include "opencl/source/context/context.inl"
#include "opencl/source/os_interface/windows/d3d_sharing_functions.h"
#include "opencl/source/sharings/d3d/cl_d3d_api.h"
#include "opencl/source/sharings/sharing_factory.h"
#include "opencl/source/sharings/sharing_factory.inl"

#include <memory>

namespace NEO {

bool D3DSharingContextBuilder<D3DTypesHelper::D3D9>::processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue, cl_int &errcodeRet) {
    if (contextData.get() == nullptr) {
        contextData = std::make_unique<D3DCreateContextProperties<D3DTypesHelper::D3D9>>();
    }
    bool res = false;

    switch (propertyType) {
    case CL_CONTEXT_ADAPTER_D3D9_KHR:
    case CL_CONTEXT_ADAPTER_D3D9EX_KHR:
    case CL_CONTEXT_ADAPTER_DXVA_KHR:
    case CL_CONTEXT_D3D9_DEVICE_INTEL:
    case CL_CONTEXT_D3D9EX_DEVICE_INTEL:
    case CL_CONTEXT_DXVA_DEVICE_INTEL:
        contextData->pDevice = (D3DTypesHelper::D3D9::D3DDevice *)propertyValue;
        contextData->argumentsDefined = true;
        res = true;
        break;
    }
    return res;
}

bool D3DSharingContextBuilder<D3DTypesHelper::D3D10>::processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue, cl_int &errcodeRet) {
    if (contextData.get() == nullptr) {
        contextData = std::make_unique<D3DCreateContextProperties<D3DTypesHelper::D3D10>>();
    }
    bool res = false;

    switch (propertyType) {
    case CL_CONTEXT_D3D10_DEVICE_KHR:
        contextData->pDevice = (D3DTypesHelper::D3D10::D3DDevice *)propertyValue;
        contextData->argumentsDefined = true;
        res = true;
        break;
    }
    return res;
}

bool D3DSharingContextBuilder<D3DTypesHelper::D3D11>::processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue, cl_int &errcodeRet) {
    if (contextData.get() == nullptr) {
        contextData = std::make_unique<D3DCreateContextProperties<D3DTypesHelper::D3D11>>();
    }
    bool res = false;
    switch (propertyType) {
    case CL_CONTEXT_D3D11_DEVICE_KHR:
        contextData->pDevice = (D3DTypesHelper::D3D11::D3DDevice *)propertyValue;
        contextData->argumentsDefined = true;
        res = true;
        break;
    }
    return res;
}

template <>
void Context::registerSharing(D3DSharingFunctions<D3DTypesHelper::D3D10> *sharing) {
    this->sharingFunctions[D3DSharingFunctions<D3DTypesHelper::D3D10>::sharingId].reset(sharing);
    this->preferD3dSharedResources = 1u;
}

template <>
void Context::registerSharing(D3DSharingFunctions<D3DTypesHelper::D3D11> *sharing) {
    this->sharingFunctions[D3DSharingFunctions<D3DTypesHelper::D3D11>::sharingId].reset(sharing);
    this->preferD3dSharedResources = 1u;
}

template <typename D3D>
bool D3DSharingContextBuilder<D3D>::finalizeProperties(Context &context, int32_t &errcodeRet) {
    if (contextData.get() == nullptr)
        return true;

    if (contextData->argumentsDefined) {
        context.registerSharing(new D3DSharingFunctions<D3D>(contextData->pDevice));
    }

    return true;
}

template <typename D3D>
std::unique_ptr<SharingContextBuilder> D3DSharingBuilderFactory<D3D>::createContextBuilder() {
    return std::make_unique<D3DSharingContextBuilder<D3D>>();
};

std::string D3DSharingBuilderFactory<D3DTypesHelper::D3D9>::getExtensions() {
    return "cl_intel_dx9_media_sharing cl_khr_dx9_media_sharing ";
}

std::string D3DSharingBuilderFactory<D3DTypesHelper::D3D10>::getExtensions() {
    return "cl_khr_d3d10_sharing ";
}

std::string D3DSharingBuilderFactory<D3DTypesHelper::D3D11>::getExtensions() {
    return "cl_khr_d3d11_sharing cl_intel_d3d11_nv12_media_sharing ";
}

void D3DSharingBuilderFactory<D3DTypesHelper::D3D9>::fillGlobalDispatchTable() {
    icdGlobalDispatchTable.clGetDeviceIDsFromDX9MediaAdapterKHR = clGetDeviceIDsFromDX9MediaAdapterKHR;
    icdGlobalDispatchTable.clCreateFromDX9MediaSurfaceKHR = clCreateFromDX9MediaSurfaceKHR;
    icdGlobalDispatchTable.clEnqueueReleaseDX9MediaSurfacesKHR = clEnqueueReleaseDX9MediaSurfacesKHR;
    icdGlobalDispatchTable.clEnqueueAcquireDX9MediaSurfacesKHR = clEnqueueAcquireDX9MediaSurfacesKHR;

    crtGlobalDispatchTable.clGetDeviceIDsFromDX9INTEL = clGetDeviceIDsFromDX9INTEL;
    crtGlobalDispatchTable.clCreateFromDX9MediaSurfaceINTEL = clCreateFromDX9MediaSurfaceINTEL;
    crtGlobalDispatchTable.clEnqueueAcquireDX9ObjectsINTEL = clEnqueueAcquireDX9ObjectsINTEL;
    crtGlobalDispatchTable.clEnqueueReleaseDX9ObjectsINTEL = clEnqueueReleaseDX9ObjectsINTEL;
}

void D3DSharingBuilderFactory<D3DTypesHelper::D3D10>::fillGlobalDispatchTable() {
    icdGlobalDispatchTable.clCreateFromD3D10BufferKHR = clCreateFromD3D10BufferKHR;
    icdGlobalDispatchTable.clCreateFromD3D10Texture2DKHR = clCreateFromD3D10Texture2DKHR;
    icdGlobalDispatchTable.clCreateFromD3D10Texture3DKHR = clCreateFromD3D10Texture3DKHR;
    icdGlobalDispatchTable.clEnqueueAcquireD3D10ObjectsKHR = clEnqueueAcquireD3D10ObjectsKHR;
    icdGlobalDispatchTable.clEnqueueReleaseD3D10ObjectsKHR = clEnqueueReleaseD3D10ObjectsKHR;
    icdGlobalDispatchTable.clGetDeviceIDsFromD3D10KHR = clGetDeviceIDsFromD3D10KHR;
}

void D3DSharingBuilderFactory<D3DTypesHelper::D3D11>::fillGlobalDispatchTable() {
    icdGlobalDispatchTable.clCreateFromD3D11BufferKHR = clCreateFromD3D11BufferKHR;
    icdGlobalDispatchTable.clCreateFromD3D11Texture2DKHR = clCreateFromD3D11Texture2DKHR;
    icdGlobalDispatchTable.clCreateFromD3D11Texture3DKHR = clCreateFromD3D11Texture3DKHR;
    icdGlobalDispatchTable.clEnqueueAcquireD3D11ObjectsKHR = clEnqueueAcquireD3D11ObjectsKHR;
    icdGlobalDispatchTable.clEnqueueReleaseD3D11ObjectsKHR = clEnqueueReleaseD3D11ObjectsKHR;
    icdGlobalDispatchTable.clGetDeviceIDsFromD3D11KHR = clGetDeviceIDsFromD3D11KHR;
}

void *D3DSharingBuilderFactory<D3DTypesHelper::D3D9>::getExtensionFunctionAddress(const std::string &functionName) {
    if (DebugManager.flags.EnableFormatQuery.get() &&
        functionName == "clGetSupportedDX9MediaSurfaceFormatsINTEL") {
        return ((void *)(clGetSupportedDX9MediaSurfaceFormatsINTEL));
    }
    return nullptr;
}

void *D3DSharingBuilderFactory<D3DTypesHelper::D3D10>::getExtensionFunctionAddress(const std::string &functionName) {
    if (DebugManager.flags.EnableFormatQuery.get() &&
        functionName == "clGetSupportedD3D10TextureFormatsINTEL") {
        return ((void *)(clGetSupportedD3D10TextureFormatsINTEL));
    }
    return nullptr;
}

void *D3DSharingBuilderFactory<D3DTypesHelper::D3D11>::getExtensionFunctionAddress(const std::string &functionName) {
    if (DebugManager.flags.EnableFormatQuery.get() &&
        functionName == "clGetSupportedD3D11TextureFormatsINTEL") {
        return ((void *)(clGetSupportedD3D11TextureFormatsINTEL));
    }
    return nullptr;
}
static SharingFactory::RegisterSharing<D3DSharingBuilderFactory<D3DTypesHelper::D3D9>, D3DSharingFunctions<D3DTypesHelper::D3D9>> D3D9Sharing;
static SharingFactory::RegisterSharing<D3DSharingBuilderFactory<D3DTypesHelper::D3D10>, D3DSharingFunctions<D3DTypesHelper::D3D10>> D3D10Sharing;
static SharingFactory::RegisterSharing<D3DSharingBuilderFactory<D3DTypesHelper::D3D11>, D3DSharingFunctions<D3DTypesHelper::D3D11>> D3D11Sharing;
} // namespace NEO
#endif
