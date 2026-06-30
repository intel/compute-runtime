/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zes_intel_gpu_sysman.h>

#include <memory>
#include <mutex>
#include <vector>

namespace L0 {
namespace Sysman {

struct OsSysman;

class InfoLog {
  public:
    virtual ~InfoLog() = default;

    virtual ze_result_t infoLogGetProperties(zes_intel_info_log_properties_exp_t *pProperties) = 0;
    virtual ze_result_t infoLogRead(uint32_t *pSize, uint8_t *pBuffer) = 0;
    virtual ze_result_t infoLogEnable(bool state) = 0;

    static InfoLog *fromHandle(zes_intel_info_log_handle_t handle) {
        return reinterpret_cast<InfoLog *>(handle);
    }

    inline zes_intel_info_log_handle_t toHandle() {
        return reinterpret_cast<zes_intel_info_log_handle_t>(this);
    }
};

struct InfoLogHandleContext {
    InfoLogHandleContext() {};
    ~InfoLogHandleContext();

    void init();
    ze_result_t infoLogGet(uint32_t *pCount, zes_intel_info_log_handle_t *phInfoLogs);
    void releaseInfoLogHandles();

    std::vector<std::unique_ptr<InfoLog>> handleList = {};

  private:
    void createHandle();
    std::once_flag initInfoLogOnce;
    bool infoLogInitDone = false;
};

} // namespace Sysman
} // namespace L0
