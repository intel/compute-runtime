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

    struct SetParam {
        uint32_t handle{0};
        uint32_t size{0};
        uint64_t param{0};
    };
    std::optional<SetParam> setParamExt{};

    struct MemoryClassInstance {
        uint16_t memoryClass{0};
        uint16_t memoryInstance{0};
    };
    std::vector<MemoryClassInstance> memoryRegions{};

    struct VmPrivate {
        std::optional<uint32_t> vmId{};
    };
    VmPrivate vmPrivateExt{};
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

struct UserFenceVmBindExt {
    uint64_t addr{0};
    uint64_t val{0};
};

struct VmAdvise {
    uint32_t handle{0};
    uint32_t flags{0};
};

struct UuidVmBindExt {
    uint32_t handle{0};
    uint64_t nextExtension{0};
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
    std::optional<UserFenceVmBindExt> receivedVmBindUserFence{};
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

    std::optional<VmAdvise> receivedVmAdvise{};
    int vmAdviseReturn{0};

    int mmapOffsetReturn{0};

    uint32_t uuidHandle{1};
    std::optional<UuidControl> receivedRegisterUuid{};
    std::optional<UuidControl> receivedUnregisterUuid{};

    int uuidControlReturn{0};

    std::optional<CreateGemExt> receivedCreateGemExt{};
    std::optional<GemContextParamAcc> receivedContextParamAcc{};
    int gemCreateExtReturn{0};

    bool failDistanceInfoQuery{false};
    bool disableCcsSupport{false};

    // Debugger ioctls
    int debuggerOpenRetval = 10; // debugFd

    int handlePrelimRequest(DrmIoctl request, void *arg);
    bool handlePrelimQueryItem(void *arg);
    void storeVmBindExtensions(uint64_t ptr, bool bind);
};

namespace DrmPrelimHelper {
uint32_t getQueryComputeSlicesIoctl();
uint32_t getDistanceInfoQueryId();
uint32_t getComputeEngineClass();
uint32_t getStringUuidClass();
uint32_t getLongRunningContextCreateFlag();
uint32_t getRunAloneContextParam();
uint32_t getAccContextParam();
uint32_t getAccContextParamSize();
std::array<uint32_t, 4> getContextAcgValues();
uint32_t getEnablePageFaultVmCreateFlag();
uint32_t getDisableScratchVmCreateFlag();
uint64_t getU8WaitUserFenceFlag();
uint64_t getU16WaitUserFenceFlag();
uint64_t getU32WaitUserFenceFlag();
uint64_t getU64WaitUserFenceFlag();
uint64_t getGTEWaitUserFenceFlag();
uint64_t getSoftWaitUserFenceFlag();
uint64_t getCaptureVmBindFlag();
uint64_t getImmediateVmBindFlag();
uint64_t getMakeResidentVmBindFlag();
uint64_t getSIPContextParamDebugFlag();
uint64_t getMemoryRegionsParamFlag();
uint32_t getVmAdviseNoneFlag();
uint32_t getVmAdviseDeviceFlag();
uint32_t getVmAdviseSystemFlag();
}; // namespace DrmPrelimHelper
