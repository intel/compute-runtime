/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "common_matchers.h"

struct IsLeoSupported {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return IsCRI::isMatched<productFamily>() || IsNVLS::isMatched<productFamily>() || IsNVLP::isMatched<productFamily>();
    }
};

struct IsNotLeoSupported {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return !IsLeoSupported::isMatched<productFamily>();
    }
};
