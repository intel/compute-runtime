/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/os_context_linux.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/os_library_linux.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/source/release_helper/release_helper.h"

namespace NEO {

OsContext *OsContextLinux::create(OSInterface *osInterface, uint32_t rootDeviceIndex, uint32_t contextId, const EngineDescriptor &engineDescriptor) {
    if (osInterface && osInterface->getDriverModel()->getDriverModelType() == DriverModelType::drm) {
        return new OsContextLinux(*osInterface->getDriverModel()->as<Drm>(), rootDeviceIndex, contextId, engineDescriptor);
    }
    return new OsContext(rootDeviceIndex, contextId, engineDescriptor);
}

OsContextLinux::OsContextLinux(Drm &drm, uint32_t rootDeviceIndex, uint32_t contextId, const EngineDescriptor &engineDescriptor)
    : OsContext(rootDeviceIndex, contextId, engineDescriptor),
      drm(drm) {
    pagingFence.fill(0u);
    fenceVal.fill(0u);
    this->isOpenVinoLoaded();
}

bool OsContextLinux::initializeContext(bool allocateInterrupt) {
    auto hwInfo = drm.getRootDeviceEnvironment().getHardwareInfo();
    auto defaultEngineType = getChosenEngineType(*hwInfo);

    if (engineType == defaultEngineType && !isLowPriority() && !isInternalEngine()) {
        this->setDefaultContext(true);
    }

    bool submitDirect = false;
    this->isDirectSubmissionAvailable(*drm.getRootDeviceEnvironment().getHardwareInfo(), submitDirect);

    if (drm.isPerContextVMRequired()) {
        this->drmVmIds.resize(deviceBitfield.size(), 0);
    }

    for (auto deviceIndex = 0u; deviceIndex < deviceBitfield.size(); deviceIndex++) {
        if (deviceBitfield.test(deviceIndex)) {
            auto drmVmId = drm.getVirtualMemoryAddressSpace(deviceIndex);

            if (drm.isPerContextVMRequired()) {
                drmVmId = deviceIndex;
                [[maybe_unused]] auto ret = drm.createDrmVirtualMemory(drmVmId);
                DEBUG_BREAK_IF(drmVmId == 0);
                DEBUG_BREAK_IF(ret != 0);

                if (ret != 0) {
                    drmVmId = 0;
                }
                UNRECOVERABLE_IF(this->drmVmIds.size() <= deviceIndex);
                this->drmVmIds[deviceIndex] = drmVmId;
            }

            auto drmContextId = drm.getIoctlHelper()->createDrmContext(drm, *this, drmVmId, deviceIndex, allocateInterrupt);
            if (drmContextId < 0) {
                return false;
            }

            if (drm.isPerContextVMRequired() && this->drmVmIds[deviceIndex] == 0) {
                drmVmId = 0;
                [[maybe_unused]] auto ret = drm.queryVmId(drmContextId, drmVmId);
                DEBUG_BREAK_IF(drmVmId == 0);
                DEBUG_BREAK_IF(ret != 0);

                this->drmVmIds[deviceIndex] = drmVmId;
            }

            this->drmContextIds.push_back(drmContextId);
        }
    }
    return true;
}

void OsContextLinux::isOpenVinoLoaded() {
    std::call_once(this->ovLoadedFlag, [this]() {
        this->ovLoaded = NEO::Linux::isLibraryLoaded("libopenvino_intel_gpu_plugin.so");
    });
}

bool OsContextLinux::isDirectSubmissionSupported() const {
    auto &rootDeviceEnvironment = this->getDrm().getRootDeviceEnvironment();
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    const auto releaseHelper = rootDeviceEnvironment.getReleaseHelper();

    return (this->getDrm().isVmBindAvailable() || (this->ovLoaded && releaseHelper && releaseHelper->isDirectSubmissionLightSupported())) && productHelper.isDirectSubmissionSupported(rootDeviceEnvironment.getReleaseHelper());
}

Drm &OsContextLinux::getDrm() const {
    return this->drm;
}
bool OsContextLinux::isDirectSubmissionLightActive() const {
    return this->isDirectSubmissionActive() && !this->getDrm().isVmBindAvailable();
}

void OsContextLinux::waitForPagingFence() {
    for (auto drmIterator = 0u; drmIterator < this->deviceBitfield.size(); drmIterator++) {
        if (this->deviceBitfield.test(drmIterator)) {
            this->waitForBind(drmIterator);
        }
    }
}

void OsContextLinux::waitForBind(uint32_t drmIterator) {

    auto fenceAddressAndValToWait = getFenceAddressAndValToWait(drmIterator, false);

    const auto fenceAddressToWait = fenceAddressAndValToWait.first;
    const auto fenceValToWait = fenceAddressAndValToWait.second;

    if (fenceAddressToWait != 0u) {
        drm.waitUserFence(0u, fenceAddressToWait, fenceValToWait, Drm::ValueWidth::u64, -1, drm.getIoctlHelper()->getWaitUserFenceSoftFlag(), false, NEO::InterruptId::notUsed, nullptr);
    }
}

[[nodiscard]] std::pair<uint64_t, uint64_t> OsContextLinux::getFenceAddressAndValToWait(uint32_t vmHandleId, bool isLocked) {
    if (drm.isPerContextVMRequired()) {
        std::pair<uint64_t, uint64_t> fenceAddressAndValToWait = std::make_pair(0, 0);
        std::unique_lock<std::mutex> lock;

        if (!isLocked) {
            lock = drm.lockBindFenceMutex();
        }

        if (!(pagingFence[vmHandleId] >= fenceVal[vmHandleId])) {

            auto fenceAddress = castToUint64(&this->pagingFence[vmHandleId]);
            auto fenceValue = this->fenceVal[vmHandleId];
            fenceAddressAndValToWait = std::make_pair(fenceAddress, fenceValue);
        }

        if (!isLocked) {
            lock.unlock();
        }

        return fenceAddressAndValToWait;

    } else {
        return drm.getFenceAddressAndValToWait(vmHandleId, isLocked);
    }
}

void OsContextLinux::reInitializeContext() {}

uint64_t OsContextLinux::getOfflineDumpContextId(uint32_t deviceIndex) const {
    if (deviceIndex < drmContextIds.size()) {
        const auto processId = SysCalls::getProcessId();
        const auto drmContextId = drmContextIds[deviceIndex];
        return static_cast<uint64_t>(processId) << 32 |
               static_cast<uint64_t>(drmContextId);
    }
    return 0;
}

OsContextLinux::~OsContextLinux() {
    if (contextInitialized) {
        for (auto drmContextId : drmContextIds) {
            drm.destroyDrmContext(drmContextId);
        }
    }
    drmContextIds.clear();
    drmVmIds.clear();
}
} // namespace NEO
