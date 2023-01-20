/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

using IsGen8 = IsGfxCore<IGFX_GEN8_CORE>;
using IsGen9 = IsGfxCore<IGFX_GEN9_CORE>;
using IsGen11HP = IsGfxCore<IGFX_GEN11_CORE>;
using IsGen11LP = IsGfxCore<IGFX_GEN11LP_CORE>;
using IsGen12LP = IsGfxCore<IGFX_GEN12LP_CORE>;
using IsXeHpCore = IsGfxCore<IGFX_XE_HP_CORE>;
using IsXeHpgCore = IsGfxCore<IGFX_XE_HPG_CORE>;
using IsXeHpcCore = IsGfxCore<IGFX_XE_HPC_CORE>;
using IsNotXeHpcCore = IsNotGfxCore<IGFX_XE_HPC_CORE>;
using IsNotXeHpgCore = IsNotGfxCore<IGFX_XE_HPG_CORE>;

using IsAtMostGen11 = IsAtMostGfxCore<IGFX_GEN11LP_CORE>;

using IsAtMostGen12lp = IsAtMostGfxCore<IGFX_GEN12LP_CORE>;

using IsAtLeastGen12lp = IsAtLeastGfxCore<IGFX_GEN12LP_CORE>;

using IsWithinXeGfxFamily = IsWithinGfxCore<IGFX_XE_HP_CORE, IGFX_XE_HPC_CORE>;

using IsAtLeastXeHpCore = IsAtLeastGfxCore<IGFX_XE_HP_CORE>;
using IsAtMostXeHpCore = IsAtMostGfxCore<IGFX_XE_HP_CORE>;

using IsAtLeastXeHpgCore = IsAtLeastGfxCore<IGFX_XE_HPG_CORE>;
using IsAtMostXeHpgCore = IsAtMostGfxCore<IGFX_XE_HPG_CORE>;

using IsAtLeastXeHpcCore = IsAtLeastGfxCore<IGFX_XE_HPC_CORE>;
using IsAtMostXeHpcCore = IsAtMostGfxCore<IGFX_XE_HPC_CORE>;

using IsXeHpOrXeHpgCore = IsAnyGfxCores<IGFX_XE_HP_CORE, IGFX_XE_HPG_CORE>;
using IsXeHpOrXeHpcCore = IsAnyGfxCores<IGFX_XE_HP_CORE, IGFX_XE_HPC_CORE>;
using IsXeHpcOrXeHpgCore = IsAnyGfxCores<IGFX_XE_HPC_CORE, IGFX_XE_HPG_CORE>;

using IsNotXeHpOrXeHpgCore = IsNotAnyGfxCores<IGFX_XE_HP_CORE, IGFX_XE_HPG_CORE>;
using IsNotXeHpOrXeHpcCore = IsNotAnyGfxCores<IGFX_XE_HP_CORE, IGFX_XE_HPC_CORE>;
using IsNotXeHpgOrXeHpcCore = IsNotAnyGfxCores<IGFX_XE_HPG_CORE, IGFX_XE_HPC_CORE>;

using IsSKL = IsProduct<IGFX_SKYLAKE>;
using IsKBL = IsProduct<IGFX_KABYLAKE>;
using IsCFL = IsProduct<IGFX_COFFEELAKE>;

using IsBXT = IsProduct<IGFX_BROXTON>;
using IsGLK = IsProduct<IGFX_GEMINILAKE>;

using IsICLLP = IsProduct<IGFX_ICELAKE_LP>;
using IsEHL = IsProduct<IGFX_ELKHARTLAKE>;
using IsLKF = IsProduct<IGFX_LAKEFIELD>;

using IsTGLLP = IsProduct<IGFX_TIGERLAKE_LP>;
using IsDG1 = IsProduct<IGFX_DG1>;
using IsADLS = IsProduct<IGFX_ALDERLAKE_S>;
using IsADLP = IsProduct<IGFX_ALDERLAKE_P>;
using IsRKL = IsProduct<IGFX_ROCKETLAKE>;

using IsICLLPOrTGLLP = IsAnyProducts<IGFX_ICELAKE_LP, IGFX_TIGERLAKE_LP>;

using IsXEHP = IsProduct<IGFX_XE_HP_SDV>;
using IsNotXEHP = IsNotWithinProducts<IGFX_XE_HP_SDV, IGFX_XE_HP_SDV>;

using IsMTL = IsProduct<IGFX_METEORLAKE>;
using IsDG2 = IsProduct<IGFX_DG2>;

using IsPVC = IsProduct<IGFX_PVC>;

using IsAtLeastSkl = IsAtLeastProduct<IGFX_SKYLAKE>;

using IsAtLeastMtl = IsAtLeastProduct<IGFX_METEORLAKE>;
using IsAtMostDg2 = IsAtMostProduct<IGFX_DG2>;

using IsNotPVC = IsNotWithinProducts<IGFX_PVC, IGFX_PVC>;

using HasStatefulSupport = IsNotAnyGfxCores<IGFX_XE_HPC_CORE>;

using HasNoStatefulSupport = IsAnyGfxCores<IGFX_XE_HPC_CORE>;
