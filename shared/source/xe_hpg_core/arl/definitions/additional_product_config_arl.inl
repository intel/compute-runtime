/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {

uint32_t getProductConfigFromHwInfoAdditionalArl(const CompilerProductHelper &compilerProductHelper, const HardwareInfo &hwInfo) {
    return compilerProductHelper.getDefaultHwIpVersion();
}

} // namespace NEO
