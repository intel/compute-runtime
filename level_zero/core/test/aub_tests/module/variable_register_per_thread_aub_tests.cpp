/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/aub_tests/module/variable_register_per_thread_aub_fixture.h"

namespace L0::ult {

HWTEST2_F(AUBVariableRegisterPerThreadL0, givenZeOptRegisterFileSizeOptionWhenExecutingKernelThenCorrectValuesAreReturned, IsAtLeastXe3Core) {
    const std::vector<uint32_t> grfSizesWithTestKernels = {32u, 64u, 96u, 128u, 160u, 192u, 256u, 512u};
    executeKernelsWithGrfSizes<FamilyType>(selectGrfSizes(getVerifiedGrfSizes(), grfSizesWithTestKernels));
}

} // namespace L0::ult
