/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/program/program_tests.h"

#include "shared/source/ail/ail_configuration.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/compiler_interface/compiler_warnings/compiler_warnings.h"
#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/addressing_mode_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/helpers/hash.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/allocations_list.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/surface.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/utilities/arrayref.h"
#include "shared/test/common/device_binary_format/patchtokens_tests.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/kernel_binary_helper.h"
#include "shared/test/common/libult/global_environment.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_compiler_interface.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/gtpin/gtpin_notify.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/program/create.inl"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/program/program_from_binary.h"
#include "opencl/test/unit_test/program/program_with_source.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "gtest/gtest.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace NEO;

void ProgramTests::SetUp() {
    ClDeviceFixture::setUp();
    cl_device_id device = pClDevice;
    ContextFixture::setUp(1, &device);
}

void ProgramTests::TearDown() {
    ContextFixture::tearDown();
    ClDeviceFixture::tearDown();
}

class NoCompilerInterfaceRootDeviceEnvironment : public RootDeviceEnvironment {
  public:
    NoCompilerInterfaceRootDeviceEnvironment(ExecutionEnvironment &executionEnvironment) : RootDeviceEnvironment(executionEnvironment) {
        *hwInfo = *defaultHwInfo;
    }

    CompilerInterface *getCompilerInterface() override {
        return nullptr;
    }

    bool initAilConfiguration() override {
        return true;
    }
};

class FailingGenBinaryProgram : public MockProgram {
  public:
    using MockProgram::MockProgram;
    cl_int processGenBinary(const ClDevice &clDevice) override { return CL_INVALID_BINARY; }
};

class SucceedingGenBinaryProgram : public MockProgram {
  public:
    using MockProgram::MockProgram;
    cl_int processGenBinary(const ClDevice &clDevice) override { return CL_SUCCESS; }
};

class PatchtokensProgramWithDebugData : public MockProgram {
  public:
    using MockProgram::MockProgram;

    PatchtokensProgramWithDebugData(ClDevice &device) : MockProgram(toClDeviceVector(device)) {
        auto rootDeviceIdx = device.getRootDeviceIndex();
        const auto &hwInfo = device.getHardwareInfo();

        this->buildInfos.resize(rootDeviceIdx + 1);
        auto &buildInfo = this->buildInfos[rootDeviceIdx];

        buildInfo.unpackedDeviceBinarySize = sizeof(SProgramBinaryHeader);
        buildInfo.unpackedDeviceBinary = std::make_unique<char[]>(buildInfo.unpackedDeviceBinarySize);
        memset(buildInfo.unpackedDeviceBinary.get(), 0, buildInfo.unpackedDeviceBinarySize);
        auto programBinaryHeader = reinterpret_cast<SProgramBinaryHeader *>(buildInfo.unpackedDeviceBinary.get());
        programBinaryHeader->Magic = iOpenCL::MAGIC_CL;
        programBinaryHeader->Version = iOpenCL::CURRENT_ICBE_VERSION;
        programBinaryHeader->Device = hwInfo.platform.eRenderCoreFamily;
        programBinaryHeader->GPUPointerSizeInBytes = sizeof(uintptr_t);

        buildInfo.debugData = std::make_unique<char[]>(0x10);
        buildInfo.debugDataSize = 0x10;
    }
};

using ProgramFromBinaryTest = ProgramFromBinaryFixture;

TEST_F(ProgramFromBinaryTest, WhenBuildingProgramThenSuccessIsReturned) {
    retVal = pProgram->build(
        pProgram->getDevices(),
        nullptr,
        false);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ProgramFromBinaryTest, WhenGettingProgramContextInfoThenCorrectContextIsReturned) {
    cl_context contextRet = reinterpret_cast<cl_context>(static_cast<uintptr_t>(0xdeaddead));
    size_t paramValueSizeRet = 0;

    retVal = pProgram->getInfo(
        CL_PROGRAM_CONTEXT,
        sizeof(cl_context),
        &contextRet,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(pContext, contextRet);
    EXPECT_EQ(sizeof(cl_context), paramValueSizeRet);
}

TEST_F(ProgramFromBinaryTest, GivenNonNullParamValueWhenGettingProgramBinaryInfoThenCorrectBinaryIsReturned) {
    size_t paramValueSize = sizeof(unsigned char **);
    size_t paramValueSizeRet = 0;
    auto testBinary = std::make_unique<char[]>(knownSourceSize);

    retVal = pProgram->getInfo(
        CL_PROGRAM_BINARIES,
        paramValueSize,
        &testBinary,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(paramValueSize, paramValueSizeRet);
    EXPECT_STREQ((const char *)knownSource.get(), (const char *)testBinary.get());
}

TEST_F(ProgramFromBinaryTest, GivenNullParamValueWhenGettingProgramBinaryInfoThenSuccessIsReturned) {
    size_t paramValueSize = sizeof(unsigned char **);
    size_t paramValueSizeRet = 0;

    retVal = pProgram->getInfo(
        CL_PROGRAM_BINARIES,
        0,
        nullptr,
        &paramValueSizeRet);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(paramValueSize, paramValueSizeRet);
}

TEST_F(ProgramFromBinaryTest, GivenNonNullParamValueAndParamValueSizeZeroWhenGettingProgramBinaryInfoThenInvalidValueErrorIsReturned) {
    size_t paramValueSizeRet = 0;
    auto testBinary = std::make_unique<char[]>(knownSourceSize);

    retVal = pProgram->getInfo(
        CL_PROGRAM_BINARIES,
        0,
        &testBinary,
        &paramValueSizeRet);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ProgramFromBinaryTest, GivenInvalidParametersWhenGettingProgramInfoThenValueSizeRetIsNotUpdated) {
    size_t paramValueSizeRet = 0x1234;
    auto testBinary = std::make_unique<char[]>(knownSourceSize);

    retVal = pProgram->getInfo(
        CL_PROGRAM_BINARIES,
        0,
        &testBinary,
        &paramValueSizeRet);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(0x1234u, paramValueSizeRet);
}

TEST_F(ProgramFromBinaryTest, GivenInvalidParamWhenGettingProgramBinaryInfoThenInvalidValueErrorIsReturned) {
    size_t paramValueSizeRet = 0;
    auto testBinary = std::make_unique<char[]>(knownSourceSize);

    retVal = pProgram->getInfo(
        CL_PROGRAM_BUILD_STATUS,
        0,
        nullptr,
        &paramValueSizeRet);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ProgramFromBinaryTest, WhenGettingBinarySizesThenCorrectSizesAreReturned) {
    size_t paramValueSize = sizeof(size_t *);
    size_t paramValue[1];
    size_t paramValueSizeRet = 0;

    retVal = pProgram->getInfo(
        CL_PROGRAM_BINARY_SIZES,
        paramValueSize,
        paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(knownSourceSize, paramValue[0]);
    EXPECT_EQ(paramValueSize, paramValueSizeRet);
}

TEST_F(ProgramFromBinaryTest, GivenProgramWithOneKernelWhenGettingNumKernelsThenOneIsReturned) {
    size_t paramValue = 0;
    size_t paramValueSize = sizeof(paramValue);
    size_t paramValueSizeRet = 0;

    retVal = pProgram->build(
        pProgram->getDevices(),
        nullptr,
        false);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = pProgram->getInfo(
        CL_PROGRAM_NUM_KERNELS,
        paramValueSize,
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, paramValue);
    EXPECT_EQ(paramValueSize, paramValueSizeRet);
}

TEST_F(ProgramFromBinaryTest, GivenProgramWithNoExecutableCodeWhenGettingNumKernelsThenInvalidProgramExecutableErrorIsReturned) {
    size_t paramValue = 0;
    size_t paramValueSize = sizeof(paramValue);
    size_t paramValueSizeRet = 0;

    createProgramFromBinary(pContext, pContext->getDevices(), binaryFileName);
    MockProgram *p = pProgram;
    p->setBuildStatus(CL_BUILD_NONE);

    retVal = pProgram->getInfo(
        CL_PROGRAM_NUM_KERNELS,
        paramValueSize,
        &paramValue,
        &paramValueSizeRet);
    EXPECT_EQ(CL_INVALID_PROGRAM_EXECUTABLE, retVal);
}

TEST_F(ProgramFromBinaryTest, WhenGettingKernelNamesThenCorrectNameIsReturned) {
    size_t paramValueSize = sizeof(size_t *);
    size_t paramValueSizeRet = 0;

    retVal = pProgram->build(
        pProgram->getDevices(),
        nullptr,
        false);
    ASSERT_EQ(CL_SUCCESS, retVal);

    // get info successfully about required sizes for kernel names
    retVal = pProgram->getInfo(
        CL_PROGRAM_KERNEL_NAMES,
        0,
        nullptr,
        &paramValueSizeRet);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(0u, paramValueSizeRet);

    // get info successfully about kernel names
    auto paramValue = std::make_unique<char[]>(paramValueSizeRet);
    paramValueSize = paramValueSizeRet;
    ASSERT_NE(paramValue, nullptr);

    size_t expectedKernelsStringSize = strlen(kernelName) + 1;
    retVal = pProgram->getInfo(
        CL_PROGRAM_KERNEL_NAMES,
        paramValueSize,
        paramValue.get(),
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_STREQ(kernelName, (char *)paramValue.get());
    EXPECT_EQ(expectedKernelsStringSize, paramValueSizeRet);
}

TEST_F(ProgramFromBinaryTest, GivenProgramWithNoExecutableCodeWhenGettingKernelNamesThenInvalidProgramExecutableErrorIsReturned) {
    size_t paramValueSize = sizeof(size_t *);
    size_t paramValueSizeRet = 0;

    createProgramFromBinary(pContext, pContext->getDevices(), binaryFileName);
    MockProgram *p = pProgram;
    p->setBuildStatus(CL_BUILD_NONE);

    retVal = pProgram->getInfo(
        CL_PROGRAM_KERNEL_NAMES,
        paramValueSize,
        nullptr,
        &paramValueSizeRet);
    EXPECT_EQ(CL_INVALID_PROGRAM_EXECUTABLE, retVal);
}

TEST_F(ProgramFromBinaryTest, WhenGettingProgramScopeGlobalCtorsAndDtorsPresentInfoThenCorrectValueIsReturned) {
    cl_uint paramRet = 0;
    cl_uint expectedParam = CL_FALSE;
    size_t paramSizeRet = 0;

    retVal = pProgram->getInfo(
        CL_PROGRAM_SCOPE_GLOBAL_CTORS_PRESENT,
        sizeof(cl_uint),
        &paramRet,
        &paramSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_uint), paramSizeRet);
    EXPECT_EQ(expectedParam, paramRet);

    retVal = pProgram->getInfo(
        CL_PROGRAM_SCOPE_GLOBAL_DTORS_PRESENT,
        sizeof(cl_uint),
        &paramRet,
        &paramSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_uint), paramSizeRet);
    EXPECT_EQ(expectedParam, paramRet);
}

TEST_F(ProgramFromBinaryTest, GivenNullDeviceWhenGettingBuildStatusThenBuildNoneIsReturned) {
    cl_device_id device = pClDevice;
    cl_build_status buildStatus = 0;
    size_t paramValueSize = sizeof(buildStatus);
    size_t paramValueSizeRet = 0;

    retVal = pProgram->getBuildInfo(
        device,
        CL_PROGRAM_BUILD_STATUS,
        paramValueSize,
        &buildStatus,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(paramValueSize, paramValueSizeRet);
    EXPECT_EQ(CL_BUILD_NONE, buildStatus);
}

TEST_F(ProgramFromBinaryTest, GivenInvalidParametersWhenGettingBuildInfoThenValueSizeRetIsNotUpdated) {
    cl_device_id device = pClDevice;
    cl_build_status buildStatus = 0;
    size_t paramValueSize = sizeof(buildStatus);
    size_t paramValueSizeRet = 0x1234;

    retVal = pProgram->getBuildInfo(
        device,
        0,
        paramValueSize,
        &buildStatus,
        &paramValueSizeRet);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(0x1234u, paramValueSizeRet);
}

TEST_F(ProgramFromBinaryTest, GivenDefaultDeviceWhenGettingBuildOptionsThenBuildOptionsAreEmpty) {
    cl_device_id device = pClDevice;
    size_t paramValueSizeRet = 0u;
    size_t paramValueSize = 0u;

    retVal = pProgram->getBuildInfo(
        device,
        CL_PROGRAM_BUILD_OPTIONS,
        0,
        nullptr,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(paramValueSizeRet, 0u);

    auto paramValue = std::make_unique<char[]>(paramValueSizeRet);
    paramValueSize = paramValueSizeRet;

    retVal = pProgram->getBuildInfo(
        device,
        CL_PROGRAM_BUILD_OPTIONS,
        paramValueSize,
        paramValue.get(),
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_STREQ("", (char *)paramValue.get());
}

TEST_F(ProgramFromBinaryTest, GivenDefaultDeviceWhenGettingLogThenLogEmpty) {
    cl_device_id device = pClDevice;
    size_t paramValueSizeRet = 0u;
    size_t paramValueSize = 0u;

    retVal = pProgram->getBuildInfo(
        device,
        CL_PROGRAM_BUILD_LOG,
        0,
        nullptr,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(paramValueSizeRet, 0u);

    auto paramValue = std::make_unique<char[]>(paramValueSizeRet);
    paramValueSize = paramValueSizeRet;

    retVal = pProgram->getBuildInfo(
        device,
        CL_PROGRAM_BUILD_LOG,
        paramValueSize,
        paramValue.get(),
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_STREQ("", (char *)paramValue.get());
}

TEST_F(ProgramFromBinaryTest, GivenLogEntriesWhenGetBuildLogThenLogIsApended) {
    cl_device_id device = pClDevice;
    size_t paramValueSizeRet = 0u;
    size_t paramValueSize = 0u;

    retVal = pProgram->getBuildInfo(
        device,
        CL_PROGRAM_BUILD_LOG,
        0,
        nullptr,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(paramValueSizeRet, 0u);

    auto paramValue = std::make_unique<char[]>(paramValueSizeRet);
    paramValueSize = paramValueSizeRet;

    retVal = pProgram->getBuildInfo(
        device,
        CL_PROGRAM_BUILD_LOG,
        paramValueSize,
        paramValue.get(),
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_STREQ("", (char *)paramValue.get());

    // Add more text to the log
    pProgram->updateBuildLog(pClDevice->getRootDeviceIndex(), "testing", 8);
    pProgram->updateBuildLog(pClDevice->getRootDeviceIndex(), "several", 8);

    retVal = pProgram->getBuildInfo(
        device,
        CL_PROGRAM_BUILD_LOG,
        0,
        nullptr,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GE(paramValueSizeRet, 16u);
    paramValue = std::make_unique<char[]>(paramValueSizeRet);

    paramValueSize = paramValueSizeRet;

    retVal = pProgram->getBuildInfo(
        device,
        CL_PROGRAM_BUILD_LOG,
        paramValueSize,
        paramValue.get(),
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(nullptr, strstr(paramValue.get(), "testing"));

    const char *paramValueContinued = strstr(paramValue.get(), "testing") + 7;
    ASSERT_NE(nullptr, strstr(paramValueContinued, "several"));
}

TEST_F(ProgramFromBinaryTest, GivenNullParamValueWhenGettingProgramBinaryTypeThenParamValueSizeIsReturned) {
    cl_device_id device = pClDevice;
    size_t paramValueSizeRet = 0u;
    size_t paramValueSize = 0u;

    retVal = pProgram->getBuildInfo(
        device,
        CL_PROGRAM_BINARY_TYPE,
        paramValueSize,
        nullptr,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(paramValueSizeRet, 0u);
}

TEST_F(ProgramFromBinaryTest, WhenGettingProgramBinaryTypeThenCorrectProgramTypeIsReturned) {
    cl_device_id device = pClDevice;
    cl_program_binary_type programType = 0;
    char *paramValue = (char *)&programType;
    size_t paramValueSizeRet = 0u;
    size_t paramValueSize = 0u;

    retVal = pProgram->getBuildInfo(
        device,
        CL_PROGRAM_BINARY_TYPE,
        paramValueSize,
        nullptr,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(paramValueSizeRet, 0u);

    paramValueSize = paramValueSizeRet;
    retVal = pProgram->getBuildInfo(
        device,
        CL_PROGRAM_BINARY_TYPE,
        paramValueSize,
        paramValue,
        &paramValueSizeRet);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ((cl_program_binary_type)CL_PROGRAM_BINARY_TYPE_EXECUTABLE, programType);
}

TEST_F(ProgramFromBinaryTest, GivenInvalidParamWhenGettingBuildInfoThenInvalidValueErrorIsReturned) {
    cl_device_id device = pClDevice;
    size_t paramValueSizeRet = 0u;

    retVal = pProgram->getBuildInfo(
        device,
        CL_PROGRAM_KERNEL_NAMES,
        0,
        nullptr,
        &paramValueSizeRet);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ProgramFromBinaryTest, GivenGlobalVariableTotalSizeSetWhenGettingBuildGlobalVariableTotalSizeThenCorrectSizeIsReturned) {
    cl_device_id device = pClDevice;
    size_t globalVarSize = 22;
    size_t paramValueSize = sizeof(globalVarSize);
    size_t paramValueSizeRet = 0;
    char *paramValue = (char *)&globalVarSize;

    // get build info as is
    retVal = pProgram->getBuildInfo(
        device,
        CL_PROGRAM_BUILD_GLOBAL_VARIABLE_TOTAL_SIZE,
        paramValueSize,
        paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(paramValueSizeRet, sizeof(globalVarSize));
    EXPECT_EQ(globalVarSize, 0u);

    // Set GlobalVariableTotalSize as 1024
    createProgramFromBinary(pContext, pContext->getDevices(), binaryFileName);
    MockProgram *p = pProgram;
    ProgramInfo programInfo;

    char constantData[1024] = {};
    programInfo.globalVariables.initData = constantData;
    programInfo.globalVariables.size = sizeof(constantData);
    p->processProgramInfo(programInfo, *pClDevice);

    // get build info once again
    retVal = pProgram->getBuildInfo(
        device,
        CL_PROGRAM_BUILD_GLOBAL_VARIABLE_TOTAL_SIZE,
        paramValueSize,
        paramValue,
        &paramValueSizeRet);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(paramValueSizeRet, sizeof(globalVarSize));
    if (castToObject<ClDevice>(pClDevice)->areOcl21FeaturesEnabled()) {
        EXPECT_EQ(globalVarSize, 1024u);
    } else {
        EXPECT_EQ(globalVarSize, 0u);
    }
}

TEST_F(ProgramFromBinaryTest, givenProgramWhenItIsBeingBuildThenItContainsGraphicsAllocationInKernelInfo) {
    pProgram->build(pProgram->getDevices(), nullptr, true);
    auto kernelInfo = pProgram->getKernelInfo(size_t(0), rootDeviceIndex);

    auto graphicsAllocation = kernelInfo->getGraphicsAllocation();
    ASSERT_NE(nullptr, graphicsAllocation);
    EXPECT_TRUE(graphicsAllocation->is32BitAllocation());
    auto &hwHelper = NEO::HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
    size_t isaPadding = hwHelper.getPaddingForISAAllocation();
    EXPECT_EQ(graphicsAllocation->getUnderlyingBufferSize(), kernelInfo->heapInfo.KernelHeapSize + isaPadding);

    auto kernelIsa = graphicsAllocation->getUnderlyingBuffer();
    EXPECT_NE(kernelInfo->heapInfo.pKernelHeap, kernelIsa);
    EXPECT_EQ(0, memcmp(kernelIsa, kernelInfo->heapInfo.pKernelHeap, kernelInfo->heapInfo.KernelHeapSize));
    auto rootDeviceIndex = graphicsAllocation->getRootDeviceIndex();
    auto gmmHelper = pDevice->getGmmHelper();
    EXPECT_EQ(gmmHelper->decanonize(graphicsAllocation->getGpuBaseAddress()), pDevice->getMemoryManager()->getInternalHeapBaseAddress(rootDeviceIndex, graphicsAllocation->isAllocatedInLocalMemoryPool()));
}

TEST_F(ProgramFromBinaryTest, whenProgramIsBeingRebuildThenOutdatedGlobalBuffersAreFreed) {
    pProgram->build(pProgram->getDevices(), nullptr, true);
    EXPECT_EQ(nullptr, pProgram->buildInfos[pClDevice->getRootDeviceIndex()].constantSurface);
    EXPECT_EQ(nullptr, pProgram->buildInfos[pClDevice->getRootDeviceIndex()].globalSurface);

    pProgram->buildInfos[pClDevice->getRootDeviceIndex()].constantSurface = new MockGraphicsAllocation();
    pProgram->processGenBinary(*pClDevice);
    EXPECT_EQ(nullptr, pProgram->buildInfos[pClDevice->getRootDeviceIndex()].constantSurface);
    EXPECT_EQ(nullptr, pProgram->buildInfos[pClDevice->getRootDeviceIndex()].globalSurface);

    pProgram->buildInfos[pClDevice->getRootDeviceIndex()].globalSurface = new MockGraphicsAllocation();
    pProgram->processGenBinary(*pClDevice);
    EXPECT_EQ(nullptr, pProgram->buildInfos[pClDevice->getRootDeviceIndex()].constantSurface);
    EXPECT_EQ(nullptr, pProgram->buildInfos[pClDevice->getRootDeviceIndex()].globalSurface);
}

TEST_F(ProgramFromBinaryTest, givenProgramWhenCleanKernelInfoIsCalledThenKernelAllocationIsFreed) {
    pProgram->build(pProgram->getDevices(), nullptr, true);
    EXPECT_EQ(1u, pProgram->getNumKernels());
    for (auto i = 0u; i < pProgram->buildInfos.size(); i++) {
        pProgram->cleanCurrentKernelInfo(i);
    }
    EXPECT_EQ(0u, pProgram->getNumKernels());
}

TEST_F(ProgramFromBinaryTest, givenReuseKernelBinariesWhenCleanCurrentKernelInfoThenDecreaseAllocationReuseCounter) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ReuseKernelBinaries.set(1);

    pProgram->build(pProgram->getDevices(), nullptr, true);
    auto &kernelAllocMap = pProgram->peekExecutionEnvironment().memoryManager->getKernelAllocationMap();
    auto kernelName = pProgram->buildInfos[0].kernelInfoArray[0]->kernelDescriptor.kernelMetadata.kernelName;
    auto kernelAllocations = kernelAllocMap.find(kernelName);
    kernelAllocations->second.reuseCounter = 2u;

    EXPECT_EQ(1u, pProgram->getNumKernels());
    for (auto i = 0u; i < pProgram->buildInfos.size(); i++) {
        pProgram->cleanCurrentKernelInfo(i);
    }
    EXPECT_EQ(0u, pProgram->getNumKernels());
    EXPECT_EQ(1u, kernelAllocations->second.reuseCounter);

    pProgram->peekExecutionEnvironment().memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(kernelAllocations->second.kernelAllocation);
}

TEST_F(ProgramFromBinaryTest, givenReuseKernelBinariesWhenCleanCurrentKernelInfoAndCounterEqualsZeroThenFreeAllocation) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ReuseKernelBinaries.set(1);

    pProgram->build(pProgram->getDevices(), nullptr, true);
    auto &kernelAllocMap = pProgram->peekExecutionEnvironment().memoryManager->getKernelAllocationMap();

    EXPECT_EQ(1u, pProgram->getNumKernels());
    for (auto i = 0u; i < pProgram->buildInfos.size(); i++) {
        pProgram->cleanCurrentKernelInfo(i);
    }
    EXPECT_EQ(0u, pProgram->getNumKernels());
    EXPECT_EQ(0u, kernelAllocMap.size());
}

HWTEST_F(ProgramFromBinaryTest, givenProgramWhenCleanCurrentKernelInfoIsCalledButGpuIsNotYetDoneThenKernelAllocationIsPutOnDeferredFreeListAndCsrRegistersCacheFlush) {
    auto &csr = pDevice->getGpgpuCommandStreamReceiver();
    EXPECT_TRUE(csr.getTemporaryAllocations().peekIsEmpty());
    pProgram->build(pProgram->getDevices(), nullptr, true);
    auto kernelAllocation = pProgram->getKernelInfo(static_cast<size_t>(0u), rootDeviceIndex)->getGraphicsAllocation();
    kernelAllocation->updateTaskCount(100, csr.getOsContext().getContextId());
    *csr.getTagAddress() = 0;
    pProgram->cleanCurrentKernelInfo(rootDeviceIndex);
    EXPECT_TRUE(csr.getTemporaryAllocations().peekIsEmpty());
    EXPECT_FALSE(csr.getDeferredAllocations().peekIsEmpty());
    EXPECT_EQ(csr.getDeferredAllocations().peekHead(), kernelAllocation);
    EXPECT_TRUE(this->pDevice->getUltCommandStreamReceiver<FamilyType>().requiresInstructionCacheFlush);
}

HWTEST_F(ProgramFromBinaryTest, givenIsaAllocationUsedByMultipleCsrsWhenItIsDeletedThenItRegistersCacheFlushInEveryCsrThatUsedIt) {
    auto &csr0 = this->pDevice->getUltCommandStreamReceiverFromIndex<FamilyType>(0u);
    auto &csr1 = this->pDevice->getUltCommandStreamReceiverFromIndex<FamilyType>(1u);

    pProgram->build(pProgram->getDevices(), nullptr, true);

    auto kernelAllocation = pProgram->getKernelInfo(static_cast<size_t>(0u), rootDeviceIndex)->getGraphicsAllocation();

    csr0.makeResident(*kernelAllocation);
    csr1.makeResident(*kernelAllocation);

    csr0.processResidency(csr0.getResidencyAllocations(), 0u);
    csr1.processResidency(csr1.getResidencyAllocations(), 0u);

    csr0.makeNonResident(*kernelAllocation);
    csr1.makeNonResident(*kernelAllocation);

    EXPECT_FALSE(csr0.requiresInstructionCacheFlush);
    EXPECT_FALSE(csr1.requiresInstructionCacheFlush);

    pProgram->cleanCurrentKernelInfo(rootDeviceIndex);
    EXPECT_TRUE(csr0.requiresInstructionCacheFlush);
    EXPECT_TRUE(csr1.requiresInstructionCacheFlush);
}

TEST_F(ProgramFromSourceTest, givenEmptyAilWhenCreateProgramWithSourcesThenSourcesDoNotChange) {

    VariableBackup<AILConfiguration *> ailConfigurationBackup(&ailConfigurationTable[productFamily]);
    ailConfigurationTable[productFamily] = nullptr;
    const char *sources[] = {"kernel() {}"};
    size_t knownSourceSize = strlen(sources[0]);

    auto pProgram = Program::create<MockProgram>(
        pContext,
        1,
        sources,
        &knownSourceSize,
        retVal);

    ASSERT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    EXPECT_STREQ(sources[0], pProgram->sourceCode.c_str());
    pProgram->release();
}

TEST_F(ProgramFromSourceTest, GivenSpecificParamatersWhenBuildingProgramThenSuccessOrCorrectErrorCodeIsReturned) {
    KernelBinaryHelper kbHelper(binaryFileName, true);
    auto device = pPlatform->getClDevice(0);

    createProgramWithSource(
        pContext,
        sourceFileName);

    // Order of following microtests is important - do not change.
    // Add new microtests at end.

    auto pMockProgram = pProgram;

    // fail build - another build is already in progress
    pMockProgram->setBuildStatus(CL_BUILD_IN_PROGRESS);
    retVal = pProgram->build(pProgram->getDevices(), nullptr, false);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    pMockProgram->setBuildStatus(CL_BUILD_NONE);

    // fail build - CompilerInterface cannot be obtained

    auto executionEnvironment = device->getExecutionEnvironment();
    std::unique_ptr<RootDeviceEnvironment> rootDeviceEnvironment = std::make_unique<NoCompilerInterfaceRootDeviceEnvironment>(*executionEnvironment);
    std::swap(rootDeviceEnvironment, executionEnvironment->rootDeviceEnvironments[device->getRootDeviceIndex()]);
    auto p2 = std::make_unique<MockProgram>(toClDeviceVector(*device));
    retVal = p2->build(p2->getDevices(), nullptr, false);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
    p2.reset(nullptr);
    std::swap(rootDeviceEnvironment, executionEnvironment->rootDeviceEnvironments[device->getRootDeviceIndex()]);

    // fail build - any build error (here caused by specifying unrecognized option)
    retVal = pProgram->build(pProgram->getDevices(), "-invalid-option", false);
    EXPECT_EQ(CL_BUILD_PROGRAM_FAILURE, retVal);

    // fail build - linked code is corrupted and cannot be postprocessed
    auto p3 = std::make_unique<FailingGenBinaryProgram>(toClDeviceVector(*device));
    std::string testFile;
    size_t sourceSize;
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd16.cl"); // source file
    auto pSourceBuffer = loadDataFromFile(testFile.c_str(), sourceSize);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSourceBuffer);
    p3->sourceCode = pSourceBuffer.get();
    p3->createdFrom = Program::CreatedFrom::SOURCE;
    retVal = p3->build(p3->getDevices(), nullptr, false);
    EXPECT_EQ(CL_INVALID_BINARY, retVal);
    p3.reset(nullptr);

    // build successfully - build kernel and write it to Kernel Cache
    pMockProgram->clearOptions();
    std::string receivedInternalOptions;

    auto debugVars = NEO::getFclDebugVars();
    debugVars.receivedInternalOptionsOutput = &receivedInternalOptions;
    gEnvironment->fclPushDebugVars(debugVars);
    retVal = pProgram->build(pProgram->getDevices(), nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(CompilerOptions::contains(receivedInternalOptions, pPlatform->getClDevice(0)->peekCompilerExtensions())) << receivedInternalOptions;
    gEnvironment->fclPopDebugVars();

    // get build log
    size_t paramValueSizeRet = 0u;
    retVal = pProgram->getBuildInfo(
        device,
        CL_PROGRAM_BUILD_LOG,
        0,
        nullptr,
        &paramValueSizeRet);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(paramValueSizeRet, 0u);

    // get build log when the log does not exist
    pMockProgram->clearLog(device->getRootDeviceIndex());
    retVal = pProgram->getBuildInfo(
        device,
        CL_PROGRAM_BUILD_LOG,
        0,
        nullptr,
        &paramValueSizeRet);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(paramValueSizeRet, 0u);

    // build successfully - build kernel but do not write it to Kernel Cache (kernel is already in the Cache)
    pMockProgram->setBuildStatus(CL_BUILD_NONE);
    retVal = pProgram->build(pProgram->getDevices(), nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // build successfully - kernel is already in Kernel Cache, do not build and take it from Cache
    retVal = pProgram->build(pProgram->getDevices(), nullptr, true);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // fail build - code to be build does not exist
    pMockProgram->sourceCode = ""; // set source code as non-existent (invalid)
    pMockProgram->createdFrom = Program::CreatedFrom::SOURCE;
    pMockProgram->setBuildStatus(CL_BUILD_NONE);
    pMockProgram->setCreatedFromBinary(false);
    retVal = pProgram->build(pProgram->getDevices(), nullptr, false);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
}

TEST_F(ProgramFromSourceTest, GivenDuplicateOptionsWhenCreatingWithSourceThenBuildSucceeds) {
    KernelBinaryHelper kbHelper(binaryFileName, false);

    retVal = pProgram->build(pProgram->getDevices(), nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pProgram->build(pProgram->getDevices(), CompilerOptions::fastRelaxedMath.data(), false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pProgram->build(pProgram->getDevices(), CompilerOptions::fastRelaxedMath.data(), false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pProgram->build(pProgram->getDevices(), CompilerOptions::finiteMathOnly.data(), false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pProgram->build(pProgram->getDevices(), nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ProgramFromSourceTest, WhenBuildingProgramThenFeaturesAndExtraExtensionsAreNotAdded) {
    auto cip = new MockCompilerInterfaceCaptureBuildOptions();
    auto pClDevice = pContext->getDevice(0);
    pClDevice->getExecutionEnvironment()->rootDeviceEnvironments[pClDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);

    auto extensionsOption = static_cast<ClDevice *>(devices[0])->peekCompilerExtensions();
    auto extensionsWithFeaturesOption = static_cast<ClDevice *>(devices[0])->peekCompilerExtensionsWithFeatures();
    EXPECT_FALSE(hasSubstr(cip->buildInternalOptions, extensionsOption));
    EXPECT_FALSE(hasSubstr(cip->buildInternalOptions, extensionsWithFeaturesOption));
    EXPECT_FALSE(hasSubstr(cip->buildInternalOptions, std::string{"+cl_khr_3d_image_writes "}));

    retVal = pProgram->build(pProgram->getDevices(), nullptr, false);
    EXPECT_TRUE(hasSubstr(cip->buildInternalOptions, extensionsOption));
    EXPECT_FALSE(hasSubstr(cip->buildInternalOptions, extensionsWithFeaturesOption));
    EXPECT_FALSE(hasSubstr(cip->buildInternalOptions, std::string{"+cl_khr_3d_image_writes "}));
}

TEST_F(ProgramFromSourceTest, WhenBuildingProgramWithOpenClC20ThenExtraExtensionsAreAdded) {
    auto cip = new MockCompilerInterfaceCaptureBuildOptions();
    auto pClDevice = pContext->getDevice(0);
    pClDevice->getExecutionEnvironment()->rootDeviceEnvironments[pClDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);
    auto pProgram = std::make_unique<SucceedingGenBinaryProgram>(toClDeviceVector(*pClDevice));
    pProgram->sourceCode = "__kernel mock() {}";
    pProgram->createdFrom = Program::CreatedFrom::SOURCE;

    MockProgram::getInternalOptionsCalled = 0;

    auto extensionsOption = static_cast<ClDevice *>(devices[0])->peekCompilerExtensions();
    auto extensionsWithFeaturesOption = static_cast<ClDevice *>(devices[0])->peekCompilerExtensionsWithFeatures();
    EXPECT_FALSE(hasSubstr(cip->buildInternalOptions, std::string{"+cl_khr_3d_image_writes "}));

    retVal = pProgram->build(pProgram->getDevices(), "-cl-std=CL2.0", false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(hasSubstr(cip->buildInternalOptions, std::string{"+cl_khr_3d_image_writes "}));
    EXPECT_EQ(1, MockProgram::getInternalOptionsCalled);
}

TEST_F(ProgramFromSourceTest, WhenBuildingProgramWithOpenClC30ThenFeaturesAreAdded) {
    auto cip = new MockCompilerInterfaceCaptureBuildOptions();
    auto pClDevice = pContext->getDevice(0);
    pClDevice->getExecutionEnvironment()->rootDeviceEnvironments[pClDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);
    auto pProgram = std::make_unique<SucceedingGenBinaryProgram>(toClDeviceVector(*pClDevice));
    pProgram->sourceCode = "__kernel mock() {}";
    pProgram->createdFrom = Program::CreatedFrom::SOURCE;

    MockProgram::getInternalOptionsCalled = 0;

    auto extensionsOption = static_cast<ClDevice *>(devices[0])->peekCompilerExtensions();
    auto extensionsWithFeaturesOption = static_cast<ClDevice *>(devices[0])->peekCompilerExtensionsWithFeatures();
    EXPECT_FALSE(hasSubstr(cip->buildInternalOptions, extensionsOption));
    EXPECT_FALSE(hasSubstr(cip->buildInternalOptions, extensionsWithFeaturesOption));

    retVal = pProgram->build(pProgram->getDevices(), "-cl-std=CL3.0", false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(hasSubstr(cip->buildInternalOptions, extensionsOption));
    EXPECT_TRUE(hasSubstr(cip->buildInternalOptions, extensionsWithFeaturesOption));
    EXPECT_EQ(1, MockProgram::getInternalOptionsCalled);
}

TEST_F(ProgramFromSourceTest, WhenBuildingProgramWithOpenClC30ThenFeaturesAreAddedOnlyOnce) {
    auto cip = new MockCompilerInterfaceCaptureBuildOptions();
    auto pClDevice = pContext->getDevice(0);
    pClDevice->getExecutionEnvironment()->rootDeviceEnvironments[pClDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);
    auto pProgram = std::make_unique<SucceedingGenBinaryProgram>(toClDeviceVector(*pClDevice));
    pProgram->sourceCode = "__kernel mock() {}";
    pProgram->createdFrom = Program::CreatedFrom::SOURCE;

    retVal = pProgram->build(pProgram->getDevices(), "-cl-std=CL3.0", false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = pProgram->build(pProgram->getDevices(), "-cl-std=CL3.0", false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto extensionsWithFeaturesOption = pClDevice->peekCompilerExtensionsWithFeatures();
    auto &internalOptions = cip->buildInternalOptions;
    auto pos = internalOptions.find(extensionsWithFeaturesOption);
    EXPECT_NE(std::string::npos, pos);

    pos = internalOptions.find(extensionsWithFeaturesOption, pos + 1);
    EXPECT_EQ(std::string::npos, pos);
}

TEST_F(ProgramFromSourceTest, WhenCompilingProgramThenFeaturesAndExtraExtensionsAreNotAdded) {
    auto pCompilerInterface = new MockCompilerInterfaceCaptureBuildOptions();
    auto pClDevice = static_cast<ClDevice *>(devices[0]);
    pClDevice->getExecutionEnvironment()->rootDeviceEnvironments[pClDevice->getRootDeviceIndex()]->compilerInterface.reset(pCompilerInterface);
    auto extensionsOption = pClDevice->peekCompilerExtensions();
    auto extensionsWithFeaturesOption = pClDevice->peekCompilerExtensionsWithFeatures();
    EXPECT_FALSE(hasSubstr(pCompilerInterface->buildInternalOptions, extensionsOption));
    EXPECT_FALSE(hasSubstr(pCompilerInterface->buildInternalOptions, extensionsWithFeaturesOption));
    EXPECT_FALSE(hasSubstr(pCompilerInterface->buildInternalOptions, std::string{"+cl_khr_3d_image_writes "}));

    MockProgram::getInternalOptionsCalled = 0;
    retVal = pProgram->compile(pProgram->getDevices(), nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(hasSubstr(pCompilerInterface->buildInternalOptions, extensionsOption));
    EXPECT_FALSE(hasSubstr(pCompilerInterface->buildInternalOptions, extensionsWithFeaturesOption));
    EXPECT_FALSE(hasSubstr(pCompilerInterface->buildInternalOptions, std::string{"+cl_khr_3d_image_writes "}));
    EXPECT_EQ(1, MockProgram::getInternalOptionsCalled);
}

TEST_F(ProgramFromSourceTest, WhenCompilingProgramWithOpenClC20ThenExtraExtensionsAreAdded) {
    auto pCompilerInterface = new MockCompilerInterfaceCaptureBuildOptions();
    auto pClDevice = static_cast<ClDevice *>(devices[0]);
    pClDevice->getExecutionEnvironment()->rootDeviceEnvironments[pClDevice->getRootDeviceIndex()]->compilerInterface.reset(pCompilerInterface);
    auto extensionsOption = pClDevice->peekCompilerExtensions();
    auto extensionsWithFeaturesOption = pClDevice->peekCompilerExtensionsWithFeatures();
    EXPECT_FALSE(hasSubstr(pCompilerInterface->buildInternalOptions, std::string{"+cl_khr_3d_image_writes "}));

    MockProgram::getInternalOptionsCalled = 0;
    retVal = pProgram->compile(pProgram->getDevices(), "-cl-std=CL2.0", 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(hasSubstr(pCompilerInterface->buildInternalOptions, std::string{"+cl_khr_3d_image_writes "}));
    EXPECT_EQ(1, MockProgram::getInternalOptionsCalled);
}

TEST_F(ProgramFromSourceTest, WhenCompilingProgramWithOpenClC30ThenFeaturesAreAdded) {
    auto pCompilerInterface = new MockCompilerInterfaceCaptureBuildOptions();
    auto pClDevice = pContext->getDevice(0);
    pClDevice->getExecutionEnvironment()->rootDeviceEnvironments[pClDevice->getRootDeviceIndex()]->compilerInterface.reset(pCompilerInterface);
    auto pProgram = std::make_unique<SucceedingGenBinaryProgram>(toClDeviceVector(*pClDevice));
    pProgram->sourceCode = "__kernel mock() {}";
    pProgram->createdFrom = Program::CreatedFrom::SOURCE;

    auto extensionsOption = pClDevice->peekCompilerExtensions();
    auto extensionsWithFeaturesOption = pClDevice->peekCompilerExtensionsWithFeatures();
    EXPECT_FALSE(hasSubstr(pCompilerInterface->buildInternalOptions, extensionsOption));
    EXPECT_FALSE(hasSubstr(pCompilerInterface->buildInternalOptions, extensionsWithFeaturesOption));

    retVal = pProgram->compile(pProgram->getDevices(), "-cl-std=CL3.0", 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(hasSubstr(pCompilerInterface->buildInternalOptions, extensionsOption));
    EXPECT_TRUE(hasSubstr(pCompilerInterface->buildInternalOptions, extensionsWithFeaturesOption));
}

class Callback {
  public:
    Callback() {
        this->oldCallback = MemoryManagement::deleteCallback;
        MemoryManagement::deleteCallback = thisCallback;
    }
    ~Callback() {
        MemoryManagement::deleteCallback = this->oldCallback;
    }
    static void watch(const void *p) {
        watchList[p] = 0u;
    }
    static void unwatch(const void *p) {
        EXPECT_GT(watchList[p], 0u);
        watchList.erase(p);
    }

  private:
    void (*oldCallback)(void *);
    static void thisCallback(void *p) {
        if (watchList.find(p) != watchList.end())
            watchList[p]++;
    }
    static std::map<const void *, uint32_t> watchList;
};

std::map<const void *, uint32_t> Callback::watchList;

TEST_F(ProgramFromSourceTest, GivenDifferentCommpilerOptionsWhenBuildingProgramThenKernelHashesAreDifferent) {
    KernelBinaryHelper kbHelper(binaryFileName, true);

    auto rootDeviceIndex = pContext->getDevice(0)->getRootDeviceIndex();

    createProgramWithSource(
        pContext,
        sourceFileName);

    Callback callback;

    retVal = pProgram->build(pProgram->getDevices(), nullptr, true);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto hash1 = pProgram->getCachedFileName();
    auto kernel1 = pProgram->getKernelInfo("CopyBuffer", rootDeviceIndex);
    Callback::watch(kernel1);
    EXPECT_NE(nullptr, kernel1);

    retVal = pProgram->build(pProgram->getDevices(), CompilerOptions::fastRelaxedMath.data(), true);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto hash2 = pProgram->getCachedFileName();
    auto kernel2 = pProgram->getKernelInfo("CopyBuffer", rootDeviceIndex);
    EXPECT_NE(nullptr, kernel2);
    EXPECT_NE(hash1, hash2);
    Callback::unwatch(kernel1);
    Callback::watch(kernel2);

    retVal = pProgram->build(pProgram->getDevices(), CompilerOptions::finiteMathOnly.data(), true);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto hash3 = pProgram->getCachedFileName();
    auto kernel3 = pProgram->getKernelInfo("CopyBuffer", rootDeviceIndex);
    EXPECT_NE(nullptr, kernel3);
    EXPECT_NE(hash1, hash3);
    EXPECT_NE(hash2, hash3);
    Callback::unwatch(kernel2);
    Callback::watch(kernel3);

    pProgram->createdFrom = NEO::Program::CreatedFrom::BINARY;
    pProgram->setIrBinary(new char[16], true);
    pProgram->setIrBinarySize(16, true);
    retVal = pProgram->build(pProgram->getDevices(), nullptr, true);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto hash4 = pProgram->getCachedFileName();
    auto kernel4 = pProgram->getKernelInfo("CopyBuffer", rootDeviceIndex);
    EXPECT_NE(nullptr, kernel4);
    EXPECT_EQ(hash3, hash4);
    Callback::unwatch(kernel3);
    Callback::watch(kernel4);

    pProgram->createdFrom = NEO::Program::CreatedFrom::SOURCE;
    retVal = pProgram->build(pProgram->getDevices(), nullptr, true);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto hash5 = pProgram->getCachedFileName();
    auto kernel5 = pProgram->getKernelInfo("CopyBuffer", rootDeviceIndex);
    EXPECT_NE(nullptr, kernel5);
    EXPECT_EQ(hash1, hash5);
    Callback::unwatch(kernel4);
}

TEST_F(ProgramFromSourceTest, GivenEmptyProgramWhenCreatingProgramThenInvalidValueErrorIsReturned) {
    auto p = Program::create(pContext, 0, nullptr, nullptr, retVal);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, p);
    delete p;
}

TEST_F(ProgramFromSourceTest, GivenSpecificParamatersWhenCompilingProgramThenSuccessOrCorrectErrorCodeIsReturned) {
    createProgramWithSource(
        pContext,
        sourceFileName);

    cl_program inputHeaders;
    const char *headerIncludeNames = "";
    cl_program nullprogram = nullptr;
    cl_program invprogram = (cl_program)pContext;

    // Order of following microtests is important - do not change.
    // Add new microtests at end.

    // invalid compile parameters: combinations of numInputHeaders==0 & inputHeaders & headerIncludeNames
    retVal = pProgram->compile(pProgram->getDevices(), nullptr, 0, &inputHeaders, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = pProgram->compile(pProgram->getDevices(), nullptr, 0, nullptr, &headerIncludeNames);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    // invalid compile parameters: combinations of numInputHeaders!=0 & inputHeaders & headerIncludeNames
    retVal = pProgram->compile(pProgram->getDevices(), nullptr, 1, &inputHeaders, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = pProgram->compile(pProgram->getDevices(), nullptr, 1, nullptr, &headerIncludeNames);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    // fail compilation - another compilation is already in progress
    pProgram->setBuildStatus(CL_BUILD_IN_PROGRESS);
    retVal = pProgram->compile(pProgram->getDevices(), nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    pProgram->setBuildStatus(CL_BUILD_NONE);

    // invalid compile parameters: invalid header Program object==nullptr
    retVal = pProgram->compile(pProgram->getDevices(), nullptr, 1, &nullprogram, &headerIncludeNames);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);

    // invalid compile parameters: invalid header Program object==non Program object
    retVal = pProgram->compile(pProgram->getDevices(), nullptr, 1, &invprogram, &headerIncludeNames);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);

    // compile successfully kernel with header
    std::string testFile;
    size_t sourceSize;
    MockProgram *p3; // header Program object
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd16.cl"); // header source file
    auto pSourceBuffer = loadDataFromFile(testFile.c_str(), sourceSize);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSourceBuffer);
    const char *sources[1] = {pSourceBuffer.get()};
    p3 = Program::create<MockProgram>(pContext, 1, sources, &sourceSize, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, p3);
    inputHeaders = p3;
    retVal = pProgram->compile(pProgram->getDevices(), nullptr, 1, &inputHeaders, &headerIncludeNames);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // fail compilation of kernel with header - header is invalid
    p3->sourceCode = ""; // set header source code as non-existent (invalid)
    retVal = p3->compile(p3->getDevices(), nullptr, 1, &inputHeaders, &headerIncludeNames);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
    delete p3;

    // fail compilation - CompilerInterface cannot be obtained
    auto device = pContext->getDevice(0);
    auto executionEnvironment = device->getExecutionEnvironment();
    std::unique_ptr<RootDeviceEnvironment> rootDeviceEnvironment = std::make_unique<NoCompilerInterfaceRootDeviceEnvironment>(*executionEnvironment);
    std::swap(rootDeviceEnvironment, executionEnvironment->rootDeviceEnvironments[device->getRootDeviceIndex()]);
    auto p2 = std::make_unique<MockProgram>(toClDeviceVector(*device));
    retVal = p2->compile(p2->getDevices(), nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
    p2.reset(nullptr);
    std::swap(rootDeviceEnvironment, executionEnvironment->rootDeviceEnvironments[device->getRootDeviceIndex()]);

    // fail compilation - any compilation error (here caused by specifying unrecognized option)
    retVal = pProgram->compile(pProgram->getDevices(), "-invalid-option", 0, nullptr, nullptr);
    EXPECT_EQ(CL_COMPILE_PROGRAM_FAILURE, retVal);

    // compile successfully
    retVal = pProgram->compile(pProgram->getDevices(), nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ProgramFromSourceTest, GivenFlagsWhenCompilingProgramThenBuildOptionsHaveBeenApplied) {
    auto cip = new MockCompilerInterfaceCaptureBuildOptions();
    auto pDevice = pContext->getDevice(0);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);
    auto program = std::make_unique<SucceedingGenBinaryProgram>(toClDeviceVector(*pDevice));
    program->sourceCode = "__kernel mock() {}";

    // Ask to build created program without NEO::CompilerOptions::gtpinRera and NEO::CompilerOptions::greaterThan4gbBuffersRequired flags.
    cl_int retVal = program->compile(pProgram->getDevices(), CompilerOptions::fastRelaxedMath.data(), 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Check build options that were applied
    EXPECT_TRUE(CompilerOptions::contains(cip->buildOptions, CompilerOptions::fastRelaxedMath)) << cip->buildOptions;
    EXPECT_FALSE(CompilerOptions::contains(cip->buildInternalOptions, CompilerOptions::gtpinRera)) << cip->buildInternalOptions;

    const auto &compilerHwInfoConfig = *CompilerHwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    if (!compilerHwInfoConfig.isForceToStatelessRequired()) {
        EXPECT_FALSE(CompilerOptions::contains(cip->buildInternalOptions, CompilerOptions::greaterThan4gbBuffersRequired)) << cip->buildInternalOptions;
    }
    EXPECT_TRUE(CompilerOptions::contains(cip->buildInternalOptions, pPlatform->getClDevice(0)->peekCompilerExtensions())) << cip->buildInternalOptions;

    // Ask to build created program with NEO::CompilerOptions::gtpinRera and NEO::CompilerOptions::greaterThan4gbBuffersRequired flags.
    cip->buildOptions.clear();
    cip->buildInternalOptions.clear();
    auto options = CompilerOptions::concatenate(CompilerOptions::greaterThan4gbBuffersRequired, CompilerOptions::gtpinRera, CompilerOptions::finiteMathOnly);
    retVal = program->compile(pProgram->getDevices(), options.c_str(),
                              0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Check build options that were applied
    EXPECT_FALSE(CompilerOptions::contains(cip->buildOptions, CompilerOptions::fastRelaxedMath)) << cip->buildOptions;
    EXPECT_TRUE(CompilerOptions::contains(cip->buildOptions, CompilerOptions::finiteMathOnly)) << cip->buildOptions;
    EXPECT_TRUE(CompilerOptions::contains(cip->buildInternalOptions, CompilerOptions::gtpinRera)) << cip->buildInternalOptions;
    EXPECT_TRUE(CompilerOptions::contains(cip->buildInternalOptions, CompilerOptions::greaterThan4gbBuffersRequired)) << cip->buildInternalOptions;
    EXPECT_TRUE(CompilerOptions::contains(cip->buildInternalOptions, pPlatform->getClDevice(0)->peekCompilerExtensions())) << cip->buildInternalOptions;
}

TEST_F(ProgramTests, GivenFlagsWhenLinkingProgramThenBuildOptionsHaveBeenApplied) {
    auto cip = new MockCompilerInterfaceCaptureBuildOptions();
    auto pProgram = std::make_unique<SucceedingGenBinaryProgram>(toClDeviceVector(*pClDevice));
    pProgram->sourceCode = "__kernel mock() {}";
    pProgram->createdFrom = Program::CreatedFrom::SOURCE;
    MockProgram::getInternalOptionsCalled = 0;

    cl_program program = pProgram.get();

    // compile successfully a kernel to be linked later
    cl_int retVal = pProgram->compile(pProgram->getDevices(), nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1, MockProgram::getInternalOptionsCalled);

    // Ask to link created program with NEO::CompilerOptions::gtpinRera and NEO::CompilerOptions::greaterThan4gbBuffersRequired flags.
    auto options = CompilerOptions::concatenate(CompilerOptions::greaterThan4gbBuffersRequired, CompilerOptions::gtpinRera, CompilerOptions::finiteMathOnly);

    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);

    retVal = pProgram->link(pProgram->getDevices(), options.c_str(), 1, &program);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2, MockProgram::getInternalOptionsCalled);

    // Check build options that were applied
    EXPECT_FALSE(CompilerOptions::contains(cip->buildOptions, CompilerOptions::fastRelaxedMath)) << cip->buildOptions;
    EXPECT_TRUE(CompilerOptions::contains(cip->buildOptions, CompilerOptions::finiteMathOnly)) << cip->buildOptions;
    EXPECT_TRUE(CompilerOptions::contains(cip->buildInternalOptions, CompilerOptions::gtpinRera)) << cip->buildInternalOptions;
    EXPECT_TRUE(CompilerOptions::contains(cip->buildInternalOptions, CompilerOptions::greaterThan4gbBuffersRequired)) << cip->buildInternalOptions;
}

TEST_F(ProgramFromSourceTest, GivenAdvancedOptionsWhenCreatingProgramThenSuccessIsReturned) {
    std::string testFile;
    size_t sourceSize = 0;

    Program *p;
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd16.cl");
    auto pSourceBuffer = loadDataFromFile(testFile.c_str(), sourceSize);
    const char *sources[1] = {pSourceBuffer.get()};
    EXPECT_NE(nullptr, pSourceBuffer);

    // According to spec: If lengths is NULL, all strings in the strings argument are considered null-terminated.
    p = Program::create(pContext, 1, sources, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, p);
    delete p;

    // According to spec: If an element in lengths is zero, its accompanying string is null-terminated.
    p = Program::create(pContext, 1, sources, &sourceSize, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, p);
    delete p;

    std::stringstream dataStream(pSourceBuffer.get());
    std::string line;
    std::vector<const char *> lines;
    while (std::getline(dataStream, line, '\n')) {
        char *ptr = new char[line.length() + 1]();
        strcpy_s(ptr, line.length() + 1, line.c_str());
        lines.push_back(ptr);
    }

    // Work on array of strings
    p = Program::create(pContext, 1, &lines[0], nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, p);
    delete p;

    std::vector<size_t> sizes;
    for (auto ptr : lines)
        sizes.push_back(strlen(ptr));
    sizes[sizes.size() / 2] = 0;

    p = Program::create(pContext, (cl_uint)sizes.size(), &lines[0], &sizes[0], retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, p);
    delete p;

    for (auto ptr : lines)
        delete[] ptr;
}

TEST_F(ProgramFromSourceTest, GivenSpecificParamatersWhenLinkingProgramThenSuccessOrCorrectErrorCodeIsReturned) {
    createProgramWithSource(
        pContext,
        sourceFileName);

    cl_program program = pProgram;
    cl_program nullprogram = nullptr;
    cl_program invprogram = (cl_program)pContext;

    // Order of following microtests is important - do not change.
    // Add new microtests at end.

    // invalid link parameters: combinations of numInputPrograms & inputPrograms
    retVal = pProgram->link(pProgram->getDevices(), nullptr, 0, &program);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = pProgram->link(pProgram->getDevices(), nullptr, 1, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    // fail linking - another linking is already in progress
    pProgram->setBuildStatus(CL_BUILD_IN_PROGRESS);
    retVal = pProgram->link(pProgram->getDevices(), nullptr, 1, &program);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    pProgram->setBuildStatus(CL_BUILD_NONE);

    // invalid link parameters: invalid Program object==nullptr
    retVal = pProgram->link(pProgram->getDevices(), nullptr, 1, &nullprogram);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);

    // invalid link parameters: invalid Program object==non Program object
    retVal = pProgram->link(pProgram->getDevices(), nullptr, 1, &invprogram);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);

    // compile successfully a kernel to be linked later
    retVal = pProgram->compile(pProgram->getDevices(), nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // fail linking - code to be linked does not exist
    bool isSpirvTmp = pProgram->getIsSpirV();
    char *pIrBin = pProgram->irBinary.get();
    pProgram->irBinary.release();
    size_t irBinSize = pProgram->irBinarySize;
    pProgram->setIrBinary(nullptr, false);
    retVal = pProgram->link(pProgram->getDevices(), nullptr, 1, &program);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
    pProgram->setIrBinary(pIrBin, isSpirvTmp);

    // fail linking - size of code to be linked is == 0
    pProgram->setIrBinarySize(0, isSpirvTmp);
    retVal = pProgram->link(pProgram->getDevices(), nullptr, 1, &program);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
    pProgram->setIrBinarySize(irBinSize, isSpirvTmp);

    // fail linking - any link error (here caused by specifying unrecognized option)
    retVal = pProgram->link(pProgram->getDevices(), "-invalid-option", 1, &program);
    EXPECT_EQ(CL_LINK_PROGRAM_FAILURE, retVal);

    // fail linking - linked code is corrupted and cannot be postprocessed
    auto p2 = std::make_unique<FailingGenBinaryProgram>(pProgram->getDevices());
    retVal = p2->link(p2->getDevices(), nullptr, 1, &program);
    EXPECT_EQ(CL_INVALID_BINARY, retVal);
    p2.reset(nullptr);

    // link successfully
    retVal = pProgram->link(pProgram->getDevices(), nullptr, 1, &program);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ProgramFromSourceTest, GivenInvalidOptionsWhenCreatingLibraryThenCorrectErrorIsReturned) {
    cl_program program = pProgram;

    // Order of following microtests is important - do not change.
    // Add new microtests at end.

    // compile successfully a kernel to be later used to create library
    retVal = pProgram->compile(pProgram->getDevices(), nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // create library successfully
    retVal = pProgram->link(pProgram->getDevices(), CompilerOptions::createLibrary.data(), 1, &program);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // fail library creation - any link error (here caused by specifying unrecognized option)
    retVal = pProgram->link(pProgram->getDevices(), CompilerOptions::concatenate(CompilerOptions::createLibrary, "-invalid-option").c_str(), 1, &program);
    EXPECT_EQ(CL_LINK_PROGRAM_FAILURE, retVal);

    auto device = pContext->getDevice(0);
    auto executionEnvironment = device->getExecutionEnvironment();
    std::unique_ptr<RootDeviceEnvironment> rootDeviceEnvironment = std::make_unique<NoCompilerInterfaceRootDeviceEnvironment>(*executionEnvironment);
    std::swap(rootDeviceEnvironment, executionEnvironment->rootDeviceEnvironments[device->getRootDeviceIndex()]);
    auto failingProgram = std::make_unique<MockProgram>(toClDeviceVector(*device));

    // fail library creation - CompilerInterface cannot be obtained
    retVal = failingProgram->link(failingProgram->getDevices(), CompilerOptions::createLibrary.data(), 1, &program);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
    std::swap(rootDeviceEnvironment, executionEnvironment->rootDeviceEnvironments[device->getRootDeviceIndex()]);
}

class PatchTokenFromBinaryTest : public ProgramSimpleFixture {
  public:
    void setUp() {
        ProgramSimpleFixture::setUp();
    }
    void tearDown() {
        ProgramSimpleFixture::tearDown();
    }
};
using PatchTokenTests = Test<PatchTokenFromBinaryTest>;

template <typename FamilyType>
class CommandStreamReceiverMock : public UltCommandStreamReceiver<FamilyType> {
    using BaseClass = UltCommandStreamReceiver<FamilyType>;
    using BaseClass::BaseClass;

  public:
    void makeResident(GraphicsAllocation &graphicsAllocation) override {
        residency[graphicsAllocation.getUnderlyingBuffer()] = graphicsAllocation.getUnderlyingBufferSize();
        CommandStreamReceiver::makeResident(graphicsAllocation);
    }

    void makeNonResident(GraphicsAllocation &graphicsAllocation) override {
        residency.erase(graphicsAllocation.getUnderlyingBuffer());
        CommandStreamReceiver::makeNonResident(graphicsAllocation);
    }

    std::map<const void *, size_t> residency;
};

TEST_F(PatchTokenTests, WhenBuildingProgramThenGwsIsSet) {
    createProgramFromBinary(pContext, pContext->getDevices(), "kernel_data_param");

    ASSERT_NE(nullptr, pProgram);
    retVal = pProgram->build(
        pProgram->getDevices(),
        nullptr,
        false);

    ASSERT_EQ(CL_SUCCESS, retVal);

    auto pKernelInfo = pProgram->getKernelInfo("test", rootDeviceIndex);

    ASSERT_NE(static_cast<uint32_t>(-1), pKernelInfo->kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[0]);
    ASSERT_NE(static_cast<uint32_t>(-1), pKernelInfo->kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[1]);
    ASSERT_NE(static_cast<uint32_t>(-1), pKernelInfo->kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[2]);
}

TEST_F(PatchTokenTests, WhenBuildingProgramThenConstantKernelArgsAreAvailable) {
    // PATCH_TOKEN_STATELESS_CONSTANT_MEMORY_OBJECT_KERNEL_ARGUMENT

    createProgramFromBinary(pContext, pContext->getDevices(), "test_basic_constant");

    ASSERT_NE(nullptr, pProgram);
    retVal = pProgram->build(
        pProgram->getDevices(),
        nullptr,
        false);

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pKernelInfo = pProgram->getKernelInfo("constant_kernel", rootDeviceIndex);
    ASSERT_NE(nullptr, pKernelInfo);

    auto pKernel = Kernel::create(
        pProgram,
        *pKernelInfo,
        *pClDevice,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, pKernel);

    uint32_t numArgs;
    retVal = pKernel->getInfo(CL_KERNEL_NUM_ARGS, sizeof(numArgs), &numArgs, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(3u, numArgs);

    uint32_t sizeOfPtr = sizeof(void *);
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(0).as<ArgDescPointer>().pointerSize, sizeOfPtr);
    EXPECT_EQ(pKernelInfo->getArgDescriptorAt(1).as<ArgDescPointer>().pointerSize, sizeOfPtr);

    delete pKernel;
}

TEST_F(PatchTokenTests, GivenVmeKernelWhenBuildingKernelThenArgAvailable) {
    if (!pDevice->getHardwareInfo().capabilityTable.supportsVme) {
        GTEST_SKIP();
    }
    // PATCH_TOKEN_INLINE_VME_SAMPLER_INFO token indicates a VME kernel.

    createProgramFromBinary(pContext, pContext->getDevices(), "vme_kernels");

    ASSERT_NE(nullptr, pProgram);
    retVal = pProgram->build(
        pProgram->getDevices(),
        nullptr,
        false);

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pKernelInfo = pProgram->getKernelInfo("device_side_block_motion_estimate_intel", rootDeviceIndex);
    ASSERT_NE(nullptr, pKernelInfo);
    EXPECT_EQ(true, pKernelInfo->kernelDescriptor.kernelAttributes.flags.usesVme);

    auto pKernel = Kernel::create(
        pProgram,
        *pKernelInfo,
        *pClDevice,
        &retVal);

    ASSERT_NE(nullptr, pKernel);

    delete pKernel;
}

class ProgramPatchTokenFromBinaryTest : public ProgramSimpleFixture {
  public:
    void setUp() {
        ProgramSimpleFixture::setUp();
    }
    void tearDown() {
        ProgramSimpleFixture::tearDown();
    }
};
typedef Test<ProgramPatchTokenFromBinaryTest> ProgramPatchTokenTests;

TEST(ProgramFromBinaryTests, givenBinaryWithInvalidICBEThenErrorIsReturned) {
    cl_int retVal = CL_INVALID_BINARY;

    SProgramBinaryHeader binHeader;
    memset(&binHeader, 0, sizeof(binHeader));
    binHeader.Magic = iOpenCL::MAGIC_CL;
    binHeader.Version = iOpenCL::CURRENT_ICBE_VERSION - 3;
    binHeader.Device = defaultHwInfo->platform.eRenderCoreFamily;
    binHeader.GPUPointerSizeInBytes = 8;
    binHeader.NumberOfKernels = 0;
    binHeader.SteppingId = 0;
    binHeader.PatchListSize = 0;
    size_t binSize = sizeof(SProgramBinaryHeader);

    {
        const unsigned char *binaries[1] = {reinterpret_cast<const unsigned char *>(&binHeader)};
        MockContext context;

        std::unique_ptr<Program> pProgram(Program::create<Program>(&context, context.getDevices(), &binSize, binaries, nullptr, retVal));
        EXPECT_EQ(nullptr, pProgram.get());
        EXPECT_EQ(CL_INVALID_BINARY, retVal);
    }

    {
        // whatever method we choose CL_INVALID_BINARY is always returned
        auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr, mockRootDeviceIndex));
        std::unique_ptr<Program> pProgram(Program::createBuiltInFromGenBinary(nullptr, toClDeviceVector(*device), &binHeader, binSize, &retVal));
        ASSERT_NE(nullptr, pProgram.get());
        EXPECT_EQ(CL_SUCCESS, retVal);

        retVal = pProgram->processGenBinary(*device);
        EXPECT_EQ(CL_INVALID_BINARY, retVal);
    }
}

TEST(ProgramFromBinaryTests, givenBinaryWithInvalidICBEAndDisableKernelRecompilationThenErrorIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.DisableKernelRecompilation.set(true);
    cl_int retVal = CL_INVALID_BINARY;

    SProgramBinaryHeader binHeader;
    memset(&binHeader, 0, sizeof(binHeader));
    binHeader.Magic = iOpenCL::MAGIC_CL;
    binHeader.Version = iOpenCL::CURRENT_ICBE_VERSION - 3;
    binHeader.Device = defaultHwInfo->platform.eRenderCoreFamily;
    binHeader.GPUPointerSizeInBytes = 8;
    binHeader.NumberOfKernels = 0;
    binHeader.SteppingId = 0;
    binHeader.PatchListSize = 0;
    size_t binSize = sizeof(SProgramBinaryHeader);

    {
        const unsigned char *binaries[1] = {reinterpret_cast<const unsigned char *>(&binHeader)};
        MockContext context;

        std::unique_ptr<Program> pProgram(Program::create<Program>(&context, context.getDevices(), &binSize, binaries, nullptr, retVal));
        EXPECT_EQ(nullptr, pProgram.get());
        EXPECT_EQ(CL_INVALID_BINARY, retVal);
    }
}

TEST(ProgramFromBinaryTests, givenEmptyProgramThenErrorIsReturned) {
    cl_int retVal = CL_INVALID_BINARY;

    SProgramBinaryHeader binHeader;
    memset(&binHeader, 0, sizeof(binHeader));
    binHeader.Magic = iOpenCL::MAGIC_CL;
    binHeader.Version = iOpenCL::CURRENT_ICBE_VERSION;
    binHeader.Device = defaultHwInfo->platform.eRenderCoreFamily;
    binHeader.GPUPointerSizeInBytes = 8;
    binHeader.NumberOfKernels = 0;
    binHeader.SteppingId = 0;
    binHeader.PatchListSize = 0;
    size_t binSize = sizeof(SProgramBinaryHeader);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr, mockRootDeviceIndex));
    std::unique_ptr<MockProgram> pProgram(MockProgram::createBuiltInFromGenBinary<MockProgram>(nullptr, toClDeviceVector(*device), &binHeader, binSize, &retVal));
    ASSERT_NE(nullptr, pProgram.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto rootDeviceIndex = mockRootDeviceIndex;
    pProgram->buildInfos[rootDeviceIndex].unpackedDeviceBinary.reset(nullptr);
    retVal = pProgram->processGenBinary(*device);
    EXPECT_EQ(CL_INVALID_BINARY, retVal);
}

using ProgramWithDebugDataTests = Test<ProgramSimpleFixture>;

TEST_F(ProgramWithDebugDataTests, GivenPatchtokensProgramWithDebugSymbolsWhenPackDeviceBinaryThenDebugDataIsAddedToSingleDeviceBinary) {
    auto clDevice = pContext->getDevices()[0];
    auto &hwInfo = clDevice->getHardwareInfo();
    auto rootDeviceIdx = clDevice->getRootDeviceIndex();

    pProgram = new PatchtokensProgramWithDebugData(*clDevice);
    auto &buildInfo = pProgram->buildInfos[rootDeviceIdx];

    pProgram->packDeviceBinary(*clDevice);
    EXPECT_NE(nullptr, buildInfo.packedDeviceBinary.get());

    auto packedDeviceBinary = ArrayRef<const uint8_t>::fromAny(buildInfo.packedDeviceBinary.get(), buildInfo.packedDeviceBinarySize);
    TargetDevice targetDevice = NEO::targetDeviceFromHwInfo(hwInfo);
    std::string decodeErrors;
    std::string decodeWarnings;
    auto singleDeviceBinary = unpackSingleDeviceBinary(packedDeviceBinary, {}, targetDevice,
                                                       decodeErrors, decodeWarnings);
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
    EXPECT_TRUE(decodeErrors.empty()) << decodeErrors;
    EXPECT_FALSE(singleDeviceBinary.debugData.empty());
    EXPECT_NE(nullptr, pProgram->getDebugData(rootDeviceIdx));
    EXPECT_NE(0u, pProgram->getDebugDataSize(rootDeviceIdx));
}

TEST_F(ProgramTests, WhenProgramIsCreatedThenCorrectOclVersionIsInOptions) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DisableStatelessToStatefulOptimization.set(false);

    MockProgram program(pContext, false, toClDeviceVector(*pClDevice));
    auto internalOptions = program.getInternalOptions();
    if (pClDevice->getEnabledClVersion() == 30) {
        EXPECT_TRUE(CompilerOptions::contains(internalOptions, "-ocl-version=300")) << internalOptions;
    } else if (pClDevice->getEnabledClVersion() == 21) {
        EXPECT_TRUE(CompilerOptions::contains(internalOptions, "-ocl-version=210")) << internalOptions;
    } else {
        EXPECT_TRUE(CompilerOptions::contains(internalOptions, "-ocl-version=120")) << internalOptions;
    }
}

TEST_F(ProgramTests, GivenForcedClVersionWhenProgramIsCreatedThenCorrectOclOptionIsPresent) {
    std::pair<unsigned int, std::string> testedValues[] = {
        {0, "-ocl-version=120"},
        {12, "-ocl-version=120"},
        {21, "-ocl-version=210"},
        {30, "-ocl-version=300"}};

    for (auto &testedValue : testedValues) {
        pClDevice->enabledClVersion = testedValue.first;
        MockProgram program{pContext, false, toClDeviceVector(*pClDevice)};
        auto internalOptions = program.getInternalOptions();
        EXPECT_TRUE(CompilerOptions::contains(internalOptions, testedValue.second));
    }
}

TEST_F(ProgramTests, GivenStatelessToStatefulIsDisabledWhenProgramIsCreatedThenGreaterThan4gbBuffersRequiredOptionIsSet) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DisableStatelessToStatefulOptimization.set(true);

    MockProgram program(pContext, false, toClDeviceVector(*pClDevice));
    auto internalOptions = program.getInternalOptions();
    EXPECT_TRUE(CompilerOptions::contains(internalOptions, NEO::CompilerOptions::greaterThan4gbBuffersRequired));
}

TEST_F(ProgramTests, whenGetInternalOptionsThenLSCPolicyIsSet) {
    MockProgram program(pContext, false, toClDeviceVector(*pClDevice));
    auto internalOptions = program.getInternalOptions();
    const auto &compilerHwInfoConfig = *CompilerHwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    auto expectedPolicy = compilerHwInfoConfig.getCachingPolicyOptions(false);
    if (expectedPolicy != nullptr) {
        EXPECT_TRUE(CompilerOptions::contains(internalOptions, expectedPolicy));
    } else {
        EXPECT_FALSE(CompilerOptions::contains(internalOptions, "-cl-store-cache-default"));
        EXPECT_FALSE(CompilerOptions::contains(internalOptions, "-cl-load-cache-default"));
    }
}

HWTEST2_F(ProgramTests, givenDebugFlagSetToWbWhenGetInternalOptionsThenCorrectBuildOptionIsSet, IsAtLeastXeHpgCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(2);
    MockProgram program(pContext, false, toClDeviceVector(*pClDevice));
    auto internalOptions = program.getInternalOptions();
    EXPECT_TRUE(CompilerOptions::contains(internalOptions, "-cl-store-cache-default=7 -cl-load-cache-default=4"));
}

HWTEST2_F(ProgramTests, givenDebugFlagSetForceAllResourcesUncachedWhenGetInternalOptionsThenCorrectBuildOptionIsSet, IsAtLeastXeHpgCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(2);
    DebugManager.flags.ForceAllResourcesUncached.set(true);
    MockProgram program(pContext, false, toClDeviceVector(*pClDevice));
    auto internalOptions = program.getInternalOptions();
    EXPECT_TRUE(CompilerOptions::contains(internalOptions, "-cl-store-cache-default=1 -cl-load-cache-default=1"));
}

HWTEST2_F(ProgramTests, givenAtLeastXeHpgCoreWhenGetInternalOptionsThenCorrectBuildOptionIsSet, IsAtLeastXeHpgCore) {
    MockProgram program(pContext, false, toClDeviceVector(*pClDevice));
    auto internalOptions = program.getInternalOptions();
    EXPECT_TRUE(CompilerOptions::contains(internalOptions, "-cl-store-cache-default=2 -cl-load-cache-default=4"));
}

TEST_F(ProgramTests, WhenCreatingProgramThenBindlessIsEnabledOnlyIfDebugFlagIsEnabled) {
    using namespace testing;
    DebugManagerStateRestore restorer;

    {

        DebugManager.flags.UseBindlessMode.set(0);
        MockProgram programNoBindless(pContext, false, toClDeviceVector(*pClDevice));
        auto internalOptionsNoBindless = programNoBindless.getInternalOptions();
        EXPECT_FALSE(CompilerOptions::contains(internalOptionsNoBindless, CompilerOptions::bindlessMode)) << internalOptionsNoBindless;
    }
    {

        DebugManager.flags.UseBindlessMode.set(1);
        MockProgram programBindless(pContext, false, toClDeviceVector(*pClDevice));
        auto internalOptionsBindless = programBindless.getInternalOptions();
        EXPECT_TRUE(CompilerOptions::contains(internalOptionsBindless, CompilerOptions::bindlessMode)) << internalOptionsBindless;
    }
}

TEST_F(ProgramTests, GivenForce32BitAddressessWhenProgramIsCreatedThenGreaterThan4gbBuffersRequiredIsCorrectlySet) {
    DebugManagerStateRestore dbgRestorer;
    cl_int retVal = CL_DEVICE_NOT_FOUND;
    DebugManager.flags.DisableStatelessToStatefulOptimization.set(false);
    if (pDevice) {
        const_cast<DeviceInfo *>(&pDevice->getDeviceInfo())->force32BitAddressess = true;
        MockProgram program(pContext, false, toClDeviceVector(*pClDevice));
        auto internalOptions = program.getInternalOptions();
        const auto &compilerHwInfoConfig = *CompilerHwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
        if (compilerHwInfoConfig.isForceToStatelessRequired()) {
            EXPECT_TRUE(CompilerOptions::contains(internalOptions, CompilerOptions::greaterThan4gbBuffersRequired)) << internalOptions;
        } else {
            EXPECT_FALSE(CompilerOptions::contains(internalOptions, NEO::CompilerOptions::greaterThan4gbBuffersRequired)) << internalOptions;
        }
    } else {
        EXPECT_NE(CL_DEVICE_NOT_FOUND, retVal);
    }
}

TEST_F(ProgramTests, Given32bitSupportWhenProgramIsCreatedThenGreaterThan4gbBuffersRequiredIsCorrectlySet) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.DisableStatelessToStatefulOptimization.set(false);
    std::unique_ptr<MockProgram> program{Program::createBuiltInFromSource<MockProgram>("", pContext, pContext->getDevices(), nullptr)};
    auto internalOptions = program->getInternalOptions();
    const auto &compilerHwInfoConfig = *CompilerHwInfoConfig::get(defaultHwInfo->platform.eProductFamily);

    if (compilerHwInfoConfig.isForceToStatelessRequired() || is32bit) {
        EXPECT_TRUE(CompilerOptions::contains(internalOptions, NEO::CompilerOptions::greaterThan4gbBuffersRequired)) << internalOptions;
    } else {
        EXPECT_FALSE(CompilerOptions::contains(internalOptions, NEO::CompilerOptions::greaterThan4gbBuffersRequired)) << internalOptions;
    }
}

TEST_F(ProgramTests, GivenStatelessToStatefulIsDisabledWhenProgramIsCreatedThenGreaterThan4gbBuffersRequiredIsCorrectlySet) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.DisableStatelessToStatefulOptimization.set(true);
    std::unique_ptr<MockProgram> program{Program::createBuiltInFromSource<MockProgram>("", pContext, pContext->getDevices(), nullptr)};
    auto internalOptions = program->getInternalOptions();
    EXPECT_TRUE(CompilerOptions::contains(internalOptions, NEO::CompilerOptions::greaterThan4gbBuffersRequired)) << internalOptions;
}

TEST_F(ProgramTests, givenProgramWhenItIsCompiledThenItAlwaysHavePreserveVec3TypeInternalOptionSet) {
    std::unique_ptr<MockProgram> program(Program::createBuiltInFromSource<MockProgram>("", pContext, pContext->getDevices(), nullptr));
    auto internalOptions = program->getInternalOptions();
    EXPECT_TRUE(CompilerOptions::contains(internalOptions, CompilerOptions::preserveVec3Type)) << internalOptions;
}

TEST_F(ProgramTests, Force32BitAddressessWhenProgramIsCreatedThenGreaterThan4gbBuffersRequiredIsCorrectlySet) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.DisableStatelessToStatefulOptimization.set(false);
    const_cast<DeviceInfo *>(&pDevice->getDeviceInfo())->force32BitAddressess = true;
    std::unique_ptr<MockProgram> program{Program::createBuiltInFromSource<MockProgram>("", pContext, pContext->getDevices(), nullptr)};
    auto internalOptions = program->getInternalOptions();
    const auto &compilerHwInfoConfig = *CompilerHwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    if (is32bit || compilerHwInfoConfig.isForceToStatelessRequired()) {
        EXPECT_TRUE(CompilerOptions::contains(internalOptions, CompilerOptions::greaterThan4gbBuffersRequired)) << internalOptions;
    } else {
        EXPECT_FALSE(CompilerOptions::contains(internalOptions, NEO::CompilerOptions::greaterThan4gbBuffersRequired)) << internalOptions;
    }
}

TEST_F(ProgramTests, whenContainsStatefulAccessIsCalledThenReturnCorrectResult) {
    std::vector<std::tuple<bool, SurfaceStateHeapOffset, CrossThreadDataOffset>> testParams = {
        {false, undefined<SurfaceStateHeapOffset>, undefined<CrossThreadDataOffset>},
        {true, 0x40, undefined<CrossThreadDataOffset>},
        {true, undefined<SurfaceStateHeapOffset>, 0x40},
        {true, 0x40, 0x40},

    };

    for (auto &[expectedResult, surfaceStateHeapOffset, crossThreadDataOffset] : testParams) {
        MockProgram program(pContext, false, toClDeviceVector(*pClDevice));
        auto kernelInfo = std::make_unique<KernelInfo>();
        kernelInfo->kernelDescriptor.payloadMappings.explicitArgs.clear();
        auto argDescriptor = ArgDescriptor(ArgDescriptor::ArgTPointer);
        argDescriptor.as<ArgDescPointer>().bindful = surfaceStateHeapOffset;
        argDescriptor.as<ArgDescPointer>().bindless = crossThreadDataOffset;

        kernelInfo->kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor);
        program.addKernelInfo(kernelInfo.release(), 0);

        EXPECT_EQ(expectedResult, AddressingModeHelper::containsStatefulAccess(program.buildInfos[0].kernelInfoArray));
    }
}

TEST_F(ProgramTests, givenStatefulAndStatelessAccessesWhenProgramBuildIsCalledThenCorrectResultIsReturned) {
    DebugManagerStateRestore restorer;
    const auto &compilerHwInfoConfig = *CompilerHwInfoConfig::get(pClDevice->getHardwareInfo().platform.eProductFamily);

    class MyMockProgram : public Program {
      public:
        using Program::buildInfos;
        using Program::createdFrom;
        using Program::irBinary;
        using Program::irBinarySize;
        using Program::isBuiltIn;
        using Program::options;
        using Program::Program;
        using Program::sourceCode;

        void setAddressingMode(bool isStateful) {
            auto kernelInfo = std::make_unique<KernelInfo>();
            kernelInfo->kernelDescriptor.payloadMappings.explicitArgs.clear();
            auto argDescriptor = ArgDescriptor(ArgDescriptor::ArgTPointer);
            if (isStateful) {
                argDescriptor.as<ArgDescPointer>().bindful = 0x40;
                argDescriptor.as<ArgDescPointer>().bindless = 0x40;
            } else {
                argDescriptor.as<ArgDescPointer>().bindful = undefined<SurfaceStateHeapOffset>;
                argDescriptor.as<ArgDescPointer>().bindless = undefined<CrossThreadDataOffset>;
            }

            kernelInfo->kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor);
            this->buildInfos[0].kernelInfoArray.clear();
            this->buildInfos[0].kernelInfoArray.push_back(kernelInfo.release());
        }

        cl_int processGenBinary(const ClDevice &clDevice) override {
            return CL_SUCCESS;
        }
    };

    std::array<std::tuple<int, bool, int32_t>, 3> testParams = {{{CL_SUCCESS, false, -1},
                                                                 {CL_SUCCESS, true, 0},
                                                                 {CL_BUILD_PROGRAM_FAILURE, true, 1}}};

    for (auto &[result, isStatefulAccess, debuyKey] : testParams) {

        if (!compilerHwInfoConfig.isForceToStatelessRequired()) {
            result = CL_SUCCESS;
        }
        MyMockProgram program(pContext, false, toClDeviceVector(*pClDevice));
        program.isBuiltIn = false;
        program.sourceCode = "test_kernel";
        program.createdFrom = Program::CreatedFrom::SOURCE;
        program.setAddressingMode(isStatefulAccess);
        DebugManager.flags.FailBuildProgramWithStatefulAccess.set(debuyKey);
        EXPECT_EQ(result, program.build(toClDeviceVector(*pClDevice), nullptr, false));
    }

    {
        MyMockProgram programWithBuiltIn(pContext, true, toClDeviceVector(*pClDevice));
        programWithBuiltIn.isBuiltIn = true;
        programWithBuiltIn.irBinary.reset(new char[16]);
        programWithBuiltIn.irBinarySize = 16;
        programWithBuiltIn.setAddressingMode(true);
        DebugManager.flags.FailBuildProgramWithStatefulAccess.set(1);
        EXPECT_EQ(CL_SUCCESS, programWithBuiltIn.build(toClDeviceVector(*pClDevice), nullptr, false));
    }
}

TEST_F(ProgramTests, GivenStatelessToStatefulBufferOffsetOptimizationWhenProgramIsCreatedThenBufferOffsetArgIsSet) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.set(1);
    cl_int errorCode = CL_SUCCESS;
    const char programSource[] = "program";
    const char *programPointer = programSource;
    const char **programSources = reinterpret_cast<const char **>(&programPointer);
    size_t length = sizeof(programSource);
    std::unique_ptr<MockProgram> program(Program::create<MockProgram>(pContext, 1u, programSources, &length, errorCode));
    auto internalOptions = program->getInternalOptions();

    EXPECT_TRUE(CompilerOptions::contains(internalOptions, CompilerOptions::hasBufferOffsetArg)) << internalOptions;
}

TEST_F(ProgramTests, givenStatelessToStatefulOptimizationOffWHenProgramIsCreatedThenOptimizationStringIsNotPresent) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.set(0);
    cl_int errorCode = CL_SUCCESS;
    const char programSource[] = "program";
    const char *programPointer = programSource;
    const char **programSources = reinterpret_cast<const char **>(&programPointer);
    size_t length = sizeof(programSource);
    std::unique_ptr<MockProgram> program(Program::create<MockProgram>(pContext, 1u, programSources, &length, errorCode));
    auto internalOptions = program->getInternalOptions();
    EXPECT_FALSE(CompilerOptions::contains(internalOptions, CompilerOptions::hasBufferOffsetArg)) << internalOptions;
}

TEST_F(ProgramTests, GivenContextWhenCreateProgramThenIncrementContextRefCount) {
    auto initialApiRefCount = pContext->getReference();
    auto initialInternalRefCount = pContext->getRefInternalCount();

    MockProgram *program = new MockProgram(pContext, false, pContext->getDevices());

    EXPECT_EQ(pContext->getReference(), initialApiRefCount);
    EXPECT_EQ(pContext->getRefInternalCount(), initialInternalRefCount + 1);
    program->release();
    EXPECT_EQ(pContext->getReference(), initialApiRefCount);
    EXPECT_EQ(pContext->getRefInternalCount(), initialInternalRefCount);
}

TEST_F(ProgramTests, GivenContextWhenCreateProgramFromSourceThenIncrementContextRefCount) {
    auto initialApiRefCount = pContext->getReference();
    auto initialInternalRefCount = pContext->getRefInternalCount();

    auto tempProgram = new Program(nullptr, false, pContext->getDevices());
    EXPECT_FALSE(tempProgram->getIsBuiltIn());
    auto program = new Program(pContext, false, pContext->getDevices());
    EXPECT_FALSE(program->getIsBuiltIn());

    EXPECT_EQ(pContext->getReference(), initialApiRefCount);
    EXPECT_EQ(pContext->getRefInternalCount(), initialInternalRefCount + 1);
    program->release();
    EXPECT_EQ(pContext->getReference(), initialApiRefCount);
    EXPECT_EQ(pContext->getRefInternalCount(), initialInternalRefCount);
    tempProgram->release();
    EXPECT_EQ(pContext->getReference(), initialApiRefCount);
    EXPECT_EQ(pContext->getRefInternalCount(), initialInternalRefCount);
}

TEST_F(ProgramTests, GivenContextWhenCreateBuiltInProgramFromSourceThenDontIncrementContextRefCount) {
    auto initialApiRefCount = pContext->getReference();
    auto initialInternalRefCount = pContext->getRefInternalCount();

    auto tempProgram = new Program(nullptr, true, pContext->getDevices());
    EXPECT_TRUE(tempProgram->getIsBuiltIn());
    auto program = new Program(pContext, true, pContext->getDevices());
    EXPECT_TRUE(program->getIsBuiltIn());

    EXPECT_EQ(pContext->getReference(), initialApiRefCount);
    EXPECT_EQ(pContext->getRefInternalCount(), initialInternalRefCount);
    program->release();
    EXPECT_EQ(pContext->getReference(), initialApiRefCount);
    EXPECT_EQ(pContext->getRefInternalCount(), initialInternalRefCount);
    tempProgram->release();
    EXPECT_EQ(pContext->getReference(), initialApiRefCount);
    EXPECT_EQ(pContext->getRefInternalCount(), initialInternalRefCount);
}

TEST_F(ProgramTests, WhenBuildingProgramThenPointerToProgramIsReturned) {
    cl_int retVal = CL_DEVICE_NOT_FOUND;
    Program *pProgram = Program::createBuiltInFromSource("", pContext, pContext->getDevices(), &retVal);
    EXPECT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
    delete pProgram;

    pProgram = Program::createBuiltInFromSource("", pContext, pContext->getDevices(), nullptr);
    EXPECT_NE(nullptr, pProgram);
    delete pProgram;
}

TEST_F(ProgramTests, GivenNullBinaryWhenCreatingProgramFromGenBinaryThenInvalidValueErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    Program *pProgram = Program::createBuiltInFromGenBinary(pContext, pContext->getDevices(), nullptr, 0, &retVal);
    EXPECT_EQ(nullptr, pProgram);
    EXPECT_NE(CL_SUCCESS, retVal);
}

TEST_F(ProgramTests, WhenCreatingProgramFromGenBinaryThenSuccessIsReturned) {
    cl_int retVal = CL_INVALID_BINARY;
    char binary[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, '\0'};
    size_t size = 10;

    Program *pProgram = Program::createBuiltInFromGenBinary(pContext, pContext->getDevices(), binary, size, &retVal);
    EXPECT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ((uint32_t)CL_PROGRAM_BINARY_TYPE_EXECUTABLE, (uint32_t)pProgram->getProgramBinaryType(pClDevice));
    EXPECT_TRUE(pProgram->getIsBuiltIn());

    cl_device_id deviceId = pContext->getDevice(0);
    cl_build_status status = 0;
    pProgram->getBuildInfo(deviceId, CL_PROGRAM_BUILD_STATUS,
                           sizeof(cl_build_status), &status, nullptr);
    EXPECT_EQ(CL_BUILD_SUCCESS, status);

    delete pProgram;
}

TEST_F(ProgramTests, GivenRetValNullPointerWhenCreatingProgramFromGenBinaryThenSuccessIsReturned) {
    char binary[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, '\0'};
    size_t size = 10;

    Program *pProgram = Program::createBuiltInFromGenBinary(pContext, pContext->getDevices(), binary, size, nullptr);
    EXPECT_NE(nullptr, pProgram);
    EXPECT_EQ((uint32_t)CL_PROGRAM_BINARY_TYPE_EXECUTABLE, (uint32_t)pProgram->getProgramBinaryType(pClDevice));

    cl_device_id deviceId = pContext->getDevice(0);
    cl_build_status status = 0;
    pProgram->getBuildInfo(deviceId, CL_PROGRAM_BUILD_STATUS,
                           sizeof(cl_build_status), &status, nullptr);
    EXPECT_EQ(CL_BUILD_SUCCESS, status);

    delete pProgram;
}

TEST_F(ProgramTests, GivenNullContextWhenCreatingProgramFromGenBinaryThenSuccessIsReturned) {
    cl_int retVal = CL_INVALID_BINARY;
    char binary[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, '\0'};
    size_t size = 10;

    Program *pProgram = Program::createBuiltInFromGenBinary(nullptr, toClDeviceVector(*pClDevice), binary, size, &retVal);
    EXPECT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ((uint32_t)CL_PROGRAM_BINARY_TYPE_EXECUTABLE, (uint32_t)pProgram->getProgramBinaryType(pClDevice));

    cl_build_status status = 0;
    pProgram->getBuildInfo(pClDevice, CL_PROGRAM_BUILD_STATUS,
                           sizeof(cl_build_status), &status, nullptr);
    EXPECT_EQ(CL_BUILD_SUCCESS, status);

    delete pProgram;
}

TEST_F(ProgramTests, givenValidZebinPrepareLinkerInput) {
    ZebinTestData::ValidEmptyProgram zebin;
    const std::string validZeInfo = std::string("version :\'") + versionToString(zeInfoDecoderVersion) + R"===('
kernels:
    - name : some_kernel
      execution_env :
        simd_size : 8
)===";
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr, mockRootDeviceIndex));
    {
        auto program = std::make_unique<MockProgram>(nullptr, false, toClDeviceVector(*pClDevice));
        program->buildInfos[rootDeviceIndex].unpackedDeviceBinary = makeCopy(zebin.storage.data(), zebin.storage.size());
        program->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = zebin.storage.size();

        auto retVal = program->processGenBinary(*pClDevice);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, program->buildInfos[rootDeviceIndex].linkerInput.get());
    }
    {
        zebin.removeSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo);
        zebin.appendSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, ArrayRef<const uint8_t>::fromAny(validZeInfo.data(), validZeInfo.size()));
        zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});

        auto program = std::make_unique<MockProgram>(nullptr, false, toClDeviceVector(*pClDevice));
        program->buildInfos[rootDeviceIndex].unpackedDeviceBinary = makeCopy(zebin.storage.data(), zebin.storage.size());
        program->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = zebin.storage.size();

        auto retVal = program->processGenBinary(*pClDevice);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, program->buildInfos[rootDeviceIndex].linkerInput.get());
    }
}

TEST_F(ProgramTests, whenCreatingFromZebinThenAppendAllowZebinFlagToBuildOptions) {
    if (sizeof(void *) != 8U) {
        GTEST_SKIP();
    }

    auto copyHwInfo = *defaultHwInfo;
    CompilerHwInfoConfig::get(copyHwInfo.platform.eProductFamily)->adjustHwInfoForIgc(copyHwInfo);

    ZebinTestData::ValidEmptyProgram zebin;
    zebin.elfHeader->machine = copyHwInfo.platform.eProductFamily;

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr, mockRootDeviceIndex));
    auto program = std::make_unique<MockProgram>(toClDeviceVector(*device));
    cl_int retVal = program->createProgramFromBinary(zebin.storage.data(), zebin.storage.size(), *device);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto expectedOptions = " " + NEO::CompilerOptions::allowZebin.str();
    EXPECT_STREQ(expectedOptions.c_str(), program->options.c_str());
}

TEST_F(ProgramTests, givenProgramFromGenBinaryWhenSLMSizeIsBiggerThenDeviceLimitThenReturnError) {
    PatchTokensTestData::ValidProgramWithKernelUsingSlm patchtokensProgram;
    patchtokensProgram.slmMutable->TotalInlineLocalMemorySize = static_cast<uint32_t>(pDevice->getDeviceInfo().localMemSize * 2);
    patchtokensProgram.recalcTokPtr();
    auto program = std::make_unique<MockProgram>(nullptr, false, toClDeviceVector(*pClDevice));
    program->buildInfos[rootDeviceIndex].unpackedDeviceBinary = makeCopy(patchtokensProgram.storage.data(), patchtokensProgram.storage.size());
    program->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = patchtokensProgram.storage.size();
    auto retVal = program->processGenBinary(*pClDevice);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);
}

TEST_F(ProgramTests, givenExistingConstantSurfacesWhenProcessGenBinaryThenCleanupTheSurfaceOnlyForSpecificDevice) {
    PatchTokensTestData::ValidProgramWithKernelUsingSlm patchtokensProgram;

    auto program = std::make_unique<MockProgram>(nullptr, false, toClDeviceVector(*pClDevice));

    program->buildInfos.resize(2);
    program->buildInfos[0].constantSurface = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({rootDeviceIndex, MemoryConstants::cacheLineSize,
                                                                                                                AllocationType::CONSTANT_SURFACE, pDevice->getDeviceBitfield()});
    program->buildInfos[1].constantSurface = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({rootDeviceIndex, MemoryConstants::cacheLineSize,
                                                                                                                AllocationType::CONSTANT_SURFACE, pDevice->getDeviceBitfield()});
    program->buildInfos[rootDeviceIndex].unpackedDeviceBinary = makeCopy(patchtokensProgram.storage.data(), patchtokensProgram.storage.size());
    program->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = patchtokensProgram.storage.size();

    auto constantSurface0 = program->buildInfos[0].constantSurface;
    EXPECT_NE(nullptr, constantSurface0);
    auto constantSurface1 = program->buildInfos[1].constantSurface;
    EXPECT_NE(nullptr, constantSurface1);

    auto retVal = program->processGenBinary(*pClDevice);

    EXPECT_EQ(nullptr, program->buildInfos[0].constantSurface);
    EXPECT_EQ(constantSurface1, program->buildInfos[1].constantSurface);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ProgramTests, givenExistingGlobalSurfacesWhenProcessGenBinaryThenCleanupTheSurfaceOnlyForSpecificDevice) {
    PatchTokensTestData::ValidProgramWithKernelUsingSlm patchtokensProgram;

    auto program = std::make_unique<MockProgram>(nullptr, false, toClDeviceVector(*pClDevice));

    program->buildInfos.resize(2);
    program->buildInfos[0].globalSurface = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({rootDeviceIndex, MemoryConstants::cacheLineSize,
                                                                                                              AllocationType::GLOBAL_SURFACE, pDevice->getDeviceBitfield()});
    program->buildInfos[1].globalSurface = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({rootDeviceIndex, MemoryConstants::cacheLineSize,
                                                                                                              AllocationType::GLOBAL_SURFACE, pDevice->getDeviceBitfield()});
    program->buildInfos[rootDeviceIndex].unpackedDeviceBinary = makeCopy(patchtokensProgram.storage.data(), patchtokensProgram.storage.size());
    program->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = patchtokensProgram.storage.size();

    auto globalSurface0 = program->buildInfos[0].globalSurface;
    EXPECT_NE(nullptr, globalSurface0);
    auto globalSurface1 = program->buildInfos[1].globalSurface;
    EXPECT_NE(nullptr, globalSurface1);

    auto retVal = program->processGenBinary(*pClDevice);

    EXPECT_EQ(nullptr, program->buildInfos[0].globalSurface);
    EXPECT_EQ(globalSurface1, program->buildInfos[1].globalSurface);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ProgramTests, GivenNoCompilerInterfaceRootDeviceEnvironmentWhenRebuildingBinaryThenOutOfHostMemoryErrorIsReturned) {
    auto pDevice = pContext->getDevice(0);
    auto executionEnvironment = pDevice->getExecutionEnvironment();
    std::unique_ptr<RootDeviceEnvironment> rootDeviceEnvironment = std::make_unique<NoCompilerInterfaceRootDeviceEnvironment>(*executionEnvironment);
    rootDeviceEnvironment->setHwInfo(&pDevice->getHardwareInfo());
    std::swap(rootDeviceEnvironment, executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    auto program = std::make_unique<MockProgram>(toClDeviceVector(*pDevice));
    EXPECT_NE(nullptr, program);

    // Load a binary program file
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "CopyBuffer_simd16_", ".bin");
    size_t binarySize = 0;
    auto pBinary = loadDataFromFile(filePath.c_str(), binarySize);
    EXPECT_NE(0u, binarySize);

    // Create program from loaded binary
    cl_int retVal = program->createProgramFromBinary(pBinary.get(), binarySize, *pClDevice);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Ask to rebuild program from its IR binary - it should fail (no Compiler Interface)
    retVal = program->rebuildProgramFromIr();
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
    std::swap(rootDeviceEnvironment, executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
}

TEST_F(ProgramTests, GivenGtpinReraFlagWhenBuildingProgramThenCorrectOptionsAreSet) {
    auto cip = new MockCompilerInterfaceCaptureBuildOptions();
    auto pDevice = pContext->getDevice(0);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);
    auto program = std::make_unique<SucceedingGenBinaryProgram>(toClDeviceVector(*pDevice));
    program->sourceCode = "__kernel mock() {}";
    program->createdFrom = Program::CreatedFrom::SOURCE;

    // Ask to build created program without NEO::CompilerOptions::gtpinRera flag.
    cl_int retVal = program->build(program->getDevices(), CompilerOptions::fastRelaxedMath.data(), false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Check build options that were applied
    EXPECT_TRUE(CompilerOptions::contains(cip->buildOptions, CompilerOptions::fastRelaxedMath)) << cip->buildOptions;
    EXPECT_FALSE(CompilerOptions::contains(cip->buildOptions, CompilerOptions::gtpinRera)) << cip->buildInternalOptions;

    // Ask to build created program with NEO::CompilerOptions::gtpinRera flag.
    cip->buildOptions.clear();
    cip->buildInternalOptions.clear();
    retVal = program->build(program->getDevices(), CompilerOptions::concatenate(CompilerOptions::gtpinRera, CompilerOptions::finiteMathOnly).c_str(), false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Check build options that were applied
    EXPECT_FALSE(CompilerOptions::contains(cip->buildOptions, CompilerOptions::fastRelaxedMath)) << cip->buildOptions;
    EXPECT_TRUE(CompilerOptions::contains(cip->buildOptions, CompilerOptions::finiteMathOnly)) << cip->buildOptions;
    EXPECT_TRUE(CompilerOptions::contains(cip->buildInternalOptions, CompilerOptions::gtpinRera)) << cip->buildInternalOptions;
}

TEST_F(ProgramTests, GivenFailureDuringProcessGenBinaryWhenProcessGenBinariesIsCalledThenErrorIsReturned) {
    auto program = std::make_unique<FailingGenBinaryProgram>(toClDeviceVector(*pClDevice));

    std::unordered_map<uint32_t, Program::BuildPhase> phaseReached;
    phaseReached[0] = Program::BuildPhase::BinaryCreation;
    cl_int retVal = program->processGenBinaries(toClDeviceVector(*pClDevice), phaseReached);
    EXPECT_EQ(CL_INVALID_BINARY, retVal);
}

class Program32BitTests : public ProgramTests {
  public:
    void SetUp() override {
        DebugManager.flags.Force32bitAddressing.set(true);
        ProgramTests::SetUp();
    }
    void TearDown() override {
        ProgramTests::TearDown();
        DebugManager.flags.Force32bitAddressing.set(false);
    }
};

TEST_F(Program32BitTests, givenDeviceWithForce32BitAddressingOnWhenBuiltinIsCreatedThenNoFlagsArePassedAsInternalOptions) {
    MockProgram program(toClDeviceVector(*pClDevice));
    auto internalOptions = program.getInternalOptions();
    EXPECT_TRUE(hasSubstr(internalOptions, std::string("")));
}

TEST_F(Program32BitTests, givenDeviceWithForce32BitAddressingOnWhenProgramIsCreatedThen32bitFlagIsPassedAsInternalOption) {
    MockProgram program(pContext, false, toClDeviceVector(*pClDevice));
    auto internalOptions = program.getInternalOptions();
    std::string s1 = internalOptions;
    size_t pos = s1.find(NEO::CompilerOptions::arch32bit.data());
    if constexpr (is64bit) {
        EXPECT_NE(pos, std::string::npos);
    } else {
        EXPECT_EQ(pos, std::string::npos);
    }
}

HWTEST_F(ProgramTests, givenNewProgramThenStatelessToStatefulBufferOffsetOptimizationIsMatchingThePlatformEnablingStatus) {
    MockProgram program(pContext, false, toClDeviceVector(*pClDevice));
    auto internalOptions = program.getInternalOptions();

    if (HwHelperHw<FamilyType>::get().isStatelessToStatefulWithOffsetSupported()) {
        EXPECT_TRUE(CompilerOptions::contains(internalOptions, CompilerOptions::hasBufferOffsetArg));
    } else {
        EXPECT_FALSE(CompilerOptions::contains(internalOptions, CompilerOptions::hasBufferOffsetArg));
    }
}

TEST(ProgramTest, givenImagesSupportedWhenCreatingProgramThenInternalOptionsAreCorrectlyInitialized) {
    VariableBackup<bool> supportsImagesCapability{&defaultHwInfo->capabilityTable.supportsImages};

    for (auto areImagesSupported : ::testing::Bool()) {
        supportsImagesCapability = areImagesSupported;
        UltClDeviceFactory clDeviceFactory{1, 0};
        MockContext context{clDeviceFactory.rootDevices[0]};
        MockProgram program(&context, false, toClDeviceVector(*clDeviceFactory.rootDevices[0]));

        auto internalOptions = program.getInternalOptions();
        EXPECT_EQ(areImagesSupported, CompilerOptions::contains(internalOptions, CompilerOptions::enableImageSupport));
    }
}

template <int32_t ErrCodeToReturn, bool spirv = true>
struct CreateProgramFromBinaryMock : public MockProgram {
    using MockProgram::MockProgram;

    cl_int createProgramFromBinary(const void *pBinary,
                                   size_t binarySize, ClDevice &clDevice) override {
        this->irBinary.reset(new char[binarySize]);
        this->irBinarySize = binarySize;
        this->isSpirV = spirv;
        memcpy_s(this->irBinary.get(), binarySize, pBinary, binarySize);
        return ErrCodeToReturn;
    }
};

TEST_F(ProgramTests, GivenFailedBinaryWhenCreatingFromIlThenInvalidBinaryErrorIsReturned) {
    const uint32_t notSpirv[16] = {0xDEADBEEF};
    cl_int retVal = CL_SUCCESS;
    auto prog = Program::createFromIL<CreateProgramFromBinaryMock<CL_INVALID_BINARY>>(pContext, reinterpret_cast<const void *>(notSpirv), sizeof(notSpirv), retVal);
    EXPECT_EQ(nullptr, prog);
    EXPECT_EQ(CL_INVALID_BINARY, retVal);
}

TEST_F(ProgramTests, GivenSuccessfullyBuiltBinaryWhenCreatingFromIlThenValidProgramIsReturned) {
    const uint32_t spirv[16] = {0x03022307};
    cl_int retVal = CL_SUCCESS;
    auto prog = Program::createFromIL<CreateProgramFromBinaryMock<CL_SUCCESS>>(pContext, reinterpret_cast<const void *>(spirv), sizeof(spirv), retVal);
    ASSERT_NE(nullptr, prog);
    EXPECT_EQ(CL_SUCCESS, retVal);
    prog->release();
}

TEST_F(ProgramTests, givenProgramCreatedFromILWhenCompileIsCalledThenReuseTheILInsteadOfCallingCompilerInterface) {
    const uint32_t spirv[16] = {0x03022307};
    cl_int errCode = 0;
    auto pProgram = Program::createFromIL<MockProgram>(pContext, reinterpret_cast<const void *>(spirv), sizeof(spirv), errCode);
    ASSERT_NE(nullptr, pProgram); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    auto debugVars = NEO::getIgcDebugVars();
    debugVars.forceBuildFailure = true;
    gEnvironment->fclPushDebugVars(debugVars);
    auto compilerErr = pProgram->compile(pProgram->getDevices(), nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, compilerErr);
    gEnvironment->fclPopDebugVars();
    pProgram->release();
}

TEST_F(ProgramTests, givenProgramCreatedFromIntermediateBinaryRepresentationWhenCompileIsCalledThenReuseTheILInsteadOfCallingCompilerInterface) {
    const uint32_t spirv[16] = {0x03022307};
    cl_int errCode = 0;
    size_t lengths = sizeof(spirv);
    const unsigned char *binaries[1] = {reinterpret_cast<const unsigned char *>(spirv)};
    auto pProgram = Program::create<MockProgram>(pContext, pContext->getDevices(), &lengths, binaries, nullptr, errCode);
    ASSERT_NE(nullptr, pProgram);
    auto debugVars = NEO::getIgcDebugVars();
    debugVars.forceBuildFailure = true;
    gEnvironment->fclPushDebugVars(debugVars);
    auto compilerErr = pProgram->compile(pProgram->getDevices(), nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, compilerErr);
    gEnvironment->fclPopDebugVars();
    pProgram->release();
}

TEST_F(ProgramTests, GivenIlIsNullptrWhenCreatingFromIlThenInvalidBinaryErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    auto prog = Program::createFromIL<CreateProgramFromBinaryMock<CL_INVALID_BINARY>>(pContext, nullptr, 16, retVal);
    EXPECT_EQ(nullptr, prog);
    EXPECT_EQ(CL_INVALID_BINARY, retVal);
}

TEST_F(ProgramTests, GivenIlSizeZeroWhenCreatingFromIlThenInvalidBinaryErrorIsReturned) {
    const uint32_t spirv[16] = {0x03022307};
    cl_int retVal = CL_SUCCESS;
    auto prog = Program::createFromIL<CreateProgramFromBinaryMock<CL_INVALID_BINARY>>(pContext, reinterpret_cast<const void *>(spirv), 0, retVal);
    EXPECT_EQ(nullptr, prog);
    EXPECT_EQ(CL_INVALID_BINARY, retVal);
}

TEST_F(ProgramTests, WhenCreatingFromIlThenIsSpirvIsSetCorrectly) {
    const uint32_t spirv[16] = {0x03022307};
    cl_int retVal = CL_SUCCESS;
    auto prog = Program::createFromIL<Program>(pContext, reinterpret_cast<const void *>(spirv), sizeof(spirv), retVal);
    EXPECT_NE(nullptr, prog);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(prog->getIsSpirV());
    prog->release();

    const char llvmBc[16] = {'B', 'C', '\xc0', '\xde'};
    prog = Program::createFromIL<Program>(pContext, reinterpret_cast<const void *>(llvmBc), sizeof(llvmBc), retVal);
    EXPECT_NE(nullptr, prog);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(prog->getIsSpirV());
    prog->release();
}

static const uint8_t llvmBinary[] = "BC\xc0\xde     ";

TEST(isValidLlvmBinary, whenLlvmMagicWasFoundThenBinaryIsValidLLvm) {
    EXPECT_TRUE(NEO::isLlvmBitcode(llvmBinary));
}

TEST(isValidLlvmBinary, whenBinaryIsNullptrThenBinaryIsNotValidLLvm) {
    EXPECT_FALSE(NEO::isLlvmBitcode(ArrayRef<const uint8_t>()));
}

TEST(isValidLlvmBinary, whenBinaryIsShorterThanLllvMagicThenBinaryIsNotValidLLvm) {
    EXPECT_FALSE(NEO::isLlvmBitcode(ArrayRef<const uint8_t>(llvmBinary, 2)));
}

TEST(isValidLlvmBinary, whenBinaryDoesNotContainLllvMagicThenBinaryIsNotValidLLvm) {
    const uint8_t notLlvmBinary[] = "ABCDEFGHIJKLMNO";
    EXPECT_FALSE(NEO::isLlvmBitcode(notLlvmBinary));
}

const uint32_t spirv[16] = {0x03022307};
const uint32_t spirvInvEndianes[16] = {0x07230203};

TEST(isValidSpirvBinary, whenSpirvMagicWasFoundThenBinaryIsValidSpirv) {
    EXPECT_TRUE(NEO::isSpirVBitcode(ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(&spirv), sizeof(spirv))));
    EXPECT_TRUE(NEO::isSpirVBitcode(ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(&spirvInvEndianes), sizeof(spirvInvEndianes))));
}

TEST(isValidSpirvBinary, whenBinaryIsNullptrThenBinaryIsNotValidLLvm) {
    EXPECT_FALSE(NEO::isSpirVBitcode(ArrayRef<const uint8_t>()));
}

TEST(isValidSpirvBinary, whenBinaryIsShorterThanLllvMagicThenBinaryIsNotValidLLvm) {
    EXPECT_FALSE(NEO::isSpirVBitcode(ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(&spirvInvEndianes), 2)));
}

TEST(isValidSpirvBinary, whenBinaryDoesNotContainLllvMagicThenBinaryIsNotValidLLvm) {
    const uint8_t notSpirvBinary[] = "ABCDEFGHIJKLMNO";
    EXPECT_FALSE(NEO::isSpirVBitcode(notSpirvBinary));
}

TEST_F(ProgramTests, WhenLinkingTwoValidSpirvProgramsThenValidProgramIsReturned) {
    const uint32_t spirv[16] = {0x03022307};
    cl_int errCode = CL_SUCCESS;

    auto node1 = Program::createFromIL<CreateProgramFromBinaryMock<CL_SUCCESS, false>>(pContext, reinterpret_cast<const void *>(spirv), sizeof(spirv), errCode);
    ASSERT_NE(nullptr, node1); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_EQ(CL_SUCCESS, errCode);

    auto node2 = Program::createFromIL<CreateProgramFromBinaryMock<CL_SUCCESS>>(pContext, reinterpret_cast<const void *>(spirv), sizeof(spirv), errCode);
    ASSERT_NE(nullptr, node2); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_EQ(CL_SUCCESS, errCode);

    auto prog = Program::createFromIL<CreateProgramFromBinaryMock<CL_SUCCESS>>(pContext, reinterpret_cast<const void *>(spirv), sizeof(spirv), errCode);
    ASSERT_NE(nullptr, prog); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    EXPECT_EQ(CL_SUCCESS, errCode);

    cl_program linkNodes[] = {node1, node2};
    errCode = prog->link(prog->getDevices(), nullptr, 2, linkNodes);
    EXPECT_EQ(CL_SUCCESS, errCode);

    prog->release();
    node2->release();
    node1->release();
}

TEST(ProgramDestructionTests, givenProgramUsingDeviceWhenItIsDestroyedAfterPlatfromCleanupThenItIsCleanedUpProperly) {
    initPlatform();
    auto device = platform()->getClDevice(0);
    MockContext *context = new MockContext(device, false);
    MockProgram *pProgram = new MockProgram(context, false, toClDeviceVector(*device));
    auto globalAllocation = device->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    pProgram->setGlobalSurface(globalAllocation);

    platformsImpl->clear();
    EXPECT_EQ(1, device->getRefInternalCount());
    EXPECT_EQ(1, pProgram->getRefInternalCount());
    context->decRefInternal();
    pProgram->decRefInternal();
}

TEST_F(ProgramTests, givenProgramWithSpirvWhenRebuildProgramIsCalledThenSpirvPathIsTaken) {
    auto compilerInterface = new MockCompilerInterface();
    auto compilerMain = new MockCIFMain();
    compilerInterface->setFclMain(compilerMain);
    compilerMain->Retain();
    compilerInterface->setIgcMain(compilerMain);
    compilerMain->setDefaultCreatorFunc<NEO::MockIgcOclDeviceCtx>(NEO::MockIgcOclDeviceCtx::Create);
    compilerMain->setDefaultCreatorFunc<NEO::MockFclOclDeviceCtx>(NEO::MockFclOclDeviceCtx::Create);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->compilerInterface.reset(compilerInterface);

    std::string receivedInput;
    MockCompilerDebugVars debugVars = {};
    debugVars.receivedInput = &receivedInput;
    debugVars.forceBuildFailure = true;
    gEnvironment->igcPushDebugVars(debugVars);
    std::unique_ptr<void, void (*)(void *)> igcDebugVarsAutoPop{&gEnvironment, [](void *) { gEnvironment->igcPopDebugVars(); }};

    auto program = clUniquePtr(new MockProgram(toClDeviceVector(*pClDevice)));
    uint32_t spirv[16] = {0x03022307, 0x23471113, 0x17192329};
    program->irBinary = makeCopy(spirv, sizeof(spirv));
    program->irBinarySize = sizeof(spirv);
    program->isSpirV = true;
    auto buildRet = program->rebuildProgramFromIr();
    EXPECT_NE(CL_SUCCESS, buildRet);
    ASSERT_EQ(sizeof(spirv), receivedInput.size());
    EXPECT_EQ(0, memcmp(spirv, receivedInput.c_str(), receivedInput.size()));
    ASSERT_EQ(1U, compilerInterface->requestedTranslationCtxs.size());
    EXPECT_EQ(IGC::CodeType::spirV, compilerInterface->requestedTranslationCtxs[0].first);
    EXPECT_EQ(IGC::CodeType::oclGenBin, compilerInterface->requestedTranslationCtxs[0].second);
}

TEST_F(ProgramTests, givenProgramWithSpirvWhenRebuildIsCalledThenRebuildWarningIsIssued) {
    const auto program{clUniquePtr(new MockProgram(toClDeviceVector(*pClDevice)))};
    uint32_t spirv[16] = {0x03022307, 0x23471113, 0x17192329};
    program->irBinary = makeCopy(spirv, sizeof(spirv));
    program->irBinarySize = sizeof(spirv);
    program->isSpirV = true;

    const auto buildResult{program->rebuildProgramFromIr()};
    ASSERT_EQ(CL_SUCCESS, buildResult);

    const std::string buildLog{program->getBuildLog(pClDevice->getRootDeviceIndex())};
    const auto containsWarning{buildLog.find(CompilerWarnings::recompiledFromIr.data()) != std::string::npos};

    EXPECT_TRUE(containsWarning);
}

TEST_F(ProgramTests, givenProgramWithSpirvWhenRebuildIsCalledButSuppressFlagIsEnabledThenRebuildWarningIsNotIssued) {
    const auto program{clUniquePtr(new MockProgram(toClDeviceVector(*pClDevice)))};
    uint32_t spirv[16] = {0x03022307, 0x23471113, 0x17192329};
    program->irBinary = makeCopy(spirv, sizeof(spirv));
    program->irBinarySize = sizeof(spirv);
    program->isSpirV = true;

    const auto buildOptions{CompilerOptions::noRecompiledFromIr};
    program->setBuildOptions(buildOptions.data());

    const auto buildResult{program->rebuildProgramFromIr()};
    ASSERT_EQ(CL_SUCCESS, buildResult);

    const std::string buildLog{program->getBuildLog(pClDevice->getRootDeviceIndex())};
    const auto containsWarning{buildLog.find(CompilerWarnings::recompiledFromIr.data()) != std::string::npos};

    EXPECT_FALSE(containsWarning);
}

TEST_F(ProgramTests, givenProgramWithSpirvWhenRecompileIsCalledThenRebuildWarningIsIssued) {
    const auto program{clUniquePtr(new MockProgram(toClDeviceVector(*pClDevice)))};
    uint32_t spirv[16] = {0x03022307, 0x23471113, 0x17192329};
    program->irBinary = makeCopy(spirv, sizeof(spirv));
    program->irBinarySize = sizeof(spirv);
    program->isSpirV = true;

    const auto compileResult{program->recompile()};
    ASSERT_EQ(CL_SUCCESS, compileResult);

    const std::string buildLog{program->getBuildLog(pClDevice->getRootDeviceIndex())};
    const auto containsWarning{buildLog.find(CompilerWarnings::recompiledFromIr.data()) != std::string::npos};

    EXPECT_TRUE(containsWarning);
}

TEST_F(ProgramTests, givenProgramWithSpirvWhenRecompileIsCalledButSuppressFlagIsEnabledThenRebuildWarningIsNotIssued) {
    const auto program{clUniquePtr(new MockProgram(toClDeviceVector(*pClDevice)))};
    uint32_t spirv[16] = {0x03022307, 0x23471113, 0x17192329};
    program->irBinary = makeCopy(spirv, sizeof(spirv));
    program->irBinarySize = sizeof(spirv);
    program->isSpirV = true;

    const auto buildOptions{CompilerOptions::noRecompiledFromIr};
    program->setBuildOptions(buildOptions.data());

    const auto compileResult{program->recompile()};
    ASSERT_EQ(CL_SUCCESS, compileResult);

    const std::string buildLog{program->getBuildLog(pClDevice->getRootDeviceIndex())};
    const auto containsWarning{buildLog.find(CompilerWarnings::recompiledFromIr.data()) != std::string::npos};

    EXPECT_FALSE(containsWarning);
}

TEST_F(ProgramTests, whenRebuildingProgramThenStoreDeviceBinaryProperly) {
    auto compilerInterface = new MockCompilerInterface();
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->compilerInterface.reset(compilerInterface);
    auto compilerMain = new MockCIFMain();
    compilerInterface->setIgcMain(compilerMain);
    compilerMain->setDefaultCreatorFunc<NEO::MockIgcOclDeviceCtx>(NEO::MockIgcOclDeviceCtx::Create);

    MockCompilerDebugVars debugVars = {};
    char binaryToReturn[] = "abcdfghijklmnop";
    debugVars.binaryToReturn = binaryToReturn;
    debugVars.binaryToReturnSize = sizeof(binaryToReturn);
    gEnvironment->igcPushDebugVars(debugVars);
    std::unique_ptr<void, void (*)(void *)> igcDebugVarsAutoPop{&gEnvironment, [](void *) { gEnvironment->igcPopDebugVars(); }};

    auto program = clUniquePtr(new MockProgram(toClDeviceVector(*pClDevice)));
    uint32_t ir[16] = {0x03022307, 0x23471113, 0x17192329};
    program->irBinary = makeCopy(ir, sizeof(ir));
    program->irBinarySize = sizeof(ir);
    EXPECT_EQ(nullptr, program->buildInfos[rootDeviceIndex].unpackedDeviceBinary);
    EXPECT_EQ(0U, program->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize);
    program->rebuildProgramFromIr();
    ASSERT_NE(nullptr, program->buildInfos[rootDeviceIndex].unpackedDeviceBinary);
    ASSERT_EQ(sizeof(binaryToReturn), program->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize);
    EXPECT_EQ(0, memcmp(binaryToReturn, program->buildInfos[rootDeviceIndex].unpackedDeviceBinary.get(), program->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize));
}

TEST_F(ProgramTests, givenProgramWhenInternalOptionsArePassedThenTheyAreAddedToProgramInternalOptions) {
    MockProgram program(toClDeviceVector(*pClDevice));
    std::string buildOptions = NEO::CompilerOptions::gtpinRera.str();
    std::string internalOptions;
    program.extractInternalOptions(buildOptions, internalOptions);
    EXPECT_STREQ(internalOptions.c_str(), NEO::CompilerOptions::gtpinRera.data());
}

TEST_F(ProgramTests, givenProgramWhenUnknownInternalOptionsArePassedThenTheyAreNotAddedToProgramInternalOptions) {
    MockProgram program(toClDeviceVector(*pClDevice));
    const char *internalOption = "-unknown-internal-options-123";
    std::string buildOptions(internalOption);
    std::string internalOptions;
    program.extractInternalOptions(buildOptions, internalOptions);
    EXPECT_EQ(0u, internalOptions.length());
}

TEST_F(ProgramTests, givenProgramWhenAllInternalOptionsArePassedMixedWithUnknownInputThenTheyAreParsedCorrectly) {
    MockProgram program(toClDeviceVector(*pClDevice));
    std::string buildOptions = CompilerOptions::concatenate("###", CompilerOptions::gtpinRera, "###", CompilerOptions::greaterThan4gbBuffersRequired, "###");
    std::string expectedOutput = CompilerOptions::concatenate(CompilerOptions::gtpinRera, CompilerOptions::greaterThan4gbBuffersRequired);
    std::string internalOptions;
    program.extractInternalOptions(buildOptions, internalOptions);
    EXPECT_EQ(expectedOutput, internalOptions);
}

TEST_F(ProgramTests, givenProgramWhenInternalOptionsArePassedWithValidValuesThenTheyAreAddedToProgramInternalOptions) {
    MockProgram program(toClDeviceVector(*pClDevice));

    program.isFlagOptionOverride = false;
    program.isOptionValueValidOverride = true;
    std::string buildOptions = CompilerOptions::concatenate(CompilerOptions::gtpinRera, "someValue");

    std::string internalOptions;
    program.extractInternalOptions(buildOptions, internalOptions);
    EXPECT_EQ(buildOptions, internalOptions) << internalOptions;
}

TEST_F(ProgramTests, givenProgramWhenInternalOptionsArePassedWithInvalidValuesThenTheyAreNotAddedToProgramInternalOptions) {
    MockProgram program(toClDeviceVector(*pClDevice));

    program.isFlagOptionOverride = false;
    std::string buildOptions = CompilerOptions::concatenate(CompilerOptions::gtpinRera, "someValue");
    std::string expectedOutput = "";

    std::string internalOptions;
    program.extractInternalOptions(buildOptions, internalOptions);
    EXPECT_EQ(expectedOutput, internalOptions);

    program.isOptionValueValidOverride = true;
    buildOptions = std::string(CompilerOptions::gtpinRera);
    internalOptions.erase();
    program.extractInternalOptions(buildOptions, internalOptions);
    EXPECT_EQ(expectedOutput, internalOptions);
}

TEST_F(ProgramTests, GivenInjectInternalBuildOptionsWhenBuildingProgramThenInternalOptionsWereAppended) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.InjectInternalBuildOptions.set("-abc");

    auto cip = new MockCompilerInterfaceCaptureBuildOptions();
    auto pDevice = pContext->getDevice(0);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);
    auto program = std::make_unique<SucceedingGenBinaryProgram>(toClDeviceVector(*pDevice));
    program->sourceCode = "__kernel mock() {}";
    program->createdFrom = Program::CreatedFrom::SOURCE;

    cl_int retVal = program->build(program->getDevices(), "", false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(CompilerOptions::contains(cip->buildInternalOptions, "-abc")) << cip->buildInternalOptions;
}

TEST_F(ProgramTests, GivenInjectInternalBuildOptionsWhenBuildingBuiltInProgramThenInternalOptionsAreNotAppended) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.InjectInternalBuildOptions.set("-abc");

    auto cip = new MockCompilerInterfaceCaptureBuildOptions();
    auto pDevice = pContext->getDevice(0);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);
    auto program = std::make_unique<SucceedingGenBinaryProgram>(toClDeviceVector(*pDevice));
    program->sourceCode = "__kernel mock() {}";
    program->createdFrom = Program::CreatedFrom::SOURCE;
    program->isBuiltIn = true;

    cl_int retVal = program->build(program->getDevices(), "", false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(CompilerOptions::contains(cip->buildInternalOptions, "-abc")) << cip->buildInternalOptions;
}

TEST_F(ProgramTests, GivenInjectInternalBuildOptionsWhenCompilingProgramThenInternalOptionsWereAppended) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.InjectInternalBuildOptions.set("-abc");

    auto cip = new MockCompilerInterfaceCaptureBuildOptions();
    auto pDevice = pContext->getDevice(0);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);
    auto program = std::make_unique<SucceedingGenBinaryProgram>(toClDeviceVector(*pDevice));
    program->sourceCode = "__kernel mock() {}";
    program->createdFrom = Program::CreatedFrom::SOURCE;

    cl_int retVal = program->compile(program->getDevices(), nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(CompilerOptions::contains(cip->buildInternalOptions, "-abc")) << cip->buildInternalOptions;
}

TEST_F(ProgramTests, GivenInjectInternalBuildOptionsWhenCompilingBuiltInProgramThenInternalOptionsAreNotAppended) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.InjectInternalBuildOptions.set("-abc");

    auto cip = new MockCompilerInterfaceCaptureBuildOptions();
    auto pDevice = pContext->getDevice(0);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);
    auto program = std::make_unique<SucceedingGenBinaryProgram>(toClDeviceVector(*pDevice));
    program->sourceCode = "__kernel mock() {}";
    program->createdFrom = Program::CreatedFrom::SOURCE;
    program->isBuiltIn = true;

    cl_int retVal = program->compile(program->getDevices(), nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(CompilerOptions::contains(cip->buildInternalOptions, "-abc")) << cip->buildInternalOptions;
}

TEST(CreateProgramFromBinaryTests, givenBinaryProgramBuiltInWhenKernelRebulildIsForcedThenDeviceBinaryIsNotUsed) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.RebuildPrecompiledKernels.set(true);
    cl_int retVal = CL_INVALID_BINARY;

    PatchTokensTestData::ValidEmptyProgram programTokens;

    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    std::unique_ptr<MockProgram> pProgram(Program::createBuiltInFromGenBinary<MockProgram>(nullptr, toClDeviceVector(*clDevice), programTokens.storage.data(), programTokens.storage.size(), &retVal));
    ASSERT_NE(nullptr, pProgram.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto rootDeviceIndex = clDevice->getRootDeviceIndex();
    retVal = pProgram->createProgramFromBinary(programTokens.storage.data(), programTokens.storage.size(), *clDevice);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, pProgram->buildInfos[rootDeviceIndex].unpackedDeviceBinary.get());
    EXPECT_EQ(0U, pProgram->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize);
    EXPECT_EQ(nullptr, pProgram->buildInfos[rootDeviceIndex].packedDeviceBinary);
    EXPECT_EQ(0U, pProgram->buildInfos[rootDeviceIndex].packedDeviceBinarySize);
}

TEST(CreateProgramFromBinaryTests, givenBinaryProgramBuiltInWhenKernelRebulildIsForcedThenRebuildWarningIsEnabled) {
    DebugManagerStateRestore dbgRestorer{};
    DebugManager.flags.RebuildPrecompiledKernels.set(true);

    PatchTokensTestData::ValidEmptyProgram programTokens;
    cl_int retVal{CL_INVALID_BINARY};

    const auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    std::unique_ptr<MockProgram> pProgram(Program::createBuiltInFromGenBinary<MockProgram>(nullptr, toClDeviceVector(*clDevice), programTokens.storage.data(), programTokens.storage.size(), &retVal));
    ASSERT_NE(nullptr, pProgram.get());
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = pProgram->createProgramFromBinary(programTokens.storage.data(), programTokens.storage.size(), *clDevice);
    ASSERT_EQ(CL_SUCCESS, retVal);

    ASSERT_TRUE(pProgram->requiresRebuild);
}

TEST(CreateProgramFromBinaryTests, givenBinaryProgramNotBuiltInWhenBuiltInKernelRebulildIsForcedThenDeviceBinaryIsUsed) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.RebuildPrecompiledKernels.set(true);
    cl_int retVal = CL_INVALID_BINARY;

    PatchTokensTestData::ValidEmptyProgram programTokens;
    const unsigned char *binaries[] = {programTokens.storage.data()};
    size_t lengths[] = {programTokens.storage.size()};
    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    std::unique_ptr<MockProgram> pProgram(Program::create<MockProgram>(
        nullptr,
        toClDeviceVector(*clDevice),
        lengths,
        binaries,
        nullptr,
        retVal));
    ASSERT_NE(nullptr, pProgram.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto rootDeviceIndex = clDevice->getRootDeviceIndex();
    EXPECT_NE(nullptr, pProgram->buildInfos[rootDeviceIndex].unpackedDeviceBinary.get());
    EXPECT_LT(0U, pProgram->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize);
    EXPECT_NE(nullptr, pProgram->buildInfos[rootDeviceIndex].packedDeviceBinary);
    EXPECT_LT(0U, pProgram->buildInfos[rootDeviceIndex].packedDeviceBinarySize);
}

TEST(CreateProgramFromBinaryTests, givenBinaryProgramWhenKernelRebulildIsNotForcedThenDeviceBinaryIsUsed) {
    cl_int retVal = CL_INVALID_BINARY;

    PatchTokensTestData::ValidEmptyProgram programTokens;

    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    std::unique_ptr<MockProgram> pProgram(Program::createBuiltInFromGenBinary<MockProgram>(nullptr, toClDeviceVector(*clDevice), programTokens.storage.data(), programTokens.storage.size(), &retVal));
    ASSERT_NE(nullptr, pProgram.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto rootDeviceIndex = clDevice->getRootDeviceIndex();
    retVal = pProgram->createProgramFromBinary(programTokens.storage.data(), programTokens.storage.size(), *clDevice);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, reinterpret_cast<uint8_t *>(pProgram->buildInfos[rootDeviceIndex].unpackedDeviceBinary.get()));
    EXPECT_EQ(programTokens.storage.size(), pProgram->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize);
    EXPECT_NE(nullptr, reinterpret_cast<uint8_t *>(pProgram->buildInfos[rootDeviceIndex].packedDeviceBinary.get()));
    EXPECT_EQ(programTokens.storage.size(), pProgram->buildInfos[rootDeviceIndex].packedDeviceBinarySize);
}

struct SpecializationConstantProgramMock : public MockProgram {
    using MockProgram::MockProgram;
    cl_int updateSpecializationConstant(cl_uint specId, size_t specSize, const void *specValue) override {
        return CL_SUCCESS;
    }
};

struct SpecializationConstantCompilerInterfaceMock : public CompilerInterface {
    TranslationOutput::ErrorCode retVal = TranslationOutput::ErrorCode::Success;
    int counter = 0;
    const char *spirV = nullptr;
    TranslationOutput::ErrorCode getSpecConstantsInfo(const NEO::Device &device, ArrayRef<const char> srcSpirV, SpecConstantInfo &output) override {
        counter++;
        spirV = srcSpirV.begin();
        return retVal;
    }
    void returnError() {
        retVal = TranslationOutput::ErrorCode::CompilationFailure;
    }
};

struct SpecializationConstantRootDeviceEnvironemnt : public RootDeviceEnvironment {
    SpecializationConstantRootDeviceEnvironemnt(ExecutionEnvironment &executionEnvironment) : RootDeviceEnvironment(executionEnvironment) {
        compilerInterface.reset(new SpecializationConstantCompilerInterfaceMock());
    }
    CompilerInterface *getCompilerInterface() override {
        return compilerInterface.get();
    }

    bool initAilConfiguration() override {
        return true;
    }
};

struct setProgramSpecializationConstantTests : public ::testing::Test {
    setProgramSpecializationConstantTests() : device(new MockDevice()) {}
    void SetUp() override {
        mockCompiler = new SpecializationConstantCompilerInterfaceMock();
        auto rootDeviceEnvironment = device.getExecutionEnvironment()->rootDeviceEnvironments[0].get();
        rootDeviceEnvironment->compilerInterface.reset(mockCompiler);
        mockProgram.reset(new SpecializationConstantProgramMock(toClDeviceVector(device)));
        mockProgram->isSpirV = true;

        EXPECT_FALSE(mockProgram->areSpecializationConstantsInitialized);
        EXPECT_EQ(0, mockCompiler->counter);
    }

    SpecializationConstantCompilerInterfaceMock *mockCompiler = nullptr;
    std::unique_ptr<SpecializationConstantProgramMock> mockProgram;
    MockClDevice device;

    int specValue = 1;
};

TEST_F(setProgramSpecializationConstantTests, whenSetProgramSpecializationConstantThenBinarySourceIsUsed) {
    auto retVal = mockProgram->setProgramSpecializationConstant(1, sizeof(int), &specValue);

    EXPECT_EQ(1, mockCompiler->counter);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(mockProgram->areSpecializationConstantsInitialized);
    EXPECT_EQ(mockProgram->irBinary.get(), mockCompiler->spirV);
}

TEST_F(setProgramSpecializationConstantTests, whenSetProgramSpecializationConstantMultipleTimesThenSpecializationConstantsAreInitializedOnce) {
    auto retVal = mockProgram->setProgramSpecializationConstant(1, sizeof(int), &specValue);

    EXPECT_EQ(1, mockCompiler->counter);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(mockProgram->areSpecializationConstantsInitialized);

    retVal = mockProgram->setProgramSpecializationConstant(1, sizeof(int), &specValue);

    EXPECT_EQ(1, mockCompiler->counter);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(mockProgram->areSpecializationConstantsInitialized);
}

TEST_F(setProgramSpecializationConstantTests, givenInvalidGetSpecConstantsInfoReturnValueWhenSetProgramSpecializationConstantThenErrorIsReturned) {
    mockCompiler->returnError();

    auto retVal = mockProgram->setProgramSpecializationConstant(1, sizeof(int), &specValue);

    EXPECT_EQ(1, mockCompiler->counter);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_FALSE(mockProgram->areSpecializationConstantsInitialized);
}

TEST(setProgramSpecializationConstantTest, givenUninitializedCompilerinterfaceWhenSetProgramSpecializationConstantThenErrorIsReturned) {
    auto executionEnvironment = new MockExecutionEnvironment();
    executionEnvironment->rootDeviceEnvironments[0] = std::make_unique<NoCompilerInterfaceRootDeviceEnvironment>(*executionEnvironment);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    MockClDevice mockDevice(new MockDevice{executionEnvironment, 0});
    SpecializationConstantProgramMock mockProgram(toClDeviceVector(mockDevice));

    mockProgram.isSpirV = true;
    int specValue = 1;

    auto retVal = mockProgram.setProgramSpecializationConstant(1, sizeof(int), &specValue);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
}

using ProgramBinTest = Test<ProgramSimpleFixture>;

TEST_F(ProgramBinTest, givenPrintProgramBinaryProcessingTimeSetWhenBuildProgramThenProcessingTimeIsPrinted) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.PrintProgramBinaryProcessingTime.set(true);
    testing::internal::CaptureStdout();

    createProgramFromBinary(pContext, pContext->getDevices(), "kernel_data_param");

    auto retVal = pProgram->build(
        pProgram->getDevices(),
        nullptr,
        false);

    auto output = testing::internal::GetCapturedStdout();
    EXPECT_FALSE(output.compare(0, 14, "Elapsed time: "));
    EXPECT_EQ(CL_SUCCESS, retVal);
}

struct DebugDataGuard {
    DebugDataGuard(const DebugDataGuard &) = delete;
    DebugDataGuard(DebugDataGuard &&) = delete;

    DebugDataGuard() {
        for (size_t n = 0; n < sizeof(mockDebugData); n++) {
            mockDebugData[n] = (char)n;
        }

        auto vars = NEO::getIgcDebugVars();
        vars.debugDataToReturn = mockDebugData;
        vars.debugDataToReturnSize = sizeof(mockDebugData);
        NEO::setIgcDebugVars(vars);
    }

    ~DebugDataGuard() {
        auto vars = NEO::getIgcDebugVars();
        vars.debugDataToReturn = nullptr;
        vars.debugDataToReturnSize = 0;
        NEO::setIgcDebugVars(vars);
    }

    char mockDebugData[32];
};

TEST_F(ProgramBinTest, GivenBuildWithDebugDataThenBuildDataAvailableViaGetInfo) {
    DebugDataGuard debugDataGuard;

    const char *sourceCode = "__kernel void\nCB(\n__global unsigned int* src, __global unsigned int* dst)\n{\nint id = (int)get_global_id(0);\ndst[id] = src[id];\n}\n";
    pProgram = Program::create<MockProgram>(
        pContext,
        1,
        &sourceCode,
        &knownSourceSize,
        retVal);
    retVal = pProgram->build(pProgram->getDevices(), nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Verify
    size_t debugDataSize = 0;
    retVal = pProgram->getInfo(CL_PROGRAM_DEBUG_INFO_SIZES_INTEL, sizeof(debugDataSize), &debugDataSize, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    std::unique_ptr<char[]> debugData{new char[debugDataSize]};
    for (size_t n = 0; n < sizeof(debugData); n++) {
        debugData[n] = 0;
    }
    char *pDebugData = &debugData[0];
    size_t retData = 0;
    bool isOK = true;
    retVal = pProgram->getInfo(CL_PROGRAM_DEBUG_INFO_INTEL, 1, &pDebugData, &retData);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    retVal = pProgram->getInfo(CL_PROGRAM_DEBUG_INFO_INTEL, debugDataSize, &pDebugData, &retData);
    EXPECT_EQ(CL_SUCCESS, retVal);
    cl_uint numDevices;
    retVal = clGetProgramInfo(pProgram, CL_PROGRAM_NUM_DEVICES, sizeof(numDevices), &numDevices, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(numDevices * sizeof(debugData), retData);
    // Check integrity of returned debug data
    for (size_t n = 0; n < debugDataSize; n++) {
        if (debugData[n] != (char)n) {
            isOK = false;
            break;
        }
    }
    EXPECT_TRUE(isOK);
    for (size_t n = debugDataSize; n < sizeof(debugData); n++) {
        if (debugData[n] != (char)0) {
            isOK = false;
            break;
        }
    }
    EXPECT_TRUE(isOK);

    retData = 0;
    retVal = pProgram->getInfo(CL_PROGRAM_DEBUG_INFO_INTEL, debugDataSize, nullptr, &retData);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(numDevices * sizeof(debugData), retData);
}

TEST_F(ProgramBinTest, givenNoDebugDataAvailableThenDebugDataIsNotAvailableViaGetInfo) {
    const char *sourceCode = "__kernel void\nCB(\n__global unsigned int* src, __global unsigned int* dst)\n{\nint id = (int)get_global_id(0);\ndst[id] = src[id];\n}\n";
    pProgram = Program::create<MockProgram>(
        pContext,
        1,
        &sourceCode,
        &knownSourceSize,
        retVal);
    EXPECT_EQ(0u, pProgram->buildInfos[rootDeviceIndex].debugDataSize);
    EXPECT_EQ(nullptr, pProgram->buildInfos[rootDeviceIndex].debugData);
    size_t debugDataSize = 0;
    retVal = pProgram->getInfo(CL_PROGRAM_DEBUG_INFO_SIZES_INTEL, sizeof(debugDataSize), &debugDataSize, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, debugDataSize);

    cl_uint numDevices;
    retVal = clGetProgramInfo(pProgram, CL_PROGRAM_NUM_DEVICES, sizeof(numDevices), &numDevices, nullptr);

    debugDataSize = numDevices * sizeof(void **);
    std::unique_ptr<char[]> debugData{new char[debugDataSize]};
    for (size_t n = 0; n < sizeof(debugData); n++) {
        debugData[n] = 0;
    }
    char *pDebugData = &debugData[0];
    size_t retData = 0;
    retVal = pProgram->getInfo(CL_PROGRAM_DEBUG_INFO_INTEL, debugDataSize, &pDebugData, &retData);
    EXPECT_EQ(CL_SUCCESS, retVal);
    for (size_t n = 0; n < sizeof(debugData); n++) {
        EXPECT_EQ(0, debugData[n]);
    }
}

TEST_F(ProgramBinTest, GivenDebugDataAvailableWhenLinkingProgramThenDebugDataIsStoredInProgram) {
    DebugDataGuard debugDataGuard;

    const char *sourceCode = "__kernel void\nCB(\n__global unsigned int* src, __global unsigned int* dst)\n{\nint id = (int)get_global_id(0);\ndst[id] = src[id];\n}\n";
    pProgram = Program::create<MockProgram>(
        pContext,
        1,
        &sourceCode,
        &knownSourceSize,
        retVal);

    retVal = pProgram->compile(pProgram->getDevices(), nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_program programToLink = pProgram;
    retVal = pProgram->link(pProgram->getDevices(), nullptr, 1, &programToLink);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(nullptr, pProgram->getDebugData(rootDeviceIndex));
}

using ProgramMultiRootDeviceTests = MultiRootDeviceFixture;

TEST_F(ProgramMultiRootDeviceTests, WhenProgramIsCreatedThenBuildInfosVectorIsProperlyResized) {
    {
        ClDeviceVector deviceVector;
        deviceVector.push_back(device1);
        deviceVector.push_back(device2);

        EXPECT_EQ(1u, deviceVector[0]->getRootDeviceIndex());
        auto program = std::make_unique<MockProgram>(context.get(), false, deviceVector);

        EXPECT_EQ(3u, program->buildInfos.size());
    }
    {
        ClDeviceVector deviceVector;
        deviceVector.push_back(device2);
        deviceVector.push_back(device1);

        EXPECT_EQ(2u, deviceVector[0]->getRootDeviceIndex());
        auto program = std::make_unique<MockProgram>(context.get(), false, deviceVector);

        EXPECT_EQ(3u, program->buildInfos.size());
    }
}

class MockCompilerInterfaceWithGtpinParam : public CompilerInterface {
  public:
    TranslationOutput::ErrorCode link(
        const NEO::Device &device,
        const TranslationInput &input,
        TranslationOutput &output) override {
        gtpinInfoPassed = input.GTPinInput;
        return CompilerInterface::link(device, input, output);
    }
    void *gtpinInfoPassed;
};

TEST_F(ProgramBinTest, GivenSourceKernelWhenLinkingProgramThenGtpinInitInfoIsPassed) {
    void *pIgcInitPtr = reinterpret_cast<void *>(0x1234);
    gtpinSetIgcInit(pIgcInitPtr);
    const char *sourceCode = "__kernel void\nCB(\n__global unsigned int* src, __global unsigned int* dst)\n{\nint id = (int)get_global_id(0);\ndst[id] = src[id];\n}\n";
    pProgram = Program::create<MockProgram>(
        pContext,
        1,
        &sourceCode,
        &knownSourceSize,
        retVal);
    std::unique_ptr<MockCompilerInterfaceWithGtpinParam> mockCompilerInterface(new MockCompilerInterfaceWithGtpinParam);

    retVal = pProgram->compile(pProgram->getDevices(), nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->compilerInterface.reset(mockCompilerInterface.get());

    cl_program programToLink = pProgram;
    retVal = pProgram->link(pProgram->getDevices(), nullptr, 1, &programToLink);

    EXPECT_EQ(pIgcInitPtr, mockCompilerInterface->gtpinInfoPassed);
    mockCompilerInterface.release();
}

TEST(ProgramReplaceDeviceBinary, GivenBinaryZebinThenUseAsBothPackedAndUnpackedBinaryContainer) {
    ZebinTestData::ValidEmptyProgram zebin;
    std::unique_ptr<char[]> src = makeCopy(zebin.storage.data(), zebin.storage.size());
    MockContext context;
    auto device = context.getDevice(0);
    auto rootDeviceIndex = device->getRootDeviceIndex();
    MockProgram program{&context, false, toClDeviceVector(*device)};
    program.replaceDeviceBinary(std::move(src), zebin.storage.size(), rootDeviceIndex);
    ASSERT_EQ(zebin.storage.size(), program.buildInfos[rootDeviceIndex].packedDeviceBinarySize);
    ASSERT_EQ(zebin.storage.size(), program.buildInfos[rootDeviceIndex].unpackedDeviceBinarySize);
    ASSERT_NE(nullptr, program.buildInfos[rootDeviceIndex].packedDeviceBinary);
    ASSERT_NE(nullptr, program.buildInfos[rootDeviceIndex].unpackedDeviceBinary);
    EXPECT_EQ(0, memcmp(program.buildInfos[rootDeviceIndex].packedDeviceBinary.get(), zebin.storage.data(), program.buildInfos[rootDeviceIndex].packedDeviceBinarySize));
    EXPECT_EQ(0, memcmp(program.buildInfos[rootDeviceIndex].unpackedDeviceBinary.get(), zebin.storage.data(), program.buildInfos[rootDeviceIndex].unpackedDeviceBinarySize));
}

TEST(ProgramCallbackTest, whenFunctionIsNullptrThenUserDataNeedsToBeNullptr) {
    void *userData = nullptr;
    EXPECT_TRUE(Program::isValidCallback(nullptr, nullptr));
    EXPECT_FALSE(Program::isValidCallback(nullptr, &userData));
}

void CL_CALLBACK callbackFuncProgram(
    cl_program program,
    void *userData) {
    *reinterpret_cast<bool *>(userData) = true;
}
TEST(ProgramCallbackTest, whenFunctionIsNotNullptrThenUserDataDoesntMatter) {
    void *userData = nullptr;
    EXPECT_TRUE(Program::isValidCallback(callbackFuncProgram, nullptr));
    EXPECT_TRUE(Program::isValidCallback(callbackFuncProgram, &userData));
}

TEST(ProgramCallbackTest, whenInvokeCallbackIsCalledThenFunctionIsProperlyInvoked) {
    bool functionCalled = false;
    MockContext context;
    MockProgram program{&context, false, context.getDevices()};
    program.invokeCallback(callbackFuncProgram, &functionCalled);

    EXPECT_TRUE(functionCalled);

    program.invokeCallback(nullptr, nullptr);
}

TEST(BuildProgramTest, givenMultiDeviceProgramWhenBuildingThenStoreAndProcessBinaryOnlyOncePerRootDevice) {
    MockProgram *pProgram = nullptr;
    std::unique_ptr<char[]> pSource = nullptr;
    size_t sourceSize = 0;
    std::string testFile;

    KernelBinaryHelper kbHelper("CopyBuffer_simd16");

    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd16.cl");

    pSource = loadDataFromFile(
        testFile.c_str(),
        sourceSize);

    ASSERT_NE(0u, sourceSize);
    ASSERT_NE(nullptr, pSource);

    const char *sources[1] = {pSource.get()};

    MockUnrestrictiveContextMultiGPU context;
    cl_int retVal = CL_INVALID_PROGRAM;

    pProgram = Program::create<MockProgram>(
        &context,
        1,
        sources,
        &sourceSize,
        retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_build_status buildStatus;
    for (const auto &device : context.getDevices()) {
        retVal = clGetProgramBuildInfo(pProgram, device, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(CL_BUILD_NONE, buildStatus);
    }

    retVal = clBuildProgram(
        pProgram,
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    for (auto &rootDeviceIndex : context.getRootDeviceIndices()) {
        EXPECT_EQ(1, pProgram->replaceDeviceBinaryCalledPerRootDevice[rootDeviceIndex]);
        EXPECT_EQ(1, pProgram->processGenBinaryCalledPerRootDevice[rootDeviceIndex]);
    }
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(BuildProgramTest, givenMultiDeviceProgramWhenBuildingThenStoreKernelInfoPerEachRootDevice) {
    MockProgram *pProgram = nullptr;
    std::unique_ptr<char[]> pSource = nullptr;
    size_t sourceSize = 0;
    std::string testFile;

    KernelBinaryHelper kbHelper("CopyBuffer_simd16");

    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd16.cl");

    pSource = loadDataFromFile(
        testFile.c_str(),
        sourceSize);

    ASSERT_NE(0u, sourceSize);
    ASSERT_NE(nullptr, pSource);

    const char *sources[1] = {pSource.get()};

    MockUnrestrictiveContextMultiGPU context;
    cl_int retVal = CL_INVALID_PROGRAM;

    pProgram = Program::create<MockProgram>(
        &context,
        1,
        sources,
        &sourceSize,
        retVal);

    EXPECT_NE(nullptr, pProgram);
    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_build_status buildStatus;
    for (const auto &device : context.getDevices()) {
        retVal = clGetProgramBuildInfo(pProgram, device, CL_PROGRAM_BUILD_STATUS, sizeof(buildStatus), &buildStatus, NULL);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(CL_BUILD_NONE, buildStatus);
    }

    retVal = clBuildProgram(
        pProgram,
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);
    for (auto &rootDeviceIndex : context.getRootDeviceIndices()) {
        EXPECT_LT(0u, pProgram->getNumKernels());
        for (auto i = 0u; i < pProgram->getNumKernels(); i++) {
            EXPECT_NE(nullptr, pProgram->getKernelInfo(i, rootDeviceIndex));
        }
    }

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(ProgramTest, whenProgramIsBuiltAsAnExecutableForAtLeastOneDeviceThenIsBuiltMethodReturnsTrue) {
    MockSpecializedContext context;
    MockProgram program(&context, false, context.getDevices());
    EXPECT_FALSE(program.isBuilt());
    program.deviceBuildInfos[context.getDevice(0)].buildStatus = CL_BUILD_SUCCESS;
    program.deviceBuildInfos[context.getDevice(0)].programBinaryType = CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT;
    program.deviceBuildInfos[context.getDevice(1)].buildStatus = CL_BUILD_ERROR;
    EXPECT_FALSE(program.isBuilt());
    program.deviceBuildInfos[context.getDevice(0)].buildStatus = CL_BUILD_SUCCESS;
    program.deviceBuildInfos[context.getDevice(0)].programBinaryType = CL_PROGRAM_BINARY_TYPE_EXECUTABLE;
    EXPECT_TRUE(program.isBuilt());
}

TEST(ProgramTest, givenUnlockedProgramWhenRetainForKernelIsCalledThenProgramIsLocked) {
    MockSpecializedContext context;
    MockProgram program(&context, false, context.getDevices());
    EXPECT_FALSE(program.isLocked());
    program.retainForKernel();
    EXPECT_TRUE(program.isLocked());
}

TEST(ProgramTest, givenLockedProgramWhenReleasingForKernelIsCalledForEachRetainThenProgramIsUnlocked) {
    MockSpecializedContext context;
    MockProgram program(&context, false, context.getDevices());
    EXPECT_FALSE(program.isLocked());
    program.retainForKernel();
    EXPECT_TRUE(program.isLocked());
    program.retainForKernel();
    EXPECT_TRUE(program.isLocked());
    program.releaseForKernel();
    EXPECT_TRUE(program.isLocked());
    program.releaseForKernel();
    EXPECT_FALSE(program.isLocked());
}

TEST_F(ProgramTests, givenValidZebinWithKernelCallingExternalFunctionThenUpdateKernelsBarrierCount) {
    ZebinTestData::ZebinWithExternalFunctionsInfo zebin;

    auto program = std::make_unique<MockProgram>(nullptr, false, toClDeviceVector(*pClDevice));
    program->buildInfos[rootDeviceIndex].unpackedDeviceBinary = makeCopy(zebin.storage.data(), zebin.storage.size());
    program->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = zebin.storage.size();

    auto retVal = program->processGenBinary(*pClDevice);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(2U, program->buildInfos[rootDeviceIndex].kernelInfoArray.size());
    auto &kernelInfo = program->buildInfos[rootDeviceIndex].kernelInfoArray[0];
    EXPECT_EQ(zebin.barrierCount, kernelInfo->kernelDescriptor.kernelAttributes.barrierCount);
}

TEST(ProgramInternalOptionsTests, givenProgramWhenPossibleInternalOptionsCheckedThenLargeGRFOptionIsPresent) {
    MockClDevice device{new MockDevice()};
    MockProgram program(toClDeviceVector(device));
    auto &optsToExtract = program.internalOptionsToExtract;
    EXPECT_EQ(1U, std::count(optsToExtract.begin(), optsToExtract.end(), CompilerOptions::largeGrf));
}

TEST(ProgramInternalOptionsTests, givenProgramWhenPossibleInternalOptionsCheckedThenDefaultGRFOptionIsPresent) {
    MockClDevice device{new MockDevice()};
    MockProgram program(toClDeviceVector(device));
    auto &optsToExtract = program.internalOptionsToExtract;
    EXPECT_EQ(1U, std::count(optsToExtract.begin(), optsToExtract.end(), CompilerOptions::defaultGrf));
}

TEST(ProgramInternalOptionsTests, givenProgramWhenPossibleInternalOptionsCheckedThenNumThreadsPerUsIsPresent) {
    MockClDevice clDevice{new MockDevice()};
    clDevice.device.deviceInfo.threadsPerEUConfigs = {2U, 3U};
    MockProgram program(toClDeviceVector(clDevice));

    auto &optionsToExtract = program.internalOptionsToExtract;
    EXPECT_TRUE(std::find(optionsToExtract.begin(), optionsToExtract.end(), CompilerOptions::numThreadsPerEu) != optionsToExtract.end());

    const char *allowedThreadPerEuCounts[] = {"2", "3"};
    for (auto allowedThreadPerEuCount : allowedThreadPerEuCounts) {
        std::string buildOptions = CompilerOptions::concatenate(CompilerOptions::numThreadsPerEu, allowedThreadPerEuCount);
        std::string expectedOutput = buildOptions;
        std::string internalOptions;
        program.extractInternalOptions(buildOptions, internalOptions);
        EXPECT_EQ(expectedOutput, internalOptions);
    }

    const char *notAllowedThreadPerEuCounts[] = {"1", "4"};
    for (auto notAllowedThreadPerEuCount : notAllowedThreadPerEuCounts) {
        std::string buildOptions = CompilerOptions::concatenate(CompilerOptions::numThreadsPerEu, notAllowedThreadPerEuCount);
        std::string internalOptions;
        program.extractInternalOptions(buildOptions, internalOptions);
        EXPECT_EQ("", internalOptions);
    }
}

TEST(ProgramInternalOptionsTests, givenProgramWhenForceLargeGrfCompilationModeIsSetThenBuildOptionIsAdded) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForceLargeGrfCompilationMode.set(true);

    MockClDevice device{new MockDevice()};
    MockProgram program(toClDeviceVector(device));
    auto internalOptions = program.getInternalOptions();
    EXPECT_FALSE(CompilerOptions::contains(internalOptions, CompilerOptions::largeGrf)) << internalOptions;
    CompilerOptions::applyAdditionalOptions(internalOptions);
    EXPECT_TRUE(CompilerOptions::contains(internalOptions, CompilerOptions::largeGrf)) << internalOptions;

    size_t internalOptionsSize = internalOptions.size();
    CompilerOptions::applyAdditionalOptions(internalOptions);
    EXPECT_EQ(internalOptionsSize, internalOptions.size());
}

TEST(ProgramInternalOptionsTests, givenProgramWhenForceDefaultGrfCompilationModeIsSetThenBuildOptionIsAdded) {
    DebugManagerStateRestore stateRestorer;
    DebugManager.flags.ForceDefaultGrfCompilationMode.set(true);

    MockClDevice device{new MockDevice()};
    MockProgram program(toClDeviceVector(device));
    auto internalOptions = program.getInternalOptions();
    EXPECT_FALSE(CompilerOptions::contains(internalOptions, CompilerOptions::defaultGrf)) << internalOptions;
    CompilerOptions::applyAdditionalOptions(internalOptions);
    EXPECT_TRUE(CompilerOptions::contains(internalOptions, CompilerOptions::defaultGrf)) << internalOptions;

    size_t internalOptionsSize = internalOptions.size();
    CompilerOptions::applyAdditionalOptions(internalOptions);
    EXPECT_EQ(internalOptionsSize, internalOptions.size());
}

TEST(ProgramInternalOptionsTests, givenProgramWhenForceDefaultGrfCompilationModeIsSetThenLargeGrfOptionIsRemoved) {
    DebugManagerStateRestore stateRestorer;
    DebugManager.flags.ForceDefaultGrfCompilationMode.set(true);

    MockClDevice device{new MockDevice()};
    MockProgram program(toClDeviceVector(device));
    auto internalOptions = program.getInternalOptions();
    CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::largeGrf);
    EXPECT_FALSE(CompilerOptions::contains(internalOptions, CompilerOptions::defaultGrf)) << internalOptions;
    EXPECT_TRUE(CompilerOptions::contains(internalOptions, CompilerOptions::largeGrf)) << internalOptions;

    CompilerOptions::applyAdditionalOptions(internalOptions);
    EXPECT_TRUE(CompilerOptions::contains(internalOptions, CompilerOptions::defaultGrf)) << internalOptions;
    EXPECT_FALSE(CompilerOptions::contains(internalOptions, CompilerOptions::largeGrf)) << internalOptions;

    size_t internalOptionsSize = internalOptions.size();
    CompilerOptions::applyAdditionalOptions(internalOptions);
    EXPECT_EQ(internalOptionsSize, internalOptions.size());
}
