/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"
#include "shared/source/release_helper/release_helper_base.inl"

#include "release_definitions.h"

namespace NEO {
constexpr auto release = ReleaseType::release1261;

template <>
bool ReleaseHelperHw<release>::isRcsExposureDisabled() const {
    return true;
}
} // namespace NEO

template class NEO::ReleaseHelperHw<NEO::release>;
