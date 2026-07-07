/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifdef LIBVA

#include "level_zero/api/opencl/source/sharings/va/leo_enable_va.h"

#include "shared/source/os_interface/driver_info.h"

#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/sharings/leo_sharing_factory.h"
#include "level_zero/api/opencl/source/sharings/leo_sharing_factory.inl"
#include "level_zero/api/opencl/source/sharings/va/leo_cl_va_api.h"
#include "level_zero/api/opencl/source/sharings/va/leo_va_sharing.h"

#include <memory>

namespace NEO {
namespace LEO {

bool VaSharingContextBuilder::processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue) {
    if (contextData.get() == nullptr) {
        contextData = std::make_unique<VaCreateContextProperties>();
    }

    switch (propertyType) {
    case CL_CONTEXT_VA_API_DISPLAY_INTEL:
        contextData->vaDisplay = reinterpret_cast<VADisplay>(propertyValue);
        return true;
    }
    return false;
}

bool VaSharingContextBuilder::finalizeProperties(Context &context, int32_t &errcodeRet) {
    if (contextData.get() == nullptr) {
        return true;
    }

    if (contextData->vaDisplay) {
        context.registerSharing(new VASharingFunctions(contextData->vaDisplay));
        if (!context.getSharing<VASharingFunctions>()->isValidVaDisplay()) {
            errcodeRet = CL_INVALID_VA_API_MEDIA_ADAPTER_INTEL;
            return false;
        }
        context.getSharing<VASharingFunctions>()->querySupportedVaImageFormats(contextData->vaDisplay);
    }

    return true;
}

std::unique_ptr<SharingContextBuilder> VaSharingBuilderFactory::createContextBuilder() {
    return std::make_unique<VaSharingContextBuilder>();
};

std::string VaSharingBuilderFactory::getExtensions(DriverInfo *driverInfo) {
    auto imageSupport = driverInfo ? driverInfo->getMediaSharingSupport() : false;
    if (imageSupport && VASharingFunctions::isVaLibraryAvailable()) {
        return "cl_intel_va_api_media_sharing ";
    }
    return "";
}

void VaSharingBuilderFactory::fillGlobalDispatchTable() {
    crtGlobalDispatchTable.clCreateFromVA_APIMediaSurfaceINTEL = clCreateFromVA_APIMediaSurfaceINTEL;
    crtGlobalDispatchTable.clGetDeviceIDsFromVA_APIMediaAdapterINTEL = clGetDeviceIDsFromVA_APIMediaAdapterINTEL;
    crtGlobalDispatchTable.clEnqueueReleaseVA_APIMediaSurfacesINTEL = clEnqueueReleaseVA_APIMediaSurfacesINTEL;
    crtGlobalDispatchTable.clEnqueueAcquireVA_APIMediaSurfacesINTEL = clEnqueueAcquireVA_APIMediaSurfacesINTEL;
}

#define RETURN_FUNC_PTR_IF_EXIST(name)             \
    {                                              \
        if (functionName == #name) {               \
            return reinterpret_cast<void *>(name); \
        }                                          \
    }
void *VaSharingBuilderFactory::getExtensionFunctionAddress(const std::string &functionName) {
    RETURN_FUNC_PTR_IF_EXIST(clCreateFromVA_APIMediaSurfaceINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clGetDeviceIDsFromVA_APIMediaAdapterINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clEnqueueAcquireVA_APIMediaSurfacesINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clEnqueueReleaseVA_APIMediaSurfacesINTEL);
    if (debugManager.flags.EnableFormatQuery.get()) {
        RETURN_FUNC_PTR_IF_EXIST(clGetSupportedVA_APIMediaSurfaceFormatsINTEL);
    }
    auto extraFunction = getExtensionFunctionAddressExtra(functionName);
    return extraFunction;
}

static SharingFactory::RegisterSharing<VaSharingBuilderFactory, VASharingFunctions> vaSharing;

} // namespace LEO
} // namespace NEO
#endif
