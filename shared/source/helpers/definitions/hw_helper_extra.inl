/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"

namespace NEO {

template <typename GfxFamily>
std::string HwHelperHw<GfxFamily>::getExtraExtensions(const HardwareInfo &hwInfo) const {
    return "";
}

} // namespace NEO
