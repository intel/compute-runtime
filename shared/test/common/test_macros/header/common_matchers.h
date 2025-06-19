/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

using IsGen12LP = IsGfxCore<IGFX_GEN12LP_CORE>;
using IsXeCore = IsWithinGfxCore<IGFX_XE_HPG_CORE, IGFX_XE_HPC_CORE>;
using IsNotXeCore = IsNotWithinGfxCore<IGFX_XE_HPG_CORE, IGFX_XE_HPC_CORE>;
using IsXeHpgCore = IsGfxCore<IGFX_XE_HPG_CORE>;
using IsNotXeHpgCore = IsNotGfxCore<IGFX_XE_HPG_CORE>;
using IsXeHpcCore = IsGfxCore<IGFX_XE_HPC_CORE>;
using IsNotXeHpcCore = IsNotGfxCore<IGFX_XE_HPC_CORE>;
using IsXe2HpgCore = IsGfxCore<IGFX_XE2_HPG_CORE>;
using IsXe3Core = IsGfxCore<IGFX_XE3_CORE>;

using IsAtLeastXeCore = IsAtLeastGfxCore<IGFX_XE_HPG_CORE>;
using IsAtMostXeHpgCore = IsAtMostGfxCore<IGFX_XE_HPG_CORE>;
using IsAtLeastXeHpcCore = IsAtLeastGfxCore<IGFX_XE_HPC_CORE>;
using IsAtMostXeCore = IsAtMostGfxCore<IGFX_XE_HPC_CORE>;
using IsAtLeastXe2HpgCore = IsAtLeastGfxCore<IGFX_XE2_HPG_CORE>;
using IsAtMostXe2HpgCore = IsAtMostGfxCore<IGFX_XE2_HPG_CORE>;
using IsAtLeastXe3Core = IsAtLeastGfxCore<IGFX_XE3_CORE>;
using IsAtMostXe3Core = IsAtMostGfxCore<IGFX_XE3_CORE>;

using IsWithinXeCoreAndXe2HpgCore = IsWithinGfxCore<IGFX_XE_HPG_CORE, IGFX_XE2_HPG_CORE>;
using IsWithinXeCoreAndXe3Core = IsWithinGfxCore<IGFX_XE_HPG_CORE, IGFX_XE3_CORE>;
using IsWithinXeHpcCoreAndXe2HpgCore = IsWithinGfxCore<IGFX_XE_HPC_CORE, IGFX_XE2_HPG_CORE>;
using IsWithinXe2HpgCoreAndXe3Core = IsWithinGfxCore<IGFX_XE2_HPG_CORE, IGFX_XE3_CORE>;
using IsWithinXeHpcCoreAndXe3Core = IsWithinGfxCore<IGFX_XE_HPC_CORE, IGFX_XE3_CORE>;

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

using IsAtMostDg2 = IsAtMostProduct<IGFX_DG2>;
using IsAtLeastPVC = IsAtLeastProduct<IGFX_PVC>;
using IsAtMostPVC = IsAtMostProduct<IGFX_PVC>;
using IsAtLeastMtl = IsAtLeastProduct<IGFX_METEORLAKE>;

using IsNotPvcOrDg2 = IsNotWithinProducts<IGFX_DG2, IGFX_PVC>;

using HasStatefulSupport = IsNotAnyGfxCores<IGFX_XE_HPC_CORE>;

using HasNoStatefulSupport = IsAnyGfxCores<IGFX_XE_HPC_CORE>;

using HasOclocZebinFormatEnforced = IsAnyProducts<IGFX_TIGERLAKE_LP,
                                                  IGFX_ROCKETLAKE,
                                                  IGFX_ALDERLAKE_S,
                                                  IGFX_ALDERLAKE_P,
                                                  IGFX_ALDERLAKE_N>;

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
