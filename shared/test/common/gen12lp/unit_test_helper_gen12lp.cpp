/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/gen12lp/hw_info.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/unit_test_helper.inl"
#include "shared/test/common/helpers/unit_test_helper_bdw_and_later.inl"
#include "shared/test/common/libult/gen12lp/special_ult_helper_gen12lp.h"

namespace NEO {

using Family = Gen12LpFamily;

template <>
bool UnitTestHelper<Family>::isL3ConfigProgrammable() {
    return false;
};

template <>
bool UnitTestHelper<Family>::isPageTableManagerSupported(const HardwareInfo &hwInfo) {
    return hwInfo.capabilityTable.ftrRenderCompressedBuffers || hwInfo.capabilityTable.ftrRenderCompressedImages;
}

template <>
bool UnitTestHelper<Family>::isPipeControlWArequired(const HardwareInfo &hwInfo) {
    return SpecialUltHelperGen12lp::isPipeControlWArequired(hwInfo.platform.eProductFamily);
}

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
    return (1u << 7) | (1u << 4);
}

template <>
bool UnitTestHelper<Family>::getDisableFusionStateFromFrontEndCommand(const typename Family::VFE_STATE_TYPE &feCmd) {
    return feCmd.getDisableSlice0Subslice2();
}

template <>
bool UnitTestHelper<Family>::getSystolicFlagValueFromPipelineSelectCommand(const typename Family::PIPELINE_SELECT &pipelineSelectCmd) {
    return pipelineSelectCmd.getSpecialModeEnable();
}

template struct UnitTestHelper<Family>;
} // namespace NEO
