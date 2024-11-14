/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zes_api.h>

struct _zes_fabric_port_handle_t {
    virtual ~_zes_fabric_port_handle_t() = default;
};

struct _zes_mem_handle_t {
    virtual ~_zes_mem_handle_t() = default;
};

struct _zes_pwr_handle_t {
    virtual ~_zes_pwr_handle_t() = default;
};

struct _zes_engine_handle_t {
    virtual ~_zes_engine_handle_t() = default;
};

struct _zes_freq_handle_t {
    virtual ~_zes_freq_handle_t() = default;
};

struct _zes_sched_handle_t {
    virtual ~_zes_sched_handle_t() = default;
};

struct _zes_firmware_handle_t {
    virtual ~_zes_firmware_handle_t() = default;
};

struct _zes_diag_handle_t {
    virtual ~_zes_diag_handle_t() = default;
};

struct _zes_ras_handle_t {
    virtual ~_zes_ras_handle_t() = default;
};

struct _zes_standby_handle_t {
    virtual ~_zes_standby_handle_t() = default;
};

struct _zes_temp_handle_t {
    virtual ~_zes_temp_handle_t() = default;
};
struct _zes_perf_handle_t {
    virtual ~_zes_perf_handle_t() = default;
};

struct _zes_fan_handle_t {
    virtual ~_zes_fan_handle_t() = default;
};

struct _zes_vf_handle_t {
    virtual ~_zes_vf_handle_t() = default;
};