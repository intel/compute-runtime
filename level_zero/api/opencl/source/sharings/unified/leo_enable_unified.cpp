/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/sharings/unified/leo_enable_unified.h"

#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/sharings/leo_sharing_factory.h"
#include "level_zero/api/opencl/source/sharings/leo_sharing_factory.inl"
#include "level_zero/api/opencl/source/sharings/unified/leo_unified_sharing.h"
#include "level_zero/api/opencl/source/sharings/unified/leo_unified_sharing_types.h"

#include <memory>

namespace NEO {
namespace LEO {

bool UnifiedSharingContextBuilder::processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue) {
    switch (propertyType) {
    case static_cast<cl_context_properties>(UnifiedSharingContextType::deviceHandle):
    case static_cast<cl_context_properties>(UnifiedSharingContextType::deviceGroup):
        this->contextData = std::make_unique<UnifiedCreateContextProperties>();
        return true;
    default:
        return false;
    }
}

bool UnifiedSharingContextBuilder::finalizeProperties(Context &context, int32_t &errcodeRet) {
    if (contextData.get() != nullptr) {
        if (context.getInteropUserSyncEnabled()) {
            context.registerSharing(new UnifiedSharingFunctions());
        }
        contextData.reset(nullptr);
    }
    return true;
}

std::unique_ptr<SharingContextBuilder> UnifiedSharingBuilderFactory::createContextBuilder() {
    return std::make_unique<UnifiedSharingContextBuilder>();
};

std::string UnifiedSharingBuilderFactory::getExtensions(DriverInfo *driverInfo) {
    return "";
}

void *UnifiedSharingBuilderFactory::getExtensionFunctionAddress(const std::string &functionName) {
    return nullptr;
}

static SharingFactory::RegisterSharing<UnifiedSharingBuilderFactory, UnifiedSharingFunctions> unifiedSharing;

} // namespace LEO
} // namespace NEO
