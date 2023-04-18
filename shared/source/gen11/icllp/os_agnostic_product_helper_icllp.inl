/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "aubstream/product_family.h"

namespace NEO {
template <>
bool ProductHelperHw<gfxProduct>::isAdditionalMediaSamplerProgrammingRequired() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isInitialFlagsProgrammingRequired() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isReturnedCmdSizeForMediaSamplerAdjustmentRequired() const {
    return true;
}

template <>
std::optional<aub_stream::ProductFamily> ProductHelperHw<gfxProduct>::getAubStreamProductFamily() const {
    return aub_stream::ProductFamily::Icllp;
};

} // namespace NEO
