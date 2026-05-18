/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/driver_experimental/zex_context.h"

#include "level_zero/core/source/context/context.h"

namespace L0 {
ze_result_t ZE_APICALL zexMemFreeRegisterCallbackExt(ze_context_handle_t hContext, zex_memory_free_callback_ext_desc_t *hFreeCallbackDesc, void *ptr) {
    auto context = Context::fromHandle(toInternalType(hContext));

    if (!context || !hFreeCallbackDesc || !ptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (hFreeCallbackDesc->pfnCallback == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    return context->registerMemoryFreeCallback(hFreeCallbackDesc, ptr);
}

} // namespace L0
