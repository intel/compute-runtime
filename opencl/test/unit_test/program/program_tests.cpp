/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/program/program_tests.h"

#include "shared/source/ail/ail_configuration.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/compiler_interface/compiler_warnings/compiler_warnings.h"
#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/addressing_mode_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hash.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/allocations_list.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/surface.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/program/program_initialization.h"
#include "shared/source/utilities/arrayref.h"
#include "shared/test/common/compiler_interface/linker_mock.h"
#include "shared/test/common/device_binary_format/patchtokens_tests.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/mock_file_io.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/libult/global_environment.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_ail_configuration.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_compiler_interface.h"
#include "shared/test/common/mocks/mock_elf.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_usm_memory_pool.h"
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
        auto hwInfo = device.getHardwareInfo();

        auto &compilerProductHelper = device.getCompilerProductHelper();
        compilerProductHelper.adjustHwInfoForIgc(hwInfo);

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
        nullptr);

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
        nullptr);
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
        nullptr);
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

class MinimalProgramFixture {
  public:
    void setUp() {
        clDevice = context.getDevice(0);
        program = std::make_unique<MockProgram>(toClDeviceVector(*clDevice));
    }

    void tearDown() {}

    MockContext context;
    std::unique_ptr<MockProgram> program = nullptr;
    NEO::ClDevice *clDevice = nullptr;
};

using ProgramGetBuildInfoTest = Test<MinimalProgramFixture>;
TEST_F(ProgramGetBuildInfoTest, WhenGettingBuildStatusThenBuildStatusIsReturned) {
    constexpr cl_build_status expectedBuildStatus = CL_BUILD_SUCCESS;
    program->deviceBuildInfos.at(clDevice).buildStatus = expectedBuildStatus;

    cl_build_status paramValue = 0;
    size_t paramValueSizeRet = 0;
    auto retVal = program->getBuildInfo(
        clDevice,
        CL_PROGRAM_BUILD_STATUS,
        sizeof(cl_build_status),
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_build_status), paramValueSizeRet);
    EXPECT_EQ(expectedBuildStatus, paramValue);
}

TEST_F(ProgramGetBuildInfoTest, WhenGettingBuildOptionsThenBuildOptionsAreReturned) {
    constexpr ConstStringRef expectedBuildOptions = "Expected build options";
    program->options = expectedBuildOptions.str();

    size_t paramValueSizeRet = 0u;
    char paramValue[expectedBuildOptions.length() + 1]{};
    auto retVal = program->getBuildInfo(
        clDevice,
        CL_PROGRAM_BUILD_OPTIONS,
        expectedBuildOptions.length() + 1,
        paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedBuildOptions.length() + 1, paramValueSizeRet);
    EXPECT_STREQ(expectedBuildOptions.data(), paramValue);
}

TEST_F(ProgramGetBuildInfoTest, WhenGettingBuildLogThenBuildLogIsReturned) {
    constexpr ConstStringRef expectedBuildLog = "Expected build log";
    program->buildInfos[0].buildLog = expectedBuildLog.str();

    size_t paramValueSizeRet = 0u;
    char paramValue[expectedBuildLog.length() + 1]{};
    auto retVal = program->getBuildInfo(
        clDevice,
        CL_PROGRAM_BUILD_LOG,
        expectedBuildLog.length() + 1,
        paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedBuildLog.length() + 1, paramValueSizeRet);
    EXPECT_STREQ(expectedBuildLog.data(), paramValue);
}

TEST_F(ProgramGetBuildInfoTest, WhenGettingBinaryTypeThenBinaryTypeIsReturned) {
    cl_program_binary_type expectedBinaryType = CL_PROGRAM_BINARY_TYPE_EXECUTABLE;
    program->deviceBuildInfos.at(clDevice).programBinaryType = expectedBinaryType;

    size_t paramValueSizeRet = 0u;
    cl_program_binary_type paramValue{};
    auto retVal = program->getBuildInfo(
        clDevice,
        CL_PROGRAM_BINARY_TYPE,
        sizeof(cl_program_binary_type),
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_program_binary_type), paramValueSizeRet);
    EXPECT_EQ(expectedBinaryType, paramValue);
}

TEST_F(ProgramGetBuildInfoTest, GivenGlobalVariableTotalSizeSetWhenGettingBuildGlobalVariableTotalSizeThenCorrectSizeIsReturned) {
    constexpr size_t expectedGlobalVarSize = 256U;
    program->buildInfos[0].globalVarTotalSize = expectedGlobalVarSize;

    size_t paramValueSizeRet = 0;
    size_t paramValue{};
    auto retVal = program->getBuildInfo(
        clDevice,
        CL_PROGRAM_BUILD_GLOBAL_VARIABLE_TOTAL_SIZE,
        sizeof(size_t),
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(size_t), paramValueSizeRet);
    EXPECT_EQ(expectedGlobalVarSize, paramValue);
}

TEST_F(ProgramGetBuildInfoTest, GivenInvalidParamWhenGettingBuildInfoThenInvalidValueErrorIsReturned) {
    auto retVal = program->getBuildInfo(
        clDevice,
        0,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

using ProgramUpdateBuildLogTest = Test<MinimalProgramFixture>;

TEST_F(ProgramUpdateBuildLogTest, GivenEmptyBuildLogWhenUpdatingBuildLogThenBuildLogIsSet) {
    constexpr ConstStringRef expectedBuildLog = "build log update";
    program->buildInfos[0].buildLog = "";
    program->updateBuildLog(0, expectedBuildLog.data(), expectedBuildLog.size());
    EXPECT_STREQ(expectedBuildLog.data(), program->buildInfos[0].buildLog.c_str());
}

TEST_F(ProgramUpdateBuildLogTest, GivenNonEmptyBuildLogWhenUpdatingBuildLogThenBuildLogIsUpdated) {
    constexpr ConstStringRef buildLogMessage = "build log update";
    program->buildInfos[0].buildLog = "current build log";
    const std::string expectedBuildLog = program->buildInfos[0].buildLog + "\n" + buildLogMessage.str();

    program->updateBuildLog(0, buildLogMessage.data(), buildLogMessage.size());
    EXPECT_STREQ(expectedBuildLog.c_str(), program->buildInfos[0].buildLog.c_str());
}

TEST_F(ProgramUpdateBuildLogTest, GivenNullTerminatedBuildLogWhenUpdatingBuildLogTwiceThenCorrectBuildLogIsSet) {
    std::string buildLogMessage1("build log update 1");
    std::string buildLogMessage2("build log update 2");
    const std::string expectedBuildLog = buildLogMessage1 + "\n" + buildLogMessage2;

    program->buildInfos[0].buildLog = "";
    const char nullChar = '\0';
    buildLogMessage1 += nullChar;
    program->updateBuildLog(0, buildLogMessage1.c_str(), buildLogMessage1.size());
    buildLogMessage2 += nullChar;
    program->updateBuildLog(0, buildLogMessage2.c_str(), buildLogMessage2.size());

    EXPECT_STREQ(expectedBuildLog.c_str(), program->buildInfos[0].buildLog.c_str());
}

TEST_F(ProgramFromBinaryTest, givenProgramWhenItIsBeingBuildThenItContainsGraphicsAllocationInKernelInfo) {
    pProgram->build(pProgram->getDevices(), nullptr);
    auto kernelInfo = pProgram->getKernelInfo(size_t(0), rootDeviceIndex);

    auto graphicsAllocation = kernelInfo->getGraphicsAllocation();
    ASSERT_NE(nullptr, graphicsAllocation);
    EXPECT_TRUE(graphicsAllocation->is32BitAllocation());
    auto &helper = pDevice->getRootDeviceEnvironment().getHelper<GfxCoreHelper>();
    size_t isaPadding = helper.getPaddingForISAAllocation();
    EXPECT_EQ(graphicsAllocation->getUnderlyingBufferSize(), kernelInfo->heapInfo.kernelHeapSize + isaPadding);

    auto kernelIsa = graphicsAllocation->getUnderlyingBuffer();
    EXPECT_NE(kernelInfo->heapInfo.pKernelHeap, kernelIsa);
    EXPECT_EQ(0, memcmp(kernelIsa, kernelInfo->heapInfo.pKernelHeap, kernelInfo->heapInfo.kernelHeapSize));
    auto rootDeviceIndex = graphicsAllocation->getRootDeviceIndex();
    auto gmmHelper = pDevice->getGmmHelper();
    EXPECT_EQ(gmmHelper->decanonize(graphicsAllocation->getGpuBaseAddress()), pDevice->getMemoryManager()->getInternalHeapBaseAddress(rootDeviceIndex, graphicsAllocation->isAllocatedInLocalMemoryPool()));
}

TEST_F(ProgramFromBinaryTest, whenProgramIsBeingRebuildThenOutdatedGlobalBuffersAreFreed) {
    pProgram->build(pProgram->getDevices(), nullptr);
    EXPECT_EQ(nullptr, pProgram->buildInfos[pClDevice->getRootDeviceIndex()].constantSurface);
    EXPECT_EQ(nullptr, pProgram->buildInfos[pClDevice->getRootDeviceIndex()].globalSurface);

    pProgram->buildInfos[pClDevice->getRootDeviceIndex()].constantSurface = std::make_unique<SharedPoolAllocation>(new MockGraphicsAllocation());
    pProgram->processGenBinary(*pClDevice);
    EXPECT_EQ(nullptr, pProgram->buildInfos[pClDevice->getRootDeviceIndex()].constantSurface.get());
    EXPECT_EQ(nullptr, pProgram->buildInfos[pClDevice->getRootDeviceIndex()].globalSurface.get());

    pProgram->buildInfos[pClDevice->getRootDeviceIndex()].globalSurface = std::make_unique<SharedPoolAllocation>(new MockGraphicsAllocation());
    pProgram->processGenBinary(*pClDevice);
    EXPECT_EQ(nullptr, pProgram->buildInfos[pClDevice->getRootDeviceIndex()].constantSurface.get());
    EXPECT_EQ(nullptr, pProgram->buildInfos[pClDevice->getRootDeviceIndex()].globalSurface.get());
}

TEST_F(ProgramFromBinaryTest, GivenUsmPoolAnd2MBAlignmentEnabledWhenProgramIsBeingRebuildThenOutdatedGlobalBuffersAreFreedFromUsmPool) {
    ASSERT_NE(nullptr, this->pContext->getSVMAllocsManager());

    pProgram->build(pProgram->getDevices(), nullptr);
    EXPECT_EQ(nullptr, pProgram->buildInfos[pClDevice->getRootDeviceIndex()].constantSurface);
    EXPECT_EQ(nullptr, pProgram->buildInfos[pClDevice->getRootDeviceIndex()].globalSurface);

    auto usmConstantSurfaceAllocPool = new MockUsmMemAllocPool;
    auto usmGlobalSurfaceAllocPool = new MockUsmMemAllocPool;

    pClDevice->getDevice().resetUsmConstantSurfaceAllocPool(usmConstantSurfaceAllocPool);
    pClDevice->getDevice().resetUsmGlobalSurfaceAllocPool(usmGlobalSurfaceAllocPool);

    auto mockProductHelper = new MockProductHelper;
    pClDevice->getDevice().getRootDeviceEnvironmentRef().productHelper.reset(mockProductHelper);
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = true;

    std::vector<unsigned char> initData(1024, 0x5B);
    WhiteBox<NEO::LinkerInput> linkerInput;
    linkerInput.traits.exportsGlobalConstants = true;
    linkerInput.traits.exportsGlobalVariables = true;

    pProgram->buildInfos[pClDevice->getRootDeviceIndex()].constantSurface.reset(allocateGlobalsSurface(pContext->getSVMAllocsManager(), pClDevice->getDevice(), initData.size(), 0u, true, &linkerInput, initData.data()));
    auto &constantSurface = pProgram->buildInfos[pClDevice->getRootDeviceIndex()].constantSurface;
    EXPECT_TRUE(pClDevice->getDevice().getUsmConstantSurfaceAllocPool()->isInPool(reinterpret_cast<void *>(constantSurface->getGpuAddress())));
    pProgram->processGenBinary(*pClDevice);
    EXPECT_EQ(nullptr, pProgram->buildInfos[pClDevice->getRootDeviceIndex()].constantSurface.get());
    EXPECT_EQ(nullptr, pProgram->buildInfos[pClDevice->getRootDeviceIndex()].globalSurface.get());

    EXPECT_EQ(1u, usmConstantSurfaceAllocPool->freeSVMAllocCalled);
    EXPECT_EQ(0u, usmGlobalSurfaceAllocPool->freeSVMAllocCalled);

    pProgram->buildInfos[pClDevice->getRootDeviceIndex()].globalSurface.reset(allocateGlobalsSurface(pContext->getSVMAllocsManager(), pClDevice->getDevice(), initData.size(), 0u, false, &linkerInput, initData.data()));
    auto &globalSurface = pProgram->buildInfos[pClDevice->getRootDeviceIndex()].globalSurface;
    EXPECT_TRUE(pClDevice->getDevice().getUsmGlobalSurfaceAllocPool()->isInPool(reinterpret_cast<void *>(globalSurface->getGpuAddress())));
    pProgram->processGenBinary(*pClDevice);
    EXPECT_EQ(nullptr, pProgram->buildInfos[pClDevice->getRootDeviceIndex()].constantSurface.get());
    EXPECT_EQ(nullptr, pProgram->buildInfos[pClDevice->getRootDeviceIndex()].globalSurface.get());

    EXPECT_EQ(1u, usmConstantSurfaceAllocPool->freeSVMAllocCalled);
    EXPECT_EQ(1u, usmGlobalSurfaceAllocPool->freeSVMAllocCalled);
}

TEST_F(ProgramFromBinaryTest, givenProgramWhenCleanKernelInfoIsCalledThenKernelAllocationIsFreed) {
    pProgram->build(pProgram->getDevices(), nullptr);
    EXPECT_EQ(1u, pProgram->getNumKernels());
    for (auto i = 0u; i < pProgram->buildInfos.size(); i++) {
        pProgram->cleanCurrentKernelInfo(i);
    }
    EXPECT_EQ(0u, pProgram->getNumKernels());
}

TEST_F(ProgramFromBinaryTest, givenReuseKernelBinariesWhenCleanCurrentKernelInfoThenDecreaseAllocationReuseCounter) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ReuseKernelBinaries.set(1);

    pProgram->build(pProgram->getDevices(), nullptr);
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
    debugManager.flags.ReuseKernelBinaries.set(1);

    pProgram->build(pProgram->getDevices(), nullptr);
    auto &kernelAllocMap = pProgram->peekExecutionEnvironment().memoryManager->getKernelAllocationMap();

    EXPECT_EQ(1u, pProgram->getNumKernels());
    for (auto i = 0u; i < pProgram->buildInfos.size(); i++) {
        pProgram->cleanCurrentKernelInfo(i);
    }
    EXPECT_EQ(0u, pProgram->getNumKernels());
    EXPECT_EQ(0u, kernelAllocMap.size());
}

TEST_F(ProgramFromBinaryTest, givenProgramWithGlobalAndConstAllocationsWhenGettingModuleAllocationsThenAllAreReturned) {
    pProgram->build(pProgram->getDevices(), nullptr);
    pProgram->processGenBinary(*pClDevice);
    pProgram->buildInfos[pClDevice->getRootDeviceIndex()].constantSurface = std::make_unique<SharedPoolAllocation>(new MockGraphicsAllocation());
    pProgram->buildInfos[pClDevice->getRootDeviceIndex()].globalSurface = std::make_unique<SharedPoolAllocation>(new MockGraphicsAllocation());

    auto allocs = pProgram->getModuleAllocations(pClDevice->getRootDeviceIndex());
    EXPECT_EQ(pProgram->getNumKernels() + 2u, allocs.size());

    auto iter = std::find(allocs.begin(), allocs.end(), pProgram->buildInfos[pClDevice->getRootDeviceIndex()].constantSurface->getGraphicsAllocation());
    EXPECT_NE(allocs.end(), iter);

    iter = std::find(allocs.begin(), allocs.end(), pProgram->buildInfos[pClDevice->getRootDeviceIndex()].globalSurface->getGraphicsAllocation());
    EXPECT_NE(allocs.end(), iter);

    iter = std::find(allocs.begin(), allocs.end(), pProgram->buildInfos[pClDevice->getRootDeviceIndex()].kernelInfoArray[0]->getGraphicsAllocation());
    EXPECT_NE(allocs.end(), iter);
}

using ProgramGetNumKernelsTest = Test<NEOProgramFixture>;
TEST_F(ProgramGetNumKernelsTest, givenProgramWithFunctionsWhenGettingNumKernelsFunctionsAreNotExposed) {
    program->resizeAndPopulateKernelInfoArray(2);
    program->exportedFunctionsKernelId = 0;
    EXPECT_EQ(1U, program->getNumKernels());
}

using ProgramGetKernelInfoTest = Test<NEOProgramFixture>;
TEST_F(ProgramGetKernelInfoTest, givenProgramWithFunctionsWhenGettingKernelInfoByIndexThenFunctionsAreNotExposed) {
    program->resizeAndPopulateKernelInfoArray(2);
    program->exportedFunctionsKernelId = 0;
    auto kernelInfo = program->getKernelInfo(size_t(0), uint32_t(0));
    EXPECT_EQ(program->buildInfos[0].kernelInfoArray[1], kernelInfo);
}

TEST_F(ProgramGetKernelInfoTest, givenProgramWithFunctionsWhenGettingKernelInfoByIndexThenCorrespondingKernelInfoIsReturned) {
    program->resizeAndPopulateKernelInfoArray(9);
    program->exportedFunctionsKernelId = 4;

    for (size_t ordinal = 0; ordinal < 8; ordinal++) {
        auto kernelInfo = program->getKernelInfo(ordinal, 0);
        EXPECT_EQ(program->buildInfos[0].kernelInfoArray[ordinal + (program->exportedFunctionsKernelId <= ordinal)], kernelInfo);
    }
}

TEST_F(ProgramGetKernelInfoTest, givenProgramFunctionsWhenGettingKernelInfoByNameThenFunctionsAreNotExposed) {
    EXPECT_EQ(nullptr, program->getKernelInfo(NEO::Zebin::Elf::SectionNames::externalFunctions.data(), uint32_t(0)));
}

HWTEST_F(ProgramFromBinaryTest, givenProgramWhenCleanCurrentKernelInfoIsCalledButGpuIsNotYetDoneThenKernelAllocationIsPutOnDeferredFreeListAndCsrRegistersCacheFlush) {
    auto &csr = pDevice->getGpgpuCommandStreamReceiver();
    EXPECT_TRUE(csr.getTemporaryAllocations().peekIsEmpty());
    pProgram->build(pProgram->getDevices(), nullptr);
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

    pProgram->build(pProgram->getDevices(), nullptr);

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

void MinimumProgramFixture::SetUp() {
    PlatformFixture::setUp();
    cl_device_id device = pPlatform->getClDevice(0);
    rootDeviceIndex = pPlatform->getClDevice(0)->getRootDeviceIndex();
    NEO::ContextFixture::setUp(1, &device);
}

void MinimumProgramFixture::TearDown() {
    NEO::ContextFixture::tearDown();
    NEO::PlatformFixture::tearDown();
}

TEST_F(MinimumProgramFixture, givenEmptyAilWhenCreateProgramWithSourcesThenSourcesDoNotChange) {
    auto pDevice = pContext->getDevice(0);
    auto rootDeviceEnvironment = pDevice->getExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex].get();
    rootDeviceEnvironment->ailConfiguration.reset(nullptr);
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
    zebinPtr->setAsMockCompilerLoadedFile("copybuffer.bin");

    auto device = pPlatform->getClDevice(0);

    // Order of following microtests is important - do not change.
    // Add new microtests at end.

    auto pMockProgram = pProgram;

    // fail build - another build is already in progress
    pMockProgram->setBuildStatus(CL_BUILD_IN_PROGRESS);
    retVal = pProgram->build(pProgram->getDevices(), nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    pMockProgram->setBuildStatus(CL_BUILD_NONE);

    // fail build - CompilerInterface cannot be obtained

    auto executionEnvironment = device->getExecutionEnvironment();
    std::unique_ptr<RootDeviceEnvironment> rootDeviceEnvironment = std::make_unique<NoCompilerInterfaceRootDeviceEnvironment>(*executionEnvironment);
    rootDeviceEnvironment->setHwInfoAndInitHelpers(&device->getHardwareInfo());
    std::swap(rootDeviceEnvironment, executionEnvironment->rootDeviceEnvironments[device->getRootDeviceIndex()]);
    auto p2 = std::make_unique<MockProgram>(toClDeviceVector(*device));
    retVal = p2->build(p2->getDevices(), nullptr);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
    p2.reset(nullptr);
    std::swap(rootDeviceEnvironment, executionEnvironment->rootDeviceEnvironments[device->getRootDeviceIndex()]);

    // fail build - any build error (here caused by specifying unrecognized option)
    retVal = pProgram->build(pProgram->getDevices(), "-invalid-option");
    EXPECT_EQ(CL_BUILD_PROGRAM_FAILURE, retVal);

    // fail build - linked code is corrupted and cannot be postprocessed
    auto p3 = std::make_unique<FailingGenBinaryProgram>(toClDeviceVector(*device));
    p3->sourceCode = "example_kernel(){}";
    p3->createdFrom = Program::CreatedFrom::source;
    retVal = p3->build(p3->getDevices(), nullptr);
    EXPECT_EQ(CL_INVALID_BINARY, retVal);
    p3.reset(nullptr);

    // build successfully - build kernel and write it to Kernel Cache
    zebinPtr->setAsMockCompilerReturnedBinary();
    pMockProgram->clearOptions();
    std::string receivedInternalOptions;

    auto debugVars = NEO::getFclDebugVars();
    debugVars.receivedInternalOptionsOutput = &receivedInternalOptions;
    gEnvironment->fclPushDebugVars(debugVars);
    retVal = pProgram->build(pProgram->getDevices(), nullptr);
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
    retVal = pProgram->build(pProgram->getDevices(), nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // build successfully - kernel is already in Kernel Cache, do not build and take it from Cache
    retVal = pProgram->build(pProgram->getDevices(), nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // fail build - code to be build does not exist
    pMockProgram->sourceCode = ""; // set source code as non-existent (invalid)
    pMockProgram->createdFrom = Program::CreatedFrom::source;
    pMockProgram->setBuildStatus(CL_BUILD_NONE);
    pMockProgram->setCreatedFromBinary(false);
    retVal = pProgram->build(pProgram->getDevices(), nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
}

TEST_F(ProgramFromSourceTest, GivenDuplicateOptionsWhenCreatingWithSourceThenBuildSucceeds) {
    zebinPtr->setAsMockCompilerReturnedBinary();
    retVal = pProgram->build(pProgram->getDevices(), nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pProgram->build(pProgram->getDevices(), CompilerOptions::fastRelaxedMath.data());
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pProgram->build(pProgram->getDevices(), CompilerOptions::fastRelaxedMath.data());
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pProgram->build(pProgram->getDevices(), CompilerOptions::finiteMathOnly.data());
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pProgram->build(pProgram->getDevices(), nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ProgramFromSourceTest, WhenBuildingProgramThenFeaturesAndExtraExtensionsAreNotAdded) {
    zebinPtr->setAsMockCompilerReturnedBinary();
    auto cip = new MockCompilerInterfaceCaptureBuildOptions();
    auto pClDevice = pContext->getDevice(0);
    pClDevice->getExecutionEnvironment()->rootDeviceEnvironments[pClDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);

    auto extensionsOption = static_cast<ClDevice *>(devices[0])->peekCompilerExtensions();
    auto extensionsWithFeaturesOption = static_cast<ClDevice *>(devices[0])->peekCompilerExtensionsWithFeatures();
    EXPECT_FALSE(hasSubstr(cip->buildInternalOptions, extensionsOption));
    EXPECT_FALSE(hasSubstr(cip->buildInternalOptions, extensionsWithFeaturesOption));
    EXPECT_FALSE(hasSubstr(cip->buildInternalOptions, std::string{"+cl_khr_3d_image_writes "}));

    retVal = pProgram->build(pProgram->getDevices(), nullptr);
    EXPECT_TRUE(hasSubstr(cip->buildInternalOptions, extensionsOption));
    EXPECT_FALSE(hasSubstr(cip->buildInternalOptions, extensionsWithFeaturesOption));
    EXPECT_FALSE(hasSubstr(cip->buildInternalOptions, std::string{"+cl_khr_3d_image_writes "}));
}

TEST_F(ProgramFromSourceTest, givenFp64EmulationEnabledWhenBuildingProgramThenExtraExtensionsAreAdded) {
    zebinPtr->setAsMockCompilerReturnedBinary();
    auto cip = new MockCompilerInterfaceCaptureBuildOptions();
    auto pClDevice = static_cast<ClDevice *>(devices[0]);
    pClDevice->getExecutionEnvironment()->setFP64EmulationEnabled();
    pClDevice->getExecutionEnvironment()->rootDeviceEnvironments[pClDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);

    auto extensionsOption = pClDevice->peekCompilerExtensions();
    auto extensionsWithFeaturesOption = pClDevice->peekCompilerExtensionsWithFeatures();
    EXPECT_FALSE(hasSubstr(cip->buildInternalOptions, std::string{"+__opencl_c_fp64"}));
    EXPECT_FALSE(hasSubstr(cip->buildInternalOptions, std::string{"+cl_khr_fp64"}));

    retVal = pProgram->build(pProgram->getDevices(), nullptr);
    EXPECT_TRUE(hasSubstr(cip->buildInternalOptions, std::string{"+__opencl_c_fp64"}));
    EXPECT_TRUE(hasSubstr(cip->buildInternalOptions, std::string{"+cl_khr_fp64"}));
}

TEST_F(ProgramFromSourceTest, WhenBuildingProgramWithOpenClC20ThenExtraExtensionsAreAdded) {
    zebinPtr->setAsMockCompilerReturnedBinary();
    auto cip = new MockCompilerInterfaceCaptureBuildOptions();
    auto pClDevice = pContext->getDevice(0);
    pClDevice->getExecutionEnvironment()->rootDeviceEnvironments[pClDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);
    auto pProgram = std::make_unique<SucceedingGenBinaryProgram>(toClDeviceVector(*pClDevice));
    pProgram->sourceCode = "__kernel mock() {}";
    pProgram->createdFrom = Program::CreatedFrom::source;

    MockProgram::getInternalOptionsCalled = 0;

    auto extensionsOption = static_cast<ClDevice *>(devices[0])->peekCompilerExtensions();
    auto extensionsWithFeaturesOption = static_cast<ClDevice *>(devices[0])->peekCompilerExtensionsWithFeatures();
    EXPECT_FALSE(hasSubstr(cip->buildInternalOptions, std::string{"+cl_khr_3d_image_writes "}));

    retVal = pProgram->build(pProgram->getDevices(), "-cl-std=CL2.0");
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(hasSubstr(cip->buildInternalOptions, std::string{"+cl_khr_3d_image_writes "}));
    EXPECT_EQ(1, MockProgram::getInternalOptionsCalled);
}

TEST_F(ProgramFromSourceTest, WhenBuildingProgramWithOpenClC30ThenFeaturesAreAdded) {
    zebinPtr->setAsMockCompilerReturnedBinary();
    auto cip = new MockCompilerInterfaceCaptureBuildOptions();
    auto pClDevice = pContext->getDevice(0);
    pClDevice->getExecutionEnvironment()->rootDeviceEnvironments[pClDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);
    auto pProgram = std::make_unique<SucceedingGenBinaryProgram>(toClDeviceVector(*pClDevice));
    pProgram->sourceCode = "__kernel mock() {}";
    pProgram->createdFrom = Program::CreatedFrom::source;

    MockProgram::getInternalOptionsCalled = 0;

    auto extensionsOption = static_cast<ClDevice *>(devices[0])->peekCompilerExtensions();
    auto extensionsWithFeaturesOption = static_cast<ClDevice *>(devices[0])->peekCompilerExtensionsWithFeatures();
    EXPECT_FALSE(hasSubstr(cip->buildInternalOptions, extensionsOption));
    EXPECT_FALSE(hasSubstr(cip->buildInternalOptions, extensionsWithFeaturesOption));

    retVal = pProgram->build(pProgram->getDevices(), "-cl-std=CL3.0");
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(hasSubstr(cip->buildInternalOptions, extensionsOption));
    EXPECT_TRUE(hasSubstr(cip->buildInternalOptions, extensionsWithFeaturesOption));
    EXPECT_EQ(1, MockProgram::getInternalOptionsCalled);
}

TEST_F(ProgramFromSourceTest, WhenBuildingProgramWithOpenClC30ThenFeaturesAreAddedOnlyOnce) {
    zebinPtr->setAsMockCompilerReturnedBinary();
    auto cip = new MockCompilerInterfaceCaptureBuildOptions();
    auto pClDevice = pContext->getDevice(0);
    pClDevice->getExecutionEnvironment()->rootDeviceEnvironments[pClDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);
    auto pProgram = std::make_unique<SucceedingGenBinaryProgram>(toClDeviceVector(*pClDevice));
    pProgram->sourceCode = "__kernel mock() {}";
    pProgram->createdFrom = Program::CreatedFrom::source;

    retVal = pProgram->build(pProgram->getDevices(), "-cl-std=CL3.0");
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = pProgram->build(pProgram->getDevices(), "-cl-std=CL3.0");
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

TEST_F(ProgramFromSourceTest, givenFp64EmulationEnabledWhenCompilingProgramThenExtraExtensionsAreAdded) {
    auto pCompilerInterface = new MockCompilerInterfaceCaptureBuildOptions();
    auto pClDevice = static_cast<ClDevice *>(devices[0]);
    pClDevice->getExecutionEnvironment()->setFP64EmulationEnabled();
    pClDevice->getExecutionEnvironment()->rootDeviceEnvironments[pClDevice->getRootDeviceIndex()]->compilerInterface.reset(pCompilerInterface);
    auto extensionsOption = pClDevice->peekCompilerExtensions();
    auto extensionsWithFeaturesOption = pClDevice->peekCompilerExtensionsWithFeatures();
    EXPECT_FALSE(hasSubstr(pCompilerInterface->buildInternalOptions, std::string{"+__opencl_c_fp64"}));
    EXPECT_FALSE(hasSubstr(pCompilerInterface->buildInternalOptions, std::string{"+cl_khr_fp64"}));

    MockProgram::getInternalOptionsCalled = 0;
    retVal = pProgram->compile(pProgram->getDevices(), nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(hasSubstr(pCompilerInterface->buildInternalOptions, std::string{"+__opencl_c_fp64"}));
    EXPECT_TRUE(hasSubstr(pCompilerInterface->buildInternalOptions, std::string{"+cl_khr_fp64"}));

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
    pProgram->createdFrom = Program::CreatedFrom::source;

    auto extensionsOption = pClDevice->peekCompilerExtensions();
    auto extensionsWithFeaturesOption = pClDevice->peekCompilerExtensionsWithFeatures();
    EXPECT_FALSE(hasSubstr(pCompilerInterface->buildInternalOptions, extensionsOption));
    EXPECT_FALSE(hasSubstr(pCompilerInterface->buildInternalOptions, extensionsWithFeaturesOption));

    retVal = pProgram->compile(pProgram->getDevices(), "-cl-std=CL3.0", 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(hasSubstr(pCompilerInterface->buildInternalOptions, extensionsOption));
    EXPECT_TRUE(hasSubstr(pCompilerInterface->buildInternalOptions, extensionsWithFeaturesOption));
}

TEST_F(ProgramFromSourceTest, GivenDumpZEBinWhenBuildingProgramFromSourceThenZebinIsDumped) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DumpZEBin.set(1);
    zebinPtr->setAsMockCompilerReturnedBinary();

    std::string fileName = "dumped_zebin_module.elf";
    EXPECT_FALSE(virtualFileExists(fileName));

    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {
        return NULL;
    });

    retVal = pProgram->build(pProgram->getDevices(), nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(virtualFileExists(fileName));
    removeVirtualFile(fileName);
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
    auto rootDeviceIndex = pContext->getDevice(0)->getRootDeviceIndex();

    zebinPtr->setAsMockCompilerReturnedBinary();

    Callback callback;

    retVal = pProgram->build(pProgram->getDevices(), nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto hash1 = pProgram->getCachedFileName();
    auto kernel1 = pProgram->getKernelInfo(zebinPtr->kernelName, rootDeviceIndex);
    Callback::watch(kernel1);
    EXPECT_NE(nullptr, kernel1);

    retVal = pProgram->build(pProgram->getDevices(), CompilerOptions::fastRelaxedMath.data());
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto hash2 = pProgram->getCachedFileName();
    auto kernel2 = pProgram->getKernelInfo(zebinPtr->kernelName, rootDeviceIndex);
    EXPECT_NE(nullptr, kernel2);
    EXPECT_NE(hash1, hash2);
    Callback::unwatch(kernel1);
    Callback::watch(kernel2);

    retVal = pProgram->build(pProgram->getDevices(), CompilerOptions::finiteMathOnly.data());
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto hash3 = pProgram->getCachedFileName();
    auto kernel3 = pProgram->getKernelInfo(zebinPtr->kernelName, rootDeviceIndex);
    EXPECT_NE(nullptr, kernel3);
    EXPECT_NE(hash1, hash3);
    EXPECT_NE(hash2, hash3);
    Callback::unwatch(kernel2);
    Callback::watch(kernel3);

    pProgram->createdFrom = NEO::Program::CreatedFrom::binary;
    pProgram->setIrBinary(new char[16], true);
    pProgram->setIrBinarySize(16, true);
    retVal = pProgram->build(pProgram->getDevices(), nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto hash4 = pProgram->getCachedFileName();
    auto kernel4 = pProgram->getKernelInfo(zebinPtr->kernelName, rootDeviceIndex);
    EXPECT_NE(nullptr, kernel4);
    EXPECT_EQ(hash3, hash4);
    Callback::unwatch(kernel3);
    Callback::watch(kernel4);

    pProgram->createdFrom = NEO::Program::CreatedFrom::source;
    retVal = pProgram->build(pProgram->getDevices(), nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto hash5 = pProgram->getCachedFileName();
    auto kernel5 = pProgram->getKernelInfo(zebinPtr->kernelName, rootDeviceIndex);
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
    cl_program inputHeaders = nullptr;
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
    MockProgram *p3; // header Program object
    const char *source = "example_kernel(){}";
    size_t sourceSize = std::strlen(source) + 1;
    const char *sources[1] = {source};
    zebinPtr->setAsMockCompilerLoadedFile("copybuffer.bin");
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
    rootDeviceEnvironment->setHwInfoAndInitHelpers(&device->getHardwareInfo());
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

    const auto &compilerProductHelper = pDevice->getRootDeviceEnvironment().getHelper<CompilerProductHelper>();
    if (!compilerProductHelper.isForceToStatelessRequired()) {
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
    MockZebinWrapper zebin{pClDevice->getHardwareInfo()};
    zebin.setAsMockCompilerReturnedBinary();

    auto cip = new MockCompilerInterfaceCaptureBuildOptions();
    auto pProgram = std::make_unique<SucceedingGenBinaryProgram>(toClDeviceVector(*pClDevice));
    pProgram->sourceCode = "__kernel mock() {}";
    pProgram->createdFrom = Program::CreatedFrom::source;
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
    Program *p;
    const char *source = R"===(
example_kernel() {
    line
}
)===";
    const char *sources[1] = {source};

    // According to spec: If lengths is NULL, all strings in the strings argument are considered null-terminated.
    p = Program::create(pContext, 1, sources, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, p);
    delete p;

    // According to spec: If an element in lengths is zero, its accompanying string is null-terminated.
    const size_t sourceSize = 0;
    p = Program::create(pContext, 1, sources, &sourceSize, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, p);
    delete p;

    std::stringstream dataStream(source);
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
    cl_program program = pProgram;
    cl_program nullprogram = nullptr;
    cl_program invprogram = (cl_program)pContext;
    zebinPtr->setAsMockCompilerLoadedFile("copybuffer.bin");

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
    zebinPtr->setAsMockCompilerReturnedBinary();
    retVal = pProgram->link(pProgram->getDevices(), nullptr, 1, &program);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ProgramFromSourceTest, GivenInvalidOptionsWhenCreatingLibraryThenCorrectErrorIsReturned) {
    zebinPtr->setAsMockCompilerLoadedFile("copybuffer.bin");
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
    rootDeviceEnvironment->setHwInfoAndInitHelpers(&device->getHardwareInfo());
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

TEST_F(PatchTokenTests, WhenBuildingProgramThenGwsIsSet) {
    createProgramFromBinary(pContext, pContext->getDevices(), "simple_kernels");

    ASSERT_NE(nullptr, pProgram);
    retVal = pProgram->build(
        pProgram->getDevices(),
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    auto pKernelInfo = pProgram->getKernelInfo("test_get_global_size", rootDeviceIndex);

    ASSERT_NE(static_cast<uint32_t>(-1), pKernelInfo->kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[0]);
    ASSERT_NE(static_cast<uint32_t>(-1), pKernelInfo->kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[1]);
    ASSERT_NE(static_cast<uint32_t>(-1), pKernelInfo->kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[2]);
}

TEST_F(PatchTokenTests, WhenBuildingProgramThenConstantKernelArgsAreAvailable) {
    // PATCH_TOKEN_STATELESS_CONSTANT_MEMORY_OBJECT_KERNEL_ARGUMENT

    createProgramFromBinary(pContext, pContext->getDevices(), "simple_kernels");

    ASSERT_NE(nullptr, pProgram);
    retVal = pProgram->build(
        pProgram->getDevices(),
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pKernelInfo = pProgram->getKernelInfo("constant_kernel", rootDeviceIndex);
    ASSERT_NE(nullptr, pKernelInfo);

    auto pKernel = Kernel::create(
        pProgram,
        *pKernelInfo,
        *pClDevice,
        retVal);

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
    debugManager.flags.DisableKernelRecompilation.set(true);
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
    auto rootDeviceIdx = clDevice->getRootDeviceIndex();

    pProgram = new PatchtokensProgramWithDebugData(*clDevice);
    auto &buildInfo = pProgram->buildInfos[rootDeviceIdx];

    pProgram->packDeviceBinary(*clDevice);
    EXPECT_NE(nullptr, buildInfo.packedDeviceBinary.get());

    auto packedDeviceBinary = ArrayRef<const uint8_t>::fromAny(buildInfo.packedDeviceBinary.get(), buildInfo.packedDeviceBinarySize);
    TargetDevice targetDevice = NEO::getTargetDevice(pDevice->getRootDeviceEnvironment());
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
    debugManager.flags.DisableStatelessToStatefulOptimization.set(false);

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
    debugManager.flags.DisableStatelessToStatefulOptimization.set(true);

    MockProgram program(pContext, false, toClDeviceVector(*pClDevice));
    auto internalOptions = program.getInternalOptions();
    EXPECT_TRUE(CompilerOptions::contains(internalOptions, NEO::CompilerOptions::greaterThan4gbBuffersRequired));
}

TEST_F(ProgramTests, whenGetInternalOptionsThenLSCPolicyIsSet) {
    MockProgram program(pContext, false, toClDeviceVector(*pClDevice));
    auto internalOptions = program.getInternalOptions();
    const auto &compilerProductHelper = pDevice->getRootDeviceEnvironment().getHelper<CompilerProductHelper>();
    auto expectedPolicy = compilerProductHelper.getCachingPolicyOptions(false);
    if (expectedPolicy != nullptr) {
        EXPECT_TRUE(CompilerOptions::contains(internalOptions, expectedPolicy));
    } else {
        EXPECT_FALSE(CompilerOptions::contains(internalOptions, "-cl-store-cache-default"));
        EXPECT_FALSE(CompilerOptions::contains(internalOptions, "-cl-load-cache-default"));
    }
}

HWTEST2_F(ProgramTests, givenDebugFlagSetToWbWhenGetInternalOptionsThenCorrectBuildOptionIsSet, IsAtLeastXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(2);
    MockProgram program(pContext, false, toClDeviceVector(*pClDevice));
    auto internalOptions = program.getInternalOptions();
    EXPECT_TRUE(CompilerOptions::contains(internalOptions, "-cl-store-cache-default=7 -cl-load-cache-default=4"));
}

HWTEST2_F(ProgramTests, givenDebugFlagSetForceAllResourcesUncachedWhenGetInternalOptionsThenCorrectBuildOptionIsSet, IsAtLeastXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(2);
    debugManager.flags.ForceAllResourcesUncached.set(true);
    MockProgram program(pContext, false, toClDeviceVector(*pClDevice));
    auto internalOptions = program.getInternalOptions();
    EXPECT_TRUE(CompilerOptions::contains(internalOptions, "-cl-store-cache-default=2 -cl-load-cache-default=2"));
}

HWTEST2_F(ProgramTests, givenAtLeastXeHpgCoreWhenGetInternalOptionsThenCorrectBuildOptionIsSet, IsAtLeastXeCore) {
    MockProgram program(pContext, false, toClDeviceVector(*pClDevice));
    auto internalOptions = program.getInternalOptions();
    EXPECT_TRUE(CompilerOptions::contains(internalOptions, "-cl-store-cache-default=2 -cl-load-cache-default=4"));
}

TEST_F(ProgramTests, WhenCreatingProgramThenBindlessIsEnabledOnlyIfDebugFlagIsEnabled) {
    using namespace testing;
    DebugManagerStateRestore restorer;

    if (!pDevice->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)) {
        debugManager.flags.UseBindlessMode.set(0);
        MockProgram programNoBindless(pContext, false, toClDeviceVector(*pClDevice));
        auto internalOptionsNoBindless = programNoBindless.getInternalOptions();
        EXPECT_FALSE(CompilerOptions::contains(internalOptionsNoBindless, CompilerOptions::bindlessMode)) << internalOptionsNoBindless;
    }

    debugManager.flags.UseBindlessMode.set(1);
    MockProgram programBindless(pContext, false, toClDeviceVector(*pClDevice));
    auto internalOptionsBindless = programBindless.getInternalOptions();
    EXPECT_TRUE(CompilerOptions::contains(internalOptionsBindless, CompilerOptions::bindlessMode)) << internalOptionsBindless;
}

TEST_F(ProgramTests, GivenForce32BitAddressesWhenProgramIsCreatedThenGreaterThan4gbBuffersRequiredIsCorrectlySet) {
    DebugManagerStateRestore dbgRestorer;
    cl_int retVal = CL_DEVICE_NOT_FOUND;
    debugManager.flags.DisableStatelessToStatefulOptimization.set(false);
    if (pDevice) {
        const_cast<DeviceInfo *>(&pDevice->getDeviceInfo())->force32BitAddresses = true;
        MockProgram program(pContext, false, toClDeviceVector(*pClDevice));
        auto internalOptions = program.getInternalOptions();
        const auto &compilerProductHelper = pDevice->getRootDeviceEnvironment().getHelper<CompilerProductHelper>();
        if (compilerProductHelper.isForceToStatelessRequired()) {
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
    debugManager.flags.DisableStatelessToStatefulOptimization.set(false);
    std::unique_ptr<MockProgram> program{Program::createBuiltInFromSource<MockProgram>("", pContext, pContext->getDevices(), nullptr)};
    auto internalOptions = program->getInternalOptions();
    const auto &compilerProductHelper = pDevice->getRootDeviceEnvironment().getHelper<CompilerProductHelper>();

    if (compilerProductHelper.isForceToStatelessRequired() || is32bit) {
        EXPECT_TRUE(CompilerOptions::contains(internalOptions, NEO::CompilerOptions::greaterThan4gbBuffersRequired)) << internalOptions;
    } else {
        EXPECT_FALSE(CompilerOptions::contains(internalOptions, NEO::CompilerOptions::greaterThan4gbBuffersRequired)) << internalOptions;
    }
}

TEST_F(ProgramTests, GivenStatelessToStatefulIsDisabledWhenProgramIsCreatedThenGreaterThan4gbBuffersRequiredIsCorrectlySet) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.DisableStatelessToStatefulOptimization.set(true);
    std::unique_ptr<MockProgram> program{Program::createBuiltInFromSource<MockProgram>("", pContext, pContext->getDevices(), nullptr)};
    auto internalOptions = program->getInternalOptions();
    EXPECT_TRUE(CompilerOptions::contains(internalOptions, NEO::CompilerOptions::greaterThan4gbBuffersRequired)) << internalOptions;
}

TEST_F(ProgramTests, givenProgramWhenItIsCompiledThenItAlwaysHavePreserveVec3TypeInternalOptionSet) {
    std::unique_ptr<MockProgram> program(Program::createBuiltInFromSource<MockProgram>("", pContext, pContext->getDevices(), nullptr));
    auto internalOptions = program->getInternalOptions();
    EXPECT_TRUE(CompilerOptions::contains(internalOptions, CompilerOptions::preserveVec3Type)) << internalOptions;
}

TEST_F(ProgramTests, Force32BitAddressesWhenProgramIsCreatedThenGreaterThan4gbBuffersRequiredIsCorrectlySet) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.DisableStatelessToStatefulOptimization.set(false);
    const_cast<DeviceInfo *>(&pDevice->getDeviceInfo())->force32BitAddresses = true;
    std::unique_ptr<MockProgram> program{Program::createBuiltInFromSource<MockProgram>("", pContext, pContext->getDevices(), nullptr)};
    auto internalOptions = program->getInternalOptions();
    const auto &compilerProductHelper = pDevice->getRootDeviceEnvironment().getHelper<CompilerProductHelper>();
    if (is32bit || compilerProductHelper.isForceToStatelessRequired()) {
        EXPECT_TRUE(CompilerOptions::contains(internalOptions, CompilerOptions::greaterThan4gbBuffersRequired)) << internalOptions;
    } else {
        EXPECT_FALSE(CompilerOptions::contains(internalOptions, NEO::CompilerOptions::greaterThan4gbBuffersRequired)) << internalOptions;
    }
}

TEST_F(ProgramTests, givenFp64EmulationInDefaultStateWhenProgramIsCreatedThenEnableFP64GenEmuBuildOptionIsNotPresent) {
    std::unique_ptr<MockProgram> program{Program::createBuiltInFromSource<MockProgram>("", pContext, pContext->getDevices(), nullptr)};
    auto internalOptions = program->getInternalOptions();
    EXPECT_FALSE(CompilerOptions::contains(internalOptions, NEO::CompilerOptions::enableFP64GenEmu)) << internalOptions;
}

TEST_F(ProgramTests, givenFp64EmulationEnabledTheWhenProgramIsCreatedThenEnableFP64GenEmuBuildOptionIsPresent) {
    std::unique_ptr<MockProgram> program{Program::createBuiltInFromSource<MockProgram>("", pContext, pContext->getDevices(), nullptr)};
    ASSERT_FALSE(pDevice->getExecutionEnvironment()->isFP64EmulationEnabled());
    pDevice->getExecutionEnvironment()->setFP64EmulationEnabled();
    auto internalOptions = program->getInternalOptions();
    EXPECT_TRUE(CompilerOptions::contains(internalOptions, NEO::CompilerOptions::enableFP64GenEmu)) << internalOptions;
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
        auto argDescriptor = ArgDescriptor(ArgDescriptor::argTPointer);
        argDescriptor.as<ArgDescPointer>().bindful = surfaceStateHeapOffset;
        argDescriptor.as<ArgDescPointer>().bindless = crossThreadDataOffset;

        kernelInfo->kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor);
        program.addKernelInfo(kernelInfo.release(), 0);

        EXPECT_EQ(expectedResult, AddressingModeHelper::containsStatefulAccess(program.buildInfos[0].kernelInfoArray, false));
    }
}

TEST_F(ProgramTests, givenSkipLastExplicitArgWhenContainsStatefulAccessIsCalledThenReturnCorrectResult) {
    std::vector<std::tuple<bool, SurfaceStateHeapOffset, CrossThreadDataOffset>> testParams = {
        {false, 0x40, undefined<CrossThreadDataOffset>},
        {false, undefined<SurfaceStateHeapOffset>, 0x40},
        {false, undefined<SurfaceStateHeapOffset>, undefined<CrossThreadDataOffset>}};

    auto skipLastExplicitArg = true;
    for (auto &[expectedResult, surfaceStateHeapOffset, crossThreadDataOffset] : testParams) {
        MockProgram program(pContext, false, toClDeviceVector(*pClDevice));
        auto kernelInfo = std::make_unique<KernelInfo>();
        kernelInfo->kernelDescriptor.payloadMappings.explicitArgs.clear();
        auto argDescriptor = ArgDescriptor(ArgDescriptor::argTPointer);
        argDescriptor.as<ArgDescPointer>().bindful = surfaceStateHeapOffset;
        argDescriptor.as<ArgDescPointer>().bindless = crossThreadDataOffset;

        kernelInfo->kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor);
        program.addKernelInfo(kernelInfo.release(), 0);

        EXPECT_EQ(expectedResult, AddressingModeHelper::containsStatefulAccess(program.buildInfos[0].kernelInfoArray, skipLastExplicitArg));
    }
}

TEST_F(ProgramTests, givenStatefulAndStatelessAccessesWhenProgramBuildIsCalledThenCorrectResultIsReturned) {
    DebugManagerStateRestore restorer;
    const auto &compilerProductHelper = pDevice->getRootDeviceEnvironment().getHelper<CompilerProductHelper>();

    class MyMockProgram : public Program {
      public:
        using Program::buildInfos;
        using Program::createdFrom;
        using Program::irBinary;
        using Program::irBinarySize;
        using Program::isBuiltIn;
        using Program::isGeneratedByIgc;
        using Program::options;
        using Program::Program;
        using Program::sourceCode;

        void setAddressingMode(bool isStateful) {
            auto kernelInfo = std::make_unique<KernelInfo>();
            kernelInfo->kernelDescriptor.payloadMappings.explicitArgs.clear();
            auto argDescriptor = ArgDescriptor(ArgDescriptor::argTPointer);
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

    std::array<std::tuple<int, bool, bool, int32_t>, 6> testParams = {{{CL_SUCCESS, true, true, -1},
                                                                       {CL_SUCCESS, true, false, -1},
                                                                       {CL_SUCCESS, false, true, -1},
                                                                       {CL_SUCCESS, true, true, 0},
                                                                       {CL_BUILD_PROGRAM_FAILURE, true, true, 1},
                                                                       {CL_SUCCESS, true, false, 1}}};

    for (auto &[expectedResult, isStatefulAccess, isIgcGenerated, debugKey] : testParams) {

        if (!compilerProductHelper.isForceToStatelessRequired()) {
            expectedResult = CL_SUCCESS;
        }
        MyMockProgram program(pContext, false, toClDeviceVector(*pClDevice));
        program.isBuiltIn = false;
        program.sourceCode = "test_kernel";
        program.createdFrom = Program::CreatedFrom::source;
        program.isGeneratedByIgc = isIgcGenerated;
        program.setAddressingMode(isStatefulAccess);
        MockZebinWrapper<>::Descriptor desc{};
        desc.isStateless = true;
        MockZebinWrapper zebin{*defaultHwInfo, desc};
        zebin.setAsMockCompilerReturnedBinary();
        debugManager.flags.FailBuildProgramWithStatefulAccess.set(debugKey);
        if (isStatefulAccess && debugKey == -1 && isIgcGenerated == true) {
            if (compilerProductHelper.failBuildProgramWithStatefulAccessPreference() == true) {
                expectedResult = CL_BUILD_PROGRAM_FAILURE;
            }
        }

        EXPECT_EQ(expectedResult, program.build(toClDeviceVector(*pClDevice), nullptr));
    }

    {
        MyMockProgram programWithBuiltIn(pContext, true, toClDeviceVector(*pClDevice));
        programWithBuiltIn.isBuiltIn = true;
        programWithBuiltIn.irBinary.reset(new char[16]);
        programWithBuiltIn.irBinarySize = 16;
        programWithBuiltIn.setAddressingMode(true);
        MockZebinWrapper<>::Descriptor desc{};
        desc.isStateless = true;
        MockZebinWrapper zebin{*defaultHwInfo, desc};
        zebin.setAsMockCompilerReturnedBinary();
        debugManager.flags.FailBuildProgramWithStatefulAccess.set(1);
        EXPECT_EQ(CL_SUCCESS, programWithBuiltIn.build(toClDeviceVector(*pClDevice), nullptr));
    }
}

TEST_F(ProgramTests, GivenStatelessToStatefulBufferOffsetOptimizationWhenProgramIsCreatedThenBufferOffsetArgIsSet) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.set(1);
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
    debugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.set(0);
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

TEST_F(ProgramTests, whenCreatingFromZebinThenDontAppendEnableZebinFlagToBuildOptions) {
    if (sizeof(void *) != 8U) {
        GTEST_SKIP();
    }

    auto copyHwInfo = *defaultHwInfo;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &compilerProductHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<CompilerProductHelper>();
    compilerProductHelper.adjustHwInfoForIgc(copyHwInfo);

    ZebinTestData::ValidEmptyProgram zebin;
    zebin.elfHeader->machine = copyHwInfo.platform.eProductFamily;

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr, mockRootDeviceIndex));
    auto program = std::make_unique<MockProgram>(toClDeviceVector(*device));
    cl_int retVal = program->createProgramFromBinary(zebin.storage.data(), zebin.storage.size(), *device);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto expectedOptions = "";
    EXPECT_STREQ(expectedOptions, program->options.c_str());
}

TEST_F(ProgramTests, givenProgramFromGenBinaryWhenSLMSizeIsBiggerThenDeviceLimitThenPrintDebugMsgAndReturnError) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.PrintDebugMessages.set(true);

    PatchTokensTestData::ValidProgramWithKernelUsingSlm patchtokensProgram;
    patchtokensProgram.slmMutable->TotalInlineLocalMemorySize = static_cast<uint32_t>(pDevice->getDeviceInfo().localMemSize * 2);
    patchtokensProgram.recalcTokPtr();
    auto program = std::make_unique<MockProgram>(nullptr, false, toClDeviceVector(*pClDevice));
    program->buildInfos[rootDeviceIndex].unpackedDeviceBinary = makeCopy(patchtokensProgram.storage.data(), patchtokensProgram.storage.size());
    program->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = patchtokensProgram.storage.size();

    StreamCapture capture;
    capture.captureStderr();

    auto retVal = program->processGenBinary(*pClDevice);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);

    std::string output = capture.getCapturedStderr();
    const auto &slmInlineSize = patchtokensProgram.slmMutable->TotalInlineLocalMemorySize;
    const auto &localMemSize = pDevice->getDeviceInfo().localMemSize;
    std::string expectedOutput = "Size of SLM (" + std::to_string(slmInlineSize) + ") larger than available (" + std::to_string(localMemSize) + ")\n";
    EXPECT_EQ(expectedOutput, output);
}

TEST_F(ProgramTests, givenExistingConstantSurfacesWhenProcessGenBinaryThenCleanupTheSurfaceOnlyForSpecificDevice) {
    PatchTokensTestData::ValidProgramWithKernelUsingSlm patchtokensProgram;

    auto program = std::make_unique<MockProgram>(nullptr, false, toClDeviceVector(*pClDevice));

    program->buildInfos.resize(2);
    program->buildInfos[0].constantSurface = std::make_unique<SharedPoolAllocation>(pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({rootDeviceIndex, MemoryConstants::cacheLineSize,
                                                                                                                                                       AllocationType::constantSurface, pDevice->getDeviceBitfield()}));
    program->buildInfos[1].constantSurface = std::make_unique<SharedPoolAllocation>(pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({rootDeviceIndex, MemoryConstants::cacheLineSize,
                                                                                                                                                       AllocationType::constantSurface, pDevice->getDeviceBitfield()}));
    program->buildInfos[rootDeviceIndex].unpackedDeviceBinary = makeCopy(patchtokensProgram.storage.data(), patchtokensProgram.storage.size());
    program->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = patchtokensProgram.storage.size();

    auto constantSurface0 = program->buildInfos[0].constantSurface.get();
    EXPECT_NE(nullptr, constantSurface0);
    EXPECT_NE(nullptr, constantSurface0->getGraphicsAllocation());
    auto constantSurface1 = program->buildInfos[1].constantSurface.get();
    EXPECT_NE(nullptr, constantSurface1);
    EXPECT_NE(nullptr, constantSurface1->getGraphicsAllocation());

    auto retVal = program->processGenBinary(*pClDevice);

    EXPECT_EQ(nullptr, program->buildInfos[0].constantSurface);
    EXPECT_EQ(constantSurface1->getGraphicsAllocation(), program->buildInfos[1].constantSurface->getGraphicsAllocation());

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ProgramTests, givenExistingGlobalSurfacesWhenProcessGenBinaryThenCleanupTheSurfaceOnlyForSpecificDevice) {
    PatchTokensTestData::ValidProgramWithKernelUsingSlm patchtokensProgram;

    auto program = std::make_unique<MockProgram>(nullptr, false, toClDeviceVector(*pClDevice));

    program->buildInfos.resize(2);
    program->buildInfos[0].globalSurface = std::make_unique<SharedPoolAllocation>(pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({rootDeviceIndex, MemoryConstants::cacheLineSize,
                                                                                                                                                     AllocationType::globalSurface, pDevice->getDeviceBitfield()}));
    program->buildInfos[1].globalSurface = std::make_unique<SharedPoolAllocation>(pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({rootDeviceIndex, MemoryConstants::cacheLineSize,
                                                                                                                                                     AllocationType::globalSurface, pDevice->getDeviceBitfield()}));
    program->buildInfos[rootDeviceIndex].unpackedDeviceBinary = makeCopy(patchtokensProgram.storage.data(), patchtokensProgram.storage.size());
    program->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = patchtokensProgram.storage.size();

    auto globalSurface0 = program->buildInfos[0].globalSurface.get();
    EXPECT_NE(nullptr, globalSurface0);
    EXPECT_NE(nullptr, globalSurface0->getGraphicsAllocation());
    auto globalSurface1 = program->buildInfos[1].globalSurface.get();
    EXPECT_NE(nullptr, globalSurface1);
    EXPECT_NE(nullptr, globalSurface1->getGraphicsAllocation());

    auto retVal = program->processGenBinary(*pClDevice);

    EXPECT_EQ(nullptr, program->buildInfos[0].globalSurface);
    EXPECT_EQ(globalSurface1->getGraphicsAllocation(), program->buildInfos[1].globalSurface->getGraphicsAllocation());

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ProgramTests, GivenNoCompilerInterfaceRootDeviceEnvironmentWhenRebuildingBinaryThenOutOfHostMemoryErrorIsReturned) {
    auto pDevice = pContext->getDevice(0);
    auto executionEnvironment = pDevice->getExecutionEnvironment();
    std::unique_ptr<RootDeviceEnvironment> rootDeviceEnvironment = std::make_unique<NoCompilerInterfaceRootDeviceEnvironment>(*executionEnvironment);
    rootDeviceEnvironment->setHwInfoAndInitHelpers(&pDevice->getHardwareInfo());
    std::swap(rootDeviceEnvironment, executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    auto program = std::make_unique<MockProgram>(toClDeviceVector(*pDevice));
    EXPECT_NE(nullptr, program);

    MockZebinWrapper zebin{pDevice->getHardwareInfo()};

    cl_int retVal = program->createProgramFromBinary(zebin.binaries[0], zebin.binarySizes[0], *pClDevice);
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
    program->createdFrom = Program::CreatedFrom::source;

    // Ask to build created program without NEO::CompilerOptions::gtpinRera flag.
    cl_int retVal = program->build(program->getDevices(), CompilerOptions::fastRelaxedMath.data());
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Check build options that were applied
    EXPECT_TRUE(CompilerOptions::contains(cip->buildOptions, CompilerOptions::fastRelaxedMath)) << cip->buildOptions;
    EXPECT_FALSE(CompilerOptions::contains(cip->buildOptions, CompilerOptions::gtpinRera)) << cip->buildInternalOptions;

    // Ask to build created program with NEO::CompilerOptions::gtpinRera flag.
    cip->buildOptions.clear();
    cip->buildInternalOptions.clear();
    retVal = program->build(program->getDevices(), CompilerOptions::concatenate(CompilerOptions::gtpinRera, CompilerOptions::finiteMathOnly).c_str());
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Check build options that were applied
    EXPECT_FALSE(CompilerOptions::contains(cip->buildOptions, CompilerOptions::fastRelaxedMath)) << cip->buildOptions;
    EXPECT_TRUE(CompilerOptions::contains(cip->buildOptions, CompilerOptions::finiteMathOnly)) << cip->buildOptions;
    EXPECT_TRUE(CompilerOptions::contains(cip->buildInternalOptions, CompilerOptions::gtpinRera)) << cip->buildInternalOptions;
}

TEST_F(ProgramTests, givenOneApiPvcSendWarWaEnvFalseWhenBuildingProgramThenInternalOptionIsAdded) {
    auto cip = new MockCompilerInterfaceCaptureBuildOptions();
    auto pDevice = pContext->getDevice(0);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);
    auto program = std::make_unique<SucceedingGenBinaryProgram>(toClDeviceVector(*pDevice));
    program->sourceCode = "__kernel mock() {}";
    program->createdFrom = Program::CreatedFrom::source;

    cl_int retVal = program->build(program->getDevices(), "");
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Check internal build options that were applied
    EXPECT_FALSE(CompilerOptions::contains(cip->buildInternalOptions, CompilerOptions::optDisableSendWarWa)) << cip->buildInternalOptions;

    cip->buildOptions.clear();
    cip->buildInternalOptions.clear();
    pDevice->getExecutionEnvironment()->setOneApiPvcWaEnv(false);

    retVal = program->build(program->getDevices(), CompilerOptions::concatenate(CompilerOptions::gtpinRera, CompilerOptions::finiteMathOnly).c_str());
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Check internal build options that were applied
    EXPECT_TRUE(CompilerOptions::contains(cip->buildInternalOptions, CompilerOptions::optDisableSendWarWa)) << cip->buildInternalOptions;
}

TEST_F(ProgramTests, GivenFailureDuringProcessGenBinaryWhenProcessGenBinariesIsCalledThenErrorIsReturned) {
    auto program = std::make_unique<FailingGenBinaryProgram>(toClDeviceVector(*pClDevice));

    std::unordered_map<uint32_t, Program::BuildPhase> phaseReached;
    phaseReached[0] = Program::BuildPhase::binaryCreation;
    cl_int retVal = program->processGenBinaries(toClDeviceVector(*pClDevice), phaseReached);
    EXPECT_EQ(CL_INVALID_BINARY, retVal);
}

class Program32BitTests : public ProgramTests {
  public:
    void SetUp() override {
        debugManager.flags.Force32bitAddressing.set(true);
        ProgramTests::SetUp();
    }
    void TearDown() override {
        ProgramTests::TearDown();
        debugManager.flags.Force32bitAddressing.set(false);
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

    auto &gfxCoreHelper = pClDevice->getGfxCoreHelper();
    if (gfxCoreHelper.isStatelessToStatefulWithOffsetSupported()) {
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

template <int32_t errCodeToReturn, bool spirv = true>
struct CreateProgramFromBinaryMock : public MockProgram {
    using MockProgram::MockProgram;

    cl_int createProgramFromBinary(const void *pBinary,
                                   size_t binarySize, ClDevice &clDevice) override {
        this->irBinary.reset(new char[binarySize]);
        this->irBinarySize = binarySize;
        this->isSpirV = spirv;
        memcpy_s(this->irBinary.get(), binarySize, pBinary, binarySize);
        return errCodeToReturn;
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
    auto prog = clUniquePtr(Program::createFromIL<CreateProgramFromBinaryMock<CL_SUCCESS>>(pContext, reinterpret_cast<const void *>(spirv), sizeof(spirv), retVal));
    ASSERT_NE(nullptr, prog);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ProgramTests, givenProgramCreatedFromILWhenCompileIsCalledThenReuseTheILInsteadOfCallingCompilerInterface) {
    const uint32_t spirv[16] = {0x03022307};
    cl_int errCode = 0;
    auto pProgram = clUniquePtr(Program::createFromIL<MockProgram>(pContext, reinterpret_cast<const void *>(spirv), sizeof(spirv), errCode));
    ASSERT_NE(nullptr, pProgram);
    auto debugVars = NEO::getIgcDebugVars();
    debugVars.forceBuildFailure = true;
    gEnvironment->fclPushDebugVars(debugVars);
    auto compilerErr = pProgram->compile(pProgram->getDevices(), nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, compilerErr);
    gEnvironment->fclPopDebugVars();
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
    MockZebinWrapper zebin{*defaultHwInfo};
    zebin.setAsMockCompilerReturnedBinary();
    errCode = prog->link(prog->getDevices(), nullptr, 2, linkNodes);
    EXPECT_EQ(CL_SUCCESS, errCode);

    prog->release();
    node2->release();
    node1->release();
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
    std::unique_ptr<void, void (*)(void *)> igcDebugVarsAutoPop{&gEnvironment, [](void *) -> void { gEnvironment->igcPopDebugVars(); }};

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
    MockZebinWrapper zebin{pClDevice->getHardwareInfo()};
    zebin.setAsMockCompilerReturnedBinary();
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
    MockZebinWrapper zebin{pClDevice->getHardwareInfo()};
    zebin.setAsMockCompilerReturnedBinary();
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
    MockZebinWrapper zebin{pClDevice->getHardwareInfo()};
    zebin.setAsMockCompilerReturnedBinary();
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
    MockZebinWrapper zebin{pClDevice->getHardwareInfo()};
    zebin.setAsMockCompilerReturnedBinary();
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
    std::unique_ptr<void, void (*)(void *)> igcDebugVarsAutoPop{&gEnvironment, [](void *) -> void { gEnvironment->igcPopDebugVars(); }};

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
    debugManager.flags.InjectInternalBuildOptions.set("-abc");

    auto cip = new MockCompilerInterfaceCaptureBuildOptions();
    auto pDevice = pContext->getDevice(0);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);
    auto program = std::make_unique<SucceedingGenBinaryProgram>(toClDeviceVector(*pDevice));
    program->sourceCode = "__kernel mock() {}";
    program->createdFrom = Program::CreatedFrom::source;

    cl_int retVal = program->build(program->getDevices(), "");
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(CompilerOptions::contains(cip->buildInternalOptions, "-abc")) << cip->buildInternalOptions;
}

TEST_F(ProgramTests, GivenInjectInternalBuildOptionsWhenBuildingBuiltInProgramThenInternalOptionsAreNotAppended) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.InjectInternalBuildOptions.set("-abc");

    auto cip = new MockCompilerInterfaceCaptureBuildOptions();
    auto pDevice = pContext->getDevice(0);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);
    auto program = std::make_unique<SucceedingGenBinaryProgram>(toClDeviceVector(*pDevice));
    program->sourceCode = "__kernel mock() {}";
    program->createdFrom = Program::CreatedFrom::source;
    program->isBuiltIn = true;

    cl_int retVal = program->build(program->getDevices(), "");
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(CompilerOptions::contains(cip->buildInternalOptions, "-abc")) << cip->buildInternalOptions;
}

TEST_F(ProgramTests, GivenAilWithHandleDivergentBarriersSetTrueOptionsWhenCompilingProgramThenInternalOptionsWereAppended) {
    auto cip = new MockCompilerInterfaceCaptureBuildOptions();
    auto pDevice = pContext->getDevice(0);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->ailConfiguration.reset(new MockAILConfiguration());
    auto mockAIL = static_cast<MockAILConfiguration *>(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->ailConfiguration.get());
    mockAIL->setHandleDivergentBarriers(true);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);
    auto program = std::make_unique<SucceedingGenBinaryProgram>(toClDeviceVector(*pDevice));
    program->sourceCode = "__kernel mock() {}";
    program->createdFrom = Program::CreatedFrom::source;

    cl_int retVal = program->build(program->getDevices(), "");
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(CompilerOptions::contains(cip->buildInternalOptions, CompilerOptions::enableDivergentBarriers)) << cip->buildInternalOptions;
}

TEST_F(ProgramTests, GivenAilWithHandleDivergentBarriersSetFalseOptionsWhenCompilingProgramThenInternalOptionsWereAppended) {
    auto cip = new MockCompilerInterfaceCaptureBuildOptions();
    auto pDevice = pContext->getDevice(0);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->ailConfiguration.reset(new MockAILConfiguration());
    auto mockAIL = static_cast<MockAILConfiguration *>(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->ailConfiguration.get());
    mockAIL->setHandleDivergentBarriers(false);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);
    auto program = std::make_unique<SucceedingGenBinaryProgram>(toClDeviceVector(*pDevice));
    program->sourceCode = "__kernel mock() {}";
    program->createdFrom = Program::CreatedFrom::source;

    cl_int retVal = program->build(program->getDevices(), "");
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(CompilerOptions::contains(cip->buildInternalOptions, CompilerOptions::enableDivergentBarriers)) << cip->buildInternalOptions;
}

TEST_F(ProgramTests, GivenInjectInternalBuildOptionsWhenCompilingProgramThenInternalOptionsWereAppended) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.InjectInternalBuildOptions.set("-abc");

    auto cip = new MockCompilerInterfaceCaptureBuildOptions();
    auto pDevice = pContext->getDevice(0);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);
    auto program = std::make_unique<SucceedingGenBinaryProgram>(toClDeviceVector(*pDevice));
    program->sourceCode = "__kernel mock() {}";
    program->createdFrom = Program::CreatedFrom::source;

    cl_int retVal = program->compile(program->getDevices(), nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(CompilerOptions::contains(cip->buildInternalOptions, "-abc")) << cip->buildInternalOptions;
}

TEST_F(ProgramTests, GivenInjectInternalBuildOptionsWhenCompilingBuiltInProgramThenInternalOptionsAreNotAppended) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.InjectInternalBuildOptions.set("-abc");

    auto cip = new MockCompilerInterfaceCaptureBuildOptions();
    auto pDevice = pContext->getDevice(0);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);
    auto program = std::make_unique<SucceedingGenBinaryProgram>(toClDeviceVector(*pDevice));
    program->sourceCode = "__kernel mock() {}";
    program->createdFrom = Program::CreatedFrom::source;
    program->isBuiltIn = true;

    cl_int retVal = program->compile(program->getDevices(), nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(CompilerOptions::contains(cip->buildInternalOptions, "-abc")) << cip->buildInternalOptions;
}

TEST(CreateProgramFromBinaryTests, givenBinaryProgramBuiltInWhenKernelRebuildIsForcedAndIrBinaryIsNotPresentThenSkipRebuildPrintDebugMessageAndReturnSuccess) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.RebuildPrecompiledKernels.set(true);
    cl_int retVal = CL_INVALID_BINARY;

    PatchTokensTestData::ValidEmptyProgram programTokens;

    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    std::unique_ptr<MockProgram> pProgram(Program::createBuiltInFromGenBinary<MockProgram>(nullptr, toClDeviceVector(*clDevice), programTokens.storage.data(), programTokens.storage.size(), &retVal));
    ASSERT_NE(nullptr, pProgram.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    pProgram->irBinarySize = 0x10;
    StreamCapture capture;
    capture.captureStderr();
    debugManager.flags.PrintDebugMessages.set(true);
    retVal = pProgram->createProgramFromBinary(programTokens.storage.data(), programTokens.storage.size(), *clDevice);
    debugManager.flags.PrintDebugMessages.set(false);
    std::string output = capture.getCapturedStderr();
    EXPECT_FALSE(pProgram->requiresRebuild);
    EXPECT_EQ(CL_SUCCESS, retVal);
    std::string expectedOutput = "Skip rebuild binary. Lack of IR, rebuild impossible.\n";
    EXPECT_EQ(expectedOutput, output);
}

TEST(CreateProgramFromBinaryTests, givenBinaryProgramBuiltInWhenKernelRebulildIsForcedAndIrBinaryIsPresentThenDeviceBinaryIsNotUsed) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.RebuildPrecompiledKernels.set(true);
    cl_int retVal = CL_INVALID_BINARY;

    PatchTokensTestData::ValidEmptyProgram programTokens;
    Elf::ElfEncoder<Elf::EI_CLASS_64> elfEncoder;
    elfEncoder.getElfFileHeader().type = Elf::ET_OPENCL_EXECUTABLE;

    constexpr auto mockSpirvDataSize = 0x10;
    uint8_t mockSpirvData[mockSpirvDataSize]{0};
    elfEncoder.appendSection(Elf::SHT_OPENCL_SPIRV, Elf::SectionNamesOpenCl::spirvObject, ArrayRef<const uint8_t>::fromAny(mockSpirvData, mockSpirvDataSize));
    programTokens.storage = elfEncoder.encode();

    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    std::unique_ptr<MockProgram> pProgram(Program::createBuiltInFromGenBinary<MockProgram>(nullptr, toClDeviceVector(*clDevice), programTokens.storage.data(), programTokens.storage.size(), &retVal));
    ASSERT_NE(nullptr, pProgram.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto rootDeviceIndex = clDevice->getRootDeviceIndex();
    pProgram->irBinarySize = 0x10;
    retVal = pProgram->createProgramFromBinary(programTokens.storage.data(), programTokens.storage.size(), *clDevice);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, pProgram->buildInfos[rootDeviceIndex].unpackedDeviceBinary.get());
    EXPECT_EQ(0U, pProgram->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize);
    EXPECT_EQ(nullptr, pProgram->buildInfos[rootDeviceIndex].packedDeviceBinary);
    EXPECT_EQ(0U, pProgram->buildInfos[rootDeviceIndex].packedDeviceBinarySize);
}

TEST(CreateProgramFromBinaryTests, givenBinaryProgramBuiltInWhenKernelRebulildIsForcedAndIrBinaryIsPresentThenRebuildWarningIsEnabled) {
    DebugManagerStateRestore dbgRestorer{};
    debugManager.flags.RebuildPrecompiledKernels.set(true);

    PatchTokensTestData::ValidEmptyProgram programTokens;
    Elf::ElfEncoder<Elf::EI_CLASS_64> elfEncoder;
    elfEncoder.getElfFileHeader().type = Elf::ET_OPENCL_EXECUTABLE;

    constexpr auto mockSpirvDataSize = 0x10;
    uint8_t mockSpirvData[mockSpirvDataSize]{0};
    elfEncoder.appendSection(Elf::SHT_OPENCL_SPIRV, Elf::SectionNamesOpenCl::spirvObject, ArrayRef<const uint8_t>::fromAny(mockSpirvData, mockSpirvDataSize));
    programTokens.storage = elfEncoder.encode();

    cl_int retVal{CL_INVALID_BINARY};

    const auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    std::unique_ptr<MockProgram> pProgram(Program::createBuiltInFromGenBinary<MockProgram>(nullptr, toClDeviceVector(*clDevice), programTokens.storage.data(), programTokens.storage.size(), &retVal));
    ASSERT_NE(nullptr, pProgram.get());
    ASSERT_EQ(CL_SUCCESS, retVal);

    pProgram->irBinarySize = 0x10;
    retVal = pProgram->createProgramFromBinary(programTokens.storage.data(), programTokens.storage.size(), *clDevice);
    ASSERT_EQ(CL_SUCCESS, retVal);

    ASSERT_TRUE(pProgram->requiresRebuild);
}

TEST(CreateProgramFromBinaryTests, givenBinaryProgramNotBuiltInWhenBuiltInKernelRebulildIsForcedThenDeviceBinaryIsNotUsed) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.RebuildPrecompiledKernels.set(true);
    cl_int retVal = CL_INVALID_BINARY;

    PatchTokensTestData::ValidEmptyProgram programTokens;
    Elf::ElfEncoder<Elf::EI_CLASS_64> elfEncoder;
    elfEncoder.getElfFileHeader().type = Elf::ET_OPENCL_EXECUTABLE;

    constexpr auto mockSpirvDataSize = 0x10;
    uint8_t mockSpirvData[mockSpirvDataSize]{0};
    elfEncoder.appendSection(Elf::SHT_OPENCL_SPIRV, Elf::SectionNamesOpenCl::spirvObject, ArrayRef<const uint8_t>::fromAny(mockSpirvData, mockSpirvDataSize));
    programTokens.storage = elfEncoder.encode();
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
    EXPECT_EQ(nullptr, pProgram->buildInfos[rootDeviceIndex].unpackedDeviceBinary.get());
    EXPECT_EQ(0U, pProgram->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize);
    EXPECT_EQ(nullptr, pProgram->buildInfos[rootDeviceIndex].packedDeviceBinary);
    EXPECT_EQ(0U, pProgram->buildInfos[rootDeviceIndex].packedDeviceBinarySize);
}

TEST(CreateProgramFromBinaryTests, givenBinaryWithBindlessKernelWhenCreateProgramFromBinaryThenDeviceBinaryIsNotUsedAndRebuildIsRequired) {
    std::string validZeInfo = std::string("version :\'") + versionToString(NEO::Zebin::ZeInfo::zeInfoDecoderVersion) + R"===('
kernels:
    - name : kernel_bindless
      execution_env:
        simd_size: 8
      payload_arguments:
        - arg_type:        arg_bypointer
          offset:          0
          size:            4
          arg_index:       0
          addrmode:        bindless
          addrspace:       global
          access_type:     readwrite
...
)===";

    uint8_t kernelIsa[8]{0U};
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
    zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(validZeInfo.data(), validZeInfo.size()));
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + "kernel_bindless", {kernelIsa, sizeof(kernelIsa)});
    constexpr auto mockSpirvDataSize = 0x10;
    uint8_t mockSpirvData[mockSpirvDataSize]{0};
    zebin.appendSection(Elf::SHT_OPENCL_SPIRV, Elf::SectionNamesOpenCl::spirvObject, ArrayRef<const uint8_t>::fromAny(mockSpirvData, mockSpirvDataSize));
    zebin.elfHeader->machine = NEO::defaultHwInfo->platform.eProductFamily;

    cl_int retVal = CL_INVALID_BINARY;

    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    std::unique_ptr<MockProgram> pProgram(Program::createBuiltInFromGenBinary<MockProgram>(nullptr, toClDeviceVector(*clDevice), zebin.storage.data(), zebin.storage.size(), &retVal));
    ASSERT_NE(nullptr, pProgram.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto rootDeviceIndex = clDevice->getRootDeviceIndex();
    retVal = pProgram->createProgramFromBinary(zebin.storage.data(), zebin.storage.size(), *clDevice);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, pProgram->buildInfos[rootDeviceIndex].unpackedDeviceBinary.get());
    EXPECT_EQ(0U, pProgram->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize);
    EXPECT_EQ(nullptr, pProgram->buildInfos[rootDeviceIndex].packedDeviceBinary);
    EXPECT_EQ(0U, pProgram->buildInfos[rootDeviceIndex].packedDeviceBinarySize);
    ASSERT_TRUE(pProgram->requiresRebuild);
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
    TranslationOutput::ErrorCode retVal = TranslationOutput::ErrorCode::success;
    int counter = 0;
    const char *spirV = nullptr;
    TranslationOutput::ErrorCode getSpecConstantsInfo(const NEO::Device &device, ArrayRef<const char> srcSpirV, SpecConstantInfo &output) override {
        counter++;
        spirV = srcSpirV.begin();
        return retVal;
    }
    void returnError() {
        retVal = TranslationOutput::ErrorCode::compilationFailure;
    }
};

struct SpecializationConstantRootDeviceEnvironment : public RootDeviceEnvironment {
    SpecializationConstantRootDeviceEnvironment(ExecutionEnvironment &executionEnvironment) : RootDeviceEnvironment(executionEnvironment) {
        compilerInterface.reset(new SpecializationConstantCompilerInterfaceMock());
    }
    CompilerInterface *getCompilerInterface() override {
        return compilerInterface.get();
    }

    bool initAilConfiguration() override {
        return true;
    }
};

struct SetProgramSpecializationConstantTests : public ::testing::Test {
    SetProgramSpecializationConstantTests() : device(new MockDevice()) {}
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
    MockClDevice device;
    std::unique_ptr<SpecializationConstantProgramMock> mockProgram;

    int specValue = 1;
};

TEST_F(SetProgramSpecializationConstantTests, whenSetProgramSpecializationConstantThenBinarySourceIsUsed) {
    auto retVal = mockProgram->setProgramSpecializationConstant(1, sizeof(int), &specValue);

    EXPECT_EQ(1, mockCompiler->counter);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(mockProgram->areSpecializationConstantsInitialized);
    EXPECT_EQ(mockProgram->irBinary.get(), mockCompiler->spirV);
}

TEST_F(SetProgramSpecializationConstantTests, whenSetProgramSpecializationConstantMultipleTimesThenSpecializationConstantsAreInitializedOnce) {
    auto retVal = mockProgram->setProgramSpecializationConstant(1, sizeof(int), &specValue);

    EXPECT_EQ(1, mockCompiler->counter);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(mockProgram->areSpecializationConstantsInitialized);

    retVal = mockProgram->setProgramSpecializationConstant(1, sizeof(int), &specValue);

    EXPECT_EQ(1, mockCompiler->counter);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(mockProgram->areSpecializationConstantsInitialized);
}

TEST_F(SetProgramSpecializationConstantTests, givenInvalidGetSpecConstantsInfoReturnValueWhenSetProgramSpecializationConstantThenErrorIsReturned) {
    mockCompiler->returnError();

    auto retVal = mockProgram->setProgramSpecializationConstant(1, sizeof(int), &specValue);

    EXPECT_EQ(1, mockCompiler->counter);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_FALSE(mockProgram->areSpecializationConstantsInitialized);
}

TEST(setProgramSpecializationConstantTest, givenUninitializedCompilerinterfaceWhenSetProgramSpecializationConstantThenErrorIsReturned) {
    auto executionEnvironment = new MockExecutionEnvironment();
    executionEnvironment->rootDeviceEnvironments[0] = std::make_unique<NoCompilerInterfaceRootDeviceEnvironment>(*executionEnvironment);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
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
    debugManager.flags.PrintProgramBinaryProcessingTime.set(true);
    StreamCapture capture;
    capture.captureStdout();

    createProgramFromBinary(pContext, pContext->getDevices(), "simple_kernels");

    auto retVal = pProgram->build(
        pProgram->getDevices(),
        nullptr);

    auto output = capture.getCapturedStdout();
    EXPECT_FALSE(output.compare(0, 14, "Elapsed time: "));
    EXPECT_EQ(CL_SUCCESS, retVal);
}

struct DebugDataGuard {
    DebugDataGuard(const DebugDataGuard &) = delete;
    DebugDataGuard(DebugDataGuard &&) = delete;

    DebugDataGuard(bool mockZebin) : useMockZebin(mockZebin) {
        for (size_t n = 0; n < sizeof(mockDebugData); n++) {
            mockDebugData[n] = (char)n;
        }

        auto vars = NEO::getIgcDebugVars();
        if (useMockZebin) {
            zebinPtr = std::make_unique<MockZebinWrapper<>>(*defaultHwInfo);
            vars.binaryToReturn = const_cast<unsigned char *>(zebinPtr->binaries[0]);
            vars.binaryToReturnSize = sizeof(unsigned char) * zebinPtr->binarySizes[0];
            NEO::setFclDebugVars(vars);
        }
        vars.debugDataToReturn = mockDebugData;
        vars.debugDataToReturnSize = sizeof(mockDebugData);
        NEO::setIgcDebugVars(vars);
    }

    DebugDataGuard() : DebugDataGuard(false) {}

    ~DebugDataGuard() {
        auto vars = NEO::getIgcDebugVars();
        if (useMockZebin) {
            vars.binaryToReturn = nullptr;
            vars.binaryToReturnSize = 0;
        }
        vars.debugDataToReturn = nullptr;
        vars.debugDataToReturnSize = 0;
        NEO::setIgcDebugVars(vars);
        if (useMockZebin) {
            NEO::setFclDebugVars(vars);
        }
    }

    bool useMockZebin = false;
    std::unique_ptr<MockZebinWrapper<>> zebinPtr;
    char mockDebugData[32];
};

TEST_F(ProgramBinTest, GivenBuildWithDebugDataThenBuildDataAvailableViaGetInfo) {
    DebugDataGuard debugDataGuard{true};

    const char *sourceCode = "__kernel void\nCB(\n__global unsigned int* src, __global unsigned int* dst)\n{\nint id = (int)get_global_id(0);\ndst[id] = src[id];\n}\n";
    pProgram = Program::create<MockProgram>(
        pContext,
        1,
        &sourceCode,
        &knownSourceSize,
        retVal);
    retVal = pProgram->build(pProgram->getDevices(), nullptr);
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
    DebugDataGuard debugDataGuard{true};

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
        gtpinInfoPassed = input.gtPinInput;
        return CompilerInterface::link(device, input, output);
    }
    void *gtpinInfoPassed;
};

TEST_F(ProgramBinTest, GivenSourceKernelWhenLinkingProgramThenGtpinInitInfoIsPassed) {
    MockZebinWrapper zebin{*defaultHwInfo};
    zebin.setAsMockCompilerReturnedBinary();
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

TEST(ProgramReplaceDeviceBinary, GivenBinaryZebinThenUseAsPackedBinaryContainer) {
    ZebinTestData::ValidEmptyProgram zebin;
    std::unique_ptr<char[]> src = makeCopy(zebin.storage.data(), zebin.storage.size());
    MockContext context;
    auto device = context.getDevice(0);
    auto rootDeviceIndex = device->getRootDeviceIndex();
    MockProgram program{&context, false, toClDeviceVector(*device)};
    program.replaceDeviceBinary(std::move(src), zebin.storage.size(), rootDeviceIndex);
    ASSERT_EQ(zebin.storage.size(), program.buildInfos[rootDeviceIndex].packedDeviceBinarySize);
    ASSERT_EQ(0u, program.buildInfos[rootDeviceIndex].unpackedDeviceBinarySize);
    ASSERT_NE(nullptr, program.buildInfos[rootDeviceIndex].packedDeviceBinary);
    ASSERT_EQ(nullptr, program.buildInfos[rootDeviceIndex].unpackedDeviceBinary);
    EXPECT_EQ(0, memcmp(program.buildInfos[rootDeviceIndex].packedDeviceBinary.get(), zebin.storage.data(), program.buildInfos[rootDeviceIndex].packedDeviceBinarySize));
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

    MockZebinWrapper zebin{*defaultHwInfo};
    zebin.setAsMockCompilerReturnedBinary();

    const char *source = "example_kernel(){}";
    size_t sourceSize = std::strlen(source) + 1;
    const char *sources[1] = {source};

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

    MockZebinWrapper zebin{*defaultHwInfo};
    zebin.setAsMockCompilerReturnedBinary();

    const char *source = "example_kernel(){}";
    size_t sourceSize = std::strlen(source) + 1;
    const char *sources[1] = {source};

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
    EXPECT_EQ(1, std::count(optsToExtract.begin(), optsToExtract.end(), CompilerOptions::largeGrf));
}

TEST(ProgramInternalOptionsTests, givenProgramWhenPossibleInternalOptionsCheckedThenDefaultGRFOptionIsPresent) {
    MockClDevice device{new MockDevice()};
    MockProgram program(toClDeviceVector(device));
    auto &optsToExtract = program.internalOptionsToExtract;
    EXPECT_EQ(1, std::count(optsToExtract.begin(), optsToExtract.end(), CompilerOptions::defaultGrf));
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
    debugManager.flags.ForceLargeGrfCompilationMode.set(true);

    MockClDevice device{new MockDevice()};
    MockProgram program(toClDeviceVector(device));
    auto internalOptions = program.getInternalOptions();
    EXPECT_FALSE(CompilerOptions::contains(internalOptions, CompilerOptions::largeGrf)) << internalOptions;
    CompilerOptions::applyAdditionalInternalOptions(internalOptions);
    EXPECT_TRUE(CompilerOptions::contains(internalOptions, CompilerOptions::largeGrf)) << internalOptions;

    size_t internalOptionsSize = internalOptions.size();
    CompilerOptions::applyAdditionalInternalOptions(internalOptions);
    EXPECT_EQ(internalOptionsSize, internalOptions.size());
}

TEST(ProgramInternalOptionsTests, givenProgramWhenForceAutoGrfCompilationModeIsSetThenBuildOptionIsAdded) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceAutoGrfCompilationMode.set(1);

    MockClDevice device{new MockDevice()};
    MockProgram program(toClDeviceVector(device));
    auto options = program.getOptions();
    EXPECT_FALSE(CompilerOptions::contains(options, CompilerOptions::autoGrf)) << options;
    CompilerOptions::applyAdditionalApiOptions(options);
    EXPECT_TRUE(CompilerOptions::contains(options, CompilerOptions::autoGrf)) << options;

    size_t optionsSize = options.size();
    CompilerOptions::applyAdditionalApiOptions(options);
    EXPECT_EQ(optionsSize, options.size());
}

TEST(ProgramInternalOptionsTests, givenProgramWhenForceDefaultGrfCompilationModeIsSetThenBuildOptionIsAdded) {
    DebugManagerStateRestore stateRestorer;
    debugManager.flags.ForceDefaultGrfCompilationMode.set(true);

    MockClDevice device{new MockDevice()};
    MockProgram program(toClDeviceVector(device));
    auto internalOptions = program.getInternalOptions();
    EXPECT_FALSE(CompilerOptions::contains(internalOptions, CompilerOptions::defaultGrf)) << internalOptions;
    CompilerOptions::applyAdditionalInternalOptions(internalOptions);
    EXPECT_TRUE(CompilerOptions::contains(internalOptions, CompilerOptions::defaultGrf)) << internalOptions;

    size_t internalOptionsSize = internalOptions.size();
    CompilerOptions::applyAdditionalInternalOptions(internalOptions);
    EXPECT_EQ(internalOptionsSize, internalOptions.size());
}

TEST(ProgramInternalOptionsTests, givenProgramWhenForceDefaultGrfCompilationModeIsSetThenLargeGrfOptionIsRemoved) {
    DebugManagerStateRestore stateRestorer;
    debugManager.flags.ForceDefaultGrfCompilationMode.set(true);

    MockClDevice device{new MockDevice()};
    MockProgram program(toClDeviceVector(device));
    auto internalOptions = program.getInternalOptions();
    CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::largeGrf);
    EXPECT_FALSE(CompilerOptions::contains(internalOptions, CompilerOptions::defaultGrf)) << internalOptions;
    EXPECT_TRUE(CompilerOptions::contains(internalOptions, CompilerOptions::largeGrf)) << internalOptions;

    CompilerOptions::applyAdditionalInternalOptions(internalOptions);
    EXPECT_TRUE(CompilerOptions::contains(internalOptions, CompilerOptions::defaultGrf)) << internalOptions;
    EXPECT_FALSE(CompilerOptions::contains(internalOptions, CompilerOptions::largeGrf)) << internalOptions;

    size_t internalOptionsSize = internalOptions.size();
    CompilerOptions::applyAdditionalInternalOptions(internalOptions);
    EXPECT_EQ(internalOptionsSize, internalOptions.size());
}

TEST(ProgramPopulateZebinExtendedArgsMetadataTests, givenNonZebinaryFormatWhenCallingPopulateZebinExtendedArgsMetadataThenMetadataIsNotPopulated) {
    MockClDevice device{new MockDevice()};
    MockProgram program(toClDeviceVector(device));
    program.callBasePopulateZebinExtendedArgsMetadataOnce = true;

    constexpr auto mockBinarySize = 0x10;
    const auto &rootDeviceIndex = device.getRootDeviceIndex();
    auto &buildInfo = program.buildInfos[rootDeviceIndex];
    buildInfo.unpackedDeviceBinary.reset(new char[mockBinarySize]{0});
    buildInfo.unpackedDeviceBinarySize = mockBinarySize;

    KernelInfo kernelInfo;
    kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "some_kernel";
    buildInfo.kernelInfoArray.push_back(&kernelInfo);

    ASSERT_TRUE(kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata.empty());
    program.callPopulateZebinExtendedArgsMetadataOnce(rootDeviceIndex);
    EXPECT_TRUE(kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata.empty());
    buildInfo.kernelInfoArray.clear();
}

TEST(ProgramPopulateZebinExtendedArgsMetadataTests, givenZebinaryFormatAndDecodeErrorOnDecodingArgsMetadataWhenCallingPopulateZebinExtendedArgsMetadataThenMetadataIsNotPopulated) {
    MockClDevice device{new MockDevice()};
    MockProgram program(toClDeviceVector(device));
    program.callBasePopulateZebinExtendedArgsMetadataOnce = true;

    const auto &rootDeviceIndex = device.getRootDeviceIndex();
    auto &buildInfo = program.buildInfos[rootDeviceIndex];

    KernelInfo kernelInfo;
    kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "some_kernel";
    buildInfo.kernelInfoArray.push_back(&kernelInfo);

    NEO::ConstStringRef invalidZeInfo = R"===(
kernels:
  - name:            some_kernel
    simd_size:       32
...
)===";

    constexpr auto numBits = is32bit ? Elf::EI_CLASS_32 : Elf::EI_CLASS_64;
    MockElfEncoder<numBits> elfEncoder;
    elfEncoder.getElfFileHeader().type = NEO::Elf::ET_REL;
    elfEncoder.appendSection(Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(invalidZeInfo.data(), invalidZeInfo.size()));
    auto storage = elfEncoder.encode();
    buildInfo.unpackedDeviceBinary.reset(reinterpret_cast<char *>(storage.data()));
    buildInfo.unpackedDeviceBinarySize = storage.size();

    ASSERT_TRUE(kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata.empty());
    ASSERT_EQ(std::string::npos, buildInfo.kernelMiscInfoPos);
    program.callPopulateZebinExtendedArgsMetadataOnce(rootDeviceIndex);
    EXPECT_TRUE(kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata.empty());
    buildInfo.kernelInfoArray.clear();
    buildInfo.unpackedDeviceBinary.release();
}

TEST(ProgramPopulateZebinExtendedArgsMetadataTests, givenZebinaryFormatWithValidZeInfoWhenCallingPopulateExtendedArgsMetadataThenMetadataIsPopulated) {
    MockClDevice device{new MockDevice()};
    MockProgram program(toClDeviceVector(device));
    program.callBasePopulateZebinExtendedArgsMetadataOnce = true;

    const auto &rootDeviceIndex = device.getRootDeviceIndex();
    auto &buildInfo = program.buildInfos[rootDeviceIndex];

    KernelInfo kernelInfo;
    kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "some_kernel";
    buildInfo.kernelInfoArray.push_back(&kernelInfo);

    NEO::ConstStringRef zeInfo = R"===(
kernels:
  - name:            some_kernel
    simd_size:       32
kernels_misc_info:
  - name:            some_kernel
    args_info:
      - name:            a
        index:           0
        address_qualifier: __global
...
)===";
    constexpr auto numBits = is32bit ? Elf::EI_CLASS_32 : Elf::EI_CLASS_64;
    MockElfEncoder<numBits> elfEncoder;
    elfEncoder.getElfFileHeader().type = NEO::Elf::ET_REL;
    elfEncoder.appendSection(Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(zeInfo.data(), zeInfo.size()));
    auto storage = elfEncoder.encode();
    buildInfo.unpackedDeviceBinary.reset(reinterpret_cast<char *>(storage.data()));
    buildInfo.unpackedDeviceBinarySize = storage.size();
    buildInfo.kernelMiscInfoPos = zeInfo.str().find(Zebin::ZeInfo::Tags::kernelMiscInfo.str());
    ASSERT_NE(std::string::npos, buildInfo.kernelMiscInfoPos);

    ASSERT_TRUE(kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata.empty());
    program.callPopulateZebinExtendedArgsMetadataOnce(rootDeviceIndex);
    EXPECT_EQ(1u, kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata.size());
    buildInfo.kernelInfoArray.clear();
    buildInfo.unpackedDeviceBinary.release();
}

TEST(ProgramGenerateDefaultArgsMetadataTests, givenNativeBinaryWhenCallingGenerateDefaultExtendedArgsMetadataThenGenerateMetadataForEachExplicitArgForEachKernel) {
    MockClDevice device{new MockDevice()};
    MockProgram program(toClDeviceVector(device));

    const auto &rootDeviceIndex = device.getRootDeviceIndex();
    auto &buildInfo = program.buildInfos[rootDeviceIndex];

    KernelInfo kernelInfo1, kernelInfo2;
    kernelInfo1.kernelDescriptor.kernelMetadata.kernelName = "some_kernel";
    kernelInfo2.kernelDescriptor.kernelMetadata.kernelName = "another_kernel";
    buildInfo.kernelInfoArray.push_back(&kernelInfo1);
    buildInfo.kernelInfoArray.push_back(&kernelInfo2);

    kernelInfo1.kernelDescriptor.payloadMappings.explicitArgs.resize(2);
    kernelInfo1.kernelDescriptor.payloadMappings.explicitArgs.at(0).type = ArgDescriptor::argTPointer;
    auto &ptr = kernelInfo1.kernelDescriptor.payloadMappings.explicitArgs.at(0).as<ArgDescPointer>();
    ptr.pointerSize = 8u;

    kernelInfo1.kernelDescriptor.payloadMappings.explicitArgs.at(1).type = ArgDescriptor::argTImage;
    auto &img = kernelInfo1.kernelDescriptor.payloadMappings.explicitArgs.at(1).as<ArgDescImage>();
    img.imageType = NEOImageType::imageType2D;

    kernelInfo2.kernelDescriptor.payloadMappings.explicitArgs.resize(1);
    kernelInfo2.kernelDescriptor.payloadMappings.explicitArgs.at(0).type = ArgDescriptor::argTSampler;

    program.callGenerateDefaultExtendedArgsMetadataOnce(rootDeviceIndex);
    EXPECT_EQ(2u, kernelInfo1.kernelDescriptor.explicitArgsExtendedMetadata.size());
    EXPECT_EQ(1u, kernelInfo2.kernelDescriptor.explicitArgsExtendedMetadata.size());

    const auto &argMetadata1 = kernelInfo1.kernelDescriptor.explicitArgsExtendedMetadata[0];
    EXPECT_STREQ("arg0", argMetadata1.argName.c_str());
    auto expectedTypeName = std::string("__opaque_ptr;" + std::to_string(ptr.pointerSize));
    EXPECT_STREQ(expectedTypeName.c_str(), argMetadata1.type.c_str());

    const auto &argMetadata2 = kernelInfo1.kernelDescriptor.explicitArgsExtendedMetadata[1];
    EXPECT_STREQ("arg1", argMetadata2.argName.c_str());
    EXPECT_STREQ("image2d_t", argMetadata2.type.c_str());

    const auto &argMetadata3 = kernelInfo2.kernelDescriptor.explicitArgsExtendedMetadata[0];
    EXPECT_STREQ("arg0", argMetadata3.argName.c_str());
    EXPECT_STREQ("sampler_t", argMetadata3.type.c_str());

    buildInfo.kernelInfoArray.clear();
    buildInfo.unpackedDeviceBinary.release();
}

TEST(ProgramGenerateDefaultArgsMetadataTests, whenGeneratingDefaultMetadataForArgByValueWithManyElementsThenGenerateProperMetadata) {
    MockClDevice device{new MockDevice()};
    MockProgram program(toClDeviceVector(device));

    const auto &rootDeviceIndex = device.getRootDeviceIndex();
    auto &buildInfo = program.buildInfos[rootDeviceIndex];

    KernelInfo kernelInfo;
    kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "some_kernel";
    buildInfo.kernelInfoArray.push_back(&kernelInfo);

    kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.resize(1);
    kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.at(0).type = ArgDescriptor::argTValue;
    auto &argAsVal = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.at(0).as<ArgDescValue>();
    argAsVal.elements.resize(3u);

    argAsVal.elements[0].sourceOffset = 0u;
    argAsVal.elements[0].size = 8u;
    argAsVal.elements[1].sourceOffset = 16u;
    argAsVal.elements[1].size = 8u;
    argAsVal.elements[2].sourceOffset = 8u;
    argAsVal.elements[2].size = 8u;

    program.callGenerateDefaultExtendedArgsMetadataOnce(rootDeviceIndex);
    EXPECT_EQ(1u, kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata.size());

    const auto &argMetadata = kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata[0];
    EXPECT_STREQ("arg0", argMetadata.argName.c_str());

    auto expectedSize = argAsVal.elements[1].sourceOffset + argAsVal.elements[1].size;
    auto expectedTypeName = std::string("__opaque_var;" + std::to_string(expectedSize));
    EXPECT_STREQ(expectedTypeName.c_str(), argMetadata.type.c_str());

    const auto &argTypeTraits = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.at(0).getTraits();
    EXPECT_EQ(KernelArgMetadata::AddrPrivate, argTypeTraits.addressQualifier);
    EXPECT_EQ(KernelArgMetadata::AccessNone, argTypeTraits.accessQualifier);
    EXPECT_TRUE(argTypeTraits.typeQualifiers.empty());

    buildInfo.kernelInfoArray.clear();
    buildInfo.unpackedDeviceBinary.release();
}

TEST(ProgramGenerateDefaultArgsMetadataTests, whenGeneratingDefaultMetadataForArgByValueWithSingleElementEachThenGenerateProperMetadata) {
    MockClDevice device{new MockDevice()};
    MockProgram program(toClDeviceVector(device));

    const auto &rootDeviceIndex = device.getRootDeviceIndex();
    auto &buildInfo = program.buildInfos[rootDeviceIndex];

    KernelInfo kernelInfo;
    kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "some_kernel";
    buildInfo.kernelInfoArray.push_back(&kernelInfo);

    kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.resize(1);
    kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.at(0).type = ArgDescriptor::argTValue;
    auto &argAsVal = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.at(0).as<ArgDescValue>();
    argAsVal.elements.resize(1u);
    argAsVal.elements[0].size = 16u;

    program.callGenerateDefaultExtendedArgsMetadataOnce(rootDeviceIndex);
    EXPECT_EQ(1u, kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata.size());

    const auto &argMetadata = kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata[0];
    EXPECT_STREQ("arg0", argMetadata.argName.c_str());

    auto expectedTypeName = std::string("__opaque;" + std::to_string(argAsVal.elements[0].size));
    EXPECT_STREQ(expectedTypeName.c_str(), argMetadata.type.c_str());

    const auto &argTypeTraits = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.at(0).getTraits();
    EXPECT_EQ(KernelArgMetadata::AddrPrivate, argTypeTraits.addressQualifier);
    EXPECT_EQ(KernelArgMetadata::AccessNone, argTypeTraits.accessQualifier);
    EXPECT_TRUE(argTypeTraits.typeQualifiers.empty());

    buildInfo.kernelInfoArray.clear();
    buildInfo.unpackedDeviceBinary.release();
}

std::array<std::pair<NEOImageType, std::string>, 12> imgTypes{
    std::make_pair<>(NEOImageType::imageTypeBuffer, "image1d_buffer_t"),
    std::make_pair<>(NEOImageType::imageType1D, "image1d_t"),
    std::make_pair<>(NEOImageType::imageType1DArray, "image1d_array_t"),
    std::make_pair<>(NEOImageType::imageType2DArray, "image2d_array_t"),
    std::make_pair<>(NEOImageType::imageType3D, "image3d_t"),
    std::make_pair<>(NEOImageType::imageType2DDepth, "image2d_depth_t"),
    std::make_pair<>(NEOImageType::imageType2DArrayDepth, "image2d_array_depth_t"),
    std::make_pair<>(NEOImageType::imageType2DMSAA, "image2d_msaa_t"),
    std::make_pair<>(NEOImageType::imageType2DMSAADepth, "image2d_msaa_depth_t"),
    std::make_pair<>(NEOImageType::imageType2DArrayMSAA, "image2d_array_msaa_t"),
    std::make_pair<>(NEOImageType::imageType2DArrayMSAADepth, "image2d_array_msaa_depth_t"),
    std::make_pair<>(NEOImageType::imageType2D, "image2d_t")};

using ProgramGenerateDefaultArgsMetadataImagesTest = ::testing::TestWithParam<std::pair<NEOImageType, std::string>>;

TEST_P(ProgramGenerateDefaultArgsMetadataImagesTest, whenGeneratingDefaultMetadataForImageArgThenSetProperCorrespondingTypeName) {
    MockClDevice device{new MockDevice()};
    MockProgram program(toClDeviceVector(device));

    const auto &rootDeviceIndex = device.getRootDeviceIndex();
    auto &buildInfo = program.buildInfos[rootDeviceIndex];

    KernelInfo kernelInfo;
    kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "some_kernel";
    buildInfo.kernelInfoArray.push_back(&kernelInfo);

    const auto &imgTypeTypenamePair = GetParam();

    kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.resize(1);
    auto &arg = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[0];
    arg.type = ArgDescriptor::argTImage;
    arg.as<ArgDescImage>().imageType = imgTypeTypenamePair.first;

    program.callGenerateDefaultExtendedArgsMetadataOnce(rootDeviceIndex);
    EXPECT_EQ(1u, kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata.size());
    const auto &argMetadata = kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata[0];
    EXPECT_STREQ(argMetadata.type.c_str(), imgTypeTypenamePair.second.c_str());

    buildInfo.kernelInfoArray.clear();
    buildInfo.unpackedDeviceBinary.release();
}

INSTANTIATE_TEST_SUITE_P(
    ProgramGenerateDefaultArgsMetadataImagesTestValues,
    ProgramGenerateDefaultArgsMetadataImagesTest,
    ::testing::ValuesIn(imgTypes));

TEST(ProgramGenerateDefaultArgsMetadataTests, whenGeneratingDefaultMetadataForSamplerArgThenSetProperTypeName) {
    MockClDevice device{new MockDevice()};
    MockProgram program(toClDeviceVector(device));

    const auto &rootDeviceIndex = device.getRootDeviceIndex();
    auto &buildInfo = program.buildInfos[rootDeviceIndex];

    KernelInfo kernelInfo;
    kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "some_kernel";
    buildInfo.kernelInfoArray.push_back(&kernelInfo);

    kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.resize(1);
    kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.at(0).type = ArgDescriptor::argTSampler;

    program.callGenerateDefaultExtendedArgsMetadataOnce(rootDeviceIndex);
    EXPECT_EQ(1u, kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata.size());

    const auto &argMetadata = kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata[0];
    EXPECT_STREQ("sampler_t", argMetadata.type.c_str());

    buildInfo.kernelInfoArray.clear();
    buildInfo.unpackedDeviceBinary.release();
}
