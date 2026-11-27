/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/definitions/storage_info.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/os_interface/product_helper.h"

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

void GmmHelper::initMocsDefaults() {
    if (!this->rootDeviceEnvironment.getProductHelper().deferMOCSToPatIndex(this->rootDeviceEnvironment.isWddmOnLinux())) {
        mocsL1Enabled = this->getDefaultMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST);
        mocsL3Enabled = this->getDefaultMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
        mocsUncached = this->getDefaultMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED);
    }
    rootDeviceEnvironment.getHelper<GfxCoreHelper>().initCommandLevelMocsDefaults(*this);
}

uint32_t GmmHelper::getMOCS(uint32_t type) const {
    return this->getMOCS(nullptr, type);
}

uint32_t GmmHelper::getMOCS(const Gmm *gmm, uint32_t type) const {
    if (allResourcesUncached || (debugManager.flags.ForceAllResourcesUncached.get() == true)) {
        type = GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED;
    }

    if (this->rootDeviceEnvironment.getProductHelper().deferMOCSToPatIndex(this->rootDeviceEnvironment.isWddmOnLinux())) {
        return 0u;
    }

    auto *resourceInfo = gmm ? gmm->gmmResourceInfo->peekGmmResourceInfo() : nullptr;
    MEMORY_OBJECT_CONTROL_STATE mocs = gmmClientContext->cachePolicyGetMemoryObject(resourceInfo, static_cast<GMM_RESOURCE_USAGE_TYPE>(type));

    return static_cast<uint32_t>(mocs.DwordValue);
}

uint32_t GmmHelper::getDefaultMOCS(GmmResourceUsageType usageType) {
    constexpr bool isSystemMemory = true;
    const auto queryGmm = this->createBufferGmm(usageType, !CacheSettingsHelper::isUncachedType(usageType), isSystemMemory, MemoryConstants::pageSize);
    return this->getMOCS(queryGmm.get(), usageType);
}

std::unique_ptr<Gmm> GmmHelper::createBufferGmm(GmmResourceUsageType usageType, bool cpuCacheable, bool isSystemMemory, size_t size) {
    StorageInfo storageInfo{};
    storageInfo.systemMemoryPlacement = isSystemMemory;
    storageInfo.isLockable = true;

    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    gmmRequirements.overriderCpuCacheable = {true, cpuCacheable};
    gmmRequirements.overriderPreferNoCpuAccess = {true, false};

    constexpr size_t alignment = 0u;
    return std::make_unique<Gmm>(this, nullptr, size, alignment, usageType, storageInfo, gmmRequirements);
}

std::unique_ptr<Gmm> GmmHelper::createBufferGmm(AllocationType allocationType, bool forceUncached, bool isSystemMemory, size_t size) {
    const auto usageType = CacheSettingsHelper::getGmmUsageType(allocationType, forceUncached, this->rootDeviceEnvironment.getProductHelper(), *this->rootDeviceEnvironment.getHardwareInfo());
    return this->createBufferGmm(usageType, !CacheSettingsHelper::isUncachedType(usageType), isSystemMemory, size);
}

uint32_t GmmHelper::getL1EnabledMOCS() const {
    return mocsL1Enabled;
}

uint32_t GmmHelper::getL3EnabledMOCS() const {
    return mocsL3Enabled;
}

uint32_t GmmHelper::getUncachedMOCS() const {
    return mocsUncached;
}

uint32_t GmmHelper::getHeapMOCS() const {
    const auto heapUsageType = CacheSettingsHelper::getGmmUsageType(AllocationType::internalHeap, debugManager.flags.DisableCachingForHeaps.get(), this->rootDeviceEnvironment.getProductHelper(), *this->rootDeviceEnvironment.getHardwareInfo());
    return CacheSettingsHelper::isUncachedType(heapUsageType) ? this->getUncachedMOCS() : this->getL3EnabledMOCS();
}

void GmmHelper::applyMocsEncryptionBit(uint32_t &index) {
    if (debugManager.flags.ForceStatelessMocsEncryptionBit.get() == 1) {
        index |= 1;
    }
}

void GmmHelper::forceAllResourcesUncached() {
    allResourcesUncached = true;
    initMocsDefaults();
}

GmmHelper::GmmHelper(const RootDeviceEnvironment &rootDeviceEnvironmentArg) : rootDeviceEnvironment(rootDeviceEnvironmentArg) {
    auto hwInfo = getHardwareInfo();
    auto hwInfoAddressWidth = Math::log2(hwInfo->capabilityTable.gpuAddressSpace + 1);
    addressWidth = std::max(hwInfoAddressWidth, 48u);

    gmmClientContext = GmmHelper::createGmmContextWrapperFunc(rootDeviceEnvironment);
    UNRECOVERABLE_IF(!gmmClientContext);

    initMocsDefaults();
}

uint64_t GmmHelper::canonize(uint64_t address) const {
    return static_cast<int64_t>(address << (64 - addressWidth)) >> (64 - addressWidth);
}

uint64_t GmmHelper::decanonize(uint64_t address) const {
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
