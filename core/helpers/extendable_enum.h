/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

struct ExtendableEnum {
    constexpr operator uint32_t() const {
        return value;
    }

    constexpr ExtendableEnum(uint32_t val) : value(val) {}

  protected:
    uint32_t value;
};
