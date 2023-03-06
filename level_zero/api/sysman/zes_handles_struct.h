/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zes_api.h>

struct _zes_fabric_port_handle_t {
    virtual ~_zes_fabric_port_handle_t() = default;
};