/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/tracing/tracing.h"
#include <level_zero/zet_api.h>

#include "third_party/level_zero/ze_api_ext.h"

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zetTracerCreate(
    zet_driver_handle_t hDriver,
    const zet_tracer_desc_t *desc,
    zet_tracer_handle_t *phTracer) {
    return L0::createAPITracer(hDriver, desc, phTracer);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetTracerDestroy(
    zet_tracer_handle_t hTracer) {
    return L0::APITracer::fromHandle(hTracer)->destroyTracer(hTracer);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetTracerSetPrologues(
    zet_tracer_handle_t hTracer,
    zet_core_callbacks_t *pCoreCbs) {
    return L0::APITracer::fromHandle(hTracer)->setPrologues(pCoreCbs);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetTracerSetEpilogues(
    zet_tracer_handle_t hTracer,
    zet_core_callbacks_t *pCoreCbs) {
    return L0::APITracer::fromHandle(hTracer)->setEpilogues(pCoreCbs);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zetTracerSetEnabled(
    zet_tracer_handle_t hTracer,
    ze_bool_t enable) {
    return L0::APITracer::fromHandle(hTracer)->enableTracer(enable);
}

} // extern C
