/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zet_api.h>

namespace L0 {
ze_result_t zetTracerExpCreate(
    zet_context_handle_t hContext,
    const zet_tracer_exp_desc_t *desc,
    zet_tracer_exp_handle_t *phTracer);

ze_result_t zetTracerExpDestroy(
    zet_tracer_exp_handle_t hTracer);

ze_result_t zetTracerExpSetPrologues(
    zet_tracer_exp_handle_t hTracer,
    zet_core_callbacks_t *pCoreCbs);

ze_result_t zetTracerExpSetEpilogues(
    zet_tracer_exp_handle_t hTracer,
    zet_core_callbacks_t *pCoreCbs);

ze_result_t zetTracerExpSetEnabled(
    zet_tracer_exp_handle_t hTracer,
    ze_bool_t enable);

} // namespace L0
