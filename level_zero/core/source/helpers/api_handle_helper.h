/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/ddi/ze_ddi_tables.h"
#include <level_zero/loader/ze_loader.h>
#include <level_zero/ze_ddi.h>
#include <level_zero/ze_ddi_common.h>
#include <level_zero/zes_ddi.h>
#include <level_zero/zet_ddi.h>

#include <cstdint>

namespace L0 {
extern decltype(&zelLoaderTranslateHandle) loaderTranslateHandleFunc;
} // namespace L0

constexpr uint64_t objMagicValue = 0x8D7E6A5D4B3E2E1FULL;

template <typename T>
inline T toInternalType(T input) {
    if (!input || input->objMagic == objMagicValue) {
        return input;
    }

    if (L0::loaderTranslateHandleFunc) {
        T output{};
        auto retVal = L0::loaderTranslateHandleFunc(input->handleType, reinterpret_cast<void *>(input), reinterpret_cast<void **>(&output));
        if (retVal == ZE_RESULT_SUCCESS) {
            return output;
        }
    }
    return nullptr;
}

struct BaseHandle : ze_handle_t {
    BaseHandle() {
        pCore = &L0::globalDriverDispatch.core;
        pTools = &L0::globalDriverDispatch.tools;
        pSysman = &L0::globalDriverDispatch.sysman;
    }
};

template <zel_handle_type_t hType>
struct BaseHandleWithLoaderTranslation : BaseHandle {
    uint64_t objMagic = objMagicValue;
    static const zel_handle_type_t handleType = hType;
};

template <typename T>
concept IsCompliantWithDdiHandlesExt = offsetof(T, pCore) ==
offsetof(ze_handle_t, pCore) &&
    offsetof(T, pTools) == offsetof(ze_handle_t, pTools) &&
    offsetof(T, pSysman) == offsetof(ze_handle_t, pSysman);