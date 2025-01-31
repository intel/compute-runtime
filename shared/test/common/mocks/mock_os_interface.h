/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_interface.h"

namespace NEO {

struct MockOsInterface : public OSInterface {
    MockOsInterface() = default;
    uint32_t getAggregatedProcessCount() const override {
        return numberOfProcesses;
    }

    uint32_t numberOfProcesses = 1;
};
} // namespace NEO
