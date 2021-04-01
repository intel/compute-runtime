/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

struct StreamProperties {
    bool setCooperativeKernelProperties(bool isCooperative) {
        return false;
    }
};

} // namespace NEO
