/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#if SUPPORT_XE_HPC_CORE
#ifdef SUPPORT_PVC
DEVICE(0x0BD0, PvcHwConfig)
NAMEDDEVICE(0x0BD1, PvcHwConfig, "Intel(R) Data Center GPU Max 1450")
NAMEDDEVICE(0x0BD2, PvcHwConfig, "Intel(R) Data Center GPU Max 1250")
NAMEDDEVICE(0x0BD5, PvcHwConfig, "Intel(R) Data Center GPU Max 1550")
NAMEDDEVICE(0x0BD6, PvcHwConfig, "Intel(R) Data Center GPU Max 1550")
NAMEDDEVICE(0x0BD7, PvcHwConfig, "Intel(R) Data Center GPU Max 1350")
NAMEDDEVICE(0x0BD8, PvcHwConfig, "Intel(R) Data Center GPU Max 1350")
NAMEDDEVICE(0x0BD9, PvcHwConfig, "Intel(R) Data Center GPU Max 1100")
NAMEDDEVICE(0x0BDA, PvcHwConfig, "Intel(R) Data Center GPU Max 1100")
NAMEDDEVICE(0x0BDB, PvcHwConfig, "Intel(R) Data Center GPU Max 1100")
#endif
#endif

#ifdef SUPPORT_XE_HPG_CORE
#ifdef SUPPORT_MTL
DEVICE(0x7D40, MtlHwConfig)
DEVICE(0x7D55, MtlHwConfig)
DEVICE(0x7DD5, MtlHwConfig)
DEVICE(0x7D45, MtlHwConfig)
DEVICE(0x7D60, MtlHwConfig)
#endif
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
NAMEDDEVICE(0x5690, Dg2HwConfig, "Intel(R) Arc(TM) A770M Graphics")
NAMEDDEVICE(0x5691, Dg2HwConfig, "Intel(R) Arc(TM) A730M Graphics")
NAMEDDEVICE(0x5692, Dg2HwConfig, "Intel(R) Arc(TM) A550M Graphics")
NAMEDDEVICE(0x5693, Dg2HwConfig, "Intel(R) Arc(TM) A370M Graphics")
NAMEDDEVICE(0x5694, Dg2HwConfig, "Intel(R) Arc(TM) A350M Graphics")
DEVICE(0x5695, Dg2HwConfig)
DEVICE(0x5696, Dg2HwConfig)
DEVICE(0x5697, Dg2HwConfig)
DEVICE(0x56A3, Dg2HwConfig)
DEVICE(0x56A4, Dg2HwConfig)
NAMEDDEVICE(0x56B0, Dg2HwConfig, "Intel(R) Arc(TM) Pro A30M Graphics")
NAMEDDEVICE(0x56B1, Dg2HwConfig, "Intel(R) Arc(TM) Pro A40/A50 Graphics")
DEVICE(0x56B2, Dg2HwConfig)
DEVICE(0x56B3, Dg2HwConfig)
NAMEDDEVICE(0x56A0, Dg2HwConfig, "Intel(R) Arc(TM) A770 Graphics")
NAMEDDEVICE(0x56A1, Dg2HwConfig, "Intel(R) Arc(TM) A750 Graphics")
NAMEDDEVICE(0x56A2, Dg2HwConfig, "Intel(R) Arc(TM) A580 Graphics")
NAMEDDEVICE(0x56A5, Dg2HwConfig, "Intel(R) Arc(TM) A380 Graphics")
NAMEDDEVICE(0x56A6, Dg2HwConfig, "Intel(R) Arc(TM) A310 Graphics")
NAMEDDEVICE(0x56C0, Dg2HwConfig, "Intel(R) Data Center GPU Flex 170")
NAMEDDEVICE(0x56C1, Dg2HwConfig, "Intel(R) Data Center GPU Flex 140")
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
