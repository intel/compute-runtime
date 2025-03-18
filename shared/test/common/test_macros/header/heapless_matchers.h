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

using IsHeapfulSupportedAndAtLeastXeHpCore = IsHeapfulSupportedAnd<IsAtLeastXeHpCore>;
using IsHeapfulSupportedAndAtLeastXeHpcCore = IsHeapfulSupportedAnd<IsAtLeastXeHpcCore>;
using IsHeapfulSupportedAndAtLeastXe3Core = IsHeapfulSupportedAnd<IsAtLeastXe3Core>;
using IsHeapfulSupportedAndAtLeastXeHpgCore = IsHeapfulSupportedAnd<IsAtLeastXeHpgCore>;
using IsHeapfulSupportedAndDG2AndLater = IsHeapfulSupportedAndAtLeastXeHpgCore;
using IsHeapfulSupportedAndAtLeastXe2HpgCore = IsHeapfulSupportedAnd<IsAtLeastXe2HpgCore>;
using IsHeapfulSupportedAndAtLeastGen12LP = IsHeapfulSupportedAnd<IsAtLeastGen12LP>;
