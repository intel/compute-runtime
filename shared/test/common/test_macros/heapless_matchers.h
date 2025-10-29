/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/state_base_address_helper.h"

#include "common_matchers.h"

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

struct IsSbaRequired {
    template <PRODUCT_FAMILY prodFamily>
    static constexpr bool isMatched() {
        [[maybe_unused]] const GFXCORE_FAMILY gfxCoreFamily = NEO::ToGfxCoreFamily<prodFamily>::get();
        using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
        if constexpr (NEO::GfxFamilyWithSBA<FamilyType>) {
            return true;
        }

        return false;
    }
};

template <typename DependentMatcher>
struct IsSbaRequiredAnd {
    template <PRODUCT_FAMILY prodFamily>
    static constexpr bool isMatched() {
        return IsSbaRequired::template isMatched<prodFamily>() && DependentMatcher::template isMatched<prodFamily>();
    }
};

using IsHeapfulRequiredAndAtLeastXeCore = IsHeapfulRequiredAnd<IsAtLeastXeCore>;
using IsSbaRequiredAndAtLeastXeCore = IsSbaRequiredAnd<IsAtLeastXeCore>;
using IsHeapfulRequiredAndAtLeastXeHpcCore = IsHeapfulRequiredAnd<IsAtLeastXeHpcCore>;
using IsHeapfulRequiredAndAtLeastXe2HpgCore = IsHeapfulRequiredAnd<IsAtLeastXe2HpgCore>;
using IsHeapfulRequiredAndAtLeastXe3Core = IsHeapfulRequiredAnd<IsAtLeastXe3Core>;
