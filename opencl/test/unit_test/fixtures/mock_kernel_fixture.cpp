/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/mock_kernel_fixture.h"

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/test/common/mocks/mock_zebin_wrapper.h"

#include "opencl/source/kernel/multi_device_kernel.h"
#include "opencl/source/program/program.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "gtest/gtest.h"

namespace NEO {

std::unique_ptr<MockKernelWithInternals> createBufferArgsKernel(Context &context) {
    auto kernelWithInternals = std::make_unique<MockKernelWithInternals>(context, MockKernelWithInternalsConfig{.addDefaultArgs = true});
    kernelWithInternals->kernelInfo.kernelDescriptor.kernelAttributes.numArgsToPatch = 2;
    return kernelWithInternals;
}

std::unique_ptr<MockKernelWithInternals> createSimpleArgKernel(Context &context) {
    auto kernelWithInternals = std::make_unique<MockKernelWithInternals>(context);
    auto &kernelInfo = kernelWithInternals->kernelInfo;
    kernelInfo.addArgImmediate(0, sizeof(int), 0);
    kernelInfo.addArgBuffer(1, 8, sizeof(uintptr_t), 0);
    kernelInfo.setAddressQualifier(1, KernelArgMetadata::AddrGlobal);
    kernelInfo.setAccessQualifier(1, KernelArgMetadata::AccessReadWrite);
    kernelWithInternals->mockKernel->initialize();
    return kernelWithInternals;
}

std::unique_ptr<MockKernelWithInternals> createBufferArgsKernelWithRequiredWorkGroupSize(Context &context, std::array<uint16_t, 3> requiredWorkGroupSize) {
    auto kernelWithInternals = createBufferArgsKernel(context);
    auto &kernelAttributes = kernelWithInternals->kernelInfo.kernelDescriptor.kernelAttributes;
    kernelAttributes.requiredWorkgroupSize[0] = requiredWorkGroupSize[0];
    kernelAttributes.requiredWorkgroupSize[1] = requiredWorkGroupSize[1];
    kernelAttributes.requiredWorkgroupSize[2] = requiredWorkGroupSize[2];
    return kernelWithInternals;
}

void MockKernelFixture::setUp(ClDevice *pDevice) {
    auto deviceVector = toClDeviceVector(*pDevice);
    pContext.reset(Context::create<MockContext>(nullptr, deviceVector, nullptr, nullptr, retVal));
    ASSERT_NE(nullptr, pContext);

    auto productHelper = NEO::CompilerProductHelper::create(pDevice->getHardwareInfo().platform.eProductFamily);
    MockZebinWrapper<>::Descriptor desc{};
    desc.isStateless = productHelper->isForceToStatelessRequired();

    constexpr auto numBits = is32bit ? Elf::EI_CLASS_32 : Elf::EI_CLASS_64;
    MockZebinWrapper<1u, numBits> zebin{pDevice->getHardwareInfo(), desc};

    pProgram.reset(Program::create<MockProgram>(pContext.get(), deviceVector, zebin.binarySizes.data(), zebin.binaries.data(), nullptr, retVal));
    ASSERT_NE(nullptr, pProgram);
    pProgram->build(pProgram->getDevices(), nullptr);

    pMultiDeviceKernelOwner.reset(MultiDeviceKernel::create<MockKernel, Program, MockMultiDeviceKernel>(pProgram.get(), pProgram->getKernelInfosForKernel("CopyBuffer"), retVal));
    ASSERT_NE(nullptr, pMultiDeviceKernelOwner);
    pMultiDeviceKernel = pMultiDeviceKernelOwner.get();
    pKernel = static_cast<MockKernel *>(pMultiDeviceKernel->getKernel(pDevice->getRootDeviceIndex()));
    ASSERT_NE(nullptr, pKernel);
}

void MockKernelFixture::tearDown() {
    pMultiDeviceKernelOwner.reset();
    pProgram.reset();
    pContext.reset();
}
} // namespace NEO
