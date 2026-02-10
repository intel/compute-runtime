/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_mapper.h"

#include "test_traits_common.h"

template <GFXCORE_FAMILY gfxCoreFamily>
struct IsGfxCore {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return NEO::ToGfxCoreFamily<productFamily>::get() == gfxCoreFamily;
    }
};

template <GFXCORE_FAMILY gfxCoreFamily>
struct IsNotGfxCore {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return NEO::ToGfxCoreFamily<productFamily>::get() != gfxCoreFamily;
    }
};

template <GFXCORE_FAMILY gfxCoreFamily, GFXCORE_FAMILY gfxCoreFamily2>
struct AreNotGfxCores {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return NEO::ToGfxCoreFamily<productFamily>::get() != gfxCoreFamily && NEO::ToGfxCoreFamily<productFamily>::get() != gfxCoreFamily2;
    }
};

template <GFXCORE_FAMILY gfxCoreFamily>
struct IsAtMostGfxCore {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return NEO::ToGfxCoreFamily<productFamily>::get() <= gfxCoreFamily;
    }
};

template <GFXCORE_FAMILY gfxCoreFamily>
struct IsAtLeastGfxCore {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return NEO::ToGfxCoreFamily<productFamily>::get() >= gfxCoreFamily;
    }
};

template <GFXCORE_FAMILY gfxCoreFamilyMin, GFXCORE_FAMILY gfxCoreFamilyMax>
struct IsWithinGfxCore {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return NEO::ToGfxCoreFamily<productFamily>::get() >= gfxCoreFamilyMin && NEO::ToGfxCoreFamily<productFamily>::get() <= gfxCoreFamilyMax;
    }
};

template <GFXCORE_FAMILY gfxCoreFamilyMin, GFXCORE_FAMILY gfxCoreFamilyMax>
struct IsNotWithinGfxCore {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return NEO::ToGfxCoreFamily<productFamily>::get() < gfxCoreFamilyMin || NEO::ToGfxCoreFamily<productFamily>::get() > gfxCoreFamilyMax;
    }
};

template <GFXCORE_FAMILY... args>
struct IsAnyGfxCores {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return (... || IsGfxCore<args>::template isMatched<productFamily>());
    }
};

template <GFXCORE_FAMILY... args>
struct IsNotAnyGfxCores {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return (... && IsNotGfxCore<args>::template isMatched<productFamily>());
    }
};

template <PRODUCT_FAMILY product>
struct IsProduct {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return productFamily == product;
    }
};

template <PRODUCT_FAMILY productFamilyMax>
struct IsAtMostProduct {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return productFamily <= productFamilyMax;
    }
};

template <PRODUCT_FAMILY productFamilyMin>
struct IsAtLeastProduct {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return productFamily >= productFamilyMin;
    }
};

template <PRODUCT_FAMILY productFamilyMin, PRODUCT_FAMILY productFamilyMax>
struct IsWithinProducts {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return productFamily >= productFamilyMin && productFamily <= productFamilyMax;
    }
};

template <PRODUCT_FAMILY productFamilyMin, PRODUCT_FAMILY productFamilyMax>
struct IsNotWithinProducts {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return (productFamily < productFamilyMin) || (productFamily > productFamilyMax);
    }
};

template <PRODUCT_FAMILY... args>
struct IsAnyProducts {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return (... || IsProduct<args>::template isMatched<productFamily>());
    }
};

template <PRODUCT_FAMILY... args>
struct IsNoneProducts {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return (... && !(IsProduct<args>::template isMatched<productFamily>()));
    }
};

struct MatchAny {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() { return true; }
};

struct SupportsSampler {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return NEO::HwMapper<productFamily>::GfxProduct::supportsSampler;
    }
};

struct HeapfulSupportedMatch {

    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        [[maybe_unused]] const GFXCORE_FAMILY gfxCoreFamily = NEO::ToGfxCoreFamily<productFamily>::get();
        using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
        using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
        constexpr bool heaplessModeEnabled = FamilyType::template isHeaplessMode<DefaultWalkerType>();
        return !heaplessModeEnabled;
    }
};

using IsGen12LP = IsGfxCore<IGFX_GEN12LP_CORE>;
using IsXeCore = IsWithinGfxCore<IGFX_XE_HPG_CORE, IGFX_XE_HPC_CORE>;
using IsNotXeCore = IsNotWithinGfxCore<IGFX_XE_HPG_CORE, IGFX_XE_HPC_CORE>;
using IsXeHpgCore = IsGfxCore<IGFX_XE_HPG_CORE>;
using IsNotXeHpgCore = IsNotGfxCore<IGFX_XE_HPG_CORE>;
using IsXeHpcCore = IsGfxCore<IGFX_XE_HPC_CORE>;
using IsNotXeHpcCore = IsNotGfxCore<IGFX_XE_HPC_CORE>;
using IsXe2HpgCore = IsGfxCore<IGFX_XE2_HPG_CORE>;
using IsXe3Core = IsGfxCore<IGFX_XE3_CORE>;
using IsXe3pCore = IsGfxCore<IGFX_XE3P_CORE>;

using IsAtLeastXeCore = IsAtLeastGfxCore<IGFX_XE_HPG_CORE>;
using IsAtMostXeHpgCore = IsAtMostGfxCore<IGFX_XE_HPG_CORE>;
using IsAtLeastXeHpcCore = IsAtLeastGfxCore<IGFX_XE_HPC_CORE>;
using IsAtMostXeCore = IsAtMostGfxCore<IGFX_XE_HPC_CORE>;
using IsAtLeastXe2HpgCore = IsAtLeastGfxCore<IGFX_XE2_HPG_CORE>;
using IsAtMostXe2HpgCore = IsAtMostGfxCore<IGFX_XE2_HPG_CORE>;
using IsAtLeastXe3Core = IsAtLeastGfxCore<IGFX_XE3_CORE>;
using IsAtMostXe3Core = IsAtMostGfxCore<IGFX_XE3_CORE>;
using IsAtLeastXe3pCore = IsAtLeastGfxCore<IGFX_XE3P_CORE>;
using IsAtMostXe3pCore = IsAtMostGfxCore<IGFX_XE3P_CORE>;

using IsWithinXeCoreAndXe2HpgCore = IsWithinGfxCore<IGFX_XE_HPG_CORE, IGFX_XE2_HPG_CORE>;
using IsWithinXeCoreAndXe3Core = IsWithinGfxCore<IGFX_XE_HPG_CORE, IGFX_XE3_CORE>;
using IsWithinXeHpcCoreAndXe2HpgCore = IsWithinGfxCore<IGFX_XE_HPC_CORE, IGFX_XE2_HPG_CORE>;
using IsWithinXe2HpgCoreAndXe3Core = IsWithinGfxCore<IGFX_XE2_HPG_CORE, IGFX_XE3_CORE>;
using IsWithinXeHpcCoreAndXe3Core = IsWithinGfxCore<IGFX_XE_HPC_CORE, IGFX_XE3_CORE>;
using IsWithinXeHpcCoreAndXe3pCore = IsWithinGfxCore<IGFX_XE_HPC_CORE, IGFX_XE3P_CORE>;

using IsTGLLP = IsProduct<IGFX_TIGERLAKE_LP>;
using IsRKL = IsProduct<IGFX_ROCKETLAKE>;
using IsADLS = IsProduct<IGFX_ALDERLAKE_S>;
using IsADLP = IsProduct<IGFX_ALDERLAKE_P>;
using IsDG1 = IsProduct<IGFX_DG1>;
using IsNotDG1 = IsNotWithinProducts<IGFX_DG1, IGFX_DG1>;
using IsDG2 = IsProduct<IGFX_DG2>;
using IsNotDG2 = IsNotWithinProducts<IGFX_DG2, IGFX_DG2>;
using IsPVC = IsProduct<IGFX_PVC>;
using IsNotPVC = IsNotWithinProducts<IGFX_PVC, IGFX_PVC>;
using IsMTL = IsProduct<IGFX_METEORLAKE>;
using IsNotMTL = IsNotWithinProducts<IGFX_METEORLAKE, IGFX_METEORLAKE>;
using IsARL = IsProduct<IGFX_ARROWLAKE>;
using IsBMG = IsProduct<IGFX_BMG>;
using IsNotBMG = IsNotWithinProducts<IGFX_BMG, IGFX_BMG>;
using IsLNL = IsProduct<IGFX_LUNARLAKE>;
using IsPTL = IsProduct<IGFX_PTL>;
using IsNVLS = IsProduct<IGFX_NVL_XE3G>;
using IsCRI = IsProduct<IGFX_CRI>;

using IsAtMostDg2 = IsAtMostProduct<IGFX_DG2>;
using IsAtLeastPVC = IsAtLeastProduct<IGFX_PVC>;
using IsAtMostPVC = IsAtMostProduct<IGFX_PVC>;
using IsAtLeastMtl = IsAtLeastProduct<IGFX_METEORLAKE>;
using IsAtMostBMG = IsAtMostProduct<IGFX_BMG>;

using IsNotPvcOrDg2 = IsNotWithinProducts<IGFX_DG2, IGFX_PVC>;

using IsNotCriOrBmg = IsNotWithinProducts<IGFX_BMG, IGFX_CRI>;

using HasStatefulSupport = IsNotAnyGfxCores<IGFX_XE_HPC_CORE>;

using HasNoStatefulSupport = IsAnyGfxCores<IGFX_XE_HPC_CORE>;

struct HasDispatchAllSupport {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return IsPVC::isMatched<productFamily>() || IsAtLeastXe2HpgCore::isMatched<productFamily>();
    }
};

struct DoesNotHaveDispatchAllSupport {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return !IsPVC::isMatched<productFamily>() && IsAtMostXeCore::isMatched<productFamily>();
    }
};

struct IsXeLpg {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return IsXeHpgCore::isMatched<productFamily>() && !IsDG2::isMatched<productFamily>();
    }
};

struct IsStatefulBufferPreferredForProduct {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return IsGen12LP::isMatched<productFamily>() ||
               IsXeHpgCore::isMatched<productFamily>();
    }
};

struct IsStatelessBufferPreferredForProduct {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return !IsStatefulBufferPreferredForProduct::isMatched<productFamily>();
    }
};

struct HeaplessSupport {
    template <PRODUCT_FAMILY productFamily>
    static consteval bool isMatched() {
        using T = TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>;
        if constexpr (requires { T::heaplessAllowed; }) {
            return static_cast<bool>(T::heaplessAllowed);
        } else {
            return false;
        }
    }
};

struct ImageSupport {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::imagesSupported;
    }
};

template <typename GfxFamily>
concept HasSemaphore64bCmd = requires {
                                 typename GfxFamily::MI_SEMAPHORE_WAIT_64;
                             };
