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
    const NEO::RootDeviceEnvironment &getRootDeviceEnvironment() const {
        return *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex];
    }
    const NEO::HardwareInfo &getHardwareInfo() const override { return *getRootDeviceEnvironment().getHardwareInfo(); }
    PRODUCT_FAMILY getProductFamily() const { return getHardwareInfo().platform.eProductFamily; }
    NEO::ExecutionEnvironment *getExecutionEnvironment() const { return executionEnvironment; }
    uint32_t getRootDeviceIndex() const { return rootDeviceIndex; }

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
