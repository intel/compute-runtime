/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/tools_init_imp.h"

#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/tools_init.h"
#include "level_zero/tools/source/tracing/tracing.h"
#include "level_zero/tools/source/tracing/tracing_imp.h"

#include "pin/pin.h"
#include "sysman/sysman.h"

#include <mutex>
#include <thread>

namespace L0 {

static void enableTools(ze_result_t &result, ze_init_flag_t flag, bool *ptoolsAreEnabled) {
    MetricContext::enableMetricApi(std::ref(result));
    if (result != ZE_RESULT_SUCCESS) {
        return;
    }
    SysmanHandleContext::init(flag);
    PinContext::init(flag);
    APITracerContextImp::apiTracingEnable(flag);
    *ptoolsAreEnabled = true;
}

ze_result_t ToolsInitImp::initTools(ze_init_flag_t flag) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    std::call_once(this->initToolsOnce, enableTools, std::ref(result), flag, &this->toolsAreEnabled);
    return result;
}

bool ToolsInitImp::areToolsEnabled() {
    return this->toolsAreEnabled;
}

} // namespace L0
