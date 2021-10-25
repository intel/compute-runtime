/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/utilities/api_intercept.h"

#include "opencl/source/api/api.h"
#include "opencl/source/api/dispatch.h"
#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/sharings/d3d/d3d_buffer.h"
#include "opencl/source/sharings/d3d/d3d_surface.h"
#include "opencl/source/sharings/d3d/d3d_texture.h"
#include "opencl/source/utilities/cl_logger.h"

using namespace NEO;

ClDevice *pickDeviceWithAdapterLuid(Platform *platform, LUID adapterLuid) {
    ClDevice *deviceToReturn = nullptr;
    for (auto i = 0u; i < platform->getNumDevices(); i++) {
        auto device = platform->getClDevice(i);
        if (device->getRootDeviceEnvironment().osInterface->getDriverModel()->as<Wddm>()->verifyAdapterLuid(adapterLuid)) {
            deviceToReturn = device;
            break;
        }
    }
    return deviceToReturn;
}

void NEO::MemObj::getOsSpecificMemObjectInfo(const cl_mem_info &paramName, size_t *srcParamSize, void **srcParam) {
    switch (paramName) {
    case CL_MEM_D3D10_RESOURCE_KHR:
        *srcParamSize = sizeof(ID3D10Resource *);
        *srcParam = static_cast<D3DSharing<D3DTypesHelper::D3D10> *>(peekSharingHandler())->getResourceHandler();
        break;
    case CL_MEM_D3D11_RESOURCE_KHR:
        *srcParamSize = sizeof(ID3D11Resource *);
        *srcParam = static_cast<D3DSharing<D3DTypesHelper::D3D11> *>(peekSharingHandler())->getResourceHandler();
        break;
    case CL_MEM_DX9_MEDIA_SURFACE_INFO_KHR:
        *srcParamSize = sizeof(cl_dx9_surface_info_khr);
        *srcParam = &static_cast<D3DSurface *>(peekSharingHandler())->getSurfaceInfo();
        break;
    case CL_MEM_DX9_MEDIA_ADAPTER_TYPE_KHR:
        *srcParamSize = sizeof(cl_dx9_media_adapter_type_khr);
        *srcParam = &static_cast<D3DSurface *>(peekSharingHandler())->getAdapterType();
        break;
    case CL_MEM_DX9_RESOURCE_INTEL:
        *srcParamSize = sizeof(IDirect3DSurface9 *);
        *srcParam = &static_cast<D3DSurface *>(peekSharingHandler())->getSurfaceInfo().resource;
        break;
    case CL_MEM_DX9_SHARED_HANDLE_INTEL:
        *srcParamSize = sizeof(HANDLE);
        *srcParam = &static_cast<D3DSurface *>(peekSharingHandler())->getSurfaceInfo().shared_handle;
        break;
    }
}

void NEO::Image::getOsSpecificImageInfo(const cl_mem_info &paramName, size_t *srcParamSize, void **srcParam) {
    switch (paramName) {
    case CL_IMAGE_D3D10_SUBRESOURCE_KHR:
        *srcParamSize = sizeof(unsigned int);
        *srcParam = &static_cast<D3DSharing<D3DTypesHelper::D3D10> *>(peekSharingHandler())->getSubresource();
        break;
    case CL_IMAGE_D3D11_SUBRESOURCE_KHR:
        *srcParamSize = sizeof(unsigned int);
        *srcParam = &static_cast<D3DSharing<D3DTypesHelper::D3D11> *>(peekSharingHandler())->getSubresource();
        break;
    case CL_IMAGE_DX9_MEDIA_PLANE_KHR:
    case CL_IMAGE_DX9_PLANE_INTEL:
        *srcParamSize = sizeof(cl_uint);
        *srcParam = &static_cast<D3DSurface *>(peekSharingHandler())->getPlane();
        break;
    }
}

void *NEO::Context::getOsContextInfo(cl_context_info &paramName, size_t *srcParamSize) {
    switch (paramName) {
    case CL_CONTEXT_D3D10_PREFER_SHARED_RESOURCES_KHR:
    case CL_CONTEXT_D3D11_PREFER_SHARED_RESOURCES_KHR:
        *srcParamSize = sizeof(cl_bool);
        return &preferD3dSharedResources;
    default:
        break;
    }
    return nullptr;
}

cl_int CL_API_CALL clGetDeviceIDsFromDX9INTEL(cl_platform_id platform, cl_dx9_device_source_intel dx9DeviceSource, void *dx9Object,
                                              cl_dx9_device_set_intel dx9DeviceSet, cl_uint numEntries, cl_device_id *devices, cl_uint *numDevices) {
    Platform *platformInternal = nullptr;
    auto retVal = validateObjects(WithCastToInternal(platform, &platformInternal));
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("platform", platform,
                   "dx9DeviceSource", dx9DeviceSource,
                   "dx9Object", dx9Object,
                   "dx9DeviceSet", dx9DeviceSet,
                   "numEntries", numEntries,
                   "devices", devices,
                   "numDevices", numDevices);
    if (retVal != CL_SUCCESS) {
        return retVal;
    }
    cl_device_id device = platformInternal->getClDevice(0);

    GetInfoHelper::set(devices, device);
    GetInfoHelper::set(numDevices, 1u);
    retVal = CL_SUCCESS;
    return retVal;
}

cl_mem CL_API_CALL clCreateFromDX9MediaSurfaceINTEL(cl_context context, cl_mem_flags flags, IDirect3DSurface9 *resource,
                                                    HANDLE sharedHandle, UINT plane, cl_int *errcodeRet) {
    API_ENTER(errcodeRet);
    DBG_LOG_INPUTS("context", context,
                   "flags", flags,
                   "resource", resource,
                   "sharedHandle", sharedHandle,
                   "plane", plane);
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);
    cl_mem_flags validFlags = CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY;
    if ((flags & (~validFlags)) != 0) {
        err.set(CL_INVALID_VALUE);
        return nullptr;
    }
    if (!resource) {
        err.set(CL_INVALID_DX9_RESOURCE_INTEL);
        return nullptr;
    }

    cl_dx9_surface_info_khr surfaceInfo = {resource, sharedHandle};
    auto ctx = castToObject<Context>(context);

    if (ctx) {
        return D3DSurface::create(ctx, &surfaceInfo, flags, 0, plane, errcodeRet);
    } else {
        err.set(CL_INVALID_CONTEXT);
        return nullptr;
    }
}

cl_int CL_API_CALL clEnqueueAcquireDX9ObjectsINTEL(cl_command_queue commandQueue, cl_uint numObjects, const cl_mem *memObjects,
                                                   cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) {
    CommandQueue *cmdQ = nullptr;

    auto retVal = validateObjects(WithCastToInternal(commandQueue, &cmdQ));
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue,
                   "numObjects", numObjects,
                   "memObjects", memObjects,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));
    if (retVal != CL_SUCCESS) {
        return retVal;
    }
    retVal = cmdQ->enqueueAcquireSharedObjects(numObjects, memObjects, numEventsInWaitList,
                                               eventWaitList, event, CL_COMMAND_ACQUIRE_DX9_OBJECTS_INTEL);
    return retVal;
}

cl_int CL_API_CALL clEnqueueReleaseDX9ObjectsINTEL(cl_command_queue commandQueue, cl_uint numObjects, cl_mem *memObjects,
                                                   cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) {
    CommandQueue *cmdQ = nullptr;
    auto retVal = validateObjects(WithCastToInternal(commandQueue, &cmdQ));
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue,
                   "numObjects", numObjects,
                   "memObjects", memObjects,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));
    if (retVal != CL_SUCCESS) {
        return retVal;
    }

    for (unsigned int object = 0; object < numObjects; object++) {
        auto memObject = castToObject<MemObj>(memObjects[object]);
        if (!static_cast<D3DSharing<D3DTypesHelper::D3D9> *>(memObject->peekSharingHandler())->isSharedResource()) {
            cmdQ->finish();
            break;
        }
    }

    retVal = cmdQ->enqueueReleaseSharedObjects(numObjects, memObjects, numEventsInWaitList,
                                               eventWaitList, event, CL_COMMAND_RELEASE_DX9_OBJECTS_INTEL);
    if (!cmdQ->getContext().getInteropUserSyncEnabled()) {
        cmdQ->finish();
    }
    return retVal;
}

cl_int CL_API_CALL clGetDeviceIDsFromDX9MediaAdapterKHR(cl_platform_id platform, cl_uint numMediaAdapters,
                                                        cl_dx9_media_adapter_type_khr *mediaAdapterType, void *mediaAdapters,
                                                        cl_dx9_media_adapter_set_khr mediaAdapterSet, cl_uint numEntries,
                                                        cl_device_id *devices, cl_uint *numDevices) {
    Platform *platformInternal = nullptr;
    auto retVal = validateObjects(WithCastToInternal(platform, &platformInternal));
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("platform", platform,
                   "numMediaAdapters", numMediaAdapters,
                   "mediaAdapterType", mediaAdapterType,
                   "mediaAdapters", mediaAdapters,
                   "mediaAdapterSet", mediaAdapterSet,
                   "numEntries", numEntries,
                   "devices", devices,
                   "numDevices", numDevices);
    if (retVal != CL_SUCCESS) {
        return retVal;
    }
    cl_device_id device = platformInternal->getClDevice(0);

    GetInfoHelper::set(devices, device);
    GetInfoHelper::set(numDevices, 1u);
    retVal = CL_SUCCESS;
    return retVal;
}

cl_mem CL_API_CALL clCreateFromDX9MediaSurfaceKHR(cl_context context, cl_mem_flags flags, cl_dx9_media_adapter_type_khr adapterType,
                                                  void *surfaceInfo, cl_uint plane, cl_int *errcodeRet) {

    API_ENTER(errcodeRet);
    DBG_LOG_INPUTS("context", context,
                   "flags", flags,
                   "adapterType", adapterType,
                   "surfaceInfo", surfaceInfo,
                   "plane", plane);
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);
    auto localSurfaceInfo = (cl_dx9_surface_info_khr *)surfaceInfo;
    auto ctx = castToObject<Context>(context);

    return D3DSurface::create(ctx, localSurfaceInfo, flags, adapterType, plane, errcodeRet);
}

cl_int CL_API_CALL clEnqueueAcquireDX9MediaSurfacesKHR(cl_command_queue commandQueue, cl_uint numObjects, const cl_mem *memObjects,
                                                       cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) {
    CommandQueue *cmdQ = nullptr;
    auto retVal = validateObjects(WithCastToInternal(commandQueue, &cmdQ));
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue,
                   "numObjects", numObjects,
                   "memObjects", memObjects,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));
    if (retVal != CL_SUCCESS) {
        return retVal;
    }
    retVal = cmdQ->enqueueAcquireSharedObjects(numObjects, memObjects, numEventsInWaitList,
                                               eventWaitList, event, CL_COMMAND_ACQUIRE_DX9_MEDIA_SURFACES_KHR);
    return retVal;
}

cl_int CL_API_CALL clEnqueueReleaseDX9MediaSurfacesKHR(cl_command_queue commandQueue, cl_uint numObjects, const cl_mem *memObjects,
                                                       cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) {
    CommandQueue *cmdQ = nullptr;

    auto retVal = validateObjects(WithCastToInternal(commandQueue, &cmdQ));
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue,
                   "numObjects", numObjects,
                   "memObjects", memObjects,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));
    if (retVal != CL_SUCCESS) {
        return retVal;
    }

    for (unsigned int object = 0; object < numObjects; object++) {
        auto memObject = castToObject<MemObj>(memObjects[object]);
        if (memObject) {
            if (!static_cast<D3DSharing<D3DTypesHelper::D3D9> *>(memObject->peekSharingHandler())->isSharedResource()) {
                cmdQ->finish();
                break;
            }

        } else {
            retVal = CL_INVALID_MEM_OBJECT;
            return retVal;
        }
    }

    retVal = cmdQ->enqueueReleaseSharedObjects(numObjects, memObjects, numEventsInWaitList,
                                               eventWaitList, event, CL_COMMAND_RELEASE_DX9_MEDIA_SURFACES_KHR);
    if (!cmdQ->getContext().getInteropUserSyncEnabled()) {
        cmdQ->finish();
    }
    return retVal;
}

cl_int CL_API_CALL clGetDeviceIDsFromD3D10KHR(cl_platform_id platform, cl_d3d10_device_source_khr d3dDeviceSource,
                                              void *d3dObject, cl_d3d10_device_set_khr d3dDeviceSet, cl_uint numEntries,
                                              cl_device_id *devices, cl_uint *numDevices) {
    DXGI_ADAPTER_DESC dxgiDesc = {{0}};
    IDXGIAdapter *dxgiAdapter = nullptr;
    ID3D10Device *d3dDevice = nullptr;
    D3DSharingFunctions<D3DTypesHelper::D3D10> sharingFcns((ID3D10Device *)nullptr);
    cl_uint localNumDevices = 0;
    cl_int retCode = CL_SUCCESS;

    Platform *platformInternal = nullptr;
    ClDevice *device = nullptr;
    auto retVal = validateObjects(WithCastToInternal(platform, &platformInternal));
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("platform", platform,
                   "d3dDeviceSource", d3dDeviceSource,
                   "d3dObject", d3dObject,
                   "d3dDeviceSet", d3dDeviceSet,
                   "numEntries", numEntries,
                   "devices", devices,
                   "numDevices", numDevices);
    if (retVal != CL_SUCCESS) {
        return retVal;
    }

    if (DebugManager.injectFcn) {
        sharingFcns.getDxgiDescFcn = (D3DSharingFunctions<D3DTypesHelper::D3D10>::GetDxgiDescFcn)DebugManager.injectFcn;
    }

    switch (d3dDeviceSource) {
    case CL_D3D10_DEVICE_KHR:
        d3dDevice = (ID3D10Device *)d3dObject;
        break;
    case CL_D3D10_DXGI_ADAPTER_KHR:
        dxgiAdapter = (IDXGIAdapter *)d3dObject;
        break;
    default:
        GetInfoHelper::set(numDevices, localNumDevices);
        retVal = CL_INVALID_VALUE;
        return retVal;
    }

    sharingFcns.getDxgiDescFcn(&dxgiDesc, dxgiAdapter, d3dDevice);
    if (dxgiDesc.VendorId != INTEL_VENDOR_ID) {
        GetInfoHelper::set(numDevices, localNumDevices);
        retVal = CL_DEVICE_NOT_FOUND;
        return retVal;
    }

    switch (d3dDeviceSet) {
    case CL_PREFERRED_DEVICES_FOR_D3D10_KHR:
    case CL_ALL_DEVICES_FOR_D3D10_KHR:
        device = pickDeviceWithAdapterLuid(platformInternal, dxgiDesc.AdapterLuid);
        if (device) {
            GetInfoHelper::set(devices, static_cast<cl_device_id>(device));
            localNumDevices = 1;
        } else {
            retCode = CL_DEVICE_NOT_FOUND;
        }
        break;
    default:
        retCode = CL_INVALID_VALUE;
        break;
    }

    GetInfoHelper::set(numDevices, localNumDevices);

    return retCode;
}

cl_mem CL_API_CALL clCreateFromD3D10BufferKHR(cl_context context, cl_mem_flags flags, ID3D10Buffer *resource, cl_int *errcodeRet) {

    API_ENTER(errcodeRet);
    DBG_LOG_INPUTS("context", context,
                   "flags", flags,
                   "resource", resource);
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);
    Context *ctx = nullptr;

    err.set(validateObjects(WithCastToInternal(context, &ctx)));

    if (err.localErrcode != CL_SUCCESS) {
        return nullptr;
    }

    if (ctx->getSharing<D3DSharingFunctions<D3DTypesHelper::D3D10>>()->isTracked(resource, 0)) {
        err.set(CL_INVALID_D3D10_RESOURCE_KHR);
        return nullptr;
    }
    return D3DBuffer<D3DTypesHelper::D3D10>::create(ctx, resource, flags, errcodeRet);
}

cl_mem CL_API_CALL clCreateFromD3D10Texture2DKHR(cl_context context, cl_mem_flags flags, ID3D10Texture2D *resource,
                                                 UINT subresource, cl_int *errcodeRet) {
    API_ENTER(errcodeRet);
    DBG_LOG_INPUTS("context", context,
                   "flags", flags,
                   "resource", resource,
                   "subresource", subresource);
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);
    Context *ctx = nullptr;

    err.set(validateObjects(WithCastToInternal(context, &ctx)));

    if (err.localErrcode != CL_SUCCESS) {
        return nullptr;
    }

    if (ctx->getSharing<D3DSharingFunctions<D3DTypesHelper::D3D10>>()->isTracked(resource, subresource)) {
        err.set(CL_INVALID_D3D10_RESOURCE_KHR);
        return nullptr;
    }
    return D3DTexture<D3DTypesHelper::D3D10>::create2d(ctx, resource, flags, subresource, errcodeRet);
}

cl_mem CL_API_CALL clCreateFromD3D10Texture3DKHR(cl_context context, cl_mem_flags flags, ID3D10Texture3D *resource,
                                                 UINT subresource, cl_int *errcodeRet) {
    API_ENTER(errcodeRet);
    DBG_LOG_INPUTS("context", context,
                   "flags", flags,
                   "resource", resource,
                   "subresource", subresource);
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);
    Context *ctx = nullptr;

    err.set(validateObjects(WithCastToInternal(context, &ctx)));

    if (err.localErrcode != CL_SUCCESS) {
        return nullptr;
    }

    if (ctx->getSharing<D3DSharingFunctions<D3DTypesHelper::D3D10>>()->isTracked(resource, subresource)) {
        err.set(CL_INVALID_D3D10_RESOURCE_KHR);
        return nullptr;
    }
    return D3DTexture<D3DTypesHelper::D3D10>::create3d(ctx, resource, flags, subresource, errcodeRet);
}

cl_int CL_API_CALL clEnqueueAcquireD3D10ObjectsKHR(cl_command_queue commandQueue, cl_uint numObjects, const cl_mem *memObjects,
                                                   cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) {
    CommandQueue *cmdQ = nullptr;

    auto retVal = validateObjects(WithCastToInternal(commandQueue, &cmdQ));
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue,
                   "numObjects", numObjects,
                   "memObjects", memObjects,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));
    if (retVal != CL_SUCCESS) {
        return retVal;
    }

    retVal = validateObjects(MemObjList(numObjects, memObjects));
    if (retVal != CL_SUCCESS) {
        return retVal;
    }

    for (unsigned int object = 0; object < numObjects; object++) {
        auto memObj = castToObject<MemObj>(memObjects[object]);
        if (memObj->acquireCount >= 1) {
            retVal = CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR;
            return retVal;
        }
    }
    retVal = cmdQ->enqueueAcquireSharedObjects(numObjects, memObjects, numEventsInWaitList,
                                               eventWaitList, event, CL_COMMAND_ACQUIRE_D3D10_OBJECTS_KHR);
    return retVal;
}

cl_int CL_API_CALL clEnqueueReleaseD3D10ObjectsKHR(cl_command_queue commandQueue, cl_uint numObjects, const cl_mem *memObjects,
                                                   cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) {
    CommandQueue *cmdQ = nullptr;

    auto retVal = validateObjects(WithCastToInternal(commandQueue, &cmdQ));
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue,
                   "numObjects", numObjects,
                   "memObjects", memObjects,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));
    if (retVal != CL_SUCCESS) {
        return retVal;
    }

    retVal = validateObjects(MemObjList(numObjects, memObjects));
    if (retVal != CL_SUCCESS) {
        return retVal;
    }

    for (unsigned int object = 0; object < numObjects; object++) {
        auto memObject = castToObject<MemObj>(memObjects[object]);
        if (memObject->acquireCount == 0) {
            retVal = CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR;
            return retVal;
        }
        if (!static_cast<D3DSharing<D3DTypesHelper::D3D10> *>(memObject->peekSharingHandler())->isSharedResource()) {
            cmdQ->finish();
            break;
        }
    }

    retVal = cmdQ->enqueueReleaseSharedObjects(numObjects, memObjects, numEventsInWaitList,
                                               eventWaitList, event, CL_COMMAND_RELEASE_D3D10_OBJECTS_KHR);
    if (!cmdQ->getContext().getInteropUserSyncEnabled()) {
        cmdQ->finish();
    }
    return retVal;
}

cl_int CL_API_CALL clGetDeviceIDsFromD3D11KHR(cl_platform_id platform, cl_d3d11_device_source_khr d3dDeviceSource,
                                              void *d3dObject, cl_d3d11_device_set_khr d3dDeviceSet,
                                              cl_uint numEntries, cl_device_id *devices, cl_uint *numDevices) {
    DXGI_ADAPTER_DESC dxgiDesc = {{0}};
    IDXGIAdapter *dxgiAdapter = nullptr;
    ID3D11Device *d3dDevice = nullptr;
    D3DSharingFunctions<D3DTypesHelper::D3D11> sharingFcns((ID3D11Device *)nullptr);
    cl_uint localNumDevices = 0;

    Platform *platformInternal = nullptr;
    ClDevice *device = nullptr;
    auto retVal = validateObjects(WithCastToInternal(platform, &platformInternal));
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("platform", platform,
                   "d3dDeviceSource", d3dDeviceSource,
                   "d3dObject", d3dObject,
                   "d3dDeviceSet", d3dDeviceSet,
                   "numEntries", numEntries,
                   "devices", devices,
                   "numDevices", numDevices);
    if (retVal != CL_SUCCESS) {
        return retVal;
    }

    if (DebugManager.injectFcn) {
        sharingFcns.getDxgiDescFcn = (D3DSharingFunctions<D3DTypesHelper::D3D11>::GetDxgiDescFcn)DebugManager.injectFcn;
    }

    switch (d3dDeviceSource) {
    case CL_D3D11_DEVICE_KHR:
        d3dDevice = (ID3D11Device *)d3dObject;
        break;
    case CL_D3D11_DXGI_ADAPTER_KHR:
        dxgiAdapter = (IDXGIAdapter *)d3dObject;
        break;
    default:
        GetInfoHelper::set(numDevices, localNumDevices);
        retVal = CL_INVALID_VALUE;
        return retVal;
        break;
    }

    sharingFcns.getDxgiDescFcn(&dxgiDesc, dxgiAdapter, d3dDevice);
    if (dxgiDesc.VendorId != INTEL_VENDOR_ID) {
        GetInfoHelper::set(numDevices, localNumDevices);
        retVal = CL_DEVICE_NOT_FOUND;
        return retVal;
    }

    switch (d3dDeviceSet) {
    case CL_PREFERRED_DEVICES_FOR_D3D11_KHR:
    case CL_ALL_DEVICES_FOR_D3D11_KHR:
        device = pickDeviceWithAdapterLuid(platformInternal, dxgiDesc.AdapterLuid);
        if (device) {
            GetInfoHelper::set(devices, static_cast<cl_device_id>(device));
            localNumDevices = 1;
        } else {
            retVal = CL_DEVICE_NOT_FOUND;
        }
        break;
    default:
        retVal = CL_INVALID_VALUE;
        break;
    }

    GetInfoHelper::set(numDevices, localNumDevices);

    return retVal;
}

cl_mem CL_API_CALL clCreateFromD3D11BufferKHR(cl_context context, cl_mem_flags flags, ID3D11Buffer *resource, cl_int *errcodeRet) {
    API_ENTER(errcodeRet);
    DBG_LOG_INPUTS("context", context,
                   "flags", flags,
                   "resource", resource);
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);
    Context *ctx = nullptr;

    err.set(validateObjects(WithCastToInternal(context, &ctx)));
    if (err.localErrcode != CL_SUCCESS) {
        return nullptr;
    }

    if (ctx->getSharing<D3DSharingFunctions<D3DTypesHelper::D3D11>>()->isTracked(resource, 0)) {
        err.set(CL_INVALID_D3D11_RESOURCE_KHR);
        return nullptr;
    }
    return D3DBuffer<D3DTypesHelper::D3D11>::create(ctx, resource, flags, errcodeRet);
}

cl_mem CL_API_CALL clCreateFromD3D11Texture2DKHR(cl_context context, cl_mem_flags flags, ID3D11Texture2D *resource,
                                                 UINT subresource, cl_int *errcodeRet) {
    API_ENTER(errcodeRet);
    DBG_LOG_INPUTS("context", context,
                   "flags", flags,
                   "resource", resource,
                   "subresource", subresource);
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);
    Context *ctx = nullptr;

    err.set(validateObjects(WithCastToInternal(context, &ctx)));
    if (err.localErrcode != CL_SUCCESS) {
        return nullptr;
    }

    if (ctx->getSharing<D3DSharingFunctions<D3DTypesHelper::D3D11>>()->isTracked(resource, subresource)) {
        err.set(CL_INVALID_D3D11_RESOURCE_KHR);
        return nullptr;
    }
    return D3DTexture<D3DTypesHelper::D3D11>::create2d(ctx, resource, flags, subresource, errcodeRet);
}

cl_mem CL_API_CALL clCreateFromD3D11Texture3DKHR(cl_context context, cl_mem_flags flags, ID3D11Texture3D *resource,
                                                 UINT subresource, cl_int *errcodeRet) {
    API_ENTER(errcodeRet);
    DBG_LOG_INPUTS("context", context,
                   "flags", flags,
                   "resource", resource,
                   "subresource", subresource);
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);
    Context *ctx = nullptr;

    err.set(validateObjects(WithCastToInternal(context, &ctx)));

    if (err.localErrcode != CL_SUCCESS) {
        return nullptr;
    }

    if (ctx->getSharing<D3DSharingFunctions<D3DTypesHelper::D3D11>>()->isTracked(resource, subresource)) {
        err.set(CL_INVALID_D3D11_RESOURCE_KHR);
        return nullptr;
    }
    return D3DTexture<D3DTypesHelper::D3D11>::create3d(ctx, resource, flags, subresource, errcodeRet);
}

cl_int CL_API_CALL clEnqueueAcquireD3D11ObjectsKHR(cl_command_queue commandQueue, cl_uint numObjects, const cl_mem *memObjects,
                                                   cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) {
    CommandQueue *cmdQ = nullptr;

    auto retVal = validateObjects(WithCastToInternal(commandQueue, &cmdQ));
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue,
                   "numObjects", numObjects,
                   "memObjects", memObjects,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));
    if (retVal != CL_SUCCESS) {
        return retVal;
    }

    retVal = validateObjects(MemObjList(numObjects, memObjects));
    if (retVal != CL_SUCCESS) {
        return retVal;
    }

    for (unsigned int object = 0; object < numObjects; object++) {
        auto memObj = castToObject<MemObj>(memObjects[object]);
        if (memObj->acquireCount >= 1) {
            retVal = CL_D3D11_RESOURCE_ALREADY_ACQUIRED_KHR;
            return retVal;
        }
    }
    retVal = cmdQ->enqueueAcquireSharedObjects(numObjects, memObjects, numEventsInWaitList,
                                               eventWaitList, event, CL_COMMAND_ACQUIRE_D3D11_OBJECTS_KHR);
    return retVal;
}

cl_int CL_API_CALL clEnqueueReleaseD3D11ObjectsKHR(cl_command_queue commandQueue, cl_uint numObjects, const cl_mem *memObjects,
                                                   cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) {
    CommandQueue *cmdQ = nullptr;

    auto retVal = validateObjects(WithCastToInternal(commandQueue, &cmdQ));
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue,
                   "numObjects", numObjects,
                   "memObjects", memObjects,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));
    if (retVal != CL_SUCCESS) {
        return retVal;
    }

    retVal = validateObjects(MemObjList(numObjects, memObjects));
    if (retVal != CL_SUCCESS) {
        return retVal;
    }

    for (unsigned int object = 0; object < numObjects; object++) {
        auto memObject = castToObject<MemObj>(memObjects[object]);
        if (memObject->acquireCount == 0) {
            retVal = CL_D3D11_RESOURCE_NOT_ACQUIRED_KHR;
            return retVal;
        }
        if (!static_cast<D3DSharing<D3DTypesHelper::D3D11> *>(memObject->peekSharingHandler())->isSharedResource()) {
            cmdQ->finish();
            break;
        }
    }

    retVal = cmdQ->enqueueReleaseSharedObjects(numObjects, memObjects, numEventsInWaitList,
                                               eventWaitList, event, CL_COMMAND_RELEASE_D3D11_OBJECTS_KHR);
    if (!cmdQ->getContext().getInteropUserSyncEnabled()) {
        cmdQ->finish();
    }
    return retVal;
}

cl_int CL_API_CALL clGetSupportedDX9MediaSurfaceFormatsINTEL(cl_context context, cl_mem_flags flags,
                                                             cl_mem_object_type imageType, cl_uint plane,
                                                             cl_uint numEntries, D3DFORMAT *dx9Formats,
                                                             cl_uint *numImageFormats) {

    if (validateObject(context) != CL_SUCCESS) {
        return CL_INVALID_CONTEXT;
    }

    if ((imageType != CL_MEM_OBJECT_BUFFER) && (imageType != CL_MEM_OBJECT_IMAGE2D) && (imageType != CL_MEM_OBJECT_IMAGE3D)) {
        return CL_INVALID_VALUE;
    }

    if (((flags & CL_MEM_READ_WRITE) == 0) && ((flags & CL_MEM_WRITE_ONLY) == 0) && ((flags & CL_MEM_READ_ONLY) == 0)) {
        return CL_INVALID_VALUE;
    }

    cl_uint i = 0;
    switch (plane) {
    case 0:
        for (auto format : D3DSurface::D3DtoClFormatConversions) {
            if (i >= numEntries) {
                break;
            }
            dx9Formats[i++] = format.first;
        }
        *numImageFormats = static_cast<cl_uint>(D3DSurface::D3DtoClFormatConversions.size());
        break;
    case 1:
        for (auto format : D3DSurface::D3DPlane1Formats) {
            if (i >= numEntries) {
                break;
            }
            dx9Formats[i++] = format;
        }
        *numImageFormats = static_cast<cl_uint>(D3DSurface::D3DPlane1Formats.size());
        break;
    case 2:
        for (auto format : D3DSurface::D3DPlane2Formats) {
            if (i >= numEntries) {
                break;
            }
            dx9Formats[i++] = format;
        }
        *numImageFormats = static_cast<cl_uint>(D3DSurface::D3DPlane2Formats.size());
        break;
    default:
        *numImageFormats = 0;
    }

    return CL_SUCCESS;
}

cl_int CL_API_CALL clGetSupportedD3D10TextureFormatsINTEL(cl_context context, cl_mem_flags flags,
                                                          cl_mem_object_type imageType,
                                                          cl_uint numEntries, DXGI_FORMAT *formats, cl_uint *numTextureFormats) {
    return getSupportedDXTextureFormats<D3DTypesHelper::D3D10>(context, imageType, 0, numEntries, formats, numTextureFormats);
}

cl_int CL_API_CALL clGetSupportedD3D11TextureFormatsINTEL(cl_context context, cl_mem_flags flags,
                                                          cl_mem_object_type imageType, cl_uint plane,
                                                          cl_uint numEntries, DXGI_FORMAT *formats, cl_uint *numTextureFormats) {
    return getSupportedDXTextureFormats<D3DTypesHelper::D3D11>(context, imageType, plane, numEntries, formats, numTextureFormats);
}
