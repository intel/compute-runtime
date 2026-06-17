/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/os_interface/os_interface.h"

#include <cstdint>
#include <limits>

namespace L0::EventHostSynchronize {

inline bool isDrm(const NEO::CommandStreamReceiver &csr) {
    auto osInterface = csr.getOSInterface();
    auto driverModel = osInterface ? osInterface->getDriverModel() : nullptr;
    return driverModel && driverModel->getDriverModelType() == NEO::DriverModelType::drm;
}

inline int64_t getWaitPkgThreshold(const NEO::CommandStreamReceiver &csr) {
    const auto debugWaitpkgThreshold = NEO::debugManager.flags.WaitpkgThreshold.get();
    if (debugWaitpkgThreshold != -1) {
        return debugWaitpkgThreshold;
    }

    return isDrm(csr) ? NEO::WaitUtils::defaultWaitPkgThresholdForDrmEventHostSyncInMicroSeconds
                      : NEO::WaitUtils::defaultWaitPkgThresholdForWddmEventHostSyncInMicroSeconds;
}

inline bool isNativeWddm(const NEO::CommandStreamReceiver &csr) {
    auto osInterface = csr.getOSInterface();
    auto driverModel = osInterface ? osInterface->getDriverModel() : nullptr;
    if (!driverModel || driverModel->getDriverModelType() != NEO::DriverModelType::wddm) {
        return false;
    }

    auto rootDeviceEnvironment = csr.peekExecutionEnvironment().rootDeviceEnvironments[csr.getRootDeviceIndex()].get();
    return !rootDeviceEnvironment->isWddmOnLinux();
}

struct WaitAction {
    int64_t sleepUs = 0;
};

class WaitController {
  public:
    explicit WaitController(const NEO::CommandStreamReceiver &csr) : csr(csr) {}

    WaitAction getAction(int64_t elapsedUs, uint64_t timeout, uint64_t elapsedNs) {
        if (!allowsSleepForTimeout(timeout, elapsedNs)) {
            return {};
        }

        auto sleepUs = getScheduledSleepUs(elapsedUs);
        if (sleepUs <= 0) {
            return {};
        }

        if (!enabledChecked) {
            enabled = isEnabled();
            enabledChecked = true;
        }
        if (!enabled) {
            return {};
        }

        return {sleepUs};
    }

  private:
    static bool allowsSleepForTimeout(uint64_t timeout, uint64_t elapsedNs) {
        if (timeout == std::numeric_limits<uint64_t>::max()) {
            return true;
        }
        if (timeout == 0) {
            return false;
        }
        if (elapsedNs >= timeout) {
            return false;
        }

        const auto minTimeoutUs = NEO::debugManager.flags.EventHostSynchronizeWaitStrategyMinTimeoutMicroseconds.get();
        if (minTimeoutUs <= 0) {
            return true;
        }

        const auto minTimeoutUsAsUint = static_cast<uint64_t>(minTimeoutUs);
        if (minTimeoutUsAsUint > std::numeric_limits<uint64_t>::max() / 1000u) {
            return false;
        }

        const auto minTimeoutNs = minTimeoutUsAsUint * 1000u;
        if (timeout <= minTimeoutNs) {
            return false;
        }

        return (timeout - elapsedNs) > minTimeoutNs;
    }

    bool isEnabled() const {
        const auto strategy = NEO::debugManager.flags.EventHostSynchronizeWaitStrategy.get();
        if ((strategy == 0) || !isNativeWddm(csr)) {
            return false;
        }

        if (strategy == 1) {
            return true;
        }

        return (strategy == 2) && !csr.getAcLineConnected(true);
    }

    static int64_t getScheduledSleepUs(int64_t elapsedUs) {
        const auto initialPollUs = NEO::debugManager.flags.EventHostSynchronizeInitialPollMicroseconds.get();
        const auto activePollUs = initialPollUs > 0 ? initialPollUs : int64_t{0};
        if (elapsedUs < activePollUs) {
            return 0;
        }

        const auto pollUs = NEO::debugManager.flags.EventHostSynchronizePollMicroseconds.get();
        const auto sleepUs = NEO::debugManager.flags.EventHostSynchronizeSleepMicroseconds.get();
        if (pollUs < 0 || sleepUs <= 0 || pollUs > std::numeric_limits<int64_t>::max() - sleepUs) {
            return 0;
        }

        const auto cycleUs = pollUs + sleepUs;
        const auto elapsedInCycleUs = (elapsedUs - activePollUs) % cycleUs;
        return elapsedInCycleUs >= pollUs ? cycleUs - elapsedInCycleUs : int64_t{0};
    }

    const NEO::CommandStreamReceiver &csr;
    bool enabled = false;
    bool enabledChecked = false;
};

} // namespace L0::EventHostSynchronize
