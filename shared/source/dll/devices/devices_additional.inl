/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#if SUPPORT_XE_HPC_CORE
#ifdef SUPPORT_PVC
DEVICE(0x0BD0, PvcHwConfig)
DEVICE(0x0BD5, PvcHwConfig)
DEVICE(0x0BD6, PvcHwConfig)
DEVICE(0x0BD7, PvcHwConfig)
DEVICE(0x0BD8, PvcHwConfig)
DEVICE(0x0BD9, PvcHwConfig)
DEVICE(0x0BDA, PvcHwConfig)
DEVICE(0x0BDB, PvcHwConfig)
#endif
#endif

#ifdef SUPPORT_XE_HPG_CORE
#ifdef SUPPORT_DG2
DEVICE(0x4F80, Dg2HwConfig)
DEVICE(0x4F81, Dg2HwConfig)
DEVICE(0x4F82, Dg2HwConfig)
DEVICE(0x4F83, Dg2HwConfig)
DEVICE(0x4F84, Dg2HwConfig)
DEVICE(0x4F85, Dg2HwConfig)
DEVICE(0x4F86, Dg2HwConfig)
DEVICE(0x4F87, Dg2HwConfig)
DEVICE(0x4F88, Dg2HwConfig)
DEVICE(0x5690, Dg2HwConfig)
DEVICE(0x5691, Dg2HwConfig)
DEVICE(0x5692, Dg2HwConfig)
DEVICE(0x5693, Dg2HwConfig)
DEVICE(0x5694, Dg2HwConfig)
DEVICE(0x5695, Dg2HwConfig)
DEVICE(0x5696, Dg2HwConfig)
DEVICE(0x5697, Dg2HwConfig)
DEVICE(0x56A3, Dg2HwConfig)
DEVICE(0x56A4, Dg2HwConfig)
DEVICE(0x56B0, Dg2HwConfig)
DEVICE(0x56B1, Dg2HwConfig)
DEVICE(0x56B2, Dg2HwConfig)
DEVICE(0x56B3, Dg2HwConfig)
DEVICE(0x56A0, Dg2HwConfig)
DEVICE(0x56A1, Dg2HwConfig)
DEVICE(0x56A2, Dg2HwConfig)
DEVICE(0x56A5, Dg2HwConfig)
DEVICE(0x56A6, Dg2HwConfig)
DEVICE(0x56C0, Dg2HwConfig)
DEVICE(0x56C1, Dg2HwConfig)
#endif
#endif

#ifdef SUPPORT_GEN12LP
#ifdef SUPPORT_ADLP
DEVICE(0x46A0, AdlpHwConfig)
DEVICE(0x46B0, AdlpHwConfig)
DEVICE(0x46A1, AdlpHwConfig)
DEVICE(0x46A2, AdlpHwConfig)
DEVICE(0x46A3, AdlpHwConfig)
DEVICE(0x46A6, AdlpHwConfig)
DEVICE(0x46A8, AdlpHwConfig)
DEVICE(0x46AA, AdlpHwConfig)
DEVICE(0x462A, AdlpHwConfig)
DEVICE(0x4626, AdlpHwConfig)
DEVICE(0x4628, AdlpHwConfig)
DEVICE(0x46B1, AdlpHwConfig)
DEVICE(0x46B2, AdlpHwConfig)
DEVICE(0x46B3, AdlpHwConfig)
DEVICE(0x46C0, AdlpHwConfig)
DEVICE(0x46C1, AdlpHwConfig)
DEVICE(0x46C2, AdlpHwConfig)
DEVICE(0x46C3, AdlpHwConfig)
// RPL-P
DEVICE(0xA7A0, AdlpHwConfig)
DEVICE(0xA720, AdlpHwConfig)
DEVICE(0xA7A8, AdlpHwConfig)
DEVICE(0xA7A1, AdlpHwConfig)
DEVICE(0xA721, AdlpHwConfig)
DEVICE(0xA7A9, AdlpHwConfig)
#endif
#endif
