/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/program/program_tests.h"

#include "core/elf/reader.h"
#include "core/gmm_helper/gmm_helper.h"
#include "core/helpers/aligned_memory.h"
#include "core/helpers/hash.h"
#include "core/helpers/hw_helper.h"
#include "core/helpers/ptr_math.h"
#include "core/helpers/string.h"
#include "core/memory_manager/graphics_allocation.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "core/unit_tests/utilities/base_object_utils.h"
#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/compiler_interface/patchtokens_decoder.h"
#include "runtime/helpers/hardware_commands_helper.h"
#include "runtime/kernel/kernel.h"
#include "runtime/memory_manager/allocations_list.h"
#include "runtime/memory_manager/surface.h"
#include "runtime/os_interface/os_context.h"
#include "runtime/program/create.inl"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/global_environment.h"
#include "unit_tests/helpers/kernel_binary_helper.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/program/program_from_binary.h"
#include "unit_tests/program/program_with_source.h"

#include "compiler_options.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace NEO;

void ProgramTests::SetUp() {
    DeviceFixture::SetUp();
    cl_device_id device = pDevice;
    ContextFixture::SetUp(1, &device);
}
void ProgramTests::TearDown() {
    ContextFixture::TearDown();
    DeviceFixture::TearDown();
}

void CL_CALLBACK notifyFunc(
    cl_program program,
    void *userData) {
    *((char *)userData) = 'a';
}

std::vector<const char *> BinaryFileNames{
    "CopyBuffer_simd32",
};

std::vector<const char *> SourceFileNames{
    "CopyBuffer_simd8.cl",
};

std::vector<const char *> BinaryForSourceFileNames{
    "CopyBuffer_simd8",
};

std::vector<const char *> KernelNames{
    "CopyBuffer",
};

class MockExecutionEnvironment : public ExecutionEnvironment {
  public:
    MockExecutionEnvironment(CompilerInterface *compilerInterface) : compilerInterface(compilerInterface) {}

    CompilerInterface *getCompilerInterface() override {
        return compilerInterface;
    }

  protected:
    CompilerInterface *compilerInterface;
};

class FailingGenBinaryProgram : public MockProgram {
  public:
    FailingGenBinaryProgram(ExecutionEnvironment &executionEnvironment) : MockProgram(executionEnvironment) {}
    cl_int processGenBinary() override { return CL_INVALID_BINARY; }
};

class SucceedingGenBinaryProgram : public MockProgram {
  public:
    SucceedingGenBinaryProgram(ExecutionEnvironment &executionEnvironment) : MockProgram(executionEnvironment) {}
    cl_int processGenBinary() override { return CL_SUCCESS; }
};

TEST_P(ProgramFromBinaryTest, BuildProgram) {
    cl_device_id device = pDevice;
    ASSERT_NE(nullptr, pProgram);
    retVal = pProgram->build(
        1,
        &device,
        nullptr,
        nullptr,
        nullptr,
        false);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_P(ProgramFromBinaryTest, GetInfo_Context) {
    cl_context contextRet = reinterpret_cast<cl_context>(static_cast<uintptr_t>(0xdeaddead));
    cl_context context = pContext;
    cl_program_info paramName = CL_PROGRAM_CONTEXT;
    size_t param_value_size = sizeof(cl_context);
    size_t param_value_size_ret = 0;

    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = pProgram->getInfo(
        paramName,
        param_value_size,
        &contextRet,
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(context, contextRet);
    EXPECT_EQ(param_value_size, param_value_size_ret);
}

TEST_P(ProgramFromBinaryTest, GetInfo_Binary) {
    cl_program_info paramName = CL_PROGRAM_BINARIES;
    size_t param_value_size = sizeof(unsigned char **);
    size_t param_value_size_ret = 0;

    ASSERT_EQ(retVal, CL_SUCCESS);
    auto testBinary = new char[knownSourceSize];
    ASSERT_NE(nullptr, testBinary);

    // get info with param_value!=nullptr
    retVal = pProgram->getInfo(
        paramName,
        param_value_size,
        &testBinary,
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(param_value_size, param_value_size_ret);

    int cmpVal = strncmp(
        (const char *)knownSource.get(),
        (const char *)testBinary,
        knownSourceSize);

    EXPECT_EQ(0, cmpVal);

    // get info with param_value==nullptr & param_value_size==0
    retVal = pProgram->getInfo(
        paramName,
        0,
        nullptr,
        &param_value_size_ret);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(param_value_size, param_value_size_ret);

    // get info with param_value!= nullptr & param_value_size==0
    retVal = pProgram->getInfo(
        paramName,
        0,
        &testBinary,
        &param_value_size_ret);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    // get info for invalid parameter
    retVal = pProgram->getInfo(
        CL_PROGRAM_BUILD_STATUS,
        0,
        nullptr,
        &param_value_size_ret);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    delete[] testBinary;
    testBinary = nullptr;
}

TEST_P(ProgramFromBinaryTest, GetInfo_BinarySize) {
    cl_program_info paramName = CL_PROGRAM_BINARY_SIZES;
    size_t param_value_size = sizeof(size_t *);
    size_t param_value[1];
    size_t param_value_size_ret = 0;

    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = pProgram->getInfo(
        paramName,
        param_value_size,
        param_value,
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(knownSourceSize, param_value[0]);
    EXPECT_EQ(param_value_size, param_value_size_ret);
}

TEST_P(ProgramFromBinaryTest, GetInfo_NumKernels) {
    cl_program_info paramName = CL_PROGRAM_NUM_KERNELS;
    size_t param_value;
    size_t param_value_size = sizeof(param_value);
    size_t param_value_size_ret;
    cl_device_id device = pDevice;

    // get info successfully
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = pProgram->build(
        1,
        &device,
        nullptr,
        nullptr,
        nullptr,
        false);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = pProgram->getInfo(
        paramName,
        param_value_size,
        &param_value,
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, param_value);
    EXPECT_EQ(param_value_size, param_value_size_ret);

    // get info when Program object does not contain valid executable code
    CreateProgramFromBinary(pContext, &device, BinaryFileName);
    MockProgram *p = pProgram;
    p->SetBuildStatus(CL_BUILD_NONE);

    retVal = pProgram->getInfo(
        paramName,
        param_value_size,
        &param_value,
        &param_value_size_ret);
    ASSERT_EQ(CL_INVALID_PROGRAM_EXECUTABLE, retVal);
}

TEST_P(ProgramFromBinaryTest, GetInfo_KernelNames) {
    cl_program_info paramName = CL_PROGRAM_KERNEL_NAMES;
    size_t paramValueSize = sizeof(size_t *);
    char *param_value = nullptr;
    size_t param_value_size_ret = 0;
    cl_device_id device = pDevice;

    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = pProgram->build(
        1,
        &device,
        nullptr,
        nullptr,
        nullptr,
        false);
    ASSERT_EQ(CL_SUCCESS, retVal);

    // get info successfully about required sizes for kernel names
    retVal = pProgram->getInfo(
        paramName,
        0,
        nullptr,
        &param_value_size_ret);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(0u, param_value_size_ret);

    // get info successfully about kernel names
    param_value = new char[param_value_size_ret];
    paramValueSize = param_value_size_ret;
    ASSERT_NE(param_value, nullptr);

    size_t expectedKernelsStringSize = strlen(KernelName) + 1;
    retVal = pProgram->getInfo(
        paramName,
        paramValueSize,
        param_value,
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, strcmp(KernelName, (char *)param_value));
    EXPECT_EQ(expectedKernelsStringSize, param_value_size_ret);

    // get info when Program object does not contain valid executable code
    CreateProgramFromBinary(pContext, &device, BinaryFileName);
    MockProgram *p = pProgram;
    p->SetBuildStatus(CL_BUILD_NONE);

    retVal = pProgram->getInfo(
        paramName,
        paramValueSize,
        &param_value,
        &param_value_size_ret);
    ASSERT_EQ(CL_INVALID_PROGRAM_EXECUTABLE, retVal);

    delete[] param_value;
    param_value = nullptr;
}

TEST_P(ProgramFromBinaryTest, GetBuildInfo_InvalidDevice) {
    cl_build_status buildStatus;
    cl_program_build_info paramName = CL_PROGRAM_BUILD_STATUS;
    size_t param_value_size = sizeof(buildStatus);
    size_t param_value_size_ret = 0;

    ASSERT_EQ(retVal, CL_SUCCESS);

    // get build info for invalid device
    size_t invalidDevice = 0xdeadbee0;
    retVal = pProgram->getBuildInfo(
        reinterpret_cast<Device *>(invalidDevice),
        paramName,
        param_value_size,
        &buildStatus,
        &param_value_size_ret);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);

    // get build info for corrupted device object
    cl_device_id device = pDevice;
    CreateProgramFromBinary(pContext, &device, BinaryFileName);
    MockProgram *p = pProgram;
    p->SetDevice(reinterpret_cast<Device *>(pContext));

    retVal = pProgram->getBuildInfo(
        reinterpret_cast<Device *>(pContext),
        paramName,
        param_value_size,
        &buildStatus,
        &param_value_size_ret);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_P(ProgramFromBinaryTest, GetBuildInfo_Status) {
    cl_device_id device = pDevice;
    cl_build_status buildStatus;
    cl_program_build_info paramName = CL_PROGRAM_BUILD_STATUS;
    size_t param_value_size = sizeof(buildStatus);
    size_t param_value_size_ret = 0;

    ASSERT_EQ(retVal, CL_SUCCESS);

    retVal = pProgram->getBuildInfo(
        device,
        paramName,
        param_value_size,
        &buildStatus,
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(param_value_size, param_value_size_ret);
    EXPECT_EQ(CL_BUILD_NONE, buildStatus);
}

TEST_P(ProgramFromBinaryTest, GetBuildInfo_Options) {
    cl_device_id device = pDevice;
    cl_program_build_info paramName = CL_PROGRAM_BUILD_OPTIONS;
    size_t param_value_size_ret = 0u;
    char *param_value = nullptr;
    size_t paramValueSize = 0u;

    ASSERT_EQ(retVal, CL_SUCCESS);

    retVal = pProgram->getBuildInfo(
        device,
        paramName,
        0,
        nullptr,
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(param_value_size_ret, 0u);

    param_value = new char[param_value_size_ret];
    paramValueSize = param_value_size_ret;
    ASSERT_NE(param_value, nullptr);

    retVal = pProgram->getBuildInfo(
        device,
        paramName,
        paramValueSize,
        param_value,
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, strcmp("", (char *)param_value));

    delete[] param_value;
    param_value = nullptr;
}

TEST_P(ProgramFromBinaryTest, GetBuildInfo_Log) {
    cl_device_id device = pDevice;
    cl_program_build_info paramName = CL_PROGRAM_BUILD_LOG;
    size_t param_value_size_ret = 0u;
    char *param_value = nullptr;
    size_t paramValueSize = 0u;

    ASSERT_EQ(retVal, CL_SUCCESS);

    retVal = pProgram->getBuildInfo(
        device,
        paramName,
        0,
        nullptr,
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(param_value_size_ret, 0u);

    param_value = new char[param_value_size_ret];
    paramValueSize = param_value_size_ret;
    ASSERT_NE(param_value, nullptr);

    retVal = pProgram->getBuildInfo(
        device,
        paramName,
        paramValueSize,
        param_value,
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, strcmp("", (char *)param_value));

    delete[] param_value;
    param_value = nullptr;
}

TEST_P(ProgramFromBinaryTest, GetBuildInfo_AppendedLog) {
    cl_device_id device = pDevice;
    cl_program_build_info paramName = CL_PROGRAM_BUILD_LOG;
    size_t param_value_size_ret = 0u;
    char *param_value = nullptr;
    size_t paramValueSize = 0u;

    ASSERT_EQ(retVal, CL_SUCCESS);

    retVal = pProgram->getBuildInfo(
        device,
        paramName,
        0,
        nullptr,
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(param_value_size_ret, 0u);

    param_value = new char[param_value_size_ret];
    paramValueSize = param_value_size_ret;
    ASSERT_NE(param_value, nullptr);

    retVal = pProgram->getBuildInfo(
        device,
        paramName,
        paramValueSize,
        param_value,
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, strcmp("", (char *)param_value));

    // Add more text to the log
    pProgram->updateBuildLog(pDevice, "testing", 8);
    pProgram->updateBuildLog(pDevice, "several", 8);

    retVal = pProgram->getBuildInfo(
        device,
        paramName,
        0,
        nullptr,
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GE(param_value_size_ret, 16u);

    delete[] param_value;

    param_value = new char[param_value_size_ret];
    paramValueSize = param_value_size_ret;
    ASSERT_NE(param_value, nullptr);

    retVal = pProgram->getBuildInfo(
        device,
        paramName,
        paramValueSize,
        param_value,
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, retVal);

    ASSERT_NE(nullptr, strstr(param_value, "testing"));

    const char *param_value_continued = strstr(param_value, "testing") + 7;
    EXPECT_NE(nullptr, strstr(param_value_continued, "several"));

    delete param_value;
    param_value = nullptr;
}

TEST_P(ProgramFromBinaryTest, GetBuildInfo_BinaryType) {
    cl_device_id device = pDevice;
    cl_program_build_info paramName = CL_PROGRAM_BINARY_TYPE;
    cl_program_binary_type program_type;
    size_t param_value_size_ret = 0u;
    char *param_value = nullptr;
    size_t paramValueSize = 0u;

    ASSERT_EQ(retVal, CL_SUCCESS);

    // get build info about program binary type - only size of output data container
    retVal = pProgram->getBuildInfo(
        device,
        paramName,
        paramValueSize,
        param_value,
        &param_value_size_ret);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(param_value_size_ret, 0u);

    // get build info about program binary type - full info
    param_value = (char *)&program_type;
    paramValueSize = param_value_size_ret;
    retVal = pProgram->getBuildInfo(
        device,
        paramName,
        paramValueSize,
        param_value,
        &param_value_size_ret);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ((cl_program_binary_type)CL_PROGRAM_BINARY_TYPE_EXECUTABLE, program_type);

    // get build info for invalid parameter
    retVal = pProgram->getBuildInfo(
        device,
        CL_PROGRAM_KERNEL_NAMES,
        0,
        nullptr,
        &param_value_size_ret);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_P(ProgramFromBinaryTest, GetBuildInfo_GlobalVariableTotalSize) {
    cl_device_id device = pDevice;
    size_t globalVarSize = 22;
    cl_program_build_info paramName = CL_PROGRAM_BUILD_GLOBAL_VARIABLE_TOTAL_SIZE;
    size_t param_value_size = sizeof(globalVarSize);
    size_t param_value_size_ret = 0;
    char *param_value = nullptr;

    ASSERT_EQ(retVal, CL_SUCCESS);

    // get build info as is
    param_value = (char *)&globalVarSize;
    retVal = pProgram->getBuildInfo(
        device,
        paramName,
        param_value_size,
        param_value,
        &param_value_size_ret);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(param_value_size_ret, sizeof(globalVarSize));
    EXPECT_EQ(globalVarSize, 0u);

    // Set GlobalVariableTotalSize as 1024
    CreateProgramFromBinary(pContext, &device, BinaryFileName);
    MockProgram *p = pProgram;
    p->SetGlobalVariableTotalSize(1024u);

    // get build info once again
    retVal = pProgram->getBuildInfo(
        device,
        paramName,
        param_value_size,
        param_value,
        &param_value_size_ret);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(param_value_size_ret, sizeof(globalVarSize));
    EXPECT_EQ(globalVarSize, 1024u);
}

TEST_P(ProgramFromBinaryTest, givenProgramWhenItIsBeingBuildThenItContainsGraphicsAllocationInKernelInfo) {
    cl_device_id device = pDevice;
    pProgram->build(1, &device, nullptr, nullptr, nullptr, true);
    auto kernelInfo = pProgram->getKernelInfo(size_t(0));

    auto graphicsAllocation = kernelInfo->getGraphicsAllocation();
    ASSERT_NE(nullptr, graphicsAllocation);
    EXPECT_TRUE(graphicsAllocation->is32BitAllocation());
    EXPECT_EQ(graphicsAllocation->getUnderlyingBufferSize(), kernelInfo->heapInfo.pKernelHeader->KernelHeapSize);

    auto kernelIsa = graphicsAllocation->getUnderlyingBuffer();
    EXPECT_NE(kernelInfo->heapInfo.pKernelHeap, kernelIsa);
    EXPECT_EQ(0, memcmp(kernelIsa, kernelInfo->heapInfo.pKernelHeap, kernelInfo->heapInfo.pKernelHeader->KernelHeapSize));
    auto rootDeviceIndex = graphicsAllocation->getRootDeviceIndex();
    EXPECT_EQ(GmmHelper::decanonize(graphicsAllocation->getGpuBaseAddress()), pProgram->getDevice(0).getMemoryManager()->getInternalHeapBaseAddress(rootDeviceIndex));
}

TEST_P(ProgramFromBinaryTest, givenProgramWhenCleanKernelInfoIsCalledThenKernelAllocationIsFreed) {
    cl_device_id device = pDevice;
    pProgram->build(1, &device, nullptr, nullptr, nullptr, true);
    EXPECT_EQ(1u, pProgram->getNumKernels());
    pProgram->cleanCurrentKernelInfo();
    EXPECT_EQ(0u, pProgram->getNumKernels());
}

HWTEST_P(ProgramFromBinaryTest, givenProgramWhenCleanCurrentKernelInfoIsCalledButGpuIsNotYetDoneThenKernelAllocationIsPutOnDefferedFreeListAndCsrRegistersCacheFlush) {
    cl_device_id device = pDevice;
    auto &csr = pDevice->getGpgpuCommandStreamReceiver();
    EXPECT_TRUE(csr.getTemporaryAllocations().peekIsEmpty());
    pProgram->build(1, &device, nullptr, nullptr, nullptr, true);
    auto kernelAllocation = pProgram->getKernelInfo(size_t(0))->getGraphicsAllocation();
    kernelAllocation->updateTaskCount(100, csr.getOsContext().getContextId());
    *csr.getTagAddress() = 0;
    pProgram->cleanCurrentKernelInfo();
    EXPECT_FALSE(csr.getTemporaryAllocations().peekIsEmpty());
    EXPECT_EQ(csr.getTemporaryAllocations().peekHead(), kernelAllocation);
    EXPECT_TRUE(this->pDevice->getUltCommandStreamReceiver<FamilyType>().requiresInstructionCacheFlush);
}

HWTEST_P(ProgramFromBinaryTest, givenIsaAllocationUsedByMultipleCsrsWhenItIsDeletedItRegistersCacheFlushInEveryCsrThatUsedIt) {
    auto &csr0 = this->pDevice->getUltCommandStreamReceiverFromIndex<FamilyType>(0u);
    auto &csr1 = this->pDevice->getUltCommandStreamReceiverFromIndex<FamilyType>(1u);

    cl_device_id device = pDevice;

    pProgram->build(1, &device, nullptr, nullptr, nullptr, true);

    auto kernelAllocation = pProgram->getKernelInfo(size_t(0))->getGraphicsAllocation();

    csr0.makeResident(*kernelAllocation);
    csr1.makeResident(*kernelAllocation);

    csr0.processResidency(csr0.getResidencyAllocations());
    csr1.processResidency(csr1.getResidencyAllocations());

    csr0.makeNonResident(*kernelAllocation);
    csr1.makeNonResident(*kernelAllocation);

    EXPECT_FALSE(csr0.requiresInstructionCacheFlush);
    EXPECT_FALSE(csr1.requiresInstructionCacheFlush);

    pProgram->cleanCurrentKernelInfo();
    EXPECT_TRUE(csr0.requiresInstructionCacheFlush);
    EXPECT_TRUE(csr1.requiresInstructionCacheFlush);
}

TEST_P(ProgramFromSourceTest, CreateWithSource_Build) {
    KernelBinaryHelper kbHelper(BinaryFileName, true);

    cl_device_id deviceList = {0};
    char data[4] = {0};

    cl_device_id usedDevice = pPlatform->getDevice(0);

    CreateProgramWithSource(
        pContext,
        &usedDevice,
        SourceFileName);

    // Order of following microtests is important - do not change.
    // Add new microtests at end.

    auto pMockProgram = pProgram;

    // invalid build parameters: combinations of numDevices & deviceList
    retVal = pProgram->build(1, nullptr, nullptr, nullptr, nullptr, false);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = pProgram->build(0, &deviceList, nullptr, nullptr, nullptr, false);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    // invalid build parameters: combinations of funcNotify & userData
    retVal = pProgram->build(0, nullptr, nullptr, nullptr, &data[0], false);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    // invalid build parameters: invalid content of deviceList
    retVal = pProgram->build(1, &deviceList, nullptr, nullptr, nullptr, false);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);

    // fail build - another build is already in progress
    pMockProgram->SetBuildStatus(CL_BUILD_IN_PROGRESS);
    retVal = pProgram->build(0, nullptr, nullptr, nullptr, nullptr, false);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    pMockProgram->SetBuildStatus(CL_BUILD_NONE);

    // fail build - CompilerInterface cannot be obtained
    auto noCompilerInterfaceExecutionEnvironment = std::make_unique<MockExecutionEnvironment>(nullptr);
    auto p2 = std::make_unique<MockProgram>(*noCompilerInterfaceExecutionEnvironment);
    retVal = p2->build(0, nullptr, nullptr, nullptr, nullptr, false);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
    p2.reset(nullptr);
    noCompilerInterfaceExecutionEnvironment.reset();

    // fail build - any build error (here caused by specifying unrecognized option)
    retVal = pProgram->build(0, nullptr, "-invalid-option", nullptr, nullptr, false);
    EXPECT_EQ(CL_BUILD_PROGRAM_FAILURE, retVal);

    // fail build - linked code is corrupted and cannot be postprocessed
    auto p3 = std::make_unique<FailingGenBinaryProgram>(*pPlatform->getDevice(0)->getExecutionEnvironment());
    Device *device = pPlatform->getDevice(0);
    p3->setDevice(device);
    std::string testFile;
    size_t sourceSize;
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd8.cl"); // source file
    auto pSourceBuffer = loadDataFromFile(testFile.c_str(), sourceSize);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSourceBuffer);
    p3->sourceCode = pSourceBuffer.get();
    retVal = p3->build(0, nullptr, nullptr, nullptr, nullptr, false);
    EXPECT_EQ(CL_INVALID_BINARY, retVal);
    p3.reset(nullptr);

    // build successfully without notifyFunc - build kernel and write it to Kernel Cache
    pMockProgram->ClearOptions();
    //    retVal = p->ClearKernelCache();
    //    EXPECT_EQ(0, retVal);
    retVal = pProgram->build(0, nullptr, nullptr, nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(CompilerOptions::contains(pProgram->getInternalOptions(), pPlatform->peekCompilerExtensions())) << pProgram->getInternalOptions();

    // get build log
    size_t param_value_size_ret = 0u;
    retVal = pProgram->getBuildInfo(
        device,
        CL_PROGRAM_BUILD_LOG,
        0,
        nullptr,
        &param_value_size_ret);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(param_value_size_ret, 0u);

    // get build log when the log does not exist
    pMockProgram->ClearLog();
    retVal = pProgram->getBuildInfo(
        device,
        CL_PROGRAM_BUILD_LOG,
        0,
        nullptr,
        &param_value_size_ret);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(param_value_size_ret, 0u);

    // build successfully without notifyFunc - build kernel but do not write it to Kernel Cache (kernel is already in the Cache)
    pMockProgram->SetBuildStatus(CL_BUILD_NONE);
    retVal = pProgram->build(0, nullptr, nullptr, nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // build successfully with notifyFunc - duplicate build (kernel already built), do not build and just take it
    retVal = pProgram->build(0, nullptr, nullptr, notifyFunc, &data[0], false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ('a', data[0]);

    // build successfully without notifyFunc - kernel is already in Kernel Cache, do not build and take it from Cache
    retVal = pProgram->build(0, nullptr, nullptr, nullptr, nullptr, true);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // fail build - code to be build does not exist
    pMockProgram->sourceCode = ""; // set source code as non-existent (invalid)
    pMockProgram->SetBuildStatus(CL_BUILD_NONE);
    pMockProgram->SetCreatedFromBinary(false);
    retVal = pProgram->build(0, nullptr, nullptr, nullptr, nullptr, false);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
}

TEST_P(ProgramFromSourceTest, CreateWithSource_Build_Options_Duplicate) {
    KernelBinaryHelper kbHelper(BinaryFileName, false);

    retVal = pProgram->build(0, nullptr, nullptr, nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pProgram->build(0, nullptr, CompilerOptions::fastRelaxedMath, nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pProgram->build(0, nullptr, CompilerOptions::fastRelaxedMath, nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pProgram->build(0, nullptr, CompilerOptions::finiteMathOnly, nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pProgram->build(0, nullptr, nullptr, nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);
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
TEST_P(ProgramFromSourceTest, CreateWithSource_BuildFromCache) {
    KernelBinaryHelper kbHelper(BinaryFileName, true);

    cl_device_id usedDevice = pPlatform->getDevice(0);
    CreateProgramWithSource(
        pContext,
        &usedDevice,
        SourceFileName);

    Callback callback;

    retVal = pProgram->build(0, nullptr, nullptr, nullptr, nullptr, true);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto hash1 = pProgram->getCachedFileName();
    auto kernel1 = pProgram->getKernelInfo("CopyBuffer");
    Callback::watch(kernel1);
    EXPECT_NE(nullptr, kernel1);

    retVal = pProgram->build(0, nullptr, CompilerOptions::fastRelaxedMath, nullptr, nullptr, true);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto hash2 = pProgram->getCachedFileName();
    auto kernel2 = pProgram->getKernelInfo("CopyBuffer");
    EXPECT_NE(nullptr, kernel2);
    EXPECT_NE(hash1, hash2);
    Callback::unwatch(kernel1);
    Callback::watch(kernel2);

    retVal = pProgram->build(0, nullptr, CompilerOptions::finiteMathOnly, nullptr, nullptr, true);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto hash3 = pProgram->getCachedFileName();
    auto kernel3 = pProgram->getKernelInfo("CopyBuffer");
    EXPECT_NE(nullptr, kernel3);
    EXPECT_NE(hash1, hash3);
    EXPECT_NE(hash2, hash3);
    Callback::unwatch(kernel2);
    Callback::watch(kernel3);

    retVal = pProgram->build(0, nullptr, nullptr, nullptr, nullptr, true);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto hash4 = pProgram->getCachedFileName();
    auto kernel4 = pProgram->getKernelInfo("CopyBuffer");
    EXPECT_NE(nullptr, kernel4);
    EXPECT_EQ(hash1, hash4);
    Callback::unwatch(kernel3);
}

TEST_P(ProgramFromSourceTest, CreateWithNoStrings) {
    auto p = Program::create(pContext, 0, nullptr, nullptr, retVal);
    EXPECT_NE(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, p);
    delete p;
}

TEST_P(ProgramFromSourceTest, CreateWithSource_Compile) {

    cl_device_id usedDevice = pPlatform->getDevice(0);
    CreateProgramWithSource(
        pContext,
        &usedDevice,
        SourceFileName);

    auto *p = (MockProgram *)pProgram;

    cl_device_id deviceList = {0};
    cl_program inputHeaders;
    const char *headerIncludeNames = "";
    cl_program nullprogram = nullptr;
    cl_program invprogram = (cl_program)pContext;
    char data[4];

    // Order of following microtests is important - do not change.
    // Add new microtests at end.

    // invalid compile parameters: combinations of numDevices & deviceList
    retVal = pProgram->compile(1, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = pProgram->compile(0, &deviceList, nullptr, 0, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    // invalid compile parameters: combinations of numInputHeaders==0 & inputHeaders & headerIncludeNames
    retVal = pProgram->compile(0, nullptr, nullptr, 0, &inputHeaders, nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = pProgram->compile(0, nullptr, nullptr, 0, nullptr, &headerIncludeNames, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    // invalid compile parameters: combinations of numInputHeaders!=0 & inputHeaders & headerIncludeNames
    retVal = pProgram->compile(0, nullptr, nullptr, 1, &inputHeaders, nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = pProgram->compile(0, nullptr, nullptr, 1, nullptr, &headerIncludeNames, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    // invalid compile parameters: combinations of funcNotify & userData with valid numInputHeaders!=0 & inputHeaders & headerIncludeNames
    retVal = pProgram->compile(0, nullptr, nullptr, 1, &inputHeaders, &headerIncludeNames, nullptr, &data[0]);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    // invalid compile parameters: invalid content of deviceList
    retVal = pProgram->compile(1, &deviceList, nullptr, 0, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);

    // fail compilation - another compilation is already in progress
    p->SetBuildStatus(CL_BUILD_IN_PROGRESS);
    retVal = pProgram->compile(0, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    p->SetBuildStatus(CL_BUILD_NONE);

    // invalid compile parameters: invalid header Program object==nullptr
    retVal = pProgram->compile(0, nullptr, nullptr, 1, &nullprogram, &headerIncludeNames, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);

    // invalid compile parameters: invalid header Program object==non Program object
    retVal = pProgram->compile(0, nullptr, nullptr, 1, &invprogram, &headerIncludeNames, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);

    // compile successfully kernel with header
    std::string testFile;
    size_t sourceSize;
    Program *p3; // header Program object
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd8.cl"); // header source file
    auto pSourceBuffer = loadDataFromFile(testFile.c_str(), sourceSize);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSourceBuffer);
    const char *sources[1] = {pSourceBuffer.get()};
    p3 = Program::create<MockProgram>(pContext, 1, sources, &sourceSize, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, p3);
    inputHeaders = p3;
    retVal = pProgram->compile(0, nullptr, nullptr, 1, &inputHeaders, &headerIncludeNames, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // fail compilation of kernel with header - header is invalid
    p = (MockProgram *)p3;
    p->sourceCode = ""; // set header source code as non-existent (invalid)
    retVal = pProgram->compile(0, nullptr, nullptr, 1, &inputHeaders, &headerIncludeNames, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
    delete p3;

    // fail compilation - CompilerInterface cannot be obtained
    auto noCompilerInterfaceExecutionEnvironment = std::make_unique<MockExecutionEnvironment>(nullptr);
    auto p2 = std::make_unique<MockProgram>(*noCompilerInterfaceExecutionEnvironment);
    retVal = p2->compile(0, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
    p2.reset(nullptr);
    noCompilerInterfaceExecutionEnvironment.reset();

    // fail compilation - any compilation error (here caused by specifying unrecognized option)
    retVal = pProgram->compile(0, nullptr, "-invalid-option", 0, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_COMPILE_PROGRAM_FAILURE, retVal);

    // compile successfully without notifyFunc
    retVal = pProgram->compile(0, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // compile successfully with notifyFunc
    data[0] = 0;
    retVal = pProgram->compile(0, nullptr, nullptr, 0, nullptr, nullptr, notifyFunc, &data[0]);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ('a', data[0]);
}

struct MockCompilerInterfaceCaptureBuildOptions : CompilerInterface {
    TranslationOutput::ErrorCode compile(const NEO::Device &device, const TranslationInput &input, TranslationOutput &) override {
        buildOptions.clear();
        if ((input.apiOptions.size() > 0) && (input.apiOptions.begin() != nullptr)) {
            buildOptions.assign(input.apiOptions.begin(), input.apiOptions.end());
        }
        buildInternalOptions.clear();
        if ((input.internalOptions.size() > 0) && (input.internalOptions.begin() != nullptr)) {
            buildInternalOptions.assign(input.internalOptions.begin(), input.internalOptions.end());
        }
        return TranslationOutput::ErrorCode::Success;
    }

    TranslationOutput::ErrorCode build(const NEO::Device &device, const TranslationInput &input, TranslationOutput &out) override {
        return this->MockCompilerInterfaceCaptureBuildOptions::compile(device, input, out);
    }

    std::string buildOptions;
    std::string buildInternalOptions;
};

TEST_P(ProgramFromSourceTest, CompileProgramWithInternalFlags) {
    auto cip = std::make_unique<MockCompilerInterfaceCaptureBuildOptions>();
    MockExecutionEnvironment executionEnvironment(cip.get());
    auto program = std::make_unique<SucceedingGenBinaryProgram>(executionEnvironment);
    cl_device_id deviceId = pContext->getDevice(0);
    Device *pDevice = castToObject<Device>(deviceId);
    program->setDevice(pDevice);
    program->sourceCode = "__kernel mock() {}";

    // Ask to build created program without NEO::CompilerOptions::gtpinRera and NEO::CompilerOptions::greaterThan4gbBuffersRequired flags.
    cl_int retVal = program->compile(0, nullptr, CompilerOptions::fastRelaxedMath, 0, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Check build options that were applied
    EXPECT_TRUE(CompilerOptions::contains(cip->buildOptions, CompilerOptions::fastRelaxedMath)) << cip->buildOptions;
    EXPECT_FALSE(CompilerOptions::contains(cip->buildInternalOptions, CompilerOptions::gtpinRera)) << cip->buildInternalOptions;
    EXPECT_FALSE(CompilerOptions::contains(cip->buildInternalOptions, CompilerOptions::greaterThan4gbBuffersRequired)) << cip->buildInternalOptions;

    // Ask to build created program with NEO::CompilerOptions::gtpinRera and NEO::CompilerOptions::greaterThan4gbBuffersRequired flags.
    cip->buildOptions.clear();
    cip->buildInternalOptions.clear();
    auto options = CompilerOptions::concatenate(CompilerOptions::greaterThan4gbBuffersRequired, CompilerOptions::gtpinRera, CompilerOptions::finiteMathOnly);
    retVal = program->compile(0, nullptr, options.c_str(),
                              0, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Check build options that were applied
    EXPECT_FALSE(CompilerOptions::contains(cip->buildOptions, CompilerOptions::fastRelaxedMath)) << cip->buildOptions;
    EXPECT_TRUE(CompilerOptions::contains(cip->buildOptions, CompilerOptions::finiteMathOnly)) << cip->buildOptions;
    EXPECT_TRUE(CompilerOptions::contains(cip->buildInternalOptions, CompilerOptions::gtpinRera)) << cip->buildInternalOptions;
    EXPECT_TRUE(CompilerOptions::contains(cip->buildInternalOptions, CompilerOptions::greaterThan4gbBuffersRequired)) << cip->buildInternalOptions;
}

TEST_P(ProgramFromSourceTest, CreateWithSourceAdvanced) {
    std::string testFile;
    size_t sourceSize = 0;

    Program *p;
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd8.cl");
    auto pSourceBuffer = loadDataFromFile(testFile.c_str(), sourceSize);
    const char *sources[1] = {pSourceBuffer.get()};
    EXPECT_NE(nullptr, pSourceBuffer);

    /*
     According to spec: If lengths is NULL, all strings in the strings argument are considered null-terminated.
    */
    p = Program::create(pContext, 1, sources, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, p);
    delete p;

    /*
     According to spec: If an element in lengths is zero, its accompanying string is null-terminated.
    */
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

TEST_P(ProgramFromSourceTest, CreateWithSource_Link) {
    cl_device_id usedDevice = pPlatform->getDevice(0);
    CreateProgramWithSource(
        pContext,
        &usedDevice,
        SourceFileName);

    cl_device_id deviceList = {0};
    char data[4];
    cl_program program = pProgram;
    cl_program nullprogram = nullptr;
    cl_program invprogram = (cl_program)pContext;

    // Order of following microtests is important - do not change.
    // Add new microtests at end.

    // invalid link parameters: combinations of numDevices & deviceList
    retVal = pProgram->link(1, nullptr, nullptr, 1, &program, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = pProgram->link(0, &deviceList, nullptr, 1, &program, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    // invalid link parameters: combinations of numInputPrograms & inputPrograms
    retVal = pProgram->link(0, nullptr, nullptr, 0, &program, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    retVal = pProgram->link(0, nullptr, nullptr, 1, nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    // invalid link parameters: combinations of funcNotify & userData with valid numInputPrograms & inputPrograms
    retVal = pProgram->link(0, nullptr, nullptr, 1, &program, nullptr, &data[0]);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    // invalid link parameters: invalid content of deviceList
    retVal = pProgram->link(1, &deviceList, nullptr, 1, &program, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);

    // fail linking - another linking is already in progress
    pProgram->SetBuildStatus(CL_BUILD_IN_PROGRESS);
    retVal = pProgram->link(0, nullptr, nullptr, 1, &program, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    pProgram->SetBuildStatus(CL_BUILD_NONE);

    // invalid link parameters: invalid Program object==nullptr
    retVal = pProgram->link(0, nullptr, nullptr, 1, &nullprogram, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);

    // invalid link parameters: invalid Program object==non Program object
    retVal = pProgram->link(0, nullptr, nullptr, 1, &invprogram, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);

    // compile successfully a kernel to be linked later
    retVal = pProgram->compile(0, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // fail linking - code to be linked does not exist
    bool isSpirvTmp = pProgram->getIsSpirV();
    char *pIrBin = pProgram->irBinary.get();
    pProgram->irBinary.release();
    size_t irBinSize = pProgram->irBinarySize;
    pProgram->SetIrBinary(nullptr, false);
    retVal = pProgram->link(0, nullptr, nullptr, 1, &program, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
    pProgram->SetIrBinary(pIrBin, isSpirvTmp);

    // fail linking - size of code to be linked is == 0
    pProgram->SetIrBinarySize(0, isSpirvTmp);
    retVal = pProgram->link(0, nullptr, nullptr, 1, &program, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
    pProgram->SetIrBinarySize(irBinSize, isSpirvTmp);

    // fail linking - any link error (here caused by specifying unrecognized option)
    retVal = pProgram->link(0, nullptr, "-invalid-option", 1, &program, nullptr, nullptr);
    EXPECT_EQ(CL_LINK_PROGRAM_FAILURE, retVal);

    // fail linking - linked code is corrupted and cannot be postprocessed
    auto p2 = std::make_unique<FailingGenBinaryProgram>(*pPlatform->getDevice(0)->getExecutionEnvironment());
    Device *device = pPlatform->getDevice(0);
    p2->setDevice(device);
    retVal = p2->link(0, nullptr, nullptr, 1, &program, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_BINARY, retVal);
    p2.reset(nullptr);

    // link successfully without notifyFunc
    retVal = pProgram->link(0, nullptr, nullptr, 1, &program, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // link successfully with notifyFunc
    data[0] = 0;
    retVal = pProgram->link(0, nullptr, "", 1, &program, notifyFunc, &data[0]);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ('a', data[0]);
}

TEST_P(ProgramFromSourceTest, CreateWithSource_CreateLibrary) {
    auto noCompilerInterfaceExecutionEnvironment = std::make_unique<MockExecutionEnvironment>(nullptr);
    auto p = std::make_unique<MockProgram>(*noCompilerInterfaceExecutionEnvironment);
    cl_program program = pProgram;

    // Order of following microtests is important - do not change.
    // Add new microtests at end.

    // compile successfully a kernel to be later used to create library
    retVal = pProgram->compile(0, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // create library successfully
    retVal = pProgram->link(0, nullptr, CompilerOptions::createLibrary, 1, &program, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // fail library creation - any link error (here caused by specifying unrecognized option)
    retVal = pProgram->link(0, nullptr, CompilerOptions::concatenate(CompilerOptions::createLibrary, "-invalid-option").c_str(), 1, &program, nullptr, nullptr);
    EXPECT_EQ(CL_LINK_PROGRAM_FAILURE, retVal);

    // fail library creation - CompilerInterface cannot be obtained
    retVal = p->link(0, nullptr, CompilerOptions::createLibrary, 1, &program, nullptr, nullptr);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
}

class PatchTokenFromBinaryTest : public ProgramSimpleFixture {
  public:
    void SetUp() override {
        ProgramSimpleFixture::SetUp();
    }
    void TearDown() override {
        ProgramSimpleFixture::TearDown();
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

HWTEST_F(PatchTokenTests, givenKernelRequiringConstantAllocationWhenMakeResidentIsCalledThenConstantAllocationIsMadeResident) {
    cl_device_id device = pDevice;

    CreateProgramFromBinary(pContext, &device, "test_constant_memory");

    ASSERT_NE(nullptr, pProgram);
    retVal = pProgram->build(
        1,
        &device,
        nullptr,
        nullptr,
        nullptr,
        false);

    ASSERT_EQ(CL_SUCCESS, retVal);

    auto pKernelInfo = pProgram->getKernelInfo("test");

    EXPECT_NE(nullptr, pKernelInfo->patchInfo.pAllocateStatelessConstantMemorySurfaceWithInitialization);
    ASSERT_NE(nullptr, pProgram->getConstantSurface());

    uint32_t expected_values[] = {0xabcd5432u, 0xaabb5533u};
    uint32_t *constBuff = reinterpret_cast<uint32_t *>(pProgram->getConstantSurface()->getUnderlyingBuffer());
    EXPECT_EQ(expected_values[0], constBuff[0]);
    EXPECT_EQ(expected_values[1], constBuff[1]);

    std::unique_ptr<Kernel> pKernel(Kernel::create(pProgram, *pKernelInfo, &retVal));

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, pKernel);

    auto pCommandStreamReceiver = new CommandStreamReceiverMock<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex());
    ASSERT_NE(nullptr, pCommandStreamReceiver);

    pDevice->resetCommandStreamReceiver(pCommandStreamReceiver);
    pCommandStreamReceiver->residency.clear();

    pKernel->makeResident(*pCommandStreamReceiver);
    EXPECT_EQ(2u, pCommandStreamReceiver->residency.size());

    auto &residencyVector = pCommandStreamReceiver->getResidencyAllocations();

    //we expect kernel ISA here and constant allocation
    auto kernelIsa = pKernel->getKernelInfo().getGraphicsAllocation();
    auto constantAllocation = pProgram->getConstantSurface();

    auto element = std::find(residencyVector.begin(), residencyVector.end(), kernelIsa);
    EXPECT_NE(residencyVector.end(), element);
    element = std::find(residencyVector.begin(), residencyVector.end(), constantAllocation);
    EXPECT_NE(residencyVector.end(), element);

    auto crossThreadData = pKernel->getCrossThreadData();
    uint32_t *constBuffGpuAddr = reinterpret_cast<uint32_t *>(pProgram->getConstantSurface()->getGpuAddressToPatch());
    uintptr_t *pDst = reinterpret_cast<uintptr_t *>(crossThreadData + pKernelInfo->patchInfo.pAllocateStatelessConstantMemorySurfaceWithInitialization->DataParamOffset);

    EXPECT_EQ(*pDst, reinterpret_cast<uintptr_t>(constBuffGpuAddr));

    pCommandStreamReceiver->makeSurfacePackNonResident(pCommandStreamReceiver->getResidencyAllocations());
    EXPECT_EQ(0u, pCommandStreamReceiver->residency.size());

    std::vector<Surface *> surfaces;
    pKernel->getResidency(surfaces);
    EXPECT_EQ(2u, surfaces.size());

    for (Surface *surface : surfaces) {
        delete surface;
    }
}

TEST_F(PatchTokenTests, DataParamGWS) {
    cl_device_id device = pDevice;

    CreateProgramFromBinary(pContext, &device, "kernel_data_param");

    ASSERT_NE(nullptr, pProgram);
    retVal = pProgram->build(
        1,
        &device,
        nullptr,
        nullptr,
        nullptr,
        false);

    ASSERT_EQ(CL_SUCCESS, retVal);

    auto pKernelInfo = pProgram->getKernelInfo("test");

    ASSERT_NE(nullptr, pKernelInfo->patchInfo.dataParameterStream);
    ASSERT_NE(static_cast<uint32_t>(-1), pKernelInfo->workloadInfo.globalWorkSizeOffsets[0]);
    ASSERT_NE(static_cast<uint32_t>(-1), pKernelInfo->workloadInfo.globalWorkSizeOffsets[1]);
    ASSERT_NE(static_cast<uint32_t>(-1), pKernelInfo->workloadInfo.globalWorkSizeOffsets[2]);
}

TEST_F(PatchTokenTests, DataParamLWS) {
    cl_device_id device = pDevice;

    CreateProgramFromBinary(pContext, &device, "kernel_data_param");

    ASSERT_NE(nullptr, pProgram);
    retVal = pProgram->build(
        1,
        &device,
        nullptr,
        nullptr,
        nullptr,
        false);

    ASSERT_EQ(CL_SUCCESS, retVal);

    auto pKernelInfo = pProgram->getKernelInfo("test");

    ASSERT_NE(nullptr, pKernelInfo->patchInfo.dataParameterStream);
    ASSERT_NE(static_cast<uint32_t>(-1), pKernelInfo->workloadInfo.localWorkSizeOffsets[0]);
    ASSERT_NE(static_cast<uint32_t>(-1), pKernelInfo->workloadInfo.localWorkSizeOffsets[1]);
    ASSERT_NE(static_cast<uint32_t>(-1), pKernelInfo->workloadInfo.localWorkSizeOffsets[2]);

    pKernelInfo = pProgram->getKernelInfo("test_get_local_size");

    ASSERT_NE(nullptr, pKernelInfo->patchInfo.dataParameterStream);
    ASSERT_NE(static_cast<uint32_t>(-1), pKernelInfo->workloadInfo.localWorkSizeOffsets[0]);
    ASSERT_NE(static_cast<uint32_t>(-1), pKernelInfo->workloadInfo.localWorkSizeOffsets[1]);
    ASSERT_NE(static_cast<uint32_t>(-1), pKernelInfo->workloadInfo.localWorkSizeOffsets[2]);
    ASSERT_NE(static_cast<uint32_t>(-1), pKernelInfo->workloadInfo.localWorkSizeOffsets2[0]);
    ASSERT_NE(static_cast<uint32_t>(-1), pKernelInfo->workloadInfo.localWorkSizeOffsets2[1]);
    ASSERT_NE(static_cast<uint32_t>(-1), pKernelInfo->workloadInfo.localWorkSizeOffsets2[2]);
}

TEST_F(PatchTokenTests, ConstantMemoryObjectKernelArg) {
    // PATCH_TOKEN_STATELESS_CONSTANT_MEMORY_OBJECT_KERNEL_ARGUMENT
    cl_device_id device = pDevice;

    CreateProgramFromBinary(pContext, &device, "test_basic_constant");

    ASSERT_NE(nullptr, pProgram);
    retVal = pProgram->build(
        1,
        &device,
        nullptr,
        nullptr,
        nullptr,
        false);

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pKernelInfo = pProgram->getKernelInfo("constant_kernel");
    ASSERT_NE(nullptr, pKernelInfo);

    auto pKernel = Kernel::create(
        pProgram,
        *pKernelInfo,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, pKernel);

    uint32_t numArgs;
    retVal = pKernel->getInfo(CL_KERNEL_NUM_ARGS, sizeof(numArgs), &numArgs, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(3u, numArgs);

    uint32_t sizeOfPtr = sizeof(void *);
    EXPECT_EQ(pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].size, sizeOfPtr);
    EXPECT_EQ(pKernelInfo->kernelArgInfo[1].kernelArgPatchInfoVector[0].size, sizeOfPtr);

    delete pKernel;
}

TEST_F(PatchTokenTests, VmeKernelArg) {
    if (!pDevice->getExecutionEnvironment()->getHardwareInfo()->capabilityTable.supportsVme) {
        GTEST_SKIP();
    }
    // PATCH_TOKEN_INLINE_VME_SAMPLER_INFO token indicates a VME kernel.
    cl_device_id device = pDevice;

    CreateProgramFromBinary(pContext, &device, "vme_kernels");

    ASSERT_NE(nullptr, pProgram);
    retVal = pProgram->build(
        1,
        &device,
        nullptr,
        nullptr,
        nullptr,
        false);

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pKernelInfo = pProgram->getKernelInfo("device_side_block_motion_estimate_intel");
    ASSERT_NE(nullptr, pKernelInfo);
    EXPECT_EQ(true, pKernelInfo->isVmeWorkload);

    auto pKernel = Kernel::create(
        pProgram,
        *pKernelInfo,
        &retVal);

    ASSERT_NE(nullptr, pKernel);

    delete pKernel;
}

class ProgramPatchTokenFromBinaryTest : public ProgramSimpleFixture {
  public:
    void SetUp() override {
        ProgramSimpleFixture::SetUp();
    }
    void TearDown() override {
        ProgramSimpleFixture::TearDown();
    }
};
typedef Test<ProgramPatchTokenFromBinaryTest> ProgramPatchTokenTests;

TEST_F(ProgramPatchTokenTests, DISABLED_ConstantMemorySurface) {
    cl_device_id device = pDevice;

    CreateProgramWithSource(pContext, &device, "CopyBuffer_simd8.cl");

    ASSERT_NE(nullptr, pProgram);
    retVal = pProgram->build(
        1,
        &device,
        nullptr,
        nullptr,
        nullptr,
        false);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(ProgramFromBinaryTests, givenBinaryWithInvalidICBEThenErrorIsReturned) {
    cl_int retVal = CL_INVALID_BINARY;

    SProgramBinaryHeader binHeader;
    memset(&binHeader, 0, sizeof(binHeader));
    binHeader.Magic = iOpenCL::MAGIC_CL;
    binHeader.Version = iOpenCL::CURRENT_ICBE_VERSION - 3;
    binHeader.Device = platformDevices[0]->platform.eRenderCoreFamily;
    binHeader.GPUPointerSizeInBytes = 8;
    binHeader.NumberOfKernels = 0;
    binHeader.SteppingId = 0;
    binHeader.PatchListSize = 0;
    size_t binSize = sizeof(SProgramBinaryHeader);

    {
        const unsigned char *binaries[1] = {reinterpret_cast<const unsigned char *>(&binHeader)};
        const cl_device_id deviceId = 0;
        MockContext context;
        std::unique_ptr<Program> pProgram(Program::create<Program>(&context, 0, &deviceId, &binSize, binaries, nullptr, retVal));
        EXPECT_EQ(nullptr, pProgram.get());
        EXPECT_EQ(CL_INVALID_BINARY, retVal);
    }

    {
        // whatever method we choose CL_INVALID_BINARY is always returned
        ExecutionEnvironment executionEnvironment;
        std::unique_ptr<Program> pProgram(Program::createFromGenBinary(executionEnvironment, nullptr, &binHeader, binSize, false, &retVal));
        ASSERT_NE(nullptr, pProgram.get());
        EXPECT_EQ(CL_SUCCESS, retVal);

        retVal = pProgram->processGenBinary();
        EXPECT_EQ(CL_INVALID_BINARY, retVal);
    }
}

class FailProgram : public Program {
  public:
    FailProgram(ExecutionEnvironment &executionEnvironment, Context *context, bool isBuiltIn = false) : Program(executionEnvironment, context, isBuiltIn) {}
    cl_int rebuildProgramFromIr() override {
        return CL_INVALID_PROGRAM;
    }
    // make method visible
    cl_int createProgramFromBinary(const void *pBinary, size_t binarySize) override {
        return Program::createProgramFromBinary(pBinary, binarySize);
    }
    cl_int processElfBinary(const void *pBinary, size_t binarySize, uint32_t &binaryVersion) override {
        binaryVersion--;
        // we should return anything but not CL_SUCCESS
        return CL_INVALID_BINARY;
    }
};

TEST(ProgramFromBinaryTests, CreateWithBinary_FailRecompile) {
    cl_int retVal = CL_INVALID_BINARY;

    SProgramBinaryHeader binHeader;
    memset(&binHeader, 0, sizeof(binHeader));
    binHeader.Magic = iOpenCL::MAGIC_CL;
    binHeader.Version = iOpenCL::CURRENT_ICBE_VERSION;
    binHeader.Device = platformDevices[0]->platform.eRenderCoreFamily;
    binHeader.GPUPointerSizeInBytes = 8;
    binHeader.NumberOfKernels = 0;
    binHeader.SteppingId = 0;
    binHeader.PatchListSize = 0;
    size_t binSize = sizeof(SProgramBinaryHeader);

    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<FailProgram> pProgram(FailProgram::createFromGenBinary<FailProgram>(executionEnvironment, nullptr, &binHeader, binSize, false, &retVal));
    ASSERT_NE(nullptr, pProgram.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    binHeader.Version = iOpenCL::CURRENT_ICBE_VERSION - 1;
    retVal = pProgram->createProgramFromBinary(&binHeader, binSize);
    EXPECT_EQ(CL_INVALID_BINARY, retVal);
}

TEST(ProgramFromBinaryTests, givenEmptyProgramThenErrorIsReturned) {
    class TestedProgram : public Program {
      public:
        TestedProgram(ExecutionEnvironment &executionEnvironment, Context *context, bool isBuiltIn) : Program(executionEnvironment, context, isBuiltIn) {}
        char *setGenBinary(char *binary) {
            auto res = genBinary.get();
            genBinary.release();
            genBinary.reset(binary);
            return res;
        }
        void setGenBinarySize(size_t size) {
            genBinarySize = size;
        }
    };
    cl_int retVal = CL_INVALID_BINARY;

    SProgramBinaryHeader binHeader;
    memset(&binHeader, 0, sizeof(binHeader));
    binHeader.Magic = iOpenCL::MAGIC_CL;
    binHeader.Version = iOpenCL::CURRENT_ICBE_VERSION;
    binHeader.Device = platformDevices[0]->platform.eRenderCoreFamily;
    binHeader.GPUPointerSizeInBytes = 8;
    binHeader.NumberOfKernels = 0;
    binHeader.SteppingId = 0;
    binHeader.PatchListSize = 0;
    size_t binSize = sizeof(SProgramBinaryHeader);

    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<TestedProgram> pProgram(TestedProgram::createFromGenBinary<TestedProgram>(executionEnvironment, nullptr, &binHeader, binSize, false, &retVal));
    ASSERT_NE(nullptr, pProgram.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto originalPtr = pProgram->setGenBinary(nullptr);
    retVal = pProgram->processGenBinary();
    EXPECT_EQ(CL_INVALID_BINARY, retVal);
    pProgram->setGenBinary(originalPtr);
}

INSTANTIATE_TEST_CASE_P(ProgramFromBinaryTests,
                        ProgramFromBinaryTest,
                        ::testing::Combine(
                            ::testing::ValuesIn(BinaryFileNames),
                            ::testing::ValuesIn(KernelNames)));

INSTANTIATE_TEST_CASE_P(ProgramFromSourceTests,
                        ProgramFromSourceTest,
                        ::testing::Combine(
                            ::testing::ValuesIn(SourceFileNames),
                            ::testing::ValuesIn(BinaryForSourceFileNames),
                            ::testing::ValuesIn(KernelNames)));

TEST_F(ProgramTests, ProgramCtorSetsProperInternalOptions) {
    auto defaultSetting = DebugManager.flags.DisableStatelessToStatefulOptimization.get();

    DebugManager.flags.DisableStatelessToStatefulOptimization.set(false);
    MockProgram program(*pDevice->getExecutionEnvironment(), pContext, false);
    char paramValue[32];
    pDevice->getDeviceInfo(CL_DEVICE_VERSION, 32, paramValue, 0);
    if (strstr(paramValue, "2.1")) {
        EXPECT_TRUE(CompilerOptions::contains(program.getInternalOptions(), "-ocl-version=210")) << program.getInternalOptions();
    } else if (strstr(paramValue, "2.0")) {
        EXPECT_TRUE(CompilerOptions::contains(program.getInternalOptions(), "-ocl-version=200")) << program.getInternalOptions();
    } else if (strstr(paramValue, "1.2")) {
        EXPECT_TRUE(CompilerOptions::contains(program.getInternalOptions(), "-ocl-version=120")) << program.getInternalOptions();
    } else {
        EXPECT_TRUE(CompilerOptions::contains(program.getInternalOptions(), "-ocl-version=000")) << program.getInternalOptions();
    }
    DebugManager.flags.DisableStatelessToStatefulOptimization.set(defaultSetting);
}

TEST_F(ProgramTests, ProgramCtorSetsProperInternalOptionsForced20) {
    auto defaultVersion = pDevice->deviceInfo.clVersion;

    pDevice->deviceInfo.clVersion = "OpenCL 2.0";
    MockProgram program(*pDevice->getExecutionEnvironment(), pContext, false);
    char paramValue[32];
    pDevice->getDeviceInfo(CL_DEVICE_VERSION, 32, paramValue, 0);
    EXPECT_STREQ("OpenCL 2.0", paramValue);
    EXPECT_TRUE(CompilerOptions::contains(program.getInternalOptions(), "-ocl-version=200"));
    pDevice->deviceInfo.clVersion = defaultVersion;
}

TEST_F(ProgramTests, ProgramCtorSetsProperInternalOptionsWhenStatelessToStatefulIsDisabled) {
    DebugManagerStateRestore restorer;

    DebugManager.flags.DisableStatelessToStatefulOptimization.set(true);
    MockProgram program(*pDevice->getExecutionEnvironment(), pContext, false);
    char paramValue[32];
    pDevice->getDeviceInfo(CL_DEVICE_VERSION, 32, paramValue, 0);
    if (strstr(paramValue, "2.1")) {
        EXPECT_TRUE(CompilerOptions::contains(program.getInternalOptions(), "-ocl-version=210")) << program.getInternalOptions();
        EXPECT_TRUE(CompilerOptions::contains(program.getInternalOptions(), NEO::CompilerOptions::greaterThan4gbBuffersRequired));
    } else if (strstr(paramValue, "2.0")) {
        EXPECT_TRUE(CompilerOptions::contains(program.getInternalOptions(), "-ocl-version=200")) << program.getInternalOptions();
        EXPECT_TRUE(CompilerOptions::contains(program.getInternalOptions(), NEO::CompilerOptions::greaterThan4gbBuffersRequired));
    } else if (strstr(paramValue, "1.2")) {
        EXPECT_TRUE(CompilerOptions::contains(program.getInternalOptions(), "-ocl-version=120")) << program.getInternalOptions();
        EXPECT_TRUE(CompilerOptions::contains(program.getInternalOptions(), NEO::CompilerOptions::greaterThan4gbBuffersRequired));
    } else {
        EXPECT_TRUE(CompilerOptions::contains(program.getInternalOptions(), "-ocl-version=000")) << program.getInternalOptions();
        EXPECT_TRUE(CompilerOptions::contains(program.getInternalOptions(), NEO::CompilerOptions::greaterThan4gbBuffersRequired));
    }
}

TEST_F(ProgramTests, WhenCreatingProgramThenBindlessIsEnabledOnlyIfDebugFlagIsEnabled) {
    using namespace testing;
    DebugManagerStateRestore restorer;

    {
        EXPECT_FALSE(DebugManager.flags.UseBindlessBuffers.get());
        EXPECT_FALSE(DebugManager.flags.UseBindlessImages.get());
        MockProgram programNoBindless(*pDevice->getExecutionEnvironment(), pContext, false);
        EXPECT_FALSE(CompilerOptions::contains(programNoBindless.getInternalOptions(), CompilerOptions::bindlessBuffers)) << programNoBindless.getInternalOptions();
        EXPECT_FALSE(CompilerOptions::contains(programNoBindless.getInternalOptions(), CompilerOptions::bindlessImages)) << programNoBindless.getInternalOptions();
    }

    {
        DebugManager.flags.UseBindlessBuffers.set(true);
        MockProgram programNoBindless(*pDevice->getExecutionEnvironment(), pContext, false);
        EXPECT_TRUE(CompilerOptions::contains(programNoBindless.getInternalOptions(), CompilerOptions::bindlessBuffers)) << programNoBindless.getInternalOptions();
        EXPECT_FALSE(CompilerOptions::contains(programNoBindless.getInternalOptions(), CompilerOptions::bindlessImages)) << programNoBindless.getInternalOptions();
    }

    {
        DebugManager.flags.UseBindlessBuffers.set(false);
        DebugManager.flags.UseBindlessImages.set(true);
        MockProgram programNoBindless(*pDevice->getExecutionEnvironment(), pContext, false);
        EXPECT_FALSE(CompilerOptions::contains(programNoBindless.getInternalOptions(), CompilerOptions::bindlessBuffers)) << programNoBindless.getInternalOptions();
        EXPECT_TRUE(CompilerOptions::contains(programNoBindless.getInternalOptions(), CompilerOptions::bindlessImages)) << programNoBindless.getInternalOptions();
    }

    {
        DebugManager.flags.UseBindlessBuffers.set(true);
        DebugManager.flags.UseBindlessImages.set(true);
        MockProgram programNoBindless(*pDevice->getExecutionEnvironment(), pContext, false);
        EXPECT_TRUE(CompilerOptions::contains(programNoBindless.getInternalOptions(), CompilerOptions::bindlessBuffers)) << programNoBindless.getInternalOptions();
        EXPECT_TRUE(CompilerOptions::contains(programNoBindless.getInternalOptions(), CompilerOptions::bindlessImages)) << programNoBindless.getInternalOptions();
    }
}

TEST_F(ProgramTests, givenDeviceThatSupportsSharedSystemMemoryAllocationWhenProgramIsCompiledThenItForcesStatelessCompilation) {
    pDevice->deviceInfo.sharedSystemMemCapabilities = CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS_INTEL;
    MockProgram program(*pDevice->getExecutionEnvironment(), pContext, false);
    EXPECT_TRUE(CompilerOptions::contains(program.getInternalOptions().c_str(), CompilerOptions::greaterThan4gbBuffersRequired)) << program.getInternalOptions();
}

TEST_F(ProgramTests, ProgramCtorSetsProperInternalOptionsWhenForcing32BitAddressess) {
    cl_int retVal = CL_DEVICE_NOT_FOUND;
    auto defaultSetting = DebugManager.flags.DisableStatelessToStatefulOptimization.get();

    DebugManager.flags.DisableStatelessToStatefulOptimization.set(false);
    if (pDevice) {
        const_cast<DeviceInfo *>(&pDevice->getDeviceInfo())->force32BitAddressess = true;
        MockProgram program(*pDevice->getExecutionEnvironment(), pContext, false);
        char paramValue[32];
        pDevice->getDeviceInfo(CL_DEVICE_VERSION, 32, paramValue, 0);
        if (strstr(paramValue, "2.1")) {
            EXPECT_TRUE(CompilerOptions::contains(program.getInternalOptions(), "-ocl-version=210")) << program.getInternalOptions();
        } else if (strstr(paramValue, "2.0")) {
            EXPECT_TRUE(CompilerOptions::contains(program.getInternalOptions(), "-ocl-version=200")) << program.getInternalOptions();
        } else if (strstr(paramValue, "1.2")) {
            EXPECT_TRUE(CompilerOptions::contains(program.getInternalOptions(), "-ocl-version=120")) << program.getInternalOptions();
        } else {
            EXPECT_TRUE(CompilerOptions::contains(program.getInternalOptions(), "-ocl-version=000")) << program.getInternalOptions();
        }
        if (pDevice->areSharedSystemAllocationsAllowed()) {
            EXPECT_TRUE(CompilerOptions::contains(program.getInternalOptions(), CompilerOptions::greaterThan4gbBuffersRequired)) << program.getInternalOptions();
        } else {
            EXPECT_FALSE(CompilerOptions::contains(program.getInternalOptions(), NEO::CompilerOptions::greaterThan4gbBuffersRequired)) << program.getInternalOptions();
        }
    } else {
        EXPECT_NE(CL_DEVICE_NOT_FOUND, retVal);
    }
    DebugManager.flags.DisableStatelessToStatefulOptimization.set(defaultSetting);
}

TEST_F(ProgramTests, BuiltinProgramCreateSetsProperInternalOptions) {
    auto defaultSetting = DebugManager.flags.DisableStatelessToStatefulOptimization.get();

    DebugManager.flags.DisableStatelessToStatefulOptimization.set(false);
    std::unique_ptr<MockProgram> program{Program::create<MockProgram>("", pContext, *pDevice, true, nullptr)};
    if ((false == pDevice->areSharedSystemAllocationsAllowed()) && (false == is32bit)) {
        EXPECT_FALSE(CompilerOptions::contains(program->getInternalOptions(), NEO::CompilerOptions::greaterThan4gbBuffersRequired)) << program->getInternalOptions();
    } else {
        EXPECT_TRUE(CompilerOptions::contains(program->getInternalOptions(), NEO::CompilerOptions::greaterThan4gbBuffersRequired)) << program->getInternalOptions();
    }
    DebugManager.flags.DisableStatelessToStatefulOptimization.set(defaultSetting);
}

TEST_F(ProgramTests, BuiltinProgramCreateSetsProperInternalOptionsWhenStatelessToStatefulIsDisabled) {
    auto defaultSetting = DebugManager.flags.DisableStatelessToStatefulOptimization.get();

    DebugManager.flags.DisableStatelessToStatefulOptimization.set(true);
    std::unique_ptr<MockProgram> program{Program::create<MockProgram>("", pContext, *pDevice, true, nullptr)};
    EXPECT_TRUE(CompilerOptions::contains(program->getInternalOptions(), NEO::CompilerOptions::greaterThan4gbBuffersRequired)) << program->getInternalOptions();
    DebugManager.flags.DisableStatelessToStatefulOptimization.set(defaultSetting);
}

TEST_F(ProgramTests, givenProgramWhenItIsCompiledThenItAlwaysHavePreserveVec3TypeInternalOptionSet) {
    std::unique_ptr<MockProgram> program(Program::create<MockProgram>("", pContext, *pDevice, true, nullptr));
    EXPECT_TRUE(CompilerOptions::contains(program->getInternalOptions(), CompilerOptions::preserveVec3Type)) << program->getInternalOptions();
}

TEST_F(ProgramTests, BuiltinProgramCreateSetsProperInternalOptionsWhenForcing32BitAddressess) {
    auto defaultSetting = DebugManager.flags.DisableStatelessToStatefulOptimization.get();

    DebugManager.flags.DisableStatelessToStatefulOptimization.set(false);
    const_cast<DeviceInfo *>(&pDevice->getDeviceInfo())->force32BitAddressess = true;
    std::unique_ptr<MockProgram> program{Program::create<MockProgram>("", pContext, *pDevice, true, nullptr)};
    if (is32bit) {
        EXPECT_TRUE(CompilerOptions::contains(program->getInternalOptions(), CompilerOptions::greaterThan4gbBuffersRequired)) << program->getInternalOptions();
    } else {
        if (false == pDevice->areSharedSystemAllocationsAllowed()) {
            EXPECT_FALSE(CompilerOptions::contains(program->getInternalOptions(), NEO::CompilerOptions::greaterThan4gbBuffersRequired)) << program->getInternalOptions();
        } else {
            EXPECT_TRUE(CompilerOptions::contains(program->getInternalOptions(), NEO::CompilerOptions::greaterThan4gbBuffersRequired)) << program->getInternalOptions();
        }
    }
    DebugManager.flags.DisableStatelessToStatefulOptimization.set(defaultSetting);
}

TEST_F(ProgramTests, BuiltinProgramCreateSetsProperInternalOptionsEnablingStatelessToStatefulBufferOffsetOptimization) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.set(1);
    cl_int errorCode = CL_SUCCESS;
    const char programSource[] = "program";
    const char *programPointer = programSource;
    const char **programSources = reinterpret_cast<const char **>(&programPointer);
    size_t length = sizeof(programSource);
    std::unique_ptr<MockProgram> program(Program::create<MockProgram>(pContext, 1u, programSources, &length, errorCode));

    EXPECT_TRUE(CompilerOptions::contains(program->getInternalOptions(), CompilerOptions::hasBufferOffsetArg)) << program->getInternalOptions();
}

TEST_F(ProgramTests, givenStatelessToStatefullOptimizationOffWHenProgramIsCreatedThenOptimizationStringIsNotPresent) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.set(0);
    cl_int errorCode = CL_SUCCESS;
    const char programSource[] = "program";
    const char *programPointer = programSource;
    const char **programSources = reinterpret_cast<const char **>(&programPointer);
    size_t length = sizeof(programSource);
    std::unique_ptr<MockProgram> program(Program::create<MockProgram>(pContext, 1u, programSources, &length, errorCode));
    EXPECT_FALSE(CompilerOptions::contains(program->getInternalOptions(), CompilerOptions::hasBufferOffsetArg)) << program->getInternalOptions();
}

TEST_F(ProgramTests, GivenContextWhenCreateProgramThenIncrementContextRefCount) {
    auto initialApiRefCount = pContext->getReference();
    auto initialInternalRefCount = pContext->getRefInternalCount();

    MockProgram *program = new MockProgram(*pDevice->getExecutionEnvironment(), pContext, false);

    EXPECT_EQ(pContext->getReference(), initialApiRefCount);
    EXPECT_EQ(pContext->getRefInternalCount(), initialInternalRefCount + 1);
    program->release();
    EXPECT_EQ(pContext->getReference(), initialApiRefCount);
    EXPECT_EQ(pContext->getRefInternalCount(), initialInternalRefCount);
}

TEST_F(ProgramTests, GivenContextWhenCreateProgramFromSourceThenIncrementContextRefCount) {
    auto initialApiRefCount = pContext->getReference();
    auto initialInternalRefCount = pContext->getRefInternalCount();

    auto tempProgram = Program::create("", nullptr, *pDevice, false, nullptr);
    EXPECT_FALSE(tempProgram->getIsBuiltIn());
    auto program = Program::create("", pContext, *pDevice, false, nullptr);
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

    auto tempProgram = Program::create("", nullptr, *pDevice, true, nullptr);
    EXPECT_TRUE(tempProgram->getIsBuiltIn());
    auto program = Program::create("", pContext, *pDevice, true, nullptr);
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

TEST_F(ProgramTests, ProgramCreateT3Success) {
    cl_int retVal = CL_DEVICE_NOT_FOUND;
    Program *pProgram = Program::create("", pContext, *pDevice, false, &retVal);
    EXPECT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
    delete pProgram;

    pProgram = Program::create("", pContext, *pDevice, false, nullptr);
    EXPECT_NE(nullptr, pProgram);
    delete pProgram;
}

TEST_F(ProgramTests, ProgramFromGenBinaryWithNullBinary) {
    cl_int retVal = CL_SUCCESS;
    Program *pProgram = Program::createFromGenBinary(*pDevice->getExecutionEnvironment(), pContext, nullptr, 0, false, &retVal);
    EXPECT_EQ(nullptr, pProgram);
    EXPECT_NE(CL_SUCCESS, retVal);
}

TEST_F(ProgramTests, ProgramFromGenBinary) {
    cl_int retVal = CL_INVALID_BINARY;
    char binary[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, '\0'};
    size_t size = 10;

    Program *pProgram = Program::createFromGenBinary(*pDevice->getExecutionEnvironment(), pContext, binary, size, false, &retVal);
    EXPECT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ((uint32_t)CL_PROGRAM_BINARY_TYPE_EXECUTABLE, (uint32_t)pProgram->getProgramBinaryType());
    EXPECT_FALSE(pProgram->getIsBuiltIn());

    cl_device_id deviceId = pContext->getDevice(0);
    cl_build_status status = 0;
    pProgram->getBuildInfo(deviceId, CL_PROGRAM_BUILD_STATUS,
                           sizeof(cl_build_status), &status, nullptr);
    EXPECT_EQ(CL_BUILD_SUCCESS, status);

    delete pProgram;
}

TEST_F(ProgramTests, ProgramFromGenBinaryWithBuiltInFlagSet) {
    cl_int retVal = CL_INVALID_BINARY;
    char binary[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, '\0'};
    size_t size = 10;

    Program *pProgram = Program::createFromGenBinary(*pDevice->getExecutionEnvironment(), pContext, binary, size, true, &retVal);
    EXPECT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(pProgram->getIsBuiltIn());

    delete pProgram;
}

TEST_F(ProgramTests, ProgramFromGenBinaryWithoutRetVal) {
    char binary[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, '\0'};
    size_t size = 10;

    Program *pProgram = Program::createFromGenBinary(*pDevice->getExecutionEnvironment(), pContext, binary, size, false, nullptr);
    EXPECT_NE(nullptr, pProgram);
    EXPECT_EQ((uint32_t)CL_PROGRAM_BINARY_TYPE_EXECUTABLE, (uint32_t)pProgram->getProgramBinaryType());

    cl_device_id deviceId = pContext->getDevice(0);
    cl_build_status status = 0;
    pProgram->getBuildInfo(deviceId, CL_PROGRAM_BUILD_STATUS,
                           sizeof(cl_build_status), &status, nullptr);
    EXPECT_EQ(CL_BUILD_SUCCESS, status);

    delete pProgram;
}

TEST_F(ProgramTests, ProgramFromGenBinaryWithNullcontext) {
    cl_int retVal = CL_INVALID_BINARY;
    char binary[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, '\0'};
    size_t size = 10;

    Program *pProgram = Program::createFromGenBinary(*pDevice->getExecutionEnvironment(), nullptr, binary, size, false, &retVal);
    EXPECT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ((uint32_t)CL_PROGRAM_BINARY_TYPE_EXECUTABLE, (uint32_t)pProgram->getProgramBinaryType());

    cl_device_id deviceId = nullptr;
    cl_build_status status = 0;
    pProgram->getBuildInfo(deviceId, CL_PROGRAM_BUILD_STATUS,
                           sizeof(cl_build_status), &status, nullptr);
    EXPECT_EQ(CL_BUILD_SUCCESS, status);

    delete pProgram;
}

TEST_F(ProgramTests, ProgramFromGenBinaryWithPATCH_TOKEN_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT) {
    cl_int retVal = CL_INVALID_BINARY;
    char genBin[1024] = {1, 2, 3, 4, 5, 6, 7, 8, 9, '\0'};
    size_t binSize = 10;

    MockProgram *pProgram = Program::createFromGenBinary<MockProgram>(*pDevice->getExecutionEnvironment(), nullptr, &genBin[0], binSize, false, &retVal);
    EXPECT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ((uint32_t)CL_PROGRAM_BINARY_TYPE_EXECUTABLE, (uint32_t)pProgram->getProgramBinaryType());

    cl_device_id deviceId = pContext->getDevice(0);
    Device *pDevice = castToObject<Device>(deviceId);
    char *pBin = &genBin[0];
    retVal = CL_INVALID_BINARY;
    binSize = 0;

    // Prepare simple program binary containing patch token PATCH_TOKEN_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT
    SProgramBinaryHeader *pBHdr = (SProgramBinaryHeader *)pBin;
    pBHdr->Magic = iOpenCL::MAGIC_CL;
    pBHdr->Version = iOpenCL::CURRENT_ICBE_VERSION;
    pBHdr->Device = pDevice->getHardwareInfo().platform.eRenderCoreFamily;
    pBHdr->GPUPointerSizeInBytes = 8;
    pBHdr->NumberOfKernels = 1;
    pBHdr->SteppingId = 0;
    pBHdr->PatchListSize = 0;
    pBin += sizeof(SProgramBinaryHeader);
    binSize += sizeof(SProgramBinaryHeader);

    SKernelBinaryHeaderCommon *pKHdr = (SKernelBinaryHeaderCommon *)pBin;
    pKHdr->CheckSum = 0;
    pKHdr->ShaderHashCode = 0;
    pKHdr->KernelNameSize = 8;
    pKHdr->PatchListSize = sizeof(iOpenCL::SPatchGlobalMemoryObjectKernelArgument);
    pKHdr->KernelHeapSize = 0;
    pKHdr->GeneralStateHeapSize = 0;
    pKHdr->DynamicStateHeapSize = 0;
    pKHdr->SurfaceStateHeapSize = 0;
    pKHdr->KernelUnpaddedSize = 0;
    pBin += sizeof(SKernelBinaryHeaderCommon);
    binSize += sizeof(SKernelBinaryHeaderCommon);

    strcpy(pBin, "TstCopy");
    pBin += pKHdr->KernelNameSize;
    binSize += pKHdr->KernelNameSize;

    SPatchGlobalMemoryObjectKernelArgument *pPatch = (SPatchGlobalMemoryObjectKernelArgument *)pBin;
    pPatch->Token = iOpenCL::PATCH_TOKEN_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT;
    pPatch->Size = sizeof(iOpenCL::SPatchGlobalMemoryObjectKernelArgument);
    pPatch->ArgumentNumber = 0;
    pPatch->Offset = 0x40;
    pPatch->LocationIndex = iOpenCL::INVALID_INDEX;
    pPatch->LocationIndex2 = iOpenCL::INVALID_INDEX;
    binSize += pPatch->Size;
    pBin += pPatch->Size;

    ArrayRef<const uint8_t> kernelBlob(reinterpret_cast<uint8_t *>(pKHdr), reinterpret_cast<uint8_t *>(pBin));
    pKHdr->CheckSum = PatchTokenBinary::calcKernelChecksum(kernelBlob);

    // Decode prepared program binary
    pProgram->genBinary = makeCopy(&genBin[0], binSize);
    pProgram->genBinarySize = binSize;
    retVal = pProgram->processGenBinary();
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete pProgram;
}

TEST_F(ProgramTests, givenProgramFromGenBinaryWhenSLMSizeIsBiggerThenDeviceLimitThenReturnError) {
    cl_int retVal = CL_INVALID_BINARY;
    char genBin[1024] = {1, 2, 3, 4, 5, 6, 7, 8, 9, '\0'};
    size_t binSize = 10;

    auto program = std::unique_ptr<MockProgram>(Program::createFromGenBinary<MockProgram>(*pDevice->getExecutionEnvironment(), nullptr, &genBin[0], binSize, false, &retVal));

    EXPECT_NE(nullptr, program.get());
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ((uint32_t)CL_PROGRAM_BINARY_TYPE_EXECUTABLE, (uint32_t)program->getProgramBinaryType());

    cl_device_id deviceId = pContext->getDevice(0);
    Device *pDevice = castToObject<Device>(deviceId);
    program->setDevice(pDevice);
    char *pBin = &genBin[0];
    retVal = CL_INVALID_BINARY;
    binSize = 0;

    // Prepare simple program binary containing patch token PATCH_TOKEN_ALLOCATE_LOCAL_SURFACE
    SProgramBinaryHeader *pBHdr = (SProgramBinaryHeader *)pBin;
    pBHdr->Magic = iOpenCL::MAGIC_CL;
    pBHdr->Version = iOpenCL::CURRENT_ICBE_VERSION;
    pBHdr->Device = pDevice->getHardwareInfo().platform.eRenderCoreFamily;
    pBHdr->GPUPointerSizeInBytes = 8;
    pBHdr->NumberOfKernels = 1;
    pBHdr->SteppingId = 0;
    pBHdr->PatchListSize = 0;
    pBin += sizeof(SProgramBinaryHeader);
    binSize += sizeof(SProgramBinaryHeader);

    SKernelBinaryHeaderCommon *pKHdr = (SKernelBinaryHeaderCommon *)pBin;
    pKHdr->CheckSum = 0;
    pKHdr->ShaderHashCode = 0;
    pKHdr->KernelNameSize = 8;
    pKHdr->PatchListSize = sizeof(iOpenCL::SPatchAllocateLocalSurface);
    pKHdr->KernelHeapSize = 0;
    pKHdr->GeneralStateHeapSize = 0;
    pKHdr->DynamicStateHeapSize = 0;
    pKHdr->SurfaceStateHeapSize = 0;
    pKHdr->KernelUnpaddedSize = 0;
    pBin += sizeof(SKernelBinaryHeaderCommon);
    binSize += sizeof(SKernelBinaryHeaderCommon);

    strcpy(pBin, "TstCopy");
    pBin += pKHdr->KernelNameSize;
    binSize += pKHdr->KernelNameSize;

    SPatchAllocateLocalSurface *pPatch = (SPatchAllocateLocalSurface *)pBin;
    pPatch->Token = iOpenCL::PATCH_TOKEN_ALLOCATE_LOCAL_SURFACE;
    pPatch->Size = sizeof(iOpenCL::SPatchAllocateLocalSurface);
    pPatch->TotalInlineLocalMemorySize = static_cast<uint32_t>(pDevice->getDeviceInfo().localMemSize * 2);

    binSize += sizeof(SPatchAllocateLocalSurface);
    pBin += sizeof(SPatchAllocateLocalSurface);
    pKHdr->CheckSum = PatchTokenBinary::calcKernelChecksum(ArrayRef<const uint8_t>(reinterpret_cast<uint8_t *>(pKHdr), reinterpret_cast<uint8_t *>(pBin)));

    // Decode prepared program binary
    program->genBinary = makeCopy(&genBin[0], binSize);
    program->genBinarySize = binSize;
    retVal = program->processGenBinary();

    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);
}

TEST_F(ProgramTests, ProgramFromGenBinaryWithPATCH_TOKEN_GTPIN_FREE_GRF_INFO) {
#define GRF_INFO_SIZE 44u
    cl_int retVal = CL_INVALID_BINARY;
    char genBin[1024] = {1, 2, 3, 4, 5, 6, 7, 8, 9, '\0'};
    size_t binSize = 10;

    MockProgram *pProgram = Program::createFromGenBinary<MockProgram>(*pDevice->getExecutionEnvironment(), nullptr, &genBin[0], binSize, false, &retVal);
    EXPECT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ((uint32_t)CL_PROGRAM_BINARY_TYPE_EXECUTABLE, (uint32_t)pProgram->getProgramBinaryType());

    cl_device_id deviceId = pContext->getDevice(0);
    Device *pDevice = castToObject<Device>(deviceId);
    char *pBin = &genBin[0];
    retVal = CL_INVALID_BINARY;
    binSize = 0;

    // Prepare simple program binary containing patch token PATCH_TOKEN_GTPIN_FREE_GRF_INFO
    SProgramBinaryHeader *pBHdr = (SProgramBinaryHeader *)pBin;
    pBHdr->Magic = iOpenCL::MAGIC_CL;
    pBHdr->Version = iOpenCL::CURRENT_ICBE_VERSION;
    pBHdr->Device = pDevice->getHardwareInfo().platform.eRenderCoreFamily;
    pBHdr->GPUPointerSizeInBytes = 8;
    pBHdr->NumberOfKernels = 1;
    pBHdr->SteppingId = 0;
    pBHdr->PatchListSize = 0;
    pBin += sizeof(SProgramBinaryHeader);
    binSize += sizeof(SProgramBinaryHeader);

    uint32_t patchTokenSize = sizeof(iOpenCL::SPatchGtpinFreeGRFInfo) + GRF_INFO_SIZE;
    SKernelBinaryHeaderCommon *pKHdr = (SKernelBinaryHeaderCommon *)pBin;
    pKHdr->CheckSum = 0;
    pKHdr->ShaderHashCode = 0;
    pKHdr->KernelNameSize = 8;
    pKHdr->PatchListSize = patchTokenSize;
    pKHdr->KernelHeapSize = 0;
    pKHdr->GeneralStateHeapSize = 0;
    pKHdr->DynamicStateHeapSize = 0;
    pKHdr->SurfaceStateHeapSize = 0;
    pKHdr->KernelUnpaddedSize = 0;
    pBin += sizeof(SKernelBinaryHeaderCommon);
    binSize += sizeof(SKernelBinaryHeaderCommon);

    strcpy(pBin, "TstCopy");
    pBin += pKHdr->KernelNameSize;
    binSize += pKHdr->KernelNameSize;

    SPatchGtpinFreeGRFInfo *pPatch = (SPatchGtpinFreeGRFInfo *)pBin;
    pPatch->Token = iOpenCL::PATCH_TOKEN_GTPIN_FREE_GRF_INFO;
    pPatch->Size = patchTokenSize;
    pPatch->BufferSize = GRF_INFO_SIZE;

    binSize += pPatch->Size;
    pBin += pPatch->Size;
    pKHdr->CheckSum = PatchTokenBinary::calcKernelChecksum(ArrayRef<const uint8_t>(reinterpret_cast<uint8_t *>(pKHdr), reinterpret_cast<uint8_t *>(pBin)));

    // Decode prepared program binary
    pProgram->genBinary = makeCopy(&genBin[0], binSize);
    pProgram->genBinarySize = binSize;
    retVal = pProgram->processGenBinary();
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete pProgram;
#undef GRF_INFO_SIZE
}

TEST_F(ProgramTests, ValidBinaryWithIGCVersionEqual0) {
    cl_int retVal;

    auto program = std::make_unique<MockProgram>(*pDevice->getExecutionEnvironment());
    EXPECT_NE(nullptr, program);
    cl_device_id deviceId = pContext->getDevice(0);
    Device *pDevice = castToObject<Device>(deviceId);
    program->setDevice(pDevice);

    // Load a binary program file
    size_t binarySize = 0;
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "CopyBuffer_simd8_", ".bin");
    auto pBinary = loadDataFromFile(filePath.c_str(), binarySize);
    EXPECT_NE(0u, binarySize);
    program->elfBinary = CLElfLib::ElfBinaryStorage(pBinary.get(), pBinary.get() + binarySize);

    // Find its OpenCL program data and mark that the data were created with unknown compiler version,
    // which means that the program has to be rebuild from its IR binary
    CLElfLib::CElfReader elfReader(program->elfBinary);
    const CLElfLib::SElf64Header *elf64Header = elfReader.getElfHeader();
    char *pSectionData = nullptr;
    SProgramBinaryHeader *pBHdr = nullptr;
    EXPECT_NE(nullptr, elf64Header);
    EXPECT_EQ(elf64Header->Type, CLElfLib::E_EH_TYPE::EH_TYPE_OPENCL_EXECUTABLE);

    for (const auto &elfHeaderSection : elfReader.getSectionHeaders()) {
        if (elfHeaderSection.Type != CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_DEV_BINARY) {
            continue;
        }
        pSectionData = elfReader.getSectionData(elfHeaderSection.DataOffset);
        EXPECT_NE(nullptr, pSectionData);
        EXPECT_NE(0u, elfHeaderSection.DataSize);
        pBHdr = (SProgramBinaryHeader *)pSectionData;
        pBHdr->Version = 0; // Simulate compiler Version = 0
        break;
    }
    EXPECT_NE(nullptr, pBHdr);

    // Create program from modified binary, is should be successfully rebuilt
    retVal = program->createProgramFromBinary(pBinary.get(), binarySize);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Get IR binary and modify its header magic,
    // then ask to rebuild program from its IR binary - it should fail
    char *pIrBinary = program->irBinary.get();
    (*pIrBinary)--;
    retVal = program->rebuildProgramFromIr();
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
}

TEST_F(ProgramTests, RebuildBinaryButNoCompilerInterface) {
    auto noCompilerInterfaceExecutionEnvironment = std::make_unique<MockExecutionEnvironment>(nullptr);
    auto program = std::make_unique<MockProgram>(*noCompilerInterfaceExecutionEnvironment);
    EXPECT_NE(nullptr, program);
    cl_device_id deviceId = pContext->getDevice(0);
    Device *pDevice = castToObject<Device>(deviceId);
    program->setDevice(pDevice);

    // Load a binary program file
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "CopyBuffer_simd8_", ".bin");
    size_t binarySize = 0;
    auto pBinary = loadDataFromFile(filePath.c_str(), binarySize);
    EXPECT_NE(0u, binarySize);

    // Create program from loaded binary
    cl_int retVal = program->createProgramFromBinary(pBinary.get(), binarySize);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Ask to rebuild program from its IR binary - it should fail (no Compiler Interface)
    retVal = program->rebuildProgramFromIr();
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
}

TEST_F(ProgramTests, RebuildBinaryWithRebuildError) {
    class MyCompilerInterface : public CompilerInterface {
      public:
        MyCompilerInterface(){};
        ~MyCompilerInterface() override{};

        TranslationOutput::ErrorCode link(const NEO::Device &device, const TranslationInput &input, TranslationOutput &output) override {
            return TranslationOutput::ErrorCode::LinkFailure;
        }
    };

    auto cip = std::make_unique<MyCompilerInterface>();
    MockExecutionEnvironment executionEnvironment(cip.get());
    auto program = std::make_unique<MockProgram>(executionEnvironment);
    cl_device_id deviceId = pContext->getDevice(0);
    Device *pDevice = castToObject<Device>(deviceId);
    program->setDevice(pDevice);

    // Load a binary program file
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "CopyBuffer_simd8_", ".bin");
    size_t binarySize = 0;
    auto pBinary = loadDataFromFile(filePath.c_str(), binarySize);
    EXPECT_NE(0u, binarySize);

    // Create program from loaded binary
    cl_int retVal = program->createProgramFromBinary(pBinary.get(), binarySize);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Ask to rebuild program from its IR binary - it should fail (linking error)
    retVal = program->rebuildProgramFromIr();
    EXPECT_EQ(CL_LINK_PROGRAM_FAILURE, retVal);
}

TEST_F(ProgramTests, BuildProgramWithReraFlag) {
    auto cip = std::make_unique<MockCompilerInterfaceCaptureBuildOptions>();
    MockExecutionEnvironment executionEnvironment(cip.get());
    auto program = std::make_unique<SucceedingGenBinaryProgram>(executionEnvironment);
    cl_device_id deviceId = pContext->getDevice(0);
    Device *pDevice = castToObject<Device>(deviceId);
    program->setDevice(pDevice);
    program->sourceCode = "__kernel mock() {}";

    // Ask to build created program without NEO::CompilerOptions::gtpinRera flag.
    cl_int retVal = program->build(0, nullptr, CompilerOptions::fastRelaxedMath, nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Check build options that were applied
    EXPECT_TRUE(CompilerOptions::contains(cip->buildOptions, CompilerOptions::fastRelaxedMath)) << cip->buildOptions;
    EXPECT_FALSE(CompilerOptions::contains(cip->buildOptions, CompilerOptions::gtpinRera)) << cip->buildInternalOptions;

    // Ask to build created program with NEO::CompilerOptions::gtpinRera flag.
    cip->buildOptions.clear();
    cip->buildInternalOptions.clear();
    retVal = program->build(0, nullptr, CompilerOptions::concatenate(CompilerOptions::gtpinRera, CompilerOptions::finiteMathOnly).c_str(), nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Check build options that were applied
    EXPECT_FALSE(CompilerOptions::contains(cip->buildOptions, CompilerOptions::fastRelaxedMath)) << cip->buildOptions;
    EXPECT_TRUE(CompilerOptions::contains(cip->buildOptions, CompilerOptions::finiteMathOnly)) << cip->buildOptions;
    EXPECT_TRUE(CompilerOptions::contains(cip->buildInternalOptions, CompilerOptions::gtpinRera)) << cip->buildInternalOptions;
}

TEST_F(ProgramTests, RebuildBinaryWithProcessGenBinaryError) {

    cl_int retVal;

    auto program = std::make_unique<FailingGenBinaryProgram>(*pDevice->getExecutionEnvironment());
    EXPECT_NE(nullptr, program);
    cl_device_id deviceId = pContext->getDevice(0);
    Device *pDevice = castToObject<Device>(deviceId);
    program->setDevice(pDevice);

    // Load a binary program file
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "CopyBuffer_simd8_", ".bin");
    size_t binarySize = 0;
    auto pBinary = loadDataFromFile(filePath.c_str(), binarySize);
    EXPECT_NE(0u, binarySize);

    // Create program from loaded binary
    retVal = program->createProgramFromBinary(pBinary.get(), binarySize);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Ask to rebuild program from its IR binary - it should fail (simulated invalid binary)
    retVal = program->rebuildProgramFromIr();
    EXPECT_EQ(CL_INVALID_BINARY, retVal);
}

TEST_F(ProgramTests, GivenZeroPrivateSizeInBlockWhenAllocateBlockProvateSurfacesCalledThenNoSurfaceIsCreated) {
    MockProgram *program = new MockProgram(*pDevice->getExecutionEnvironment(), pContext, false);

    uint32_t crossThreadOffsetBlock = 0;

    KernelInfo *infoBlock = new KernelInfo;

    SPatchAllocateStatelessPrivateSurface *privateSurfaceBlock = new SPatchAllocateStatelessPrivateSurface;
    privateSurfaceBlock->DataParamOffset = crossThreadOffsetBlock;
    privateSurfaceBlock->DataParamSize = 8;
    privateSurfaceBlock->Size = 8;
    privateSurfaceBlock->SurfaceStateHeapOffset = 0;
    privateSurfaceBlock->Token = 0;
    privateSurfaceBlock->PerThreadPrivateMemorySize = 0;
    infoBlock->patchInfo.pAllocateStatelessPrivateSurface = privateSurfaceBlock;

    program->blockKernelManager->addBlockKernelInfo(infoBlock);

    program->allocateBlockPrivateSurfaces(pDevice->getRootDeviceIndex());

    EXPECT_EQ(nullptr, program->getBlockKernelManager()->getPrivateSurface(0));

    delete privateSurfaceBlock;
    delete program;
}

TEST_F(ProgramTests, GivenNonZeroPrivateSizeInBlockWhenAllocateBlockProvateSurfacesCalledThenSurfaceIsCreated) {
    MockProgram *program = new MockProgram(*pDevice->getExecutionEnvironment(), pContext, false);

    uint32_t crossThreadOffsetBlock = 0;

    KernelInfo *infoBlock = new KernelInfo;

    SPatchAllocateStatelessPrivateSurface *privateSurfaceBlock = new SPatchAllocateStatelessPrivateSurface;
    privateSurfaceBlock->DataParamOffset = crossThreadOffsetBlock;
    privateSurfaceBlock->DataParamSize = 8;
    privateSurfaceBlock->Size = 8;
    privateSurfaceBlock->SurfaceStateHeapOffset = 0;
    privateSurfaceBlock->Token = 0;
    privateSurfaceBlock->PerThreadPrivateMemorySize = 1000;
    infoBlock->patchInfo.pAllocateStatelessPrivateSurface = privateSurfaceBlock;

    program->blockKernelManager->addBlockKernelInfo(infoBlock);

    program->allocateBlockPrivateSurfaces(pDevice->getRootDeviceIndex());

    EXPECT_NE(nullptr, program->getBlockKernelManager()->getPrivateSurface(0));

    delete privateSurfaceBlock;
    delete program;
}

TEST_F(ProgramTests, GivenNonZeroPrivateSizeInBlockWhenAllocateBlockProvateSurfacesCalledThenSecondSurfaceIsNotCreated) {
    MockProgram *program = new MockProgram(*pDevice->getExecutionEnvironment(), pContext, false);

    uint32_t crossThreadOffsetBlock = 0;

    KernelInfo *infoBlock = new KernelInfo;

    SPatchAllocateStatelessPrivateSurface *privateSurfaceBlock = new SPatchAllocateStatelessPrivateSurface;
    privateSurfaceBlock->DataParamOffset = crossThreadOffsetBlock;
    privateSurfaceBlock->DataParamSize = 8;
    privateSurfaceBlock->Size = 8;
    privateSurfaceBlock->SurfaceStateHeapOffset = 0;
    privateSurfaceBlock->Token = 0;
    privateSurfaceBlock->PerThreadPrivateMemorySize = 1000;
    infoBlock->patchInfo.pAllocateStatelessPrivateSurface = privateSurfaceBlock;

    program->blockKernelManager->addBlockKernelInfo(infoBlock);

    program->allocateBlockPrivateSurfaces(pDevice->getRootDeviceIndex());

    GraphicsAllocation *privateSurface = program->getBlockKernelManager()->getPrivateSurface(0);

    EXPECT_NE(nullptr, privateSurface);

    program->allocateBlockPrivateSurfaces(pDevice->getRootDeviceIndex());

    GraphicsAllocation *privateSurface2 = program->getBlockKernelManager()->getPrivateSurface(0);

    EXPECT_EQ(privateSurface, privateSurface2);

    delete privateSurfaceBlock;
    delete program;
}

TEST_F(ProgramTests, givenProgramWithBlockKernelsWhenfreeBlockResourcesisCalledThenFreeGraphhicsAllocationsFromBlockKernelManagerIsCalled) {
    MockProgram *program = new MockProgram(*pDevice->getExecutionEnvironment(), pContext, false);

    uint32_t crossThreadOffsetBlock = 0;

    KernelInfo *infoBlock = new KernelInfo;

    SPatchAllocateStatelessPrivateSurface *privateSurfaceBlock = new SPatchAllocateStatelessPrivateSurface;
    privateSurfaceBlock->DataParamOffset = crossThreadOffsetBlock;
    privateSurfaceBlock->DataParamSize = 8;
    privateSurfaceBlock->Size = 8;
    privateSurfaceBlock->SurfaceStateHeapOffset = 0;
    privateSurfaceBlock->Token = 0;
    privateSurfaceBlock->PerThreadPrivateMemorySize = 1000;
    infoBlock->patchInfo.pAllocateStatelessPrivateSurface = privateSurfaceBlock;

    program->blockKernelManager->addBlockKernelInfo(infoBlock);

    GraphicsAllocation *privateSurface = program->getDevice(0).getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    EXPECT_NE(nullptr, privateSurface);

    program->getBlockKernelManager()->pushPrivateSurface(privateSurface, 0);

    program->freeBlockResources();

    delete privateSurfaceBlock;
    delete program;
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
    MockProgram program(*pDevice->getExecutionEnvironment());
    auto &internalOptions = program.getInternalOptions();
    EXPECT_THAT(internalOptions, testing::HasSubstr(std::string("")));
}

TEST_F(Program32BitTests, givenDeviceWithForce32BitAddressingOnWhenProgramIsCreatedThen32bitFlagIsPassedAsInternalOption) {
    MockProgram program(*pDevice->getExecutionEnvironment(), pContext, false);
    auto &internalOptions = program.getInternalOptions();
    std::string s1 = internalOptions;
    size_t pos = s1.find(NEO::CompilerOptions::arch32bit);
    if (is64bit) {
        EXPECT_NE(pos, std::string::npos);
    } else {
        EXPECT_EQ(pos, std::string::npos);
    }
}

TEST_F(ProgramTests, givenNewProgramTheStatelessToStatefulBufferOffsetOtimizationIsMatchingThePlatformEnablingStatus) {
    MockProgram prog(*pDevice->getExecutionEnvironment(), pContext, false);
    auto &internalOpts = prog.getInternalOptions();

    HardwareCapabilities hwCaps = {0};
    HwHelper::get(prog.getDevice(0).getHardwareInfo().platform.eRenderCoreFamily).setupHardwareCapabilities(&hwCaps, prog.getDevice(0).getHardwareInfo());
    if (hwCaps.isStatelesToStatefullWithOffsetSupported) {
        EXPECT_TRUE(CompilerOptions::contains(internalOpts, CompilerOptions::hasBufferOffsetArg));
    } else {
        EXPECT_FALSE(CompilerOptions::contains(internalOpts, CompilerOptions::hasBufferOffsetArg));
    }
}

template <int32_t ErrCodeToReturn, bool spirv = true>
struct CreateProgramFromBinaryMock : public MockProgram {
    CreateProgramFromBinaryMock(ExecutionEnvironment &executionEnvironment, Context *context, bool isBuiltIn)
        : MockProgram(executionEnvironment, context, isBuiltIn) {
    }

    cl_int createProgramFromBinary(const void *pBinary,
                                   size_t binarySize) override {
        this->irBinary.reset(new char[binarySize]);
        this->irBinarySize = binarySize;
        this->isSpirV = spirv;
        memcpy_s(this->irBinary.get(), binarySize, pBinary, binarySize);
        return ErrCodeToReturn;
    }
};

TEST_F(ProgramTests, createFromILWhenCreateProgramFromBinaryFailedThenReturnsNullptr) {
    const uint32_t notSpirv[16] = {0xDEADBEEF};
    cl_int errCode = CL_SUCCESS;
    constexpr cl_int expectedErrCode = CL_INVALID_BINARY;
    auto prog = Program::createFromIL<CreateProgramFromBinaryMock<expectedErrCode>>(pContext, reinterpret_cast<const void *>(notSpirv), sizeof(notSpirv), errCode);
    EXPECT_EQ(nullptr, prog);
    EXPECT_EQ(expectedErrCode, errCode);
}

TEST_F(ProgramTests, createFromILWhenCreateProgramFromBinaryIsSuccessfulThenReturnsValidProgram) {
    const uint32_t spirv[16] = {0x03022307};
    cl_int errCode = CL_SUCCESS;
    constexpr cl_int expectedErrCode = CL_SUCCESS;
    auto prog = Program::createFromIL<CreateProgramFromBinaryMock<expectedErrCode>>(pContext, reinterpret_cast<const void *>(spirv), sizeof(spirv), errCode);
    ASSERT_NE(nullptr, prog);
    EXPECT_EQ(expectedErrCode, errCode);
    prog->release();
}

TEST_F(ProgramTests, givenProgramCreatedFromILWhenCompileIsCalledThenReuseTheILInsteadOfCallingCompilerInterface) {
    const uint32_t spirv[16] = {0x03022307};
    cl_int errCode = 0;
    auto prog = Program::createFromIL<MockProgram>(pContext, reinterpret_cast<const void *>(spirv), sizeof(spirv), errCode);
    ASSERT_NE(nullptr, prog);
    cl_device_id deviceId = pDevice;
    auto debugVars = NEO::getIgcDebugVars();
    debugVars.forceBuildFailure = true;
    gEnvironment->fclPushDebugVars(debugVars);
    auto compilerErr = prog->compile(1, &deviceId, nullptr, 0, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, compilerErr);
    gEnvironment->fclPopDebugVars();
    prog->release();
}

TEST_F(ProgramTests, givenProgramCreatedFromIntermediateBinaryRepresentationWhenCompileIsCalledThenReuseTheILInsteadOfCallingCompilerInterface) {
    const uint32_t spirv[16] = {0x03022307};
    cl_int errCode = 0;
    cl_device_id deviceId = pDevice;
    cl_context ctx = pContext;
    size_t lengths = sizeof(spirv);
    const unsigned char *binaries[1] = {reinterpret_cast<const unsigned char *>(spirv)};
    auto prog = Program::create<MockProgram>(ctx, 1U, &deviceId, &lengths, binaries, nullptr, errCode);
    ASSERT_NE(nullptr, prog);
    auto debugVars = NEO::getIgcDebugVars();
    debugVars.forceBuildFailure = true;
    gEnvironment->fclPushDebugVars(debugVars);
    auto compilerErr = prog->compile(1, &deviceId, nullptr, 0, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, compilerErr);
    gEnvironment->fclPopDebugVars();
    prog->release();
}

TEST_F(ProgramTests, createFromILWhenIlIsNullptrThenReturnsInvalidBinaryError) {
    cl_int errCode = CL_SUCCESS;
    constexpr cl_int expectedErrCode = CL_INVALID_BINARY;
    auto prog = Program::createFromIL<CreateProgramFromBinaryMock<expectedErrCode>>(pContext, nullptr, 16, errCode);
    EXPECT_EQ(nullptr, prog);
    EXPECT_EQ(expectedErrCode, errCode);
}

TEST_F(ProgramTests, createFromILWhenIlIsSizeIs0ThenReturnsInvalidBinaryError) {
    const uint32_t spirv[16] = {0x03022307};
    cl_int errCode = CL_SUCCESS;
    constexpr cl_int expectedErrCode = CL_INVALID_BINARY;
    auto prog = Program::createFromIL<CreateProgramFromBinaryMock<expectedErrCode>>(pContext, reinterpret_cast<const void *>(spirv), 0, errCode);
    EXPECT_EQ(nullptr, prog);
    EXPECT_EQ(expectedErrCode, errCode);
}

TEST_F(ProgramTests, createFromILWhenCreatingProgramFromBinaryThenProperFlagIsSignalled) {
    const uint32_t spirv[16] = {0x03022307};
    cl_int errCode = CL_SUCCESS;
    auto prog = Program::createFromIL<Program>(pContext, reinterpret_cast<const void *>(spirv), sizeof(spirv), errCode);
    EXPECT_NE(nullptr, prog);
    EXPECT_EQ(CL_SUCCESS, errCode);
    EXPECT_TRUE(prog->getIsSpirV());
    prog->release();

    const char llvmBc[16] = {'B', 'C', '\xc0', '\xde'};
    prog = Program::createFromIL<Program>(pContext, reinterpret_cast<const void *>(llvmBc), sizeof(llvmBc), errCode);
    EXPECT_NE(nullptr, prog);
    EXPECT_EQ(CL_SUCCESS, errCode);
    EXPECT_FALSE(prog->getIsSpirV());
    prog->release();
}

static const char llvmBinary[] = "BC\xc0\xde     ";

TEST(isValidLlvmBinary, whenLlvmMagicWasFoundThenBinaryIsValidLLvm) {
    EXPECT_TRUE(Program::isValidLlvmBinary(llvmBinary, sizeof(llvmBinary)));
}

TEST(isValidLlvmBinary, whenBinaryIsNullptrThenBinaryIsNotValidLLvm) {
    EXPECT_FALSE(Program::isValidLlvmBinary(nullptr, sizeof(llvmBinary)));
}

TEST(isValidLlvmBinary, whenBinaryIsShorterThanLllvMagicThenBinaryIsNotValidLLvm) {
    EXPECT_FALSE(Program::isValidLlvmBinary(llvmBinary, 2));
}

TEST(isValidLlvmBinary, whenBinaryDoesNotContainLllvMagicThenBinaryIsNotValidLLvm) {
    char notLlvmBinary[] = "ABCDEFGHIJKLMNO";
    EXPECT_FALSE(Program::isValidLlvmBinary(notLlvmBinary, sizeof(notLlvmBinary)));
}

const uint32_t spirv[16] = {0x03022307};
const uint32_t spirvInvEndianes[16] = {0x07230203};

TEST(isValidSpirvBinary, whenSpirvMagicWasFoundThenBinaryIsValidSpirv) {
    EXPECT_TRUE(Program::isValidSpirvBinary(spirv, sizeof(spirv)));
    EXPECT_TRUE(Program::isValidSpirvBinary(spirvInvEndianes, sizeof(spirvInvEndianes)));
}

TEST(isValidSpirvBinary, whenBinaryIsNullptrThenBinaryIsNotValidLLvm) {
    EXPECT_FALSE(Program::isValidSpirvBinary(nullptr, sizeof(spirv)));
}

TEST(isValidSpirvBinary, whenBinaryIsShorterThanLllvMagicThenBinaryIsNotValidLLvm) {
    EXPECT_FALSE(Program::isValidSpirvBinary(spirv, 2));
}

TEST(isValidSpirvBinary, whenBinaryDoesNotContainLllvMagicThenBinaryIsNotValidLLvm) {
    char notSpirvBinary[] = "ABCDEFGHIJKLMNO";
    EXPECT_FALSE(Program::isValidSpirvBinary(notSpirvBinary, sizeof(notSpirvBinary)));
}

TEST_F(ProgramTests, linkingTwoValidSpirvProgramsReturnsValidProgram) {
    const uint32_t spirv[16] = {0x03022307};
    cl_int errCode = CL_SUCCESS;

    auto node1 = Program::createFromIL<CreateProgramFromBinaryMock<CL_SUCCESS, false>>(pContext, reinterpret_cast<const void *>(spirv), sizeof(spirv), errCode);
    ASSERT_NE(nullptr, node1);
    EXPECT_EQ(CL_SUCCESS, errCode);

    auto node2 = Program::createFromIL<CreateProgramFromBinaryMock<CL_SUCCESS>>(pContext, reinterpret_cast<const void *>(spirv), sizeof(spirv), errCode);
    ASSERT_NE(nullptr, node2);
    EXPECT_EQ(CL_SUCCESS, errCode);

    auto prog = Program::createFromIL<CreateProgramFromBinaryMock<CL_SUCCESS>>(pContext, reinterpret_cast<const void *>(spirv), sizeof(spirv), errCode);
    ASSERT_NE(nullptr, prog);
    EXPECT_EQ(CL_SUCCESS, errCode);

    cl_program linkNodes[] = {node1, node2};
    errCode = prog->link(0, nullptr, nullptr, 2, linkNodes, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, errCode);

    prog->release();
    node2->release();
    node1->release();
}

TEST_F(ProgramTests, givenSeparateBlockKernelsWhenNoParentAndSubgroupKernelsThenSeparateNoneKernel) {
    MockProgram program(*pDevice->getExecutionEnvironment(), pContext, false);

    EXPECT_EQ(0u, program.getKernelInfoArray().size());
    EXPECT_EQ(0u, program.getParentKernelInfoArray().size());
    EXPECT_EQ(0u, program.getSubgroupKernelInfoArray().size());

    program.separateBlockKernels();

    EXPECT_EQ(0u, program.getKernelInfoArray().size());
    EXPECT_EQ(0u, program.getBlockKernelManager()->getCount());
}

TEST_F(ProgramTests, givenSeparateBlockKernelsWhenRegularKernelsThenSeparateNoneKernel) {
    MockProgram program(*pDevice->getExecutionEnvironment(), pContext, false);

    auto pRegularKernel1Info = new KernelInfo();
    pRegularKernel1Info->name = "regular_kernel_1";
    program.getKernelInfoArray().push_back(pRegularKernel1Info);

    auto pRegularKernel2Info = new KernelInfo();
    pRegularKernel2Info->name = "regular_kernel_2";
    program.getKernelInfoArray().push_back(pRegularKernel2Info);

    EXPECT_EQ(2u, program.getKernelInfoArray().size());

    program.separateBlockKernels();

    EXPECT_EQ(2u, program.getKernelInfoArray().size());
    EXPECT_EQ(0, strcmp("regular_kernel_1", program.getKernelInfoArray().at(0)->name.c_str()));
    EXPECT_EQ(0, strcmp("regular_kernel_2", program.getKernelInfoArray().at(1)->name.c_str()));

    EXPECT_EQ(0u, program.getBlockKernelManager()->getCount());
}

TEST_F(ProgramTests, givenSeparateBlockKernelsWhenChildLikeKernelWithoutParentKernelThenSeparateNoneKernel) {
    MockProgram program(*pDevice->getExecutionEnvironment(), pContext, false);

    auto pParentKernelInfo = new KernelInfo();
    pParentKernelInfo->name = "another_parent_kernel";
    program.getKernelInfoArray().push_back(pParentKernelInfo);
    program.getParentKernelInfoArray().push_back(pParentKernelInfo);

    auto pChildKernelInfo = new KernelInfo();
    pChildKernelInfo->name = "childlike_kernel_dispatch_0";
    program.getKernelInfoArray().push_back(pChildKernelInfo);

    EXPECT_EQ(2u, program.getKernelInfoArray().size());
    EXPECT_EQ(1u, program.getParentKernelInfoArray().size());

    program.separateBlockKernels();

    EXPECT_EQ(2u, program.getKernelInfoArray().size());
    EXPECT_EQ(0, strcmp("another_parent_kernel", program.getKernelInfoArray().at(0)->name.c_str()));
    EXPECT_EQ(0, strcmp("childlike_kernel_dispatch_0", program.getKernelInfoArray().at(1)->name.c_str()));

    EXPECT_EQ(0u, program.getBlockKernelManager()->getCount());
}

TEST_F(ProgramTests, givenSeparateBlockKernelsWhenChildLikeKernelWithoutSubgroupKernelThenSeparateNoneKernel) {
    MockProgram program(*pDevice->getExecutionEnvironment(), pContext, false);

    auto pSubgroupKernelInfo = new KernelInfo();
    pSubgroupKernelInfo->name = "another_subgroup_kernel";
    program.getKernelInfoArray().push_back(pSubgroupKernelInfo);
    program.getSubgroupKernelInfoArray().push_back(pSubgroupKernelInfo);

    auto pChildKernelInfo = new KernelInfo();
    pChildKernelInfo->name = "childlike_kernel_dispatch_0";
    program.getKernelInfoArray().push_back(pChildKernelInfo);

    EXPECT_EQ(2u, program.getKernelInfoArray().size());
    EXPECT_EQ(1u, program.getSubgroupKernelInfoArray().size());

    program.separateBlockKernels();

    EXPECT_EQ(2u, program.getKernelInfoArray().size());
    EXPECT_EQ(0, strcmp("another_subgroup_kernel", program.getKernelInfoArray().at(0)->name.c_str()));
    EXPECT_EQ(0, strcmp("childlike_kernel_dispatch_0", program.getKernelInfoArray().at(1)->name.c_str()));

    EXPECT_EQ(0u, program.getBlockKernelManager()->getCount());
}

TEST_F(ProgramTests, givenSeparateBlockKernelsWhenParentKernelWithChildKernelThenSeparateChildKernel) {
    MockProgram program(*pDevice->getExecutionEnvironment(), pContext, false);

    auto pParentKernelInfo = new KernelInfo();
    pParentKernelInfo->name = "parent_kernel";
    program.getKernelInfoArray().push_back(pParentKernelInfo);
    program.getParentKernelInfoArray().push_back(pParentKernelInfo);

    auto pChildKernelInfo = new KernelInfo();
    pChildKernelInfo->name = "parent_kernel_dispatch_0";
    program.getKernelInfoArray().push_back(pChildKernelInfo);

    EXPECT_EQ(2u, program.getKernelInfoArray().size());
    EXPECT_EQ(1u, program.getParentKernelInfoArray().size());

    program.separateBlockKernels();

    EXPECT_EQ(1u, program.getKernelInfoArray().size());
    EXPECT_EQ(0, strcmp("parent_kernel", program.getKernelInfoArray().at(0)->name.c_str()));

    EXPECT_EQ(1u, program.getBlockKernelManager()->getCount());
    EXPECT_EQ(0, strcmp("parent_kernel_dispatch_0", program.getBlockKernelManager()->getBlockKernelInfo(0)->name.c_str()));
}

TEST_F(ProgramTests, givenSeparateBlockKernelsWhenSubgroupKernelWithChildKernelThenSeparateChildKernel) {
    MockProgram program(*pDevice->getExecutionEnvironment(), pContext, false);

    auto pSubgroupKernelInfo = new KernelInfo();
    pSubgroupKernelInfo->name = "subgroup_kernel";
    program.getKernelInfoArray().push_back(pSubgroupKernelInfo);
    program.getSubgroupKernelInfoArray().push_back(pSubgroupKernelInfo);

    auto pChildKernelInfo = new KernelInfo();
    pChildKernelInfo->name = "subgroup_kernel_dispatch_0";
    program.getKernelInfoArray().push_back(pChildKernelInfo);

    EXPECT_EQ(2u, program.getKernelInfoArray().size());
    EXPECT_EQ(1u, program.getSubgroupKernelInfoArray().size());

    program.separateBlockKernels();

    EXPECT_EQ(1u, program.getKernelInfoArray().size());
    EXPECT_EQ(0, strcmp("subgroup_kernel", program.getKernelInfoArray().at(0)->name.c_str()));

    EXPECT_EQ(1u, program.getBlockKernelManager()->getCount());
    EXPECT_EQ(0, strcmp("subgroup_kernel_dispatch_0", program.getBlockKernelManager()->getBlockKernelInfo(0)->name.c_str()));
}

TEST(SimpleProgramTests, givenDefaultProgramWhenSetDeviceIsCalledThenDeviceIsSet) {
    ExecutionEnvironment executionEnvironment;
    MockProgram program(executionEnvironment);
    EXPECT_EQ(nullptr, program.getDevicePtr());
    auto dummyDevice = (Device *)0x1337;
    program.SetDevice(dummyDevice);
    EXPECT_EQ(dummyDevice, program.getDevicePtr());
    program.SetDevice(nullptr);
    EXPECT_EQ(nullptr, program.getDevicePtr());
}

TEST(ProgramDestructionTests, givenProgramUsingDeviceWhenItIsDestroyedAfterPlatfromCleanupThenItIsCleanedUpProperly) {
    platformImpl->initialize();
    auto device = platformImpl->getDevice(0);
    MockContext *context = new MockContext(device, false);
    MockProgram *pProgram = new MockProgram(*device->getExecutionEnvironment(), context, false);
    auto globalAllocation = device->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    pProgram->setGlobalSurface(globalAllocation);

    platformImpl.reset(nullptr);
    EXPECT_EQ(1, device->getRefInternalCount());
    EXPECT_EQ(1, pProgram->getRefInternalCount());
    context->decRefInternal();
    pProgram->decRefInternal();
}

TEST_F(ProgramTests, givenProgramWithSpirvWhenRebuildProgramIsCalledThenSpirvPathIsTaken) {
    auto device = castToObject<Device>(pContext->getDevice(0));

    auto compilerInterface = new MockCompilerInterface();
    auto compilerMain = new MockCIFMain();
    compilerInterface->SetFclMain(compilerMain);
    compilerMain->Retain();
    compilerInterface->SetIgcMain(compilerMain);
    compilerMain->setDefaultCreatorFunc<NEO::MockIgcOclDeviceCtx>(NEO::MockIgcOclDeviceCtx::Create);
    compilerMain->setDefaultCreatorFunc<NEO::MockFclOclDeviceCtx>(NEO::MockFclOclDeviceCtx::Create);
    pDevice->getExecutionEnvironment()->compilerInterface.reset(compilerInterface);

    std::string receivedInput;
    MockCompilerDebugVars debugVars = {};
    debugVars.receivedInput = &receivedInput;
    debugVars.forceBuildFailure = true;
    gEnvironment->igcPushDebugVars(debugVars);
    std::unique_ptr<void, void (*)(void *)> igcDebugVarsAutoPop{&gEnvironment, [](void *) { gEnvironment->igcPopDebugVars(); }};

    auto program = clUniquePtr(new MockProgram(*pDevice->getExecutionEnvironment()));
    program->setDevice(device);
    uint32_t spirv[16] = {0x03022307, 0x23471113, 0x17192329};
    program->irBinary = makeCopy(spirv, sizeof(spirv));
    program->irBinarySize = sizeof(spirv);
    program->isSpirV = true;
    auto buildRet = program->rebuildProgramFromIr();
    EXPECT_NE(CL_SUCCESS, buildRet);

    CLElfLib::ElfBinaryStorage elfBin(receivedInput.begin(), receivedInput.end());
    CLElfLib::CElfReader elfReader(elfBin);

    char *spvSectionData = nullptr;
    size_t spvSectionDataSize = 0;
    for (const auto &elfSectionHeader : elfReader.getSectionHeaders()) {
        if (elfSectionHeader.Type == CLElfLib::E_SH_TYPE::SH_TYPE_SPIRV) {
            spvSectionData = elfReader.getSectionData(elfSectionHeader.DataOffset);
            spvSectionDataSize = static_cast<size_t>(elfSectionHeader.DataSize);
        }
    }
    EXPECT_EQ(sizeof(spirv), spvSectionDataSize);
    EXPECT_EQ(0, memcmp(spirv, spvSectionData, spvSectionDataSize));
}

TEST_F(ProgramTests, whenRebuildingProgramThenStoreDeviceBinaryProperly) {
    auto device = castToObject<Device>(pContext->getDevice(0));

    auto compilerInterface = new MockCompilerInterface();
    pDevice->getExecutionEnvironment()->compilerInterface.reset(compilerInterface);
    auto compilerMain = new MockCIFMain();
    compilerInterface->SetIgcMain(compilerMain);
    compilerMain->setDefaultCreatorFunc<NEO::MockIgcOclDeviceCtx>(NEO::MockIgcOclDeviceCtx::Create);

    MockCompilerDebugVars debugVars = {};
    char binaryToReturn[] = "abcdfghijklmnop";
    debugVars.binaryToReturn = binaryToReturn;
    debugVars.binaryToReturnSize = sizeof(binaryToReturn);
    gEnvironment->igcPushDebugVars(debugVars);
    std::unique_ptr<void, void (*)(void *)> igcDebugVarsAutoPop{&gEnvironment, [](void *) { gEnvironment->igcPopDebugVars(); }};

    auto program = clUniquePtr(new MockProgram(*pDevice->getExecutionEnvironment()));
    program->setDevice(device);
    uint32_t ir[16] = {0x03022307, 0x23471113, 0x17192329};
    program->irBinary = makeCopy(ir, sizeof(ir));
    program->irBinarySize = sizeof(ir);
    EXPECT_EQ(nullptr, program->genBinary);
    EXPECT_EQ(0U, program->genBinarySize);
    program->rebuildProgramFromIr();
    ASSERT_NE(nullptr, program->genBinary);
    ASSERT_EQ(sizeof(binaryToReturn), program->genBinarySize);
    EXPECT_EQ(0, memcmp(binaryToReturn, program->genBinary.get(), program->genBinarySize));
}

TEST_F(ProgramTests, givenProgramWhenInternalOptionsArePassedThenTheyAreAddedToProgramInternalOptions) {
    ExecutionEnvironment executionEnvironment;
    MockProgram program(executionEnvironment);
    program.getInternalOptions().erase();
    EXPECT_EQ(nullptr, program.getDevicePtr());
    std::string buildOptions(NEO::CompilerOptions::gtpinRera);
    program.extractInternalOptions(buildOptions);
    EXPECT_STREQ(program.getInternalOptions().c_str(), NEO::CompilerOptions::gtpinRera.data());
}

TEST_F(ProgramTests, givenProgramWhenUnknownInternalOptionsArePassedThenTheyAreNotAddedToProgramInternalOptions) {
    ExecutionEnvironment executionEnvironment;
    MockProgram program(executionEnvironment);
    program.getInternalOptions().erase();
    EXPECT_EQ(nullptr, program.getDevicePtr());
    const char *internalOption = "-unknown-internal-options-123";
    std::string buildOptions(internalOption);
    program.extractInternalOptions(buildOptions);
    EXPECT_EQ(0u, program.getInternalOptions().length());
}

TEST_F(ProgramTests, givenProgramWhenAllInternalOptionsArePassedMixedWithUnknownInputThenTheyAreParsedCorrectly) {
    ExecutionEnvironment executionEnvironment;
    MockProgram program(executionEnvironment);
    program.getInternalOptions().erase();
    EXPECT_EQ(nullptr, program.getDevicePtr());
    std::string buildOptions = CompilerOptions::concatenate("###", CompilerOptions::gtpinRera, "###", CompilerOptions::greaterThan4gbBuffersRequired, "###");
    std::string expectedOutput = CompilerOptions::concatenate(CompilerOptions::gtpinRera, CompilerOptions::greaterThan4gbBuffersRequired);
    program.extractInternalOptions(buildOptions);
    EXPECT_EQ(expectedOutput, program.getInternalOptions());
}

TEST_F(ProgramTests, givenProgramWhenInternalOptionsArePassedWithValidValuesThenTheyAreAddedToProgramInternalOptions) {
    ExecutionEnvironment executionEnvironment;
    MockProgram program(executionEnvironment);
    program.getInternalOptions().erase();
    EXPECT_EQ(nullptr, program.getDevicePtr());

    program.isFlagOptionOverride = false;
    program.isOptionValueValidOverride = true;
    std::string buildOptions = CompilerOptions::concatenate(CompilerOptions::gtpinRera, "someValue");
    program.extractInternalOptions(buildOptions);
    EXPECT_EQ(buildOptions, program.getInternalOptions()) << program.getInternalOptions();
}

TEST_F(ProgramTests, givenProgramWhenInternalOptionsArePassedWithInvalidValuesThenTheyAreNotAddedToProgramInternalOptions) {
    ExecutionEnvironment executionEnvironment;
    MockProgram program(executionEnvironment);
    EXPECT_EQ(nullptr, program.getDevicePtr());

    program.isFlagOptionOverride = false;
    std::string buildOptions = CompilerOptions::concatenate(CompilerOptions::gtpinRera, "someValue");
    std::string expectedOutput = "";
    program.getInternalOptions().erase();
    program.extractInternalOptions(buildOptions);
    EXPECT_EQ(expectedOutput, program.getInternalOptions());

    program.isOptionValueValidOverride = true;
    buildOptions = CompilerOptions::gtpinRera;
    program.getInternalOptions().erase();
    program.extractInternalOptions(buildOptions);
    EXPECT_EQ(expectedOutput, program.getInternalOptions());
}

TEST_F(ProgramTests, givenProgramWhenGetSymbolsIsCalledThenMapWithExportedSymbolsIsReturned) {
    ExecutionEnvironment executionEnvironment;
    MockProgram program(executionEnvironment);
    EXPECT_EQ(&program.symbols, &program.getSymbols());
}

class AdditionalOptionsMockProgram : public MockProgram {
  public:
    AdditionalOptionsMockProgram() : MockProgram(executionEnvironment) {}
    void applyAdditionalOptions() override {
        applyAdditionalOptionsCalled++;
        MockProgram::applyAdditionalOptions();
    }
    uint32_t applyAdditionalOptionsCalled = 0;
    ExecutionEnvironment executionEnvironment;
};

TEST_F(ProgramTests, givenProgramWhenBuiltThenAdditionalOptionsAreApplied) {
    AdditionalOptionsMockProgram program;
    cl_device_id device = pDevice;

    program.build(1, &device, nullptr, nullptr, nullptr, false);
    EXPECT_EQ(1u, program.applyAdditionalOptionsCalled);
}

struct RebuildProgram : public Program {
    using Program::createProgramFromBinary;
    RebuildProgram(ExecutionEnvironment &executionEnvironment, Context *context, bool isBuiltIn = false) : Program(executionEnvironment, context, isBuiltIn) {}
    cl_int rebuildProgramFromIr() override {
        rebuildProgramFromIrCalled = true;
        return CL_SUCCESS;
    }
    cl_int processElfBinary(const void *pBinary, size_t binarySize, uint32_t &binaryVersion) override {
        processElfBinaryCalled = true;
        return CL_SUCCESS;
    }
    bool rebuildProgramFromIrCalled = false;
    bool processElfBinaryCalled = false;
};

TEST(RebuildProgramFromIrTests, givenBinaryProgramWhenKernelRebulildIsForcedThenRebuildProgramFromIrCalled) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.RebuildPrecompiledKernels.set(true);
    cl_int retVal = CL_INVALID_BINARY;

    SProgramBinaryHeader binHeader;
    memset(&binHeader, 0, sizeof(binHeader));
    binHeader.Magic = iOpenCL::MAGIC_CL;
    binHeader.Version = iOpenCL::CURRENT_ICBE_VERSION;
    binHeader.Device = platformDevices[0]->platform.eRenderCoreFamily;
    binHeader.GPUPointerSizeInBytes = 8;
    binHeader.NumberOfKernels = 0;
    binHeader.SteppingId = 0;
    binHeader.PatchListSize = 0;
    size_t binSize = sizeof(SProgramBinaryHeader);

    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<RebuildProgram> pProgram(RebuildProgram::createFromGenBinary<RebuildProgram>(executionEnvironment, nullptr, &binHeader, binSize, false, &retVal));
    ASSERT_NE(nullptr, pProgram.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    binHeader.Version = iOpenCL::CURRENT_ICBE_VERSION;
    retVal = pProgram->createProgramFromBinary(&binHeader, binSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(pProgram->processElfBinaryCalled);
    EXPECT_TRUE(pProgram->rebuildProgramFromIrCalled);
}

TEST(RebuildProgramFromIrTests, givenBinaryProgramWhenKernelRebulildIsNotForcedThenRebuildProgramFromIrNotCalled) {
    cl_int retVal = CL_INVALID_BINARY;

    SProgramBinaryHeader binHeader;
    memset(&binHeader, 0, sizeof(binHeader));
    binHeader.Magic = iOpenCL::MAGIC_CL;
    binHeader.Version = iOpenCL::CURRENT_ICBE_VERSION;
    binHeader.Device = platformDevices[0]->platform.eRenderCoreFamily;
    binHeader.GPUPointerSizeInBytes = 8;
    binHeader.NumberOfKernels = 0;
    binHeader.SteppingId = 0;
    binHeader.PatchListSize = 0;
    size_t binSize = sizeof(SProgramBinaryHeader);

    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<RebuildProgram> pProgram(RebuildProgram::createFromGenBinary<RebuildProgram>(executionEnvironment, nullptr, &binHeader, binSize, false, &retVal));
    ASSERT_NE(nullptr, pProgram.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    binHeader.Version = iOpenCL::CURRENT_ICBE_VERSION;
    retVal = pProgram->createProgramFromBinary(&binHeader, binSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(pProgram->processElfBinaryCalled);
    EXPECT_FALSE(pProgram->rebuildProgramFromIrCalled);
}

TEST(Program, whenGetKernelNamesStringIsCalledThenNamesAreProperlyConcatenated) {
    ExecutionEnvironment execEnv;
    MockProgram program{execEnv};
    KernelInfo kernel1 = {};
    kernel1.name = "kern1";
    KernelInfo kernel2 = {};
    kernel2.name = "kern2";
    program.getKernelInfoArray().push_back(&kernel1);
    program.getKernelInfoArray().push_back(&kernel2);
    EXPECT_EQ("kern1;kern2", program.getKernelNamesString());
    program.getKernelInfoArray().clear();
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
    TranslationOutput::ErrorCode getSpecConstantsInfo(const NEO::Device &device, ArrayRef<const char> srcSpirV, SpecConstantInfo &output) override {
        counter++;
        return retVal;
    }
    void returnError() {
        retVal = TranslationOutput::ErrorCode::CompilationFailure;
    }
};

struct SpecializationConstantExecutionEnvironmentMock : public ExecutionEnvironment {
    SpecializationConstantExecutionEnvironmentMock() {
        compilerInterface.reset(new SpecializationConstantCompilerInterfaceMock());
    }
    CompilerInterface *getCompilerInterface() override {
        return compilerInterface.get();
    }
};

struct setProgramSpecializationConstantTests : public ::testing::Test {
    void SetUp() override {
        mockProgram.reset(new SpecializationConstantProgramMock(executionEnvironment));
        mockProgram->isSpirV = true;
        mockProgram->SetDevice(&device);

        EXPECT_FALSE(mockProgram->areSpecializationConstantsInitialized);
        EXPECT_EQ(0, mockCompiler->counter);
    }

    SpecializationConstantExecutionEnvironmentMock executionEnvironment;
    SpecializationConstantCompilerInterfaceMock *mockCompiler = reinterpret_cast<SpecializationConstantCompilerInterfaceMock *>(executionEnvironment.getCompilerInterface());
    std::unique_ptr<SpecializationConstantProgramMock> mockProgram;
    MockDevice device;

    int specValue = 1;
};

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
    reinterpret_cast<SpecializationConstantCompilerInterfaceMock *>(executionEnvironment.getCompilerInterface())->returnError();

    auto retVal = mockProgram->setProgramSpecializationConstant(1, sizeof(int), &specValue);

    EXPECT_EQ(1, mockCompiler->counter);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_FALSE(mockProgram->areSpecializationConstantsInitialized);
}

TEST(setProgramSpecializationConstantTest, givenUninitializedCompilerinterfaceWhenSetProgramSpecializationConstantThenErrorIsReturned) {
    struct MockExecutionEnvironment : public ExecutionEnvironment {
        CompilerInterface *getCompilerInterface() override {
            return compilerInterface.get();
        }
    };

    MockExecutionEnvironment executionEnvironment;
    SpecializationConstantProgramMock mockProgram(executionEnvironment);
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

    cl_device_id device = pDevice;
    CreateProgramFromBinary(pContext, &device, "kernel_data_param");

    auto retVal = pProgram->build(
        1,
        &device,
        nullptr,
        nullptr,
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

    cl_device_id device = pDevice;
    const char *sourceCode = "__kernel void\nCB(\n__global unsigned int* src, __global unsigned int* dst)\n{\nint id = (int)get_global_id(0);\ndst[id] = src[id];\n}\n";
    pProgram = Program::create<MockProgram>(
        pContext,
        1,
        &sourceCode,
        &knownSourceSize,
        retVal);
    retVal = pProgram->build(1, &device, nullptr, nullptr, nullptr, false);
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

TEST_F(ProgramBinTest, GivenDebugDataAvailableWhenLinkingProgramThenDebugDataIsStoredInProgram) {
    DebugDataGuard debugDataGuard;

    cl_device_id device = pDevice;
    const char *sourceCode = "__kernel void\nCB(\n__global unsigned int* src, __global unsigned int* dst)\n{\nint id = (int)get_global_id(0);\ndst[id] = src[id];\n}\n";
    pProgram = Program::create<MockProgram>(
        pContext,
        1,
        &sourceCode,
        &knownSourceSize,
        retVal);

    retVal = pProgram->compile(1, &device, nullptr, 0, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_program programToLink = pProgram;
    retVal = pProgram->link(1, &device, nullptr, 1, &programToLink, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(nullptr, pProgram->getDebugData());
}
