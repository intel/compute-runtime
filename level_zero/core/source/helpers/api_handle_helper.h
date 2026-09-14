/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/ddi/ze_ddi_tables.h"
#include <level_zero/ze_ddi.h>
#include <level_zero/ze_ddi_common.h>
#include <level_zero/zes_ddi.h>
#include <level_zero/zet_ddi.h>

#include <cstdint>
#include <type_traits>

namespace L0 {
enum class IpcHandleType : uint8_t {
    fdHandle = 0,
    ntHandle = 1,
    maxHandle
};
} // namespace L0

struct BaseHandle {
    ze_handle_t baseHandle{
        .pCore = &L0::globalDriverDispatch.core,
        .pTools = &L0::globalDriverDispatch.tools,
        .pSysman = &L0::globalDriverDispatch.sysman,
        .pRuntime = &L0::globalDriverDispatch.runtime

    };
};

template <typename T>
concept IsCompliantWithDdiHandlesExt =
    std::is_standard_layout_v<T> &&
    offsetof(T, baseHandle) == 0;
