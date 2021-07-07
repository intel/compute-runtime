/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hp_core/hw_info.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/unit_test_helper.inl"
using Family = NEO::XeHpFamily;
#include "shared/test/common/helpers/unit_test_helper_xehp_plus.inl"

namespace NEO {
template <>
const AuxTranslationMode UnitTestHelper<Family>::requiredAuxTranslationMode = AuxTranslationMode::Blit;

template <>
const bool UnitTestHelper<Family>::additionalMiFlushDwRequired = true;

template struct UnitTestHelper<Family>;
} // namespace NEO
