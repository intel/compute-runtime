/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/sysman_device.h"

#include <unordered_map>

namespace L0 {
namespace Sysman {
struct OsSysman;

struct SysmanDeviceImp : SysmanDevice, NEO::NonCopyableOrMovableClass {

    SysmanDeviceImp(NEO::ExecutionEnvironment *executionEnvironment, const uint32_t rootDeviceIndex);
    ~SysmanDeviceImp() override;

    SysmanDeviceImp() = delete;
    ze_result_t init();

    OsSysman *pOsSysman = nullptr;
    PowerHandleContext *pPowerHandleContext = nullptr;

    ze_result_t powerGet(uint32_t *pCount, zes_pwr_handle_t *phPower) override;
    ze_result_t powerGetCardDomain(zes_pwr_handle_t *phPower) override;

    const NEO::RootDeviceEnvironment &getRootDeviceEnvironment() const {
        return *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex];
    }
    const NEO::HardwareInfo &getHardwareInfo() const override { return *getRootDeviceEnvironment().getHardwareInfo(); }
    PRODUCT_FAMILY getProductFamily() const { return getHardwareInfo().platform.eProductFamily; }
    NEO::ExecutionEnvironment *getExecutionEnvironment() const { return executionEnvironment; }
    uint32_t getRootDeviceIndex() const { return rootDeviceIndex; }

    FabricPortHandleContext *pFabricPortHandleContext = nullptr;
    MemoryHandleContext *pMemoryHandleContext = nullptr;
    EngineHandleContext *pEngineHandleContext = nullptr;

    ze_result_t memoryGet(uint32_t *pCount, zes_mem_handle_t *phMemory) override;
    ze_result_t fabricPortGet(uint32_t *pCount, zes_fabric_port_handle_t *phPort) override;
    ze_result_t engineGet(uint32_t *pCount, zes_engine_handle_t *phEngine) override;

  private:
    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    const uint32_t rootDeviceIndex;
    template <typename T>
    void inline freeResource(T *&resource) {
        if (resource) {
            delete resource;
            resource = nullptr;
        }
    }
};

} // namespace Sysman
} // namespace L0
