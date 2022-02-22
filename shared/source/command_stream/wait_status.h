/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

enum class WaitStatus {
    NotReady = 0,
    Ready = 1,
    GpuHang = 2,
};

} // namespace NEO