/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zet_api.h>

struct _zet_tracer_exp_handle_t {};

namespace L0 {

struct APITracer : _zet_tracer_exp_handle_t {
    static APITracer *create();
    virtual ~APITracer() = default;
    static APITracer *fromHandle(zet_tracer_exp_handle_t handle) { return static_cast<APITracer *>(handle); }
    inline zet_tracer_exp_handle_t toHandle() { return this; }
    virtual ze_result_t destroyTracer(zet_tracer_exp_handle_t phTracer) = 0;
    virtual ze_result_t setPrologues(zet_core_callbacks_t *pCoreCbs) = 0;
    virtual ze_result_t setEpilogues(zet_core_callbacks_t *pCoreCbs) = 0;
    virtual ze_result_t enableTracer(ze_bool_t enable) = 0;
};

ze_result_t createAPITracer(zet_context_handle_t hContext, const zet_tracer_exp_desc_t *desc, zet_tracer_exp_handle_t *phTracer);

struct APITracerContext {
    virtual ~APITracerContext() = default;
    virtual void *getActiveTracersList() = 0;
    virtual void releaseActivetracersList() = 0;
};

} // namespace L0
