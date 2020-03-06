/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/tracing/tracing.h"
#include <level_zero/zet_api.h>

extern "C" {

__zedllexport ze_result_t __zecall
zetTracerCreate(
    zet_driver_handle_t hDriver,
    const zet_tracer_desc_t *desc,
    zet_tracer_handle_t *phTracer) {
    {
        if (ZET_TRACER_DESC_VERSION_CURRENT > desc->version) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
    }
    return L0::createAPITracer(hDriver, desc, phTracer);
}

__zedllexport ze_result_t __zecall
zetTracerDestroy(
    zet_tracer_handle_t hTracer) {
    {
        if (nullptr == hTracer) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }
    return L0::APITracer::fromHandle(hTracer)->destroyTracer(hTracer);
}

__zedllexport ze_result_t __zecall
zetTracerSetPrologues(
    zet_tracer_handle_t hTracer,
    zet_core_callbacks_t *pCoreCbs) {
    {
        if (nullptr == hTracer) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        if (nullptr == pCoreCbs) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }
    return L0::APITracer::fromHandle(hTracer)->setPrologues(pCoreCbs);
}

__zedllexport ze_result_t __zecall
zetTracerSetEpilogues(
    zet_tracer_handle_t hTracer,
    zet_core_callbacks_t *pCoreCbs) {
    {
        if (nullptr == hTracer) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        if (nullptr == pCoreCbs) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }
    return L0::APITracer::fromHandle(hTracer)->setEpilogues(pCoreCbs);
}

__zedllexport ze_result_t __zecall
zetTracerSetEnabled(
    zet_tracer_handle_t hTracer,
    ze_bool_t enable) {
    ze_result_t result;
    {
        if (nullptr == hTracer) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }
    result = L0::APITracer::fromHandle(hTracer)->enableTracer(enable);
    return result;
}
}
