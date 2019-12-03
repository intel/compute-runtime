/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gmm_helper/gmm_helper.h"

#include "core/helpers/debug_helpers.h"
#include "core/helpers/hw_info.h"
#include "core/memory_manager/graphics_allocation.h"
#include "core/os_interface/os_library.h"
#include "core/sku_info/operations/sku_info_transfer.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/platform/platform.h"

#include "gmm_client_context.h"

namespace NEO {

GmmClientContext *GmmHelper::getClientContext() {
    return getInstance()->gmmClientContext.get();
}

const HardwareInfo *GmmHelper::getHardwareInfo() {
    return hwInfo;
}

GmmHelper *GmmHelper::getInstance() {
    return platform()->peekExecutionEnvironment()->getGmmHelper();
}

uint32_t GmmHelper::getMOCS(uint32_t type) {
    MEMORY_OBJECT_CONTROL_STATE mocs = gmmClientContext->cachePolicyGetMemoryObject(nullptr, static_cast<GMM_RESOURCE_USAGE_TYPE>(type));

    return static_cast<uint32_t>(mocs.DwordValue);
}

GmmHelper::GmmHelper(const HardwareInfo *pHwInfo) : hwInfo(pHwInfo) {
    loadLib();
    gmmClientContext = GmmHelper::createGmmContextWrapperFunc(const_cast<HardwareInfo *>(pHwInfo), this->initGmmFunc, this->destroyGmmFunc);
    UNRECOVERABLE_IF(!gmmClientContext);
}

GmmHelper::~GmmHelper() = default;

decltype(GmmHelper::createGmmContextWrapperFunc) GmmHelper::createGmmContextWrapperFunc = GmmClientContextBase::create<GmmClientContext>;
} // namespace NEO
