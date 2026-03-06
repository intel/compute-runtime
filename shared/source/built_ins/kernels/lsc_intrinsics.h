/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#ifdef USE_LSC_INTRINSICS_WB

enum LSC_LDCC {
    LSC_LDCC_DEFAULT = 0,
    LSC_LDCC_L1UC_L3UC = 1,
    LSC_LDCC_L1UC_L3C = 2,
    LSC_LDCC_L1C_L3UC = 3,
    LSC_LDCC_L1C_L3C = 4,
    LSC_LDCC_L1S_L3UC = 5,
    LSC_LDCC_L1S_L3C = 6,
    LSC_LDCC_L1IAR_L3C = 7
};

enum LSC_STCC {
    LSC_STCC_DEFAULT = 0,
    LSC_STCC_L1UC_L3UC = 1,
    LSC_STCC_L1UC_L3WB = 2,
    LSC_STCC_L1WT_L3UC = 3,
    LSC_STCC_L1WT_L3WB = 4,
    LSC_STCC_L1S_L3UC = 5,
    LSC_STCC_L1S_L3WB = 6,
    LSC_STCC_L1WB_L3WB = 7
};

uint4 __builtin_IB_lsc_load_global_uint4(const __global uint4 *base, int immElemOff, enum LSC_LDCC cacheOpt);
void __builtin_IB_lsc_store_global_uint4(__global uint4 *base, int immElemOff, uint4 val, enum LSC_STCC cacheOpt);

#endif
