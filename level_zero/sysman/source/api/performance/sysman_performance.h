/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/sysman/zes_handles_struct.h"
#include <level_zero/zes_api.h>

#include <mutex>
#include <vector>

namespace L0 {
namespace Sysman {

struct OsSysman;

class Performance : _zes_perf_handle_t {
  public:
    virtual ~Performance() = default;
    virtual ze_result_t performanceGetProperties(zes_perf_properties_t *pProperties) = 0;
    virtual ze_result_t performanceGetConfig(double *pFactor) = 0;
    virtual ze_result_t performanceSetConfig(double pFactor) = 0;

    inline zes_perf_handle_t toPerformanceHandle() { return this; }

    static Performance *fromHandle(zes_perf_handle_t handle) {
        return static_cast<Performance *>(handle);
    }
    bool isPerformanceEnabled = false;
};

struct PerformanceHandleContext {
    PerformanceHandleContext(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~PerformanceHandleContext();

    ze_result_t init(uint32_t subDeviceCount);

    ze_result_t performanceGet(uint32_t *pCount, zes_perf_handle_t *phPerformance);

    OsSysman *pOsSysman = nullptr;
    std::vector<Performance *> handleList = {};

  private:
    void createHandle(bool onSubdevice, uint32_t subDeviceId, zes_engine_type_flag_t domain);
    std::once_flag initPerformanceOnce;
};

} // namespace Sysman
} // namespace L0
