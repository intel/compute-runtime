/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/kernel_data_fixture.h"

TEST_F(KernelDataTest, givenDataParameterPreferredWorkgroupMultipleTokenWhenBinaryIsdecodedThenCorrectOffsetIsAssigned) {
    const uint32_t offset = 0x100;

    SPatchDataParameterBuffer dataParameterToken;
    dataParameterToken.Token = PATCH_TOKEN_DATA_PARAMETER_BUFFER;
    dataParameterToken.Size = sizeof(SPatchDataParameterBuffer);
    dataParameterToken.Type = DATA_PARAMETER_PREFERRED_WORKGROUP_MULTIPLE;
    dataParameterToken.ArgumentNumber = 0;
    dataParameterToken.Offset = offset;
    dataParameterToken.DataSize = sizeof(uint32_t);
    dataParameterToken.SourceOffset = 0;

    pPatchList = &dataParameterToken;
    patchListSize = dataParameterToken.Size;

    buildAndDecode();

    EXPECT_EQ(pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.preferredWkgMultiple, offset);
}
