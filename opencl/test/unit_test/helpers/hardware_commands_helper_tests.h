/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/fixtures/built_in_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include <memory>

using namespace NEO;

struct HardwareCommandsTest : ClDeviceFixture,
                              ContextFixture,
                              BuiltInFixture,
                              ::testing::Test {

    using BuiltInFixture::setUp;
    using ContextFixture::setUp;

    void SetUp() override;
    void TearDown() override;

    void addSpaceForSingleKernelArg();

    size_t sizeRequiredCS;
    size_t sizeRequiredISH;

    std::unique_ptr<MockKernelWithInternals> mockKernelWithInternal;
    Kernel::SimpleKernelArgInfo kernelArgInfo = {};
    std::vector<Kernel::SimpleKernelArgInfo> kernelArguments;

    template <typename GfxFamily>
    size_t pushBindingTableAndSurfaceStates(IndirectHeap &dstHeap, const Kernel &srcKernel) {
        return EncodeSurfaceState<GfxFamily>::pushBindingTableAndSurfaceStates(dstHeap, srcKernel.getKernelInfo().kernelDescriptor.payloadMappings.bindingTable.numEntries,
                                                                               srcKernel.getSurfaceStateHeap(), srcKernel.getSurfaceStateHeapSize(),
                                                                               srcKernel.getNumberOfBindingTableStates(), srcKernel.getBindingTableOffset());
    }
};
