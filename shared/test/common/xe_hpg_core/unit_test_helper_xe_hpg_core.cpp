/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"
#include "shared/source/xe_hpg_core/hw_info.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/unit_test_helper.inl"
#include "shared/test/common/helpers/unit_test_helper_xehp_and_later.inl"
using Family = NEO::XeHpgCoreFamily;

namespace NEO {
template <>
const AuxTranslationMode UnitTestHelper<Family>::requiredAuxTranslationMode = AuxTranslationMode::Blit;

template <>
const bool UnitTestHelper<Family>::additionalMiFlushDwRequired = true;

template <>
uint32_t UnitTestHelper<Family>::getDebugModeRegisterOffset() {
    return 0x20d8;
}

template <>
uint32_t UnitTestHelper<Family>::getDebugModeRegisterValue() {
    return (1u << 5) | (1u << 21);
}

template <>
uint32_t UnitTestHelper<Family>::getTdCtlRegisterOffset() {
    return 0xe400;
}

template <>
uint32_t UnitTestHelper<Family>::getTdCtlRegisterValue() {
    return (1u << 7) | (1u << 4) | (1u << 2) | (1u << 0);
}

template <>
bool UnitTestHelper<Family>::getDisableFusionStateFromFrontEndCommand(const typename Family::VFE_STATE_TYPE &feCmd) {
    return feCmd.getFusedEuDispatch();
}

template struct UnitTestHelper<Family>;
} // namespace NEO
