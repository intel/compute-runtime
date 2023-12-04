/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

template <size_t x, size_t y, size_t z>
struct StatickSize3 {
    operator const size_t *() {
        static const size_t v[] = {z, y, z};
        return v;
    }
};
