/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/loader/ze_loader.h>

#include <cstdint>
namespace L0 {
extern decltype(&zelLoaderTranslateHandle) loaderTranslateHandleFunc;
}

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
