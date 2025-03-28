/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/api/sysman/zes_handles_struct.h"
#include <level_zero/zes_api.h>

#include <memory>
#include <mutex>
#include <vector>

namespace L0 {
namespace Sysman {

struct OsSysman;

class VfManagement : _zes_vf_handle_t {
  public:
    virtual ~VfManagement() = default;
    virtual ze_result_t vfGetCapabilities(zes_vf_exp2_capabilities_t *pCapability) = 0;
    virtual ze_result_t vfGetMemoryUtilization(uint32_t *pCount, zes_vf_util_mem_exp2_t *pMemUtil) = 0;
    virtual ze_result_t vfGetEngineUtilization(uint32_t *pCount, zes_vf_util_engine_exp2_t *pEngineUtil) = 0;

    inline zes_vf_handle_t toVfManagementHandle() { return this; }

    static VfManagement *fromHandle(zes_vf_handle_t handle) {
        return static_cast<VfManagement *>(handle);
    }
};

struct VfManagementHandleContext : NEO::NonCopyableAndNonMovableClass {
    VfManagementHandleContext(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~VfManagementHandleContext();

    ze_result_t init();

    ze_result_t vfManagementGet(uint32_t *pCount, zes_vf_handle_t *phVfManagement);

    OsSysman *pOsSysman = nullptr;
    std::vector<std::unique_ptr<VfManagement>> handleList = {};

  private:
    void createHandle(uint32_t vfId);
    std::once_flag initVfManagementOnce;
};

static_assert(NEO::NonCopyableAndNonMovable<VfManagementHandleContext>);

} // namespace Sysman
} // namespace L0
