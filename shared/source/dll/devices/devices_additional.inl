/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

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
#endif
