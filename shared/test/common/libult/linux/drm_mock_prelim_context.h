/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/cache_info.h"

#include <optional>

using namespace NEO;

struct UuidControl {
    char uuid[36]{};
    uint32_t uuidClass{0};
    void *ptr{nullptr};
    uint64_t size{0};
    uint32_t handle{0};
    uint32_t flags{0};
    uint64_t extensions{0};
};

struct DrmMockPrelimContext {
    const HardwareInfo *hwInfo;
    const RootDeviceEnvironment &rootDeviceEnvironment;
    const CacheInfo *cacheInfo;
    const bool &failRetTopology;
    const BcsInfoMask &supportedCopyEnginesMask;
    const bool &contextDebugSupported;

    uint16_t closIndex{0};
    uint16_t maxNumWays{32};
    uint32_t allocNumWays{0};

    size_t vmBindQueryCalled{0};
    int vmBindQueryValue{0};
    int vmBindQueryReturn{0};

    size_t vmBindCalled{0};
    int vmBindReturn{0};

    size_t vmUnbindCalled{0};
    int vmUnbindReturn{0};

    int hasPageFaultQueryValue{0};
    int hasPageFaultQueryReturn{0};

    uint32_t uuidHandle{1};
    std::optional<UuidControl> receivedRegisterUuid{};
    std::optional<UuidControl> receivedUnregisterUuid{};

    int uuidControlReturn{0};

    bool failDistanceInfoQuery{false};
    bool disableCcsSupport{false};

    int handlePrelimRequest(unsigned long request, void *arg);
    bool handlePrelimQueryItem(void *arg);
};

namespace DrmPrelimHelper {
uint32_t getQueryComputeSlicesIoctl();
uint32_t getDistanceInfoQueryId();
uint32_t getComputeEngineClass();
uint32_t getStringUuidClass();
}; // namespace DrmPrelimHelper
