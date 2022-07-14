/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

// clang-format off
#ifdef SUPPORT_XE_HP_CORE
#ifdef SUPPORT_XE_HP_SDV
DEVICE(0x0201, XehpSdvHwConfig)
DEVICE(0x0202, XehpSdvHwConfig)
DEVICE(0x0203, XehpSdvHwConfig)
DEVICE(0x0204, XehpSdvHwConfig)
DEVICE(0x0205, XehpSdvHwConfig)
DEVICE(0x0206, XehpSdvHwConfig)
DEVICE(0x0207, XehpSdvHwConfig)
DEVICE(0x0208, XehpSdvHwConfig)
DEVICE(0x0209, XehpSdvHwConfig)
DEVICE(0x020A, XehpSdvHwConfig)
DEVICE(0x020B, XehpSdvHwConfig)
DEVICE(0x020C, XehpSdvHwConfig)
DEVICE(0x020D, XehpSdvHwConfig)
DEVICE(0x020E, XehpSdvHwConfig)
DEVICE(0x020F, XehpSdvHwConfig)
DEVICE(0x0210, XehpSdvHwConfig)
#endif
#endif

#ifdef SUPPORT_GEN12LP

#ifdef SUPPORT_TGLLP
DEVICE( 0xFF20, TgllpHw1x6x16 )
NAMEDDEVICE( 0x9A49, TgllpHw1x6x16, "Intel(R) Iris(R) Xe Graphics" )
NAMEDDEVICE( 0x9A40, TgllpHw1x6x16, "Intel(R) Iris(R) Xe Graphics" )
DEVICE( 0x9A59, TgllpHw1x6x16 )
NAMEDDEVICE( 0x9A60, TgllpHw1x2x16, "Intel(R) UHD Graphics" )
NAMEDDEVICE( 0x9A68, TgllpHw1x2x16, "Intel(R) UHD Graphics" )
NAMEDDEVICE( 0x9A70, TgllpHw1x2x16, "Intel(R) UHD Graphics" )
NAMEDDEVICE( 0x9A78, TgllpHw1x2x16, "Intel(R) UHD Graphics" )
#endif

#ifdef SUPPORT_DG1
NAMEDDEVICE( 0x4905, Dg1HwConfig, "Intel(R) Iris(R) Xe MAX Graphics" )
DEVICE( 0x4906, Dg1HwConfig )
NAMEDDEVICE( 0x4907, Dg1HwConfig, "Intel(R) Server GPU SG-18M" )
NAMEDDEVICE( 0x4908, Dg1HwConfig, "Intel(R) Iris(R) Xe Graphics" )
#endif

#ifdef SUPPORT_RKL
DEVICE( 0x4C80, RklHwConfig )
NAMEDDEVICE( 0x4C8A, RklHwConfig, "Intel(R) UHD Graphics 750" )
NAMEDDEVICE( 0x4C8B, RklHwConfig, "Intel(R) UHD Graphics 730" )
DEVICE( 0x4C8C, RklHwConfig )
NAMEDDEVICE( 0x4C90, RklHwConfig, "Intel(R) UHD Graphics P750" )
DEVICE( 0x4C9A, RklHwConfig )
#endif

#ifdef SUPPORT_ADLS
NAMEDDEVICE( 0x4680, AdlsHwConfig, "Intel(R) UHD Graphics 770" )
NAMEDDEVICE( 0x4682, AdlsHwConfig, "Intel(R) UHD Graphics 730" )
DEVICE( 0x4688, AdlsHwConfig )
DEVICE( 0x468A, AdlsHwConfig )
NAMEDDEVICE( 0x4690, AdlsHwConfig, "Intel(R) UHD Graphics 770" )
NAMEDDEVICE( 0x4692, AdlsHwConfig, "Intel(R) UHD Graphics 730" )
NAMEDDEVICE( 0x4693, AdlsHwConfig, "Intel(R) UHD Graphics 710" )
DEVICE( 0xA780, AdlsHwConfig )
DEVICE( 0xA781, AdlsHwConfig )
DEVICE( 0xA782, AdlsHwConfig )
DEVICE( 0xA783, AdlsHwConfig )
DEVICE( 0xA788, AdlsHwConfig )
DEVICE( 0xA789, AdlsHwConfig )
#endif

#ifdef SUPPORT_ADLN
DEVICE(0x46D0, AdlnHwConfig)
DEVICE(0x46D1, AdlnHwConfig)
DEVICE(0x46D2, AdlnHwConfig)
#endif

#endif

#ifdef SUPPORT_GEN11

#ifdef SUPPORT_ICLLP
DEVICE( 0xFF05, IcllpHw1x4x8 )
NAMEDDEVICE( 0x8A56, IcllpHw1x4x8, "Intel(R) UHD Graphics" )
NAMEDDEVICE( 0x8A58, IcllpHw1x4x8, "Intel(R) UHD Graphics" )
NAMEDDEVICE( 0x8A5C, IcllpHw1x6x8, "Intel(R) Iris(R) Plus Graphics" )
NAMEDDEVICE( 0x8A5A, IcllpHw1x6x8, "Intel(R) Iris(R) Plus Graphics" )
DEVICE( 0x8A50, IcllpHw1x8x8 )
NAMEDDEVICE( 0x8A52, IcllpHw1x8x8, "Intel(R) Iris(R) Plus Graphics" )
NAMEDDEVICE( 0x8A51, IcllpHw1x8x8, "Intel(R) Iris(R) Plus Graphics" )
#endif

#ifdef SUPPORT_LKF
NAMEDDEVICE( 0x9840, LkfHw1x8x8, "Intel(R) UHD Graphics" )
#endif

#ifdef SUPPORT_EHL
DEVICE( 0x4500, EhlHwConfig )
DEVICE( 0x4541, EhlHwConfig )
DEVICE( 0x4551, EhlHwConfig )
NAMEDDEVICE( 0x4571, EhlHwConfig, "Intel(R) UHD Graphics" )
NAMEDDEVICE( 0x4555, EhlHwConfig, "Intel(R) UHD Graphics" )
DEVICE( 0x4E51, EhlHwConfig )
NAMEDDEVICE( 0x4E61, EhlHwConfig, "Intel(R) UHD Graphics" )
NAMEDDEVICE( 0x4E71, EhlHwConfig, "Intel(R) UHD Graphics" )
NAMEDDEVICE( 0x4E55, EhlHwConfig, "Intel(R) UHD Graphics" )
#endif
#endif

#ifdef SUPPORT_GEN9

#ifdef SUPPORT_SKL
NAMEDDEVICE( 0x1902, SklHw1x2x6, "Intel(R) HD Graphics 510" )
NAMEDDEVICE( 0x190B, SklHw1x2x6, "Intel(R) HD Graphics 510" )
DEVICE( 0x190A, SklHw1x2x6 )
NAMEDDEVICE( 0x1906, SklHw1x2x6, "Intel(R) HD Graphics 510" )
DEVICE( 0x190E, SklHw1x2x6 )
DEVICE( 0x1917, SklHw1x3x6 )
DEVICE( 0x1913, SklHw1x3x6 )
DEVICE( 0X1915, SklHw1x3x6 )
NAMEDDEVICE( 0x1912, SklHw1x3x8, "Intel(R) HD Graphics 530" )
NAMEDDEVICE( 0x191B, SklHw1x3x8, "Intel(R) HD Graphics 530" )
DEVICE( 0x191A, SklHw1x3x8 )
NAMEDDEVICE( 0x1916, SklHw1x3x8, "Intel(R) HD Graphics 520" )
NAMEDDEVICE( 0x191E, SklHw1x3x8, "Intel(R) HD Graphics 515" )
NAMEDDEVICE( 0x191D, SklHw1x3x8, "Intel(R) HD Graphics P530" )
NAMEDDEVICE( 0x1921, SklHw1x3x8, "Intel(R) HD Graphics 520" )
DEVICE( 0x9905, SklHw1x3x8 )
NAMEDDEVICE( 0x192B, SklHw2x3x8, "Intel(R) Iris(R) Graphics 555" )
NAMEDDEVICE( 0x192D, SklHw2x3x8, "Intel(R) Iris(R) Graphics P555" )
DEVICE( 0x192A, SklHw2x3x8 )
NAMEDDEVICE( 0x1923, SklHw2x3x8, "Intel(R) HD Graphics 535" )
NAMEDDEVICE( 0x1926, SklHw2x3x8, "Intel(R) Iris(R) Graphics 540" )
NAMEDDEVICE( 0x1927, SklHw2x3x8, "Intel(R) Iris(R) Graphics 550" )
DEVICE( 0x1932, SklHw3x3x8 )
NAMEDDEVICE( 0x193B, SklHw3x3x8, "Intel(R) Iris(R) Pro Graphics 580" )
NAMEDDEVICE( 0x193A, SklHw3x3x8, "Intel(R) Iris(R) Pro Graphics P580" )
NAMEDDEVICE( 0x193D, SklHw3x3x8, "Intel(R) Iris(R) Pro Graphics P580" )
#endif

#ifdef SUPPORT_KBL
NAMEDDEVICE( 0x5902, KblHw1x2x6, "Intel(R) HD Graphics 610" )
NAMEDDEVICE( 0x590B, KblHw1x2x6, "Intel(R) HD Graphics 610" )
DEVICE( 0x590A, KblHw1x2x6 )
NAMEDDEVICE( 0x5906, KblHw1x2x6, "Intel(R) HD Graphics 610" )
DEVICE( 0x590E, KblHw1x3x6 )
DEVICE( 0x5908, KblHw1x2x6 )
DEVICE( 0x5913, KblHw1x3x6 )
DEVICE( 0x5915, KblHw1x2x6 )
NAMEDDEVICE( 0x5912, KblHw1x3x8, "Intel(R) HD Graphics 630" )
NAMEDDEVICE( 0x591B, KblHw1x3x8, "Intel(R) HD Graphics 630" )
NAMEDDEVICE( 0x5917, KblHw1x3x8, "Intel(R) UHD Graphics 620" )
DEVICE( 0x591A, KblHw1x3x8 )
NAMEDDEVICE( 0x5916, KblHw1x3x8, "Intel(R) HD Graphics 620" )
NAMEDDEVICE( 0x591E, KblHw1x3x8, "Intel(R) HD Graphics 615" )
NAMEDDEVICE( 0x591D, KblHw1x3x8, "Intel(R) HD Graphics P630" )
NAMEDDEVICE( 0x591C, KblHw1x3x8, "Intel(R) UHD Graphics 615" )
NAMEDDEVICE( 0x5921, KblHw1x3x8, "Intel(R) HD Graphics 620" )
NAMEDDEVICE( 0x5926, KblHw2x3x8, "Intel(R) Iris(R) Plus Graphics 640" )
NAMEDDEVICE( 0x5927, KblHw2x3x8, "Intel(R) Iris(R) Plus Graphics 650" )
DEVICE( 0x592B, KblHw2x3x8 )
DEVICE( 0x592A, KblHw2x3x8 )
DEVICE( 0x5923, KblHw2x3x8 )
DEVICE( 0x5932, KblHw3x3x8 )
DEVICE( 0x593B, KblHw3x3x8 )
DEVICE( 0x593A, KblHw3x3x8 )
DEVICE( 0x593D, KblHw3x3x8 )
#endif

#ifdef SUPPORT_CFL
NAMEDDEVICE( 0x3E90, CflHw1x2x6, "Intel(R) UHD Graphics 610" )
NAMEDDEVICE( 0x3E93, CflHw1x2x6, "Intel(R) UHD Graphics 610" )
DEVICE( 0x3EA4, CflHw1x2x6 )
DEVICE( 0x3E99, CflHw1x2x6 )
NAMEDDEVICE( 0x3EA1, CflHw1x2x6, "Intel(R) UHD Graphics 610" )
NAMEDDEVICE( 0x3E92, CflHw1x3x8, "Intel(R) UHD Graphics 630" )
NAMEDDEVICE( 0x3E9B, CflHw1x3x8, "Intel(R) UHD Graphics 630" )
NAMEDDEVICE( 0x3E94, CflHw1x3x8, "Intel(R) UHD Graphics P630" )
NAMEDDEVICE( 0x3E91, CflHw1x3x8, "Intel(R) UHD Graphics 630" )
NAMEDDEVICE( 0x3E96, CflHw1x3x8, "Intel(R) UHD Graphics P630" )
NAMEDDEVICE( 0x3E9A, CflHw1x3x8, "Intel(R) UHD Graphics P630" )
DEVICE( 0x3EA3, CflHw1x3x8 )
NAMEDDEVICE( 0x3EA9, CflHw1x3x8, "Intel(R) UHD Graphics 620" )
NAMEDDEVICE( 0x3EA0, CflHw1x3x8, "Intel(R) UHD Graphics 620" )
NAMEDDEVICE( 0x3E98, CflHw1x3x8, "Intel(R) UHD Graphics 630" )
DEVICE( 0x3E95, CflHw2x3x8 )
NAMEDDEVICE( 0x3EA6, CflHw2x3x8, "Intel(R) Iris(R) Plus Graphics 645" )
DEVICE( 0x3EA7, CflHw2x3x8 )
NAMEDDEVICE( 0x3EA8, CflHw2x3x8, "Intel(R) Iris(R) Plus Graphics 655" )
NAMEDDEVICE( 0x3EA5, CflHw2x3x8, "Intel(R) Iris(R) Plus Graphics 655" )
DEVICE( 0x3EA2, CflHw2x3x8 )
NAMEDDEVICE( 0x9B21, CflHw1x2x6, "Intel(R) UHD Graphics" )
NAMEDDEVICE( 0x9BAA, CflHw1x2x6, "Intel(R) UHD Graphics" )
DEVICE( 0x9BAB, CflHw1x2x6 )
NAMEDDEVICE( 0x9BAC, CflHw1x2x6, "Intel(R) UHD Graphics" )
DEVICE( 0x9BA0, CflHw1x2x6 )
DEVICE( 0x9BA5, CflHw1x2x6 )
NAMEDDEVICE( 0x9BA8, CflHw1x2x6, "Intel(R) UHD Graphics 610" )
DEVICE( 0x9BA4, CflHw1x2x6 )
DEVICE( 0x9BA2, CflHw1x2x6 )
NAMEDDEVICE( 0x9B41, CflHw1x3x8, "Intel(R) UHD Graphics" )
NAMEDDEVICE( 0x9BCA, CflHw1x3x8, "Intel(R) UHD Graphics" )
DEVICE( 0x9BCB, CflHw1x3x8 )
NAMEDDEVICE( 0x9BCC, CflHw1x3x8, "Intel(R) UHD Graphics" )
DEVICE( 0x9BC0, CflHw1x3x8 )
NAMEDDEVICE( 0x9BC5, CflHw1x3x8, "Intel(R) UHD Graphics 630" )
NAMEDDEVICE( 0x9BC8, CflHw1x3x8, "Intel(R) UHD Graphics 630" )
NAMEDDEVICE( 0x9BC4, CflHw1x3x8, "Intel(R) UHD Graphics" )
DEVICE( 0x9BC2, CflHw1x3x8 )
NAMEDDEVICE( 0x9BC6, CflHw1x3x8, "Intel(R) UHD Graphics P630" )
NAMEDDEVICE( 0x9BE6, CflHw1x3x8, "Intel(R) UHD Graphics P630" )
NAMEDDEVICE( 0x9BF6, CflHw1x3x8, "Intel(R) UHD Graphics P630" )
#endif

#ifdef SUPPORT_GLK
NAMEDDEVICE( 0x3184, GlkHw1x3x6, "Intel(R) UHD Graphics 605" )
NAMEDDEVICE( 0x3185, GlkHw1x2x6, "Intel(R) UHD Graphics 600" )
#endif

#ifdef SUPPORT_BXT
DEVICE( 0x9906, BxtHw1x3x6)
DEVICE( 0x9907, BxtHw1x3x6)
DEVICE( 0x0A84, BxtHw1x3x6)
NAMEDDEVICE( 0x5A84, BxtHw1x3x6, "Intel(R) HD Graphics 505" )
NAMEDDEVICE( 0x5A85, BxtHw1x2x6, "Intel(R) HD Graphics 500" )
DEVICE( 0x1A85, BxtHw1x2x6)
DEVICE( 0x1A84, BxtHw1x3x6)
DEVICE( 0x9908, BxtHw1x3x6)
#endif
#endif

#ifdef SUPPORT_GEN8

DEVICE( 0x1602, BdwHw1x2x6 )
DEVICE( 0x160A, BdwHw1x2x6 )
DEVICE( 0x1606, BdwHw1x2x6 )
NAMEDDEVICE( 0x160E, BdwHw1x2x6, "Intel(R) HD Graphics" )
DEVICE( 0x160D, BdwHw1x2x6 )
NAMEDDEVICE( 0x1612, BdwHw1x3x8, "Intel(R) HD Graphics 5600" )
NAMEDDEVICE( 0x161A, BdwHw1x3x8, "Intel(R) HD Graphics P5700" )
NAMEDDEVICE( 0x1616, BdwHw1x3x8, "Intel(R) HD Graphics 5500" )
NAMEDDEVICE( 0x161E, BdwHw1x3x8, "Intel(R) HD Graphics 5300" )
DEVICE( 0x161D, BdwHw1x3x8 )
DEVICE( 0x1622, BdwHw2x3x8 )
NAMEDDEVICE( 0x162A, BdwHw2x3x8, "Intel(R) Iris(TM) Pro Graphics P6300" )
NAMEDDEVICE( 0x1626, BdwHw2x3x8, "Intel(R) HD Graphics 6000" )
DEVICE( 0x162B, BdwHw2x3x8 )
DEVICE( 0x162E, BdwHw2x3x8 )
DEVICE( 0x162D, BdwHw2x3x8 )
#endif
// clang-format on
