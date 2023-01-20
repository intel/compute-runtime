/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/numeric.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/sampler/sampler_hw.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_sampler.h"

namespace L0 {
namespace ult {

using ContextCreateSamplerTestPVC = Test<DeviceFixture>;

PVCTEST_F(ContextCreateSamplerTestPVC, givenValidHandleReturnUnitialized) {
    ze_sampler_address_mode_t addressMode = ZE_SAMPLER_ADDRESS_MODE_NONE;
    ze_sampler_filter_mode_t filterMode = ZE_SAMPLER_FILTER_MODE_LINEAR;
    ze_bool_t isNormalized = false;

    ze_sampler_desc_t desc = {};
    desc.addressMode = addressMode;
    desc.filterMode = filterMode;
    desc.isNormalized = isNormalized;

    ze_sampler_handle_t hSampler;
    ze_result_t res = context->createSampler(device, &desc, &hSampler);

    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, res);
}

} // namespace ult
} // namespace L0