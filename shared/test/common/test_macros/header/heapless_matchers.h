/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_traits_common.h"

struct IsHeapfulRequired {
    template <PRODUCT_FAMILY prodFamily>
    static constexpr bool isMatched() {
        return TestTraits<NEO::template ToGfxCoreFamily<prodFamily>::get()>::heaplessRequired == false;
    }
};

struct IsHeaplessRequired {
    template <PRODUCT_FAMILY prodFamily>
    static constexpr bool isMatched() {
        return TestTraits<NEO::template ToGfxCoreFamily<prodFamily>::get()>::heaplessRequired == true;
    }
};

template <typename DependentMatcher>
struct IsHeapfulRequiredAnd {
    template <PRODUCT_FAMILY prodFamily>
    static constexpr bool isMatched() {
        return IsHeapfulRequired::template isMatched<prodFamily>() && DependentMatcher::template isMatched<prodFamily>();
    }
};

using IsHeapfulRequiredAndAtLeastXeCore = IsHeapfulRequiredAnd<IsAtLeastXeCore>;
using IsHeapfulRequiredAndAtLeastXeHpcCore = IsHeapfulRequiredAnd<IsAtLeastXeHpcCore>;
using IsHeapfulRequiredAndAtLeastXe2HpgCore = IsHeapfulRequiredAnd<IsAtLeastXe2HpgCore>;
using IsHeapfulRequiredAndAtLeastXe3Core = IsHeapfulRequiredAnd<IsAtLeastXe3Core>;
