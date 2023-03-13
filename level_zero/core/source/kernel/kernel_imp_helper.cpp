/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/kernel/kernel_imp.h"

namespace L0 {

KernelExt *KernelImp::getExtension(uint32_t extensionType) { return nullptr; }

void KernelImp::getExtendedKernelProperties(ze_base_desc_t *pExtendedProperties) {}

} // namespace L0
