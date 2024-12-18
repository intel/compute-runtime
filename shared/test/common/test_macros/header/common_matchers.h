/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

using IsGen12LP = IsGfxCore<IGFX_GEN12LP_CORE>;
using IsXeHpgCore = IsGfxCore<IGFX_XE_HPG_CORE>;
using IsXeHpcCore = IsGfxCore<IGFX_XE_HPC_CORE>;
using IsNotXeHpcCore = IsNotGfxCore<IGFX_XE_HPC_CORE>;
using IsNotXeHpgCore = IsNotGfxCore<IGFX_XE_HPG_CORE>;
using IsXe2HpgCore = IsGfxCore<IGFX_XE2_HPG_CORE>;
using IsNotXe2HpgCore = IsNotGfxCore<IGFX_XE2_HPG_CORE>;

using IsWithinXeGfxFamily = IsWithinGfxCore<IGFX_XE_HP_CORE, IGFX_XE_HPC_CORE>;
using IsNotWithinXeGfxFamily = IsNotAnyGfxCores<IGFX_XE_HP_CORE, IGFX_XE_HPG_CORE, IGFX_XE_HPC_CORE>;

using IsAtLeastXeHpCore = IsAtLeastGfxCore<IGFX_XE_HP_CORE>;
using IsAtMostXeHpCore = IsAtMostGfxCore<IGFX_XE_HP_CORE>;
using IsBeforeXeHpCore = IsBeforeGfxCore<IGFX_XE_HP_CORE>;

using IsAtLeastXeHpgCore = IsAtLeastGfxCore<IGFX_XE_HPG_CORE>;
using IsAtMostXeHpgCore = IsAtMostGfxCore<IGFX_XE_HPG_CORE>;
using IsBeforeXeHpgCore = IsBeforeGfxCore<IGFX_XE_HPG_CORE>;

using IsAtLeastXeHpcCore = IsAtLeastGfxCore<IGFX_XE_HPC_CORE>;
using IsAtMostXeHpcCore = IsAtMostGfxCore<IGFX_XE_HPC_CORE>;
using IsBeforeXeHpcCore = IsBeforeGfxCore<IGFX_XE_HPC_CORE>;

using IsAtLeastXe2HpgCore = IsAtLeastGfxCore<IGFX_XE2_HPG_CORE>;
using IsAtMostXe2HpgCore = IsAtMostGfxCore<IGFX_XE2_HPG_CORE>;
using IsWithinXeHpCoreAndXe2HpgCore = IsWithinGfxCore<IGFX_XE_HP_CORE, IGFX_XE2_HPG_CORE>;

using IsXeHpOrXeHpgCore = IsAnyGfxCores<IGFX_XE_HP_CORE, IGFX_XE_HPG_CORE>;
using IsXeHpOrXeHpcCore = IsAnyGfxCores<IGFX_XE_HP_CORE, IGFX_XE_HPC_CORE>;
using IsXeHpcOrXeHpgCore = IsAnyGfxCores<IGFX_XE_HPC_CORE, IGFX_XE_HPG_CORE>;
using IsXeHpOrXeHpcOrXeHpgCore = IsAnyGfxCores<IGFX_XE_HP_CORE, IGFX_XE_HPC_CORE, IGFX_XE_HPG_CORE>;

using IsNotXeHpOrXeHpgCore = IsNotAnyGfxCores<IGFX_XE_HP_CORE, IGFX_XE_HPG_CORE>;
using IsNotXeHpOrXeHpcCore = IsNotAnyGfxCores<IGFX_XE_HP_CORE, IGFX_XE_HPC_CORE>;
using IsNotXeHpgOrXeHpcCore = IsNotAnyGfxCores<IGFX_XE_HPG_CORE, IGFX_XE_HPC_CORE>;

using IsTGLLP = IsProduct<IGFX_TIGERLAKE_LP>;
using IsDG1 = IsProduct<IGFX_DG1>;
using IsADLS = IsProduct<IGFX_ALDERLAKE_S>;
using IsADLP = IsProduct<IGFX_ALDERLAKE_P>;
using IsRKL = IsProduct<IGFX_ROCKETLAKE>;

using IsMTL = IsProduct<IGFX_METEORLAKE>;
using IsARL = IsProduct<IGFX_ARROWLAKE>;
using IsDG2 = IsProduct<IGFX_DG2>;

using IsPVC = IsProduct<IGFX_PVC>;

using IsBMG = IsProduct<IGFX_BMG>;
using IsNotBMG = IsNotWithinProducts<IGFX_BMG, IGFX_BMG>;

using IsLNL = IsProduct<IGFX_LUNARLAKE>;

using IsAtLeastMtl = IsAtLeastProduct<IGFX_METEORLAKE>;
using IsAtMostDg2 = IsAtMostProduct<IGFX_DG2>;

using IsNotDG1 = IsNotWithinProducts<IGFX_DG1, IGFX_DG1>;
using IsAtLeastPVC = IsAtLeastProduct<IGFX_PVC>;
using IsAtMostPVC = IsAtMostProduct<IGFX_PVC>;
using IsNotPVC = IsNotWithinProducts<IGFX_PVC, IGFX_PVC>;
using IsNotPvcOrDg2 = IsNotWithinProducts<IGFX_DG2, IGFX_PVC>;

using IsAtMostArl = IsAtMostProduct<IGFX_ARROWLAKE>;
using IsAtLeastBmg = IsAtLeastProduct<IGFX_BMG>;

using HasStatefulSupport = IsNotAnyGfxCores<IGFX_XE_HPC_CORE>;

using HasNoStatefulSupport = IsAnyGfxCores<IGFX_XE_HPC_CORE>;

using HasOclocZebinFormatEnforced = IsAnyProducts<IGFX_TIGERLAKE_LP,
                                                  IGFX_ROCKETLAKE,
                                                  IGFX_ALDERLAKE_S,
                                                  IGFX_ALDERLAKE_P,
                                                  IGFX_ALDERLAKE_N>;

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
