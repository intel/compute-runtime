/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/unified/enable_unified.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "opencl/source/context/context.h"
#include "opencl/source/context/context.inl"
#include "opencl/source/sharings/sharing_factory.h"
#include "opencl/source/sharings/sharing_factory.inl"
#include "opencl/source/sharings/unified/unified_sharing.h"
#include "opencl/source/sharings/unified/unified_sharing_types.h"

#include <memory>

namespace NEO {

bool UnifiedSharingContextBuilder::processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue,
                                                     cl_int &errcodeRet) {
    switch (propertyType) {
    case static_cast<cl_context_properties>(UnifiedSharingContextType::DeviceHandle):
    case static_cast<cl_context_properties>(UnifiedSharingContextType::DeviceGroup):
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

void *UnifiedSharingBuilderFactory::getExtensionFunctionAddress(const std::string &functionName) {
    return nullptr;
}

static SharingFactory::RegisterSharing<UnifiedSharingBuilderFactory, UnifiedSharingFunctions> unifiedSharing;
} // namespace NEO
