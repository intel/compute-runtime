/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/built_ins/built_ins.h"
#include "runtime/kernel/kernel.h"
#include "test.h"
#include "unit_tests/fixtures/built_in_fixture.h"
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"

#include <memory>

using namespace NEO;

struct KernelCommandsTest : DeviceFixture,
                            ContextFixture,
                            BuiltInFixture,
                            ::testing::Test {

    using BuiltInFixture::SetUp;
    using ContextFixture::SetUp;

    void SetUp() override;
    void TearDown() override;

    void addSpaceForSingleKernelArg();

    size_t sizeRequiredCS;
    size_t sizeRequiredISH;

    std::unique_ptr<MockKernelWithInternals> mockKernelWithInternal;
    Kernel::SimpleKernelArgInfo kernelArgInfo = {};
    std::vector<Kernel::SimpleKernelArgInfo> kernelArguments;
};
