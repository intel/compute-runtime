/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_traits_common.h"

struct IsHeapfulSupported {
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
struct IsHeapfulSupportedAnd {
    template <PRODUCT_FAMILY prodFamily>
    static constexpr bool isMatched() {
        return IsHeapfulSupported::template isMatched<prodFamily>() && DependentMatcher::template isMatched<prodFamily>();
    }
};

using IsHeapfulSupportedAndAtLeastXeCore = IsHeapfulSupportedAnd<IsAtLeastXeCore>;
using IsHeapfulSupportedAndAtLeastXeHpcCore = IsHeapfulSupportedAnd<IsAtLeastXeHpcCore>;
using IsHeapfulSupportedAndAtLeastXe2HpgCore = IsHeapfulSupportedAnd<IsAtLeastXe2HpgCore>;
using IsHeapfulSupportedAndAtLeastXe3Core = IsHeapfulSupportedAnd<IsAtLeastXe3Core>;
