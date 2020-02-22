/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

template <size_t X, size_t Y, size_t Z>
struct StatickSize3 {
    operator const size_t *() {
        static const size_t v[] = {X, Y, Z};
        return v;
    }
};
