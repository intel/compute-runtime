/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#if SUPPORT_XE3_CORE
#ifdef SUPPORT_PTL
DEVICE(0xB080, PtlHwConfig)
DEVICE(0xB081, PtlHwConfig)
DEVICE(0xB082, PtlHwConfig)
DEVICE(0xB083, PtlHwConfig)
DEVICE(0xB08F, PtlHwConfig)
DEVICE(0xB090, PtlHwConfig)
DEVICE(0xB0A0, PtlHwConfig)
DEVICE(0xB0B0, PtlHwConfig)
DEVICE(0xFD80, PtlHwConfig)
DEVICE(0xFD81, PtlHwConfig)
#endif
#endif

#if SUPPORT_XE2_HPG_CORE
#ifdef SUPPORT_BMG
DEVICE(0xE202, BmgHwConfig)
NAMEDDEVICE(0xE20B, BmgHwConfig, "Intel(R) Arc(TM) B580 Graphics")
NAMEDDEVICE(0xE20C, BmgHwConfig, "Intel(R) Arc(TM) B570 Graphics")
DEVICE(0xE20D, BmgHwConfig)
DEVICE(0xE210, BmgHwConfig)
DEVICE(0xE211, BmgHwConfig)
DEVICE(0xE212, BmgHwConfig)
DEVICE(0xE215, BmgHwConfig)
DEVICE(0xE216, BmgHwConfig)
DEVICE(0xE220, BmgHwConfig)
DEVICE(0xE221, BmgHwConfig)
DEVICE(0xE222, BmgHwConfig)
DEVICE(0xE223, BmgHwConfig)
#endif
#ifdef SUPPORT_LNL
NAMEDDEVICE(0x6420, LnlHwConfig, "Intel(R) Graphics")
NAMEDDEVICE(0x64A0, LnlHwConfig, "Intel(R) Arc(TM) Graphics")
NAMEDDEVICE(0x64B0, LnlHwConfig, "Intel(R) Graphics")
#endif
#endif

#if SUPPORT_XE_HPC_CORE
#ifdef SUPPORT_PVC
DEVICE(0x0BD0, PvcHwConfig)
NAMEDDEVICE(0x0BD5, PvcHwConfig, "Intel(R) Data Center GPU Max 1550")
NAMEDDEVICE(0x0BD6, PvcHwConfig, "Intel(R) Data Center GPU Max 1550")
NAMEDDEVICE(0x0BD7, PvcHwConfig, "Intel(R) Data Center GPU Max 1350")
NAMEDDEVICE(0x0BD8, PvcHwConfig, "Intel(R) Data Center GPU Max 1350")
NAMEDDEVICE(0x0BD9, PvcHwConfig, "Intel(R) Data Center GPU Max 1100")
NAMEDDEVICE(0x0BDA, PvcHwConfig, "Intel(R) Data Center GPU Max 1100")
NAMEDDEVICE(0x0BDB, PvcHwConfig, "Intel(R) Data Center GPU Max 1100")
NAMEDDEVICE(0x0B69, PvcHwConfig, "Intel(R) Data Center GPU Max 1450")
NAMEDDEVICE(0x0B6E, PvcHwConfig, "Intel(R) Data Center GPU Max 1100C")
NAMEDDEVICE(0x0BD4, PvcHwConfig, "Intel(R) Data Center GPU Max 1550VG")
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
NAMEDDEVICE(0x5690, Dg2HwConfig, "Intel(R) Arc(TM) A770M Graphics")
NAMEDDEVICE(0x5691, Dg2HwConfig, "Intel(R) Arc(TM) A730M Graphics")
NAMEDDEVICE(0x5692, Dg2HwConfig, "Intel(R) Arc(TM) A550M Graphics")
NAMEDDEVICE(0x5693, Dg2HwConfig, "Intel(R) Arc(TM) A370M Graphics")
NAMEDDEVICE(0x5694, Dg2HwConfig, "Intel(R) Arc(TM) A350M Graphics")
DEVICE(0x5695, Dg2HwConfig)
NAMEDDEVICE(0x5696, Dg2HwConfig, "Intel(R) Arc(TM) A570M Graphics")
NAMEDDEVICE(0x5697, Dg2HwConfig, "Intel(R) Arc(TM) A530M Graphics")
DEVICE(0x56A3, Dg2HwConfig)
DEVICE(0x56A4, Dg2HwConfig)
NAMEDDEVICE(0x56B0, Dg2HwConfig, "Intel(R) Arc(TM) Pro A30M Graphics")
NAMEDDEVICE(0x56B1, Dg2HwConfig, "Intel(R) Arc(TM) Pro A40/A50 Graphics")
NAMEDDEVICE(0x56B2, Dg2HwConfig, "Intel(R) Arc(TM) Pro A60M Graphics")
NAMEDDEVICE(0x56B3, Dg2HwConfig, "Intel(R) Arc(TM) Pro A60 Graphics")
NAMEDDEVICE(0x56BA, Dg2HwConfig, "Intel(R) Arc(TM) A380E Graphics")
NAMEDDEVICE(0x56BB, Dg2HwConfig, "Intel(R) Arc(TM) A310E Graphics")
NAMEDDEVICE(0x56BC, Dg2HwConfig, "Intel(R) Arc(TM) A370E Graphics")
NAMEDDEVICE(0x56BD, Dg2HwConfig, "Intel(R) Arc(TM) A350E Graphics")
NAMEDDEVICE(0x56BE, Dg2HwConfig, "Intel(R) Arc(TM) A750E Graphics")
NAMEDDEVICE(0x56BF, Dg2HwConfig, "Intel(R) Arc(TM) A580E Graphics")
NAMEDDEVICE(0x56A0, Dg2HwConfig, "Intel(R) Arc(TM) A770 Graphics")
NAMEDDEVICE(0x56A1, Dg2HwConfig, "Intel(R) Arc(TM) A750 Graphics")
NAMEDDEVICE(0x56A2, Dg2HwConfig, "Intel(R) Arc(TM) A580 Graphics")
NAMEDDEVICE(0x56A5, Dg2HwConfig, "Intel(R) Arc(TM) A380 Graphics")
NAMEDDEVICE(0x56A6, Dg2HwConfig, "Intel(R) Arc(TM) A310 LP Graphics")
NAMEDDEVICE(0x56AF, Dg2HwConfig, "Intel(R) Arc(TM) A760A Graphics")
NAMEDDEVICE(0x56C0, Dg2HwConfig, "Intel(R) Data Center GPU Flex 170")
NAMEDDEVICE(0x56C1, Dg2HwConfig, "Intel(R) Data Center GPU Flex 140")
NAMEDDEVICE(0x56C2, Dg2HwConfig, "Intel(R) Data Center GPU Flex 170V")
#endif
#ifdef SUPPORT_MTL
NAMEDDEVICE(0x7D40, MtlHwConfig, "Intel(R) Graphics")
NAMEDDEVICE(0x7D55, MtlHwConfig, "Intel(R) Arc(TM) Graphics")
NAMEDDEVICE(0x7DD5, MtlHwConfig, "Intel(R) Graphics")
NAMEDDEVICE(0x7D45, MtlHwConfig, "Intel(R) Graphics")
#endif
#ifdef SUPPORT_ARL
NAMEDDEVICE(0x7D67, ArlHwConfig, "Intel(R) Graphics")
NAMEDDEVICE(0x7D51, ArlHwConfig, "Intel(R) Arc(TM) Graphics")
NAMEDDEVICE(0x7DD1, ArlHwConfig, "Intel(R) Graphics")
NAMEDDEVICE(0x7D41, ArlHwConfig, "Intel(R) Graphics")
#endif
#endif

#ifdef SUPPORT_GEN12LP

#ifdef SUPPORT_TGLLP
NAMEDDEVICE(0x9A49, TgllpHw1x6x16, "Intel(R) Iris(R) Xe Graphics")
NAMEDDEVICE(0x9A40, TgllpHw1x6x16, "Intel(R) Iris(R) Xe Graphics")
DEVICE(0x9A59, TgllpHw1x6x16)
NAMEDDEVICE(0x9A60, TgllpHw1x2x16, "Intel(R) UHD Graphics")
NAMEDDEVICE(0x9A68, TgllpHw1x2x16, "Intel(R) UHD Graphics")
NAMEDDEVICE(0x9A70, TgllpHw1x2x16, "Intel(R) UHD Graphics")
NAMEDDEVICE(0x9A78, TgllpHw1x2x16, "Intel(R) UHD Graphics")
#endif

#ifdef SUPPORT_DG1
NAMEDDEVICE(0x4905, Dg1HwConfig, "Intel(R) Iris(R) Xe MAX Graphics")
DEVICE(0x4906, Dg1HwConfig)
NAMEDDEVICE(0x4907, Dg1HwConfig, "Intel(R) Server GPU SG-18M")
NAMEDDEVICE(0x4908, Dg1HwConfig, "Intel(R) Iris(R) Xe Graphics")
NAMEDDEVICE(0x4909, Dg1HwConfig, "Intel(R) Iris(R) Xe MAX 100 Graphics")
#endif

#ifdef SUPPORT_RKL
DEVICE(0x4C80, RklHwConfig)
NAMEDDEVICE(0x4C8A, RklHwConfig, "Intel(R) UHD Graphics 750")
NAMEDDEVICE(0x4C8B, RklHwConfig, "Intel(R) UHD Graphics 730")
DEVICE(0x4C8C, RklHwConfig)
NAMEDDEVICE(0x4C90, RklHwConfig, "Intel(R) UHD Graphics P750")
DEVICE(0x4C9A, RklHwConfig)
#endif

#ifdef SUPPORT_ADLS
NAMEDDEVICE(0x4680, AdlsHwConfig, "Intel(R) UHD Graphics 770")
NAMEDDEVICE(0x4682, AdlsHwConfig, "Intel(R) UHD Graphics 730")
NAMEDDEVICE(0x4688, AdlsHwConfig, "Intel(R) UHD Graphics")
NAMEDDEVICE(0x468A, AdlsHwConfig, "Intel(R) UHD Graphics")
NAMEDDEVICE(0x468B, AdlsHwConfig, "Intel(R) UHD Graphics")
NAMEDDEVICE(0x4690, AdlsHwConfig, "Intel(R) UHD Graphics 770")
NAMEDDEVICE(0x4692, AdlsHwConfig, "Intel(R) UHD Graphics 730")
NAMEDDEVICE(0x4693, AdlsHwConfig, "Intel(R) UHD Graphics 710")
NAMEDDEVICE(0xA780, AdlsHwConfig, "Intel(R) UHD Graphics 770")
DEVICE(0xA781, AdlsHwConfig)
NAMEDDEVICE(0xA782, AdlsHwConfig, "Intel(R) UHD Graphics 730")
DEVICE(0xA783, AdlsHwConfig)
NAMEDDEVICE(0xA788, AdlsHwConfig, "Intel(R) UHD Graphics")
DEVICE(0xA789, AdlsHwConfig)
NAMEDDEVICE(0xA78A, AdlsHwConfig, "Intel(R) UHD Graphics")
NAMEDDEVICE(0xA78B, AdlsHwConfig, "Intel(R) UHD Graphics")
#endif

#ifdef SUPPORT_ADLN
NAMEDDEVICE(0x46D0, AdlnHwConfig, "Intel(R) UHD Graphics")
NAMEDDEVICE(0x46D1, AdlnHwConfig, "Intel(R) UHD Graphics")
NAMEDDEVICE(0x46D2, AdlnHwConfig, "Intel(R) UHD Graphics")
NAMEDDEVICE(0x46D3, AdlnHwConfig, "Intel(R) Graphics")
NAMEDDEVICE(0x46D4, AdlnHwConfig, "Intel(R) Graphics")
#endif

#ifdef SUPPORT_ADLP
DEVICE(0x46A0, AdlpHwConfig)
DEVICE(0x46B0, AdlpHwConfig)
DEVICE(0x46A1, AdlpHwConfig)
NAMEDDEVICE(0x46A3, AdlpHwConfig, "Intel(R) UHD Graphics")
NAMEDDEVICE(0x46A6, AdlpHwConfig, "Intel(R) Iris(R) Xe Graphics")
NAMEDDEVICE(0x46A8, AdlpHwConfig, "Intel(R) Iris(R) Xe Graphics")
NAMEDDEVICE(0x46AA, AdlpHwConfig, "Intel(R) Iris(R) Xe Graphics")
NAMEDDEVICE(0x462A, AdlpHwConfig, "Intel(R) UHD Graphics")
NAMEDDEVICE(0x4626, AdlpHwConfig, "Intel(R) UHD Graphics")
NAMEDDEVICE(0x4628, AdlpHwConfig, "Intel(R) UHD Graphics")
DEVICE(0x46B1, AdlpHwConfig)
NAMEDDEVICE(0x46B3, AdlpHwConfig, "Intel(R) UHD Graphics")
DEVICE(0x46C0, AdlpHwConfig)
DEVICE(0x46C1, AdlpHwConfig)
NAMEDDEVICE(0x46C3, AdlpHwConfig, "Intel(R) UHD Graphics")
// RPL-P
NAMEDDEVICE(0xA7A0, AdlpHwConfig, "Intel(R) Iris(R) Xe Graphics")
NAMEDDEVICE(0xA720, AdlpHwConfig, "Intel(R) UHD Graphics")
NAMEDDEVICE(0xA7A8, AdlpHwConfig, "Intel(R) UHD Graphics")
NAMEDDEVICE(0xA7A1, AdlpHwConfig, "Intel(R) Iris(R) Xe Graphics")
NAMEDDEVICE(0xA721, AdlpHwConfig, "Intel(R) UHD Graphics")
NAMEDDEVICE(0xA7A9, AdlpHwConfig, "Intel(R) UHD Graphics")
NAMEDDEVICE(0xA7AA, AdlpHwConfig, "Intel(R) Graphics")
NAMEDDEVICE(0xA7AB, AdlpHwConfig, "Intel(R) Graphics")
NAMEDDEVICE(0xA7AC, AdlpHwConfig, "Intel(R) Graphics")
NAMEDDEVICE(0xA7AD, AdlpHwConfig, "Intel(R) Graphics")
#endif
#endif
