/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_in_ops_base.h"
#include "shared/source/built_ins/built_ins.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/kernel/kernel_arg_descriptor.h"
#include "shared/source/program/kernel_info.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/cl_device/cl_device_vector.h"
#include "opencl/source/program/program.h"
#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

struct KernelAddressingTest : public ClDeviceFixture, public ::testing::Test, public ::testing::WithParamInterface<bool> {
    void SetUp() override {
        ClDeviceFixture::setUp();

        auto &compilerProductHelper = pDevice->getCompilerProductHelper();
        isHeapless = compilerProductHelper.isHeaplessModeEnabled(pDevice->getHardwareInfo());
        isStateless = compilerProductHelper.isForceToStatelessRequired();
        isWideness = GetParam();
    }

    void TearDown() override {
        prog.reset();
        ClDeviceFixture::tearDown();
    }

    bool isHeapless = false;
    bool isStateless = false;
    bool isWideness = false;
    std::unique_ptr<Program> prog;
};

TEST_P(KernelAddressingTest, givenBuiltinCopyBufferToBufferKernelsWhenWidenessIsEnabledThenCorrectArgumentSizesAreUsed) {
    const auto builtinType = EBuiltInOps::adjustBuiltinType<EBuiltInOps::copyBufferToBuffer>(isStateless, isHeapless, isWideness);

    auto src = pDevice->getBuiltIns()->getBuiltinsLib().getBuiltinCode(builtinType, BuiltinCode::ECodeType::any, *pDevice);
    ClDeviceVector deviceVector;
    deviceVector.push_back(pClDevice);
    prog.reset(BuiltinDispatchInfoBuilder::createProgramFromCode(src, deviceVector).release());
    prog->build(deviceVector, "");

    const std::vector<const char *> bufferToBufferKernelNames = {
        "CopyBufferToBufferBytes",
        "CopyBufferToBufferMiddle",
        "CopyBufferToBufferMiddleMisaligned",
        "CopyBufferToBufferRightLeftover",
        "CopyBufferToBufferSideRegion",
        "CopyBufferToBufferMiddleRegion"};

    for (const auto &kernelName : bufferToBufferKernelNames) {
        auto pKernelInfo = prog->getKernelInfo(kernelName, 0);
        ASSERT_NE(pKernelInfo, nullptr);
        EXPECT_EQ(pKernelInfo->getArgDescriptorAt(2).as<ArgDescValue>().elements[0].size,
                  isStateless && isStateless && isWideness ? sizeof(uint64_t) : sizeof(uint32_t));
        EXPECT_EQ(pKernelInfo->getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size,
                  isStateless && isStateless && isWideness ? sizeof(uint64_t) : sizeof(uint32_t));
    }
}

TEST_P(KernelAddressingTest, givenBuiltinCopyBufferRectKernelsWhenFetchedFromProgramThenCorrectArgumentSizesAreUsed) {
    const auto builtinType = EBuiltInOps::adjustBuiltinType<EBuiltInOps::copyBufferRect>(isStateless, isHeapless, isWideness);

    auto src = pDevice->getBuiltIns()->getBuiltinsLib().getBuiltinCode(builtinType, BuiltinCode::ECodeType::any, *pDevice);
    ClDeviceVector deviceVector;
    deviceVector.push_back(pClDevice);
    prog.reset(BuiltinDispatchInfoBuilder::createProgramFromCode(src, deviceVector).release());
    prog->build(deviceVector, "");
    auto pKernelInfo = prog->getKernelInfo("CopyBufferRectBytes2d", 0);
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(2).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? 2 * sizeof(uint64_t) : 2 * sizeof(uint32_t));
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? 2 * sizeof(uint64_t) : 2 * sizeof(uint32_t));
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(4).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? sizeof(uint64_t) : sizeof(uint32_t));
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(5).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? sizeof(uint64_t) : sizeof(uint32_t));
    pKernelInfo = prog->getKernelInfo("CopyBufferRectBytesMiddle2d", 0);
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(2).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? 2 * sizeof(uint64_t) : 2 * sizeof(uint32_t));
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? 2 * sizeof(uint64_t) : 2 * sizeof(uint32_t));
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(4).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? sizeof(uint64_t) : sizeof(uint32_t));
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(5).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? sizeof(uint64_t) : sizeof(uint32_t));
    pKernelInfo = prog->getKernelInfo("CopyBufferRectBytes3d", 0);
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(2).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? 4 * sizeof(uint64_t) : 4 * sizeof(uint32_t));
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? 4 * sizeof(uint64_t) : 4 * sizeof(uint32_t));
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(4).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? 2 * sizeof(uint64_t) : 2 * sizeof(uint32_t));
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(5).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? 2 * sizeof(uint64_t) : 2 * sizeof(uint32_t));
    pKernelInfo = prog->getKernelInfo("CopyBufferRectBytesMiddle3d", 0);
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(2).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? 4 * sizeof(uint64_t) : 4 * sizeof(uint32_t));
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? 4 * sizeof(uint64_t) : 4 * sizeof(uint32_t));
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(4).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? 2 * sizeof(uint64_t) : 2 * sizeof(uint32_t));
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(5).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? 2 * sizeof(uint64_t) : 2 * sizeof(uint32_t));
}

TEST_P(KernelAddressingTest, givenBuiltinFillBufferKernelsWhenFetchedFromProgramThenCorrectArgumentSizesAreUsed) {
    const auto builtinType = EBuiltInOps::adjustBuiltinType<EBuiltInOps::fillBuffer>(isStateless, isHeapless, isWideness);

    auto src = pDevice->getBuiltIns()->getBuiltinsLib().getBuiltinCode(builtinType, BuiltinCode::ECodeType::any, *pDevice);
    ClDeviceVector deviceVector;
    deviceVector.push_back(pClDevice);
    prog.reset(BuiltinDispatchInfoBuilder::createProgramFromCode(src, deviceVector).release());
    prog->build(deviceVector, "");
    auto pKernelInfo = prog->getKernelInfo("FillBufferBytes", 0);
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(1).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? sizeof(uint64_t) : sizeof(uint32_t));
    pKernelInfo = prog->getKernelInfo("FillBufferLeftLeftover", 0);
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(1).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? sizeof(uint64_t) : sizeof(uint32_t));
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? sizeof(uint64_t) : sizeof(uint32_t));
    pKernelInfo = prog->getKernelInfo("FillBufferMiddle", 0);
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(1).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? sizeof(uint64_t) : sizeof(uint32_t));
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? sizeof(uint64_t) : sizeof(uint32_t));
    pKernelInfo = prog->getKernelInfo("FillBufferRightLeftover", 0);
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(1).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? sizeof(uint64_t) : sizeof(uint32_t));
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? sizeof(uint64_t) : sizeof(uint32_t));
    pKernelInfo = prog->getKernelInfo("FillBufferImmediate", 0);
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(1).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? sizeof(uint64_t) : sizeof(uint32_t));
    pKernelInfo = prog->getKernelInfo("FillBufferImmediateLeftOver", 0);
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(1).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? sizeof(uint64_t) : sizeof(uint32_t));
    pKernelInfo = prog->getKernelInfo("FillBufferSSHOffset", 0);
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(1).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? sizeof(uint64_t) : sizeof(uint32_t));
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size, isStateless && isWideness ? sizeof(uint64_t) : sizeof(uint32_t));
}

TEST_P(KernelAddressingTest, givenBuiltinCopyBufferToImage3dKernelsWhenFetchedFromProgramThenCorrectArgumentSizesAreUsed) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    const auto builtinType = EBuiltInOps::adjustBuiltinType<EBuiltInOps::copyBufferToImage3d>(isStateless, isHeapless, isWideness);

    auto src = pDevice->getBuiltIns()->getBuiltinsLib().getBuiltinCode(builtinType, BuiltinCode::ECodeType::any, *pDevice);
    ClDeviceVector deviceVector;
    deviceVector.push_back(pClDevice);
    prog.reset(BuiltinDispatchInfoBuilder::createProgramFromCode(src, deviceVector).release());
    prog->build(deviceVector, "");

    const char *bufferToImageKernelNames[] = {
        "CopyBufferToImage3dBytes",
        "CopyBufferToImage3d2Bytes",
        "CopyBufferToImage3d4Bytes",
        "CopyBufferToImage3d3To4Bytes",
        "CopyBufferToImage3d8Bytes",
        "CopyBufferToImage3d6To8Bytes",
        "CopyBufferToImage3d16Bytes",
        "CopyBufferToImage3d16BytesAligned"};

    for (const auto &kernelName : bufferToImageKernelNames) {
        auto pKernelInfo = prog->getKernelInfo(kernelName, 0);
        ASSERT_NE(pKernelInfo, nullptr);
        EXPECT_EQ(pKernelInfo->getArgDescriptorAt(2).as<ArgDescValue>().elements[0].size,
                  isStateless && isWideness ? sizeof(uint64_t) : sizeof(uint32_t));
        EXPECT_EQ(pKernelInfo->getArgDescriptorAt(4).as<ArgDescValue>().elements[0].size,
                  isStateless && isWideness ? 2 * sizeof(uint64_t) : 2 * sizeof(uint32_t));
    }
}

TEST_P(KernelAddressingTest, givenBuiltinCopyImage3dToBufferKernelsWhenFetchedFromProgramThenCorrectArgumentSizesAreUsed) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    const auto builtinType = EBuiltInOps::adjustBuiltinType<EBuiltInOps::copyImage3dToBuffer>(isStateless, isHeapless, isWideness);

    auto src = pDevice->getBuiltIns()->getBuiltinsLib().getBuiltinCode(builtinType, BuiltinCode::ECodeType::any, *pDevice);
    ClDeviceVector deviceVector;
    deviceVector.push_back(pClDevice);
    prog.reset(BuiltinDispatchInfoBuilder::createProgramFromCode(src, deviceVector).release());
    auto ret = prog->build(deviceVector, "");
    ASSERT_EQ(ret, CL_SUCCESS);

    const char *kernelNames[] = {
        "CopyImage3dToBufferBytes",
        "CopyImage3dToBuffer2Bytes",
        "CopyImage3dToBuffer4Bytes",
        "CopyImage3dToBuffer4To3Bytes",
        "CopyImage3dToBuffer8Bytes",
        "CopyImage3dToBuffer8To6Bytes",
        "CopyImage3dToBuffer16Bytes",
        "CopyImage3dToBuffer16BytesAligned"};

    for (const auto &kernelName : kernelNames) {
        auto pKernelInfo = prog->getKernelInfo(kernelName, 0);
        ASSERT_NE(pKernelInfo, nullptr);
        EXPECT_EQ(pKernelInfo->getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size,
                  isStateless && isWideness ? sizeof(uint64_t) : sizeof(uint32_t));
        EXPECT_EQ(pKernelInfo->getArgDescriptorAt(4).as<ArgDescValue>().elements[0].size,
                  isStateless && isWideness ? 2 * sizeof(uint64_t) : 2 * sizeof(uint32_t));
    }
}

INSTANTIATE_TEST_SUITE_P(WidenessOnOff, KernelAddressingTest,
                         ::testing::Values(false, true));
