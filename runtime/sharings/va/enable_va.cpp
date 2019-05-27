/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifdef LIBVA

#include "runtime/sharings/va/enable_va.h"

#include "runtime/api/api.h"
#include "runtime/context/context.h"
#include "runtime/context/context.inl"
#include "runtime/sharings/sharing_factory.h"
#include "runtime/sharings/sharing_factory.inl"
#include "runtime/sharings/va/va_sharing.h"

#include <memory>

namespace NEO {

bool VaSharingContextBuilder::processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue, cl_int &errcodeRet) {
    if (contextData.get() == nullptr) {
        contextData.reset(new VaCreateContextProperties);
    }
    bool res = false;

    switch (propertyType) {
    case CL_CONTEXT_VA_API_DISPLAY_INTEL:
        contextData->vaDisplay = (VADisplay)propertyValue;
        res = true;
        break;
    }
    return res;
}

bool VaSharingContextBuilder::finalizeProperties(Context &context, int32_t &errcodeRet) {
    if (contextData.get() == nullptr)
        return true;

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
    return std::unique_ptr<SharingContextBuilder>(new VaSharingContextBuilder());
};

std::string VaSharingBuilderFactory::getExtensions() {
    if (VASharingFunctions::isVaLibraryAvailable()) {
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

#define RETURN_FUNC_PTR_IF_EXIST(name) \
    {                                  \
        if (functionName == #name) {   \
            return ((void *)(name));   \
        }                              \
    }
void *VaSharingBuilderFactory::getExtensionFunctionAddress(const std::string &functionName) {
    RETURN_FUNC_PTR_IF_EXIST(clCreateFromVA_APIMediaSurfaceINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clGetDeviceIDsFromVA_APIMediaAdapterINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clEnqueueAcquireVA_APIMediaSurfacesINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clEnqueueReleaseVA_APIMediaSurfacesINTEL);

    return nullptr;
}

static SharingFactory::RegisterSharing<VaSharingBuilderFactory, VASharingFunctions> vaSharing;
} // namespace NEO
#endif
