/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/driver_experimental/mcl_ext/zex_mutable_cmdlist_variable_ext.h"
#include "level_zero/driver_experimental/mcl_ext/zex_mutable_cmdlist_variable_info_ext.h"

struct _zex_variable_handle_t {
    zex_variable_info_t info;
};

namespace L0::MCL {
using VariableHandle = _zex_variable_handle_t;
} // namespace L0::MCL
