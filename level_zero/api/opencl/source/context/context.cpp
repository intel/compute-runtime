/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/context/context.h"

#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/hw_info.h"

#include "level_zero/api/opencl/source/extensions/public/cl_ext_private.h"
#include "level_zero/api/opencl/source/helpers/get_info_status_mapper.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/helpers/surface_formats.h"
#include "level_zero/api/opencl/source/sharings/sharing_factory.h"
#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include <level_zero/api/opencl/source/platform/platform.h>
#include <level_zero/ze_api.h>

#include <ranges>
#include <set>

namespace NEO {
namespace LEO {

Context::Context(const cl_context_properties *properties, ze_context_handle_t contextHandle, cl_uint numDevices, const cl_device_id *devices, bool externalHandle) : contextHandle(contextHandle), externalHandle(externalHandle) {
    this->clDevices.reserve(numDevices);
    for (uint32_t i = 0; i < numDevices; ++i) {
        this->clDevices.push_back(NEO::LEO::castToObject<ClDevice>(devices[i]));
    }

    this->storeProperties(properties);
}

Context::~Context() {
    if (this->internalComputeCmdList) {
        zeCommandListDestroy(this->internalComputeCmdList);
    }
    if (this->internalCopyCmdList) {
        zeCommandListDestroy(this->internalCopyCmdList);
    }
    if (!externalHandle && this->contextHandle) {
        zeContextDestroy(this->contextHandle);
    }

    for (auto callback : std::ranges::reverse_view(callbacks)) {
        callback.first(this, callback.second);
    }
}

cl_int Context::initialize() {
    ze_result_t resultValue = ZE_RESULT_SUCCESS;
    ze_command_queue_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
                                           nullptr,
                                           0,
                                           0,
                                           ZE_COMMAND_QUEUE_FLAG_COPY_OFFLOAD_HINT | ZE_COMMAND_QUEUE_FLAG_IN_ORDER,
                                           ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
                                           ZE_COMMAND_QUEUE_PRIORITY_NORMAL};

    this->internalCopyCmdList = L0::CommandList::createImmediate(clDevices[0]->getHardwareInfo().platform.eProductFamily, clDevices[0]->getL0Object(), &cmdListDesc, true, NEO::EngineGroupType::copy, resultValue);
    if (resultValue != ZE_RESULT_SUCCESS) {
        return L0ToClResultMapper(resultValue);
    }

    this->internalComputeCmdList = L0::CommandList::createImmediate(clDevices[0]->getHardwareInfo().platform.eProductFamily, clDevices[0]->getL0Object(), &cmdListDesc, true, NEO::EngineGroupType::compute, resultValue);
    if (resultValue != ZE_RESULT_SUCCESS) {
        return L0ToClResultMapper(resultValue);
    }

    sharingFunctions.resize(SharingType::MAX_SHARING_VALUE);

    auto sharingBuilder = sharingFactory.build();
    auto properties = this->contextProperties.data();

    std::set<cl_context_properties> parsedProperties{};

    if (properties != nullptr) {
        while (*properties != 0) {
            auto propertyType = properties[0];
            auto propertyValue = properties[1];
            properties += 2;

            auto insertRet = parsedProperties.insert(propertyType);
            if (!insertRet.second) {
                return CL_INVALID_PROPERTY;
            }

            switch (propertyType) {
            case CL_CONTEXT_PLATFORM: {
                auto pPlatform = NEO::LEO::castToObject<NEO::LEO::Platform>(reinterpret_cast<cl_platform_id>(propertyValue));
                if (!pPlatform) {
                    return CL_INVALID_PLATFORM;
                }
                break;
            }
            case CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL:
            case CL_L0_CONTEXT_HANDLE:
                break;
            case CL_CONTEXT_INTEROP_USER_SYNC:
                if (propertyValue != CL_FALSE && propertyValue != CL_TRUE) {
                    return CL_INVALID_PROPERTY;
                }
                this->setInteropUserSyncEnabled(propertyValue > 0);
                break;
            default: {
                if (sharingBuilder->processProperties(propertyType, propertyValue)) {
                    break;
                }
                return CL_INVALID_PROPERTY;
            }
            }
        }
    }

    cl_int errcodeRet = CL_SUCCESS;
    sharingBuilder->finalizeProperties(*this, errcodeRet);

    return errcodeRet;
}

cl_int Context::getInfo(cl_context_info paramName, size_t paramValueSize,
                        void *paramValue, size_t *paramValueSizeRet) {
    size_t valueSize = GetInfo::invalidSourceSize;
    const void *pValue = nullptr;
    cl_uint numDevices = 0;
    cl_uint refCount = 0;
    std::vector<cl_device_id> devIDs(clDevices.begin(), clDevices.end());

    switch (paramName) {
    case CL_CONTEXT_DEVICES:
        valueSize = clDevices.size() * sizeof(cl_device_id);
        pValue = devIDs.data();
        break;

    case CL_CONTEXT_NUM_DEVICES:
        numDevices = static_cast<cl_uint>(clDevices.size());
        valueSize = sizeof(numDevices);
        pValue = &numDevices;
        break;

    case CL_CONTEXT_PROPERTIES:
        valueSize = this->contextProperties.size() * sizeof(cl_context_properties);
        pValue = this->contextProperties.data();
        break;

    case CL_CONTEXT_REFERENCE_COUNT:
        refCount = static_cast<cl_uint>(this->getReference());
        valueSize = sizeof(refCount);
        pValue = &refCount;
        break;

    case CL_L0_CONTEXT_HANDLE: {
        pValue = &this->contextHandle;
        valueSize = sizeof(this->contextHandle);
        break;
    }

    default: {
        pValue = this->getOsContextInfo(paramName, &valueSize);
        break;
    }
    }

    GetInfoStatus getInfoStatus = GetInfoStatus::success;
    getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, pValue, valueSize);

    auto retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
    GetInfo::setParamValueReturnSize(paramValueSizeRet, valueSize, getInfoStatus);

    return retVal;
}

cl_int Context::getMemAllocInfo(const void *ptr,
                                cl_mem_info_intel paramName,
                                size_t paramValueSize,
                                void *paramValue,
                                size_t *paramValueSizeRet) {
    size_t valueSize = GetInfo::invalidSourceSize;
    const void *pValue = nullptr;

    cl_int memAllocType = CL_MEM_TYPE_UNKNOWN_INTEL;
    cl_device_id deviceId = nullptr;
    void *basePtr = nullptr;
    size_t allocSize = 0u;
    cl_mem_alloc_flags_intel allocFlags = 0u;

    ze_memory_allocation_properties_t memoryProperties{};
    ze_device_handle_t l0Device = nullptr;

    switch (paramName) {
    case CL_MEM_ALLOC_TYPE_INTEL:
    case CL_MEM_ALLOC_DEVICE_INTEL: {
        auto status = zeMemGetAllocProperties(this->getL0ContextHandle(), ptr, &memoryProperties, &l0Device);
        if (ZE_RESULT_SUCCESS != status) {
            return L0ToClResultMapper(status);
        }
    } break;

    case CL_MEM_ALLOC_BASE_PTR_INTEL:
    case CL_MEM_ALLOC_SIZE_INTEL: {
        auto status = zeMemGetAddressRange(this->getL0ContextHandle(), ptr, &basePtr, &allocSize);
        if (ZE_RESULT_SUCCESS != status) {
            return L0ToClResultMapper(status);
        }
    } break;

    case CL_MEM_ALLOC_FLAGS_INTEL: {
        auto *svmAllocsManager = this->getL0Object()->getDriverHandle()->getSvmAllocsManager();
        if (svmAllocsManager) {
            auto *allocData = svmAllocsManager->getSVMAlloc(ptr);
            if (allocData) {
                allocFlags = static_cast<cl_mem_alloc_flags_intel>(allocData->allocationFlagsProperty.allAllocFlags);
            }
        }
    } break;
    }

    switch (paramName) {
    case CL_MEM_ALLOC_TYPE_INTEL:
        valueSize = sizeof(cl_int);
        pValue = &memAllocType;
        if (ZE_MEMORY_TYPE_HOST == memoryProperties.type) {
            memAllocType = CL_MEM_TYPE_HOST_INTEL;
        } else if (ZE_MEMORY_TYPE_DEVICE == memoryProperties.type) {
            memAllocType = CL_MEM_TYPE_DEVICE_INTEL;
        } else if (ZE_MEMORY_TYPE_SHARED == memoryProperties.type) {
            memAllocType = CL_MEM_TYPE_SHARED_INTEL;
        }
        break;

    case CL_MEM_ALLOC_DEVICE_INTEL:
        valueSize = sizeof(cl_device_id);
        pValue = &deviceId;
        deviceId = this->findClDevice(l0Device);
        break;

    case CL_MEM_ALLOC_BASE_PTR_INTEL:
        valueSize = sizeof(uint64_t);
        pValue = &basePtr;
        break;

    case CL_MEM_ALLOC_SIZE_INTEL:
        valueSize = sizeof(size_t);
        pValue = &allocSize;
        break;

    case CL_MEM_ALLOC_FLAGS_INTEL:
        valueSize = sizeof(cl_mem_alloc_flags_intel);
        pValue = &allocFlags;
        break;

    default:
        return CL_INVALID_VALUE;
    }
    GetInfoStatus getInfoStatus = GetInfoStatus::success;
    getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, pValue, valueSize);

    auto retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
    GetInfo::setParamValueReturnSize(paramValueSizeRet, valueSize, getInfoStatus);

    return retVal;
}

ClDevice *Context::findClDevice(ze_device_handle_t l0Device) const {
    for (auto device : this->clDevices) {
        if (l0Device == device->getL0Handle()) {
            return device;
        }
    }
    return nullptr;
}

cl_int Context::getSupportedImageFormats(
    Device *device,
    cl_mem_flags flags,
    cl_mem_object_type imageType,
    cl_uint numEntries,
    cl_image_format *imageFormats,
    cl_uint *numImageFormatsReturned) {
    size_t numImageFormats = 0;

    const bool nv12ExtensionEnabled = device->getSpecializedDevice<ClDevice>()->getDeviceInfo().nv12Extension;
    const bool packedYuvExtensionEnabled = device->getSpecializedDevice<ClDevice>()->getDeviceInfo().packedYuvExtension;

    auto appendImageFormats = [&](ArrayRef<const ClSurfaceFormatInfo> formats) {
        if (imageFormats) {
            size_t offset = numImageFormats;
            for (size_t i = 0; i < formats.size() && offset < numEntries; ++i) {
                imageFormats[offset++] = formats[i].oclImageFormat;
            }
        }
        numImageFormats += formats.size();
    };

    if (flags & CL_MEM_READ_ONLY) {
        appendImageFormats(SurfaceFormats::readOnly());
        if (SurfaceFormats::isImage2d(imageType) && nv12ExtensionEnabled) {
            appendImageFormats(SurfaceFormats::planarYuv());
        }
        if (SurfaceFormats::isImage2dOr2dArray(imageType)) {
            appendImageFormats(SurfaceFormats::readOnlyDepth());
        }
        if (SurfaceFormats::isImage2d(imageType) && packedYuvExtensionEnabled) {
            appendImageFormats(SurfaceFormats::packedYuv());
        }
    } else if (flags & CL_MEM_WRITE_ONLY) {
        appendImageFormats(SurfaceFormats::writeOnly());
        if (SurfaceFormats::isImage2dOr2dArray(imageType)) {
            appendImageFormats(SurfaceFormats::readWriteDepth());
        }
    } else if (nv12ExtensionEnabled && (flags & CL_MEM_NO_ACCESS_INTEL)) {
        appendImageFormats(SurfaceFormats::readOnly());
        if (SurfaceFormats::isImage2d(imageType)) {
            appendImageFormats(SurfaceFormats::planarYuv());
        }
    } else {
        appendImageFormats(SurfaceFormats::readWrite());
        if (SurfaceFormats::isImage2dOr2dArray(imageType)) {
            appendImageFormats(SurfaceFormats::readWriteDepth());
        }
    }
    if (numImageFormatsReturned) {
        *numImageFormatsReturned = static_cast<cl_uint>(numImageFormats);
    }
    return CL_SUCCESS;
}

ze_event_handle_t Context::createUserEvent() {
    constexpr uint32_t eventCountInPool = 8192;

    static std::mutex mtx;
    std::lock_guard<std::mutex> lock(mtx);

    if (this->userEventPools.empty() || this->createdFromLatestPool == eventCountInPool) {
        std::vector<ze_device_handle_t> l0Devices(clDevices.size());
        std::transform(clDevices.begin(), clDevices.end(), l0Devices.begin(), [](ClDevice *clDevice) { return clDevice->getL0Handle(); });
        ze_event_pool_desc_t eventPoolDesc{ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr, ZE_EVENT_POOL_FLAG_HOST_VISIBLE, eventCountInPool};
        zeEventPoolCreate(this->contextHandle, &eventPoolDesc, static_cast<uint32_t>(l0Devices.size()), l0Devices.data(), &this->userEventPools.emplace_back());
        this->createdFromLatestPool = 0;
    }

    ze_event_handle_t event{};
    ze_event_desc_t eventDesc{ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, this->createdFromLatestPool++, ZE_EVENT_SCOPE_FLAG_DEVICE, ZE_EVENT_SCOPE_FLAG_HOST};
    zeEventCreate(this->userEventPools.back(), &eventDesc, &event);

    return event;
}

void Context::storeProperties(const cl_context_properties *properties) {
    if (properties != nullptr) {
        while (*properties != 0) {
            this->contextProperties.push_back(*properties);
            ++properties;
            this->contextProperties.push_back(*properties);
            ++properties;
        }
        this->contextProperties.push_back(0u);
    }
}

} // namespace LEO
} // namespace NEO
