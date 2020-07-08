/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zet_api.h>
#include <level_zero/zet_tracing.h>

struct _zet_tracer_handle_t {};

namespace L0 {

struct APITracer : _zet_tracer_handle_t {
    static APITracer *create();
    virtual ~APITracer() = default;
    static APITracer *fromHandle(zet_tracer_handle_t handle) { return static_cast<APITracer *>(handle); }
    inline zet_tracer_handle_t toHandle() { return this; }
    virtual ze_result_t destroyTracer(zet_tracer_handle_t phTracer) = 0;
    virtual ze_result_t setPrologues(zet_core_callbacks_t *pCoreCbs) = 0;
    virtual ze_result_t setEpilogues(zet_core_callbacks_t *pCoreCbs) = 0;
    virtual ze_result_t enableTracer(ze_bool_t enable) = 0;
};

ze_result_t createAPITracer(zet_driver_handle_t hDriver, const zet_tracer_desc_t *desc, zet_tracer_handle_t *phTracer);

struct APITracerContext {
    virtual ~APITracerContext() = default;
    virtual void *getActiveTracersList() = 0;
    virtual void releaseActivetracersList() = 0;
};

} // namespace L0
