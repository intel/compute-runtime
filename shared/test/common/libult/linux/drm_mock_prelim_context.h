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
#include "shared/source/os_interface/linux/ioctl_helper.h"

#include <optional>
#include <vector>

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

struct CreateGemExt {
    uint64_t size{0};
    uint32_t handle{0};

    struct MemoryClassInstance {
        uint16_t memoryClass{0};
        uint16_t memoryInstance{0};
    };
    std::vector<MemoryClassInstance> memoryRegions{};
};

struct GemContextParamAcc {
    uint16_t trigger{0};
    uint16_t notify{0};
    uint8_t granularity{0};
};

struct WaitUserFence {
    uint64_t extensions{0};
    uint64_t addr{0};
    uint32_t ctxId{0};
    uint16_t op{0};
    uint16_t flags{0};
    uint64_t value{0};
    uint64_t mask{0};
    int64_t timeout{0};
};

struct SyncFenceVmBindExt {
    uint64_t addr;
    uint64_t val;
};

struct UuidVmBindExt {
    uint32_t handle;
    uint64_t nextExtension;
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

    uint64_t receivedSetContextParamValue{0};
    uint64_t receivedSetContextParamCtxId{0};
    uint64_t receivedContextCreateExtSetParamRunaloneCount{0};

    size_t vmBindQueryCalled{0};
    int vmBindQueryValue{0};
    int vmBindQueryReturn{0};

    size_t vmBindCalled{0};
    std::optional<VmBindParams> receivedVmBind{};
    std::optional<SyncFenceVmBindExt> receivedVmBindSyncFence{};
    std::optional<UuidVmBindExt> receivedVmBindUuidExt[2]{};
    std::optional<uint64_t> receivedVmBindPatIndex{};
    int vmBindReturn{0};

    size_t vmUnbindCalled{0};
    std::optional<VmBindParams> receivedVmUnbind{};
    std::optional<uint64_t> receivedVmUnbindPatIndex{};
    int vmUnbindReturn{0};

    int hasPageFaultQueryValue{0};
    int hasPageFaultQueryReturn{0};

    uint32_t queryMemoryRegionInfoSuccessCount{std::numeric_limits<uint32_t>::max()};

    size_t waitUserFenceCalled{0};
    std::optional<WaitUserFence> receivedWaitUserFence{};

    uint32_t uuidHandle{1};
    std::optional<UuidControl> receivedRegisterUuid{};
    std::optional<UuidControl> receivedUnregisterUuid{};

    int uuidControlReturn{0};

    std::optional<CreateGemExt> receivedCreateGemExt{};
    std::optional<GemContextParamAcc> receivedContextParamAcc{};

    bool failDistanceInfoQuery{false};
    bool disableCcsSupport{false};

    int handlePrelimRequest(unsigned long request, void *arg);
    bool handlePrelimQueryItem(void *arg);
    void storeVmBindExtensions(uint64_t ptr, bool bind);
};

namespace DrmPrelimHelper {
uint32_t getQueryComputeSlicesIoctl();
uint32_t getDistanceInfoQueryId();
uint32_t getComputeEngineClass();
uint32_t getStringUuidClass();
uint32_t getULLSContextCreateFlag();
uint32_t getRunAloneContextParam();
uint32_t getAccContextParam();
uint32_t getAccContextParamSize();
std::array<uint32_t, 4> getContextAcgValues();
uint32_t getEnablePageFaultVmCreateFlag();
uint32_t getDisableScratchVmCreateFlag();
uint64_t getU8WaitUserFenceFlag();
uint64_t getU16WaitUserFenceFlag();
uint64_t getCaptureVmBindFlag();
uint64_t getImmediateVmBindFlag();
uint64_t getMakeResidentVmBindFlag();
uint64_t getSIPContextParamDebugFlag();
}; // namespace DrmPrelimHelper
