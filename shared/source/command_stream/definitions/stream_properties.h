/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

struct StreamProperties {
    bool setCooperativeKernelProperties(int32_t cooperativeKernelProperties, const HardwareInfo &hwInfo) {
        return false;
    }

    int32_t getCooperativeKernelProperties() const {
        return -1;
    }
};

} // namespace NEO
