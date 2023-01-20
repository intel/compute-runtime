/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/graphics_allocation.h"

#include <algorithm>

namespace NEO {

GmmClientContext *GmmHelper::getClientContext() const {
    return gmmClientContext.get();
}

const HardwareInfo *GmmHelper::getHardwareInfo() {
    return rootDeviceEnvironment.getHardwareInfo();
}

const RootDeviceEnvironment &GmmHelper::getRootDeviceEnvironment() const {
    return rootDeviceEnvironment;
}

uint32_t GmmHelper::getMOCS(uint32_t type) const {
    if (allResourcesUncached || (DebugManager.flags.ForceAllResourcesUncached.get() == true)) {
        type = GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED;
    }

    MEMORY_OBJECT_CONTROL_STATE mocs = gmmClientContext->cachePolicyGetMemoryObject(nullptr, static_cast<GMM_RESOURCE_USAGE_TYPE>(type));

    return static_cast<uint32_t>(mocs.DwordValue);
}

void GmmHelper::applyMocsEncryptionBit(uint32_t &index) {
    if (DebugManager.flags.ForceStatelessMocsEncryptionBit.get() == 1) {
        index |= 1;
    }
}

GmmHelper::GmmHelper(const RootDeviceEnvironment &rootDeviceEnvironmentArg) : rootDeviceEnvironment(rootDeviceEnvironmentArg) {
    auto hwInfo = getHardwareInfo();
    auto hwInfoAddressWidth = Math::log2(hwInfo->capabilityTable.gpuAddressSpace + 1);
    addressWidth = std::max(hwInfoAddressWidth, 48u);

    gmmClientContext = GmmHelper::createGmmContextWrapperFunc(rootDeviceEnvironment);
    UNRECOVERABLE_IF(!gmmClientContext);
}

uint64_t GmmHelper::canonize(uint64_t address) {
    return static_cast<int64_t>(address << (64 - addressWidth)) >> (64 - addressWidth);
}

uint64_t GmmHelper::decanonize(uint64_t address) {
    return (address & maxNBitValue(addressWidth));
}

bool GmmHelper::isValidCanonicalGpuAddress(uint64_t address) {
    auto decanonizedAddress = this->decanonize(address);
    auto canonizedAddress = this->canonize(decanonizedAddress);

    if (address == canonizedAddress) {
        return true;
    }
    return false;
}

GmmHelper::~GmmHelper() = default;

decltype(GmmHelper::createGmmContextWrapperFunc) GmmHelper::createGmmContextWrapperFunc = GmmClientContext::create<GmmClientContext>;
} // namespace NEO
