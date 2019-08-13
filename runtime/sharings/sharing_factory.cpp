/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sharing_factory.h"

#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/platform/extensions.h"

namespace NEO {

std::unique_ptr<SharingFactory> SharingFactory::build() {
    auto res = std::make_unique<SharingFactory>();

    for (auto &builder : sharingContextBuilder) {
        if (builder == nullptr)
            continue;
        res->sharings.push_back(builder->createContextBuilder());
    }

    return res;
}

std::string SharingFactory::getExtensions() {
    std::string res;
    bool sharingAvailable = false;

    for (auto &builder : sharingContextBuilder) {
        if (builder == nullptr)
            continue;
        res += builder->getExtensions();
        sharingAvailable = true;
    }

    if (DebugManager.flags.EnableFormatQuery.get() && sharingAvailable) {
        res += Extensions::sharingFormatQuery;
    }

    return res;
}

void SharingFactory::fillGlobalDispatchTable() {
    for (auto &builder : sharingContextBuilder) {
        if (builder == nullptr)
            continue;
        builder->fillGlobalDispatchTable();
    }
}

void *SharingFactory::getExtensionFunctionAddress(const std::string &functionName) {
    for (auto &builder : sharingContextBuilder) {
        if (builder == nullptr)
            continue;
        auto ret = builder->getExtensionFunctionAddress(functionName);
        if (ret != nullptr)
            return ret;
    }
    return nullptr;
}

bool SharingFactory::processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue, cl_int &errcodeRet) {
    for (auto &sharing : sharings) {
        if (sharing->processProperties(propertyType, propertyValue, errcodeRet))
            return true;
    }
    return false;
}

bool SharingFactory::finalizeProperties(Context &context, int32_t &errcodeRet) {
    for (auto &sharing : sharings) {
        if (!sharing->finalizeProperties(context, errcodeRet))
            return false;
    }
    return true;
}

SharingBuilderFactory *SharingFactory::sharingContextBuilder[SharingType::MAX_SHARING_VALUE] = {
    nullptr,
};
SharingFactory sharingFactory;
} // namespace NEO
