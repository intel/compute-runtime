/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/tracing/tracing.h"
#include <level_zero/zet_api.h>

ZE_APIEXPORT ze_result_t ZE_APICALL
zetTracerExpCreate(
    zet_context_handle_t hContext,
    const zet_tracer_exp_desc_t *desc,
    zet_tracer_exp_handle_t *phTracer) {
    return L0::createAPITracer(hContext, desc, phTracer);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetTracerExpDestroy(
    zet_tracer_exp_handle_t hTracer) {
    return L0::APITracer::fromHandle(hTracer)->destroyTracer(hTracer);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetTracerExpSetPrologues(
    zet_tracer_exp_handle_t hTracer,
    zet_core_callbacks_t *pCoreCbs) {
    return L0::APITracer::fromHandle(hTracer)->setPrologues(pCoreCbs);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetTracerExpSetEpilogues(
    zet_tracer_exp_handle_t hTracer,
    zet_core_callbacks_t *pCoreCbs) {
    return L0::APITracer::fromHandle(hTracer)->setEpilogues(pCoreCbs);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetTracerExpSetEnabled(
    zet_tracer_exp_handle_t hTracer,
    ze_bool_t enable) {
    return L0::APITracer::fromHandle(hTracer)->enableTracer(enable);
}
