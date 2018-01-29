/*
 * Copyright (c) 2017, Intel Corporation
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
#ifdef LIBVA

#include "runtime/api/api.h"
#include "runtime/context/context.h"
#include "runtime/context/context.inl"
#include "runtime/sharings/va/enable_va.h"
#include "runtime/sharings/va/va_sharing.h"
#include "runtime/sharings/sharing_factory.h"
#include "runtime/sharings/sharing_factory.inl"

#include <memory>

namespace OCLRT {

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
} // namespace OCLRT
#endif
