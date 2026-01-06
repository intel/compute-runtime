/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/device_caps_reader.h"

using namespace NEO;

class DeviceCapsReaderMock : public DeviceCapsReader {
  public:
    DeviceCapsReaderMock(std::vector<uint32_t> &caps) : caps(caps) {}
    std::vector<uint32_t> caps;

    uint32_t operator[](size_t offsetDw) const override {
        if (offsetDw >= caps.size()) {
            DEBUG_BREAK_IF(true);
            return 0;
        }

        return caps[offsetDw];
    }
};
