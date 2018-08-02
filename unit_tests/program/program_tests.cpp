/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
#include "runtime/kernel/kernel.h"
#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/compiler_interface/compiler_options.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "runtime/indirect_heap/indirect_heap.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/hash.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/string.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/surface.h"
#include "runtime/program/create.inl"
#include "program_tests.h"
#include "unit_tests/fixtures/program_fixture.inl"
#include "unit_tests/global_environment.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/kernel_binary_helper.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/program/program_from_binary.h"
#include "unit_tests/program/program_with_source.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_program.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "elf/reader.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace OCLRT;

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

////////////////////////////////////////////////////////////////////////////////
// Program::createProgramWithBinary
////////////////////////////////////////////////////////////////////////////////
TEST_P(ProgramFromBinaryTest, CreateWithBinary_Simple) {}

////////////////////////////////////////////////////////////////////////////////
// Program::BuildProgram
////////////////////////////////////////////////////////////////////////////////
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

////////////////////////////////////////////////////////////////////////////////
// Program::getInfo( context )
////////////////////////////////////////////////////////////////////////////////
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

////////////////////////////////////////////////////////////////////////////////
// Program::getInfo( binary )
////////////////////////////////////////////////////////////////////////////////
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
        (const char *)knownSource,
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

////////////////////////////////////////////////////////////////////////////////
// Program::getInfo( binary size )
////////////////////////////////////////////////////////////////////////////////
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

////////////////////////////////////////////////////////////////////////////////
// Program::getInfo( num kernels )
////////////////////////////////////////////////////////////////////////////////
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
    CreateProgramFromBinary<MockProgram>(pContext, &device, BinaryFileName);
    MockProgram *p = (MockProgram *)pProgram;
    p->SetBuildStatus(CL_BUILD_NONE);

    retVal = pProgram->getInfo(
        paramName,
        param_value_size,
        &param_value,
        &param_value_size_ret);
    ASSERT_EQ(CL_INVALID_PROGRAM_EXECUTABLE, retVal);
}

////////////////////////////////////////////////////////////////////////////////
// Program::getInfo( kernel names )
////////////////////////////////////////////////////////////////////////////////
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
    CreateProgramFromBinary<MockProgram>(pContext, &device, BinaryFileName);
    MockProgram *p = (MockProgram *)pProgram;
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

////////////////////////////////////////////////////////////////////////////////
// Program::getBuildInfo( status )
////////////////////////////////////////////////////////////////////////////////
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
    CreateProgramFromBinary<MockProgram>(pContext, &device, BinaryFileName);
    MockProgram *p = (MockProgram *)pProgram;
    p->SetDevice(reinterpret_cast<Device *>(pContext));

    retVal = pProgram->getBuildInfo(
        reinterpret_cast<Device *>(pContext),
        paramName,
        param_value_size,
        &buildStatus,
        &param_value_size_ret);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

////////////////////////////////////////////////////////////////////////////////
// Program::getBuildInfo( status )
////////////////////////////////////////////////////////////////////////////////
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

////////////////////////////////////////////////////////////////////////////////
// Program::getBuildInfo( options )
////////////////////////////////////////////////////////////////////////////////
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

////////////////////////////////////////////////////////////////////////////////
// Program::getBuildInfo( log )
////////////////////////////////////////////////////////////////////////////////
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

////////////////////////////////////////////////////////////////////////////////
// Program::getBuildInfo( log )
////////////////////////////////////////////////////////////////////////////////
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

////////////////////////////////////////////////////////////////////////////////
// Program::getBuildInfo( binary type )
////////////////////////////////////////////////////////////////////////////////
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

////////////////////////////////////////////////////////////////////////////////
// Program::getBuildInfo( global variable total size )
////////////////////////////////////////////////////////////////////////////////
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
    CreateProgramFromBinary<MockProgram>(pContext, &device, BinaryFileName);
    MockProgram *p = (MockProgram *)pProgram;
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
    EXPECT_TRUE(graphicsAllocation->is32BitAllocation);
    EXPECT_EQ(graphicsAllocation->getUnderlyingBufferSize(), kernelInfo->heapInfo.pKernelHeader->KernelHeapSize);

    auto kernelIsa = graphicsAllocation->getUnderlyingBuffer();
    EXPECT_NE(kernelInfo->heapInfo.pKernelHeap, kernelIsa);
    EXPECT_EQ(0, memcmp(kernelIsa, kernelInfo->heapInfo.pKernelHeap, kernelInfo->heapInfo.pKernelHeader->KernelHeapSize));
    if (sizeof(void *) == sizeof(uint32_t)) {
        EXPECT_EQ(0u, graphicsAllocation->gpuBaseAddress);
    } else {
        EXPECT_NE(0u, graphicsAllocation->gpuBaseAddress);
    }
}

TEST_P(ProgramFromBinaryTest, givenProgramWhenCleanKernelInfoIsCalledThenKernelAllocationIsFreed) {
    cl_device_id device = pDevice;
    pProgram->build(1, &device, nullptr, nullptr, nullptr, true);
    EXPECT_EQ(1u, pProgram->getNumKernels());
    pProgram->cleanCurrentKernelInfo();
    EXPECT_EQ(0u, pProgram->getNumKernels());
}

TEST_P(ProgramFromBinaryTest, givenProgramWhenCleanCurrentKernelInfoIsCalledButGpuIsNotYetDoneThenKernelAllocationIsPutOnDefferedFreeList) {
    cl_device_id device = pDevice;
    auto memoryManager = pDevice->getMemoryManager();
    EXPECT_TRUE(memoryManager->graphicsAllocations.peekIsEmpty());
    pProgram->build(1, &device, nullptr, nullptr, nullptr, true);
    auto kernelAllocation = pProgram->getKernelInfo(size_t(0))->getGraphicsAllocation();
    kernelAllocation->taskCount = 100;
    *pDevice->getTagAddress() = 0;
    pProgram->cleanCurrentKernelInfo();
    EXPECT_FALSE(memoryManager->graphicsAllocations.peekIsEmpty());
    EXPECT_EQ(memoryManager->graphicsAllocations.peekHead(), kernelAllocation);
}

////////////////////////////////////////////////////////////////////////////////
// Program::Build (source)
////////////////////////////////////////////////////////////////////////////////
TEST_P(ProgramFromSourceTest, CreateWithSource_Build) {
    class MyProgram2 : public Program {
      public:
        MyProgram2(){};

      protected:
        CompilerInterface *getCompilerInterface() const override { return nullptr; }
    };
    class MyProgram3 : public Program {
      public:
        MyProgram3(){};
        cl_int processGenBinary() override { return CL_INVALID_BINARY; }
        void setDevice(Device *device) { pDevice = device; }
        void SetSourceCode(const char *ptr) { sourceCode = ptr; }
    };
    KernelBinaryHelper kbHelper(BinaryFileName, true);

    cl_device_id deviceList = {0};
    char data[4] = {0};

    cl_device_id usedDevice = pPlatform->getDevice(0);

    CreateProgramWithSource<MockProgram>(
        pContext,
        &usedDevice,
        SourceFileName);

    // Order of following microtests is important - do not change.
    // Add new microtests at end.

    auto pMockProgram = (MockProgram *)pProgram;

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
    MyProgram2 *p2 = new MyProgram2();
    retVal = p2->build(0, nullptr, nullptr, nullptr, nullptr, false);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
    delete p2;

    // fail build - any build error (here caused by specifying unrecognized option)
    retVal = pProgram->build(0, nullptr, "-invalid-option", nullptr, nullptr, false);
    EXPECT_EQ(CL_BUILD_PROGRAM_FAILURE, retVal);

    // fail build - linked code is corrupted and cannot be postprocessed
    MyProgram3 *p3 = new MyProgram3();
    Device *device = pPlatform->getDevice(0);
    p3->setDevice(device);
    std::string testFile;
    void *pSourceBuffer;
    size_t sourceSize;
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd8.cl"); // source file
    sourceSize = loadDataFromFile(testFile.c_str(), pSourceBuffer);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSourceBuffer);
    p3->SetSourceCode((const char *)pSourceBuffer);
    retVal = p3->build(0, nullptr, nullptr, nullptr, nullptr, false);
    EXPECT_EQ(CL_INVALID_BINARY, retVal);
    delete p3;

    // build successfully without notifyFunc - build kernel and write it to Kernel Cache
    pMockProgram->ClearOptions();
    //    retVal = p->ClearKernelCache();
    //    EXPECT_EQ(0, retVal);
    retVal = pProgram->build(0, nullptr, nullptr, nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_THAT(pProgram->getInternalOptions(), ::testing::HasSubstr(std::string("-cl-ext=-all,+cl")));

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
    pMockProgram->SetSourceCode(""); // set source code as non-existent (invalid)
    pMockProgram->SetBuildStatus(CL_BUILD_NONE);
    pMockProgram->SetCreatedFromBinary(false);
    retVal = pProgram->build(0, nullptr, nullptr, nullptr, nullptr, false);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
    delete[](char *) pSourceBuffer;
}

////////////////////////////////////////////////////////////////////////////////
// Program::Build (duplicate)
////////////////////////////////////////////////////////////////////////////////
TEST_P(ProgramFromSourceTest, CreateWithSource_Build_Options_Duplicate) {
    KernelBinaryHelper kbHelper(BinaryFileName, false);

    retVal = pProgram->build(0, nullptr, nullptr, nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pProgram->build(0, nullptr, "-cl-fast-relaxed-math", nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pProgram->build(0, nullptr, "-cl-fast-relaxed-math", nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pProgram->build(0, nullptr, "-cl-finite-math-only", nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pProgram->build(0, nullptr, nullptr, nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

////////////////////////////////////////////////////////////////////////////////
// Program::Build (use cache)
////////////////////////////////////////////////////////////////////////////////
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
    CreateProgramWithSource<MockProgram>(
        pContext,
        &usedDevice,
        SourceFileName);

    auto *p = (MockProgram *)pProgram;

    Callback callback;

    retVal = pProgram->build(0, nullptr, nullptr, nullptr, nullptr, true);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto hash1 = p->getCachedFileName();
    auto kernel1 = pProgram->getKernelInfo("CopyBuffer");
    Callback::watch(kernel1);
    EXPECT_NE(nullptr, kernel1);

    retVal = pProgram->build(0, nullptr, "-cl-fast-relaxed-math", nullptr, nullptr, true);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto hash2 = p->getCachedFileName();
    auto kernel2 = pProgram->getKernelInfo("CopyBuffer");
    EXPECT_NE(nullptr, kernel2);
    EXPECT_NE(hash1, hash2);
    Callback::unwatch(kernel1);
    Callback::watch(kernel2);

    retVal = pProgram->build(0, nullptr, "-cl-finite-math-only", nullptr, nullptr, true);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto hash3 = p->getCachedFileName();
    auto kernel3 = pProgram->getKernelInfo("CopyBuffer");
    EXPECT_NE(nullptr, kernel3);
    EXPECT_NE(hash1, hash3);
    EXPECT_NE(hash2, hash3);
    Callback::unwatch(kernel2);
    Callback::watch(kernel3);

    retVal = pProgram->build(0, nullptr, nullptr, nullptr, nullptr, true);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto hash4 = p->getCachedFileName();
    auto kernel4 = pProgram->getKernelInfo("CopyBuffer");
    EXPECT_NE(nullptr, kernel4);
    EXPECT_EQ(hash1, hash4);
    Callback::unwatch(kernel3);
}

////////////////////////////////////////////////////////////////////////////////
// Program::Compile (source)
////////////////////////////////////////////////////////////////////////////////
TEST_P(ProgramFromSourceTest, CreateWithNoStrings) {
    auto p = Program::create(pContext, 0, nullptr, nullptr, retVal);
    EXPECT_NE(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, p);
    delete p;
}

TEST_P(ProgramFromSourceTest, CreateWithSource_Compile) {
    class MyProgram2 : public Program {
      public:
        MyProgram2(){};

      protected:
        CompilerInterface *getCompilerInterface() const override { return nullptr; }
    };

    cl_device_id usedDevice = pPlatform->getDevice(0);
    CreateProgramWithSource<MockProgram>(
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
    void *pSourceBuffer;
    size_t sourceSize;
    Program *p3; // header Program object
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd8.cl"); // header source file
    sourceSize = loadDataFromFile(testFile.c_str(), pSourceBuffer);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSourceBuffer);
    p3 = Program::create<MockProgram>(pContext, 1, (const char **)(&pSourceBuffer), &sourceSize, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, p3);
    inputHeaders = p3;
    retVal = pProgram->compile(0, nullptr, nullptr, 1, &inputHeaders, &headerIncludeNames, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // fail compilation of kernel with header - header is invalid
    p = (MockProgram *)p3;
    p->SetSourceCode(""); // set header source code as non-existent (invalid)
    retVal = pProgram->compile(0, nullptr, nullptr, 1, &inputHeaders, &headerIncludeNames, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
    delete p3;

    // fail compilation - CompilerInterface cannot be obtained
    MyProgram2 *p2 = new MyProgram2();
    retVal = p2->compile(0, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
    delete p2;

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
    delete[](char *) pSourceBuffer;
}

TEST_P(ProgramFromSourceTest, CompileProgramWithReraFlag) {
    class MyCompilerInterface2 : public CompilerInterface {
      public:
        MyCompilerInterface2() { buildOptions[0] = buildInternalOptions[0] = '\0'; };
        ~MyCompilerInterface2() override{};

        cl_int compile(Program &program, const TranslationArgs &inputArgs) override {
            if ((inputArgs.OptionsSize > 0) && (inputArgs.pOptions != nullptr)) {
                buildOptions.assign(inputArgs.pOptions, inputArgs.pOptions + inputArgs.OptionsSize);
            }
            if ((inputArgs.OptionsSize > 0) && (inputArgs.pOptions != nullptr)) {
                buildInternalOptions.assign(inputArgs.pInternalOptions, inputArgs.pInternalOptions + inputArgs.InternalOptionsSize);
            }
            return CL_SUCCESS;
        }
        void getBuildOptions(std::string &s) { s = buildOptions; }
        void getBuildInternalOptions(std::string &s) { s = buildInternalOptions; }

      protected:
        std::string buildOptions;
        std::string buildInternalOptions;
    };

    class MyProgram2 : public Program {
      public:
        MyProgram2() { cip = nullptr; };

        void setDevice(Device *device) { pDevice = device; }
        cl_int processGenBinary() override { return CL_SUCCESS; }
        void releaseCompilerInterface() {
            delete cip;
            cip = nullptr;
        }
        MyCompilerInterface2 *getCompilerInterfacePub() {
            getCompilerInterface();
            return cip;
        }

      protected:
        CompilerInterface *getCompilerInterface() const override {
            if (cip == nullptr) {
                cip = new MyCompilerInterface2;
            }
            return cip;
        }
        mutable MyCompilerInterface2 *cip;
    };

    MyProgram2 *pProgram = new MyProgram2();
    EXPECT_NE(nullptr, pProgram);
    cl_device_id deviceId = pContext->getDevice(0);
    Device *pDevice = castToObject<Device>(deviceId);
    pProgram->setDevice(pDevice);
    MyCompilerInterface2 *cip = pProgram->getCompilerInterfacePub();
    EXPECT_NE(nullptr, cip);
    pProgram->setSource((char *)"__kernel mock() {}");

    // Check default build options
    std::string s1;
    std::string s2;
    cip->getBuildOptions(s1);
    size_t pos = s1.find("-cl-fast-relaxed-math");
    EXPECT_EQ(pos, std::string::npos);
    cip->getBuildInternalOptions(s2);
    pos = s2.find("-cl-intel-gtpin-rera");
    EXPECT_EQ(pos, std::string::npos);

    // Ask to build created program without "-cl-intel-gtpin-rera" flag.
    s1.assign("");
    s2.assign("");
    cl_int retVal = pProgram->compile(0, nullptr, "-cl-fast-relaxed-math", 0, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Check build options that were applied
    cip->getBuildOptions(s1);
    pos = s1.find("-cl-fast-relaxed-math");
    EXPECT_NE(pos, std::string::npos);
    cip->getBuildInternalOptions(s2);
    pos = s2.find("-cl-intel-gtpin-rera");
    EXPECT_EQ(pos, std::string::npos);

    // Ask to build created program with "-cl-intel-gtpin-rera" flag.
    s1.assign("");
    s2.assign("");
    retVal = pProgram->compile(0, nullptr, "-cl-intel-gtpin-rera -cl-finite-math-only", 0, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Check build options that were applied
    cip->getBuildOptions(s1);
    pos = s1.find("-cl-fast-relaxed-math");
    EXPECT_EQ(pos, std::string::npos);
    pos = s1.find("-cl-finite-math-only");
    EXPECT_NE(pos, std::string::npos);
    cip->getBuildInternalOptions(s2);
    pos = s2.find("-cl-intel-gtpin-rera");
    EXPECT_NE(pos, std::string::npos);

    // Cleanup
    pProgram->releaseCompilerInterface();
    delete pProgram;
}

TEST_P(ProgramFromSourceTest, CreateWithSourceAdvanced) {
    std::string testFile;
    void *pSourceBuffer;
    size_t sourceSize = 0;

    Program *p;
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd8.cl");
    loadDataFromFile(testFile.c_str(), pSourceBuffer);
    EXPECT_NE(nullptr, pSourceBuffer);

    /*
     According to spec: If lengths is NULL, all strings in the strings argument are considered null-terminated.
    */
    p = Program::create(pContext, 1, (const char **)(&pSourceBuffer), nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, p);
    delete p;

    /*
     According to spec: If an element in lengths is zero, its accompanying string is null-terminated.
    */
    p = Program::create(pContext, 1, (const char **)(&pSourceBuffer), &sourceSize, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, p);
    delete p;

    std::stringstream dataStream(static_cast<char *>(pSourceBuffer));
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

    deleteDataReadFromFile(pSourceBuffer);
}

////////////////////////////////////////////////////////////////////////////////
// Program::Link (compiled source)
////////////////////////////////////////////////////////////////////////////////
TEST_P(ProgramFromSourceTest, CreateWithSource_Link) {
    class MyProgram2 : public Program {
      public:
        MyProgram2(){};
        cl_int processGenBinary() override { return CL_INVALID_BINARY; }
        void setDevice(Device *device) { pDevice = device; }
    };

    cl_device_id usedDevice = pPlatform->getDevice(0);
    CreateProgramWithSource<MockProgram>(
        pContext,
        &usedDevice,
        SourceFileName);

    auto *p = (MockProgram *)pProgram;

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
    p->SetBuildStatus(CL_BUILD_IN_PROGRESS);
    retVal = pProgram->link(0, nullptr, nullptr, 1, &program, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    p->SetBuildStatus(CL_BUILD_NONE);

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
    bool isSpirvTmp = p->getIsSpirV();
    char *pIrBin = p->GetIrBinary();
    size_t irBinSize = p->GetIrBinarySize();
    p->SetIrBinary(nullptr, false);
    retVal = pProgram->link(0, nullptr, nullptr, 1, &program, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
    p->SetIrBinary(pIrBin, isSpirvTmp);

    // fail linking - size of code to be linked is == 0
    p->SetIrBinarySize(0, isSpirvTmp);
    retVal = pProgram->link(0, nullptr, nullptr, 1, &program, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
    p->SetIrBinarySize(irBinSize, isSpirvTmp);

    // fail linking - any link error (here caused by specifying unrecognized option)
    retVal = pProgram->link(0, nullptr, "-invalid-option", 1, &program, nullptr, nullptr);
    EXPECT_EQ(CL_BUILD_PROGRAM_FAILURE, retVal);

    // fail linking - linked code is corrupted and cannot be postprocessed
    MyProgram2 *p2 = new MyProgram2();
    Device *device = pPlatform->getDevice(0);
    p2->setDevice(device);
    retVal = p2->link(0, nullptr, nullptr, 1, &program, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_BINARY, retVal);
    delete p2;

    // link successfully without notifyFunc
    retVal = pProgram->link(0, nullptr, nullptr, 1, &program, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // link successfully with notifyFunc
    data[0] = 0;
    retVal = pProgram->link(0, nullptr, "", 1, &program, notifyFunc, &data[0]);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ('a', data[0]);
}

////////////////////////////////////////////////////////////////////////////////
// Program::Link (create library)
////////////////////////////////////////////////////////////////////////////////
TEST_P(ProgramFromSourceTest, CreateWithSource_CreateLibrary) {
    class MyProgram : public Program {
      public:
        MyProgram(){};

      protected:
        CompilerInterface *getCompilerInterface() const override { return nullptr; }
    };
    MyProgram *p = new MyProgram();
    cl_program program = pProgram;

    // Order of following microtests is important - do not change.
    // Add new microtests at end.

    // compile successfully a kernel to be later used to create library
    retVal = pProgram->compile(0, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // create library successfully
    retVal = pProgram->link(0, nullptr, "-create-library", 1, &program, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // fail library creation - any link error (here caused by specifying unrecognized option)
    retVal = pProgram->link(0, nullptr, "-create-library -invalid-option", 1, &program, nullptr, nullptr);
    EXPECT_EQ(CL_BUILD_PROGRAM_FAILURE, retVal);

    // fail library creation - CompilerInterface cannot be obtaine
    retVal = p->link(0, nullptr, "-create-library", 1, &program, nullptr, nullptr);
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);

    delete p;
}

////////////////////////////////////////////////////////////////////////////////
// Program::  (PatchToken)
////////////////////////////////////////////////////////////////////////////////
class PatchTokenFromBinaryTest : public ProgramSimpleFixture {
  public:
    void SetUp() override {
        ProgramSimpleFixture::SetUp();
    }
    void TearDown() override {
        ProgramSimpleFixture::TearDown();
    }
};
typedef Test<PatchTokenFromBinaryTest> PatchTokenTests;
////////////////////////////////////////////////////////////

template <typename FamilyType>
class CommandStreamReceiverMock : public UltCommandStreamReceiver<FamilyType> {
    typedef UltCommandStreamReceiver<FamilyType> BaseClass;

  public:
    CommandStreamReceiverMock() : BaseClass(*platformDevices[0]) {
    }

    void makeResident(GraphicsAllocation &graphicsAllocation) override {
        residency[graphicsAllocation.getUnderlyingBuffer()] = graphicsAllocation.getUnderlyingBufferSize();
        CommandStreamReceiver::makeResident(graphicsAllocation);
    }

    void makeNonResident(GraphicsAllocation &graphicsAllocation) override {
        residency.erase(graphicsAllocation.getUnderlyingBuffer());
        CommandStreamReceiver::makeNonResident(graphicsAllocation);
    }
    ResidencyContainer &getResidencyContainer() {
        return this->memoryManager->getResidencyAllocations();
    }

    std::map<const void *, size_t> residency;
};

HWTEST_F(PatchTokenTests, givenKernelRequiringConstantAllocationWhenMakeResidentIsCalledThenConstantAllocationIsMadeResident) {
    cl_device_id device = pDevice;

    CreateProgramFromBinary<Program>(pContext, &device, "test_constant_memory");

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

    auto pCommandStreamReceiver = new CommandStreamReceiverMock<FamilyType>();
    ASSERT_NE(nullptr, pCommandStreamReceiver);

    pDevice->resetCommandStreamReceiver(pCommandStreamReceiver);
    pCommandStreamReceiver->residency.clear();

    pKernel->makeResident(*pCommandStreamReceiver);
    EXPECT_EQ(2u, pCommandStreamReceiver->residency.size());

    auto &residencyVector = pCommandStreamReceiver->getResidencyContainer();

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

    pCommandStreamReceiver->makeSurfacePackNonResident(nullptr);
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

    CreateProgramFromBinary<Program>(pContext, &device, "kernel_data_param");

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

    CreateProgramFromBinary<Program>(pContext, &device, "kernel_data_param");

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

    CreateProgramFromBinary<Program>(pContext, &device, "test_basic_constant");

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
    // PATCH_TOKEN_INLINE_VME_SAMPLER_INFO token indicates a VME kernel.
    cl_device_id device = pDevice;

    CreateProgramFromBinary<Program>(pContext, &device, "vme_kernels");

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
    EXPECT_EQ(0u, pProgram->getProgramScopePatchListSize());
    CompilerInterface::shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// Program::  Simple tests against some custom sceanarios
////////////////////////////////////////////////////////////////////////////////

TEST(ProgramFromBinaryTests, givenBinaryWithInvalidICBEThenErrorIsReturned) {
    cl_int retVal = CL_INVALID_BINARY;

    SProgramBinaryHeader binHeader;
    memset(&binHeader, 0, sizeof(binHeader));
    binHeader.Magic = iOpenCL::MAGIC_CL;
    binHeader.Version = iOpenCL::CURRENT_ICBE_VERSION - 3;
    binHeader.Device = platformDevices[0]->pPlatform->eRenderCoreFamily;
    binHeader.GPUPointerSizeInBytes = 8;
    binHeader.NumberOfKernels = 0;
    binHeader.SteppingId = 0;
    binHeader.PatchListSize = 0;
    size_t binSize = sizeof(SProgramBinaryHeader);

    {
        const unsigned char *binaries[1] = {reinterpret_cast<const unsigned char *>(&binHeader)};
        const cl_device_id deviceId = 0;
        std::unique_ptr<Program> pProgram(Program::create<Program>(nullptr, 0, &deviceId, &binSize, binaries, nullptr, retVal));
        EXPECT_EQ(nullptr, pProgram.get());
        EXPECT_EQ(CL_INVALID_BINARY, retVal);
    }

    {
        // whatever method we choose CL_INVALID_BINARY is always returned
        std::unique_ptr<Program> pProgram(Program::createFromGenBinary(nullptr, &binHeader, binSize, false, &retVal));
        ASSERT_NE(nullptr, pProgram.get());
        EXPECT_EQ(CL_SUCCESS, retVal);

        retVal = pProgram->processGenBinary();
        EXPECT_EQ(CL_INVALID_BINARY, retVal);
    }
}

class FailProgram : public Program {
  public:
    FailProgram(Context *context, bool isBuiltIn = false) : Program(context, isBuiltIn) {}
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
    binHeader.Device = platformDevices[0]->pPlatform->eRenderCoreFamily;
    binHeader.GPUPointerSizeInBytes = 8;
    binHeader.NumberOfKernels = 0;
    binHeader.SteppingId = 0;
    binHeader.PatchListSize = 0;
    size_t binSize = sizeof(SProgramBinaryHeader);

    std::unique_ptr<FailProgram> pProgram(FailProgram::createFromGenBinary<FailProgram>(nullptr, &binHeader, binSize, false, &retVal));
    ASSERT_NE(nullptr, pProgram.get());
    EXPECT_EQ(CL_SUCCESS, retVal);

    binHeader.Version = iOpenCL::CURRENT_ICBE_VERSION - 1;
    retVal = pProgram->createProgramFromBinary(&binHeader, binSize);
    EXPECT_EQ(CL_INVALID_BINARY, retVal);
}

TEST(ProgramFromBinaryTests, givenEmptyProgramThenErrorIsReturned) {
    class TestedProgram : public Program {
      public:
        TestedProgram(Context *context, bool isBuiltIn) : Program(context, isBuiltIn) {}
        char *setGenBinary(char *binary) {
            auto res = genBinary;
            genBinary = binary;
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
    binHeader.Device = platformDevices[0]->pPlatform->eRenderCoreFamily;
    binHeader.GPUPointerSizeInBytes = 8;
    binHeader.NumberOfKernels = 0;
    binHeader.SteppingId = 0;
    binHeader.PatchListSize = 0;
    size_t binSize = sizeof(SProgramBinaryHeader);

    std::unique_ptr<TestedProgram> pProgram(TestedProgram::createFromGenBinary<TestedProgram>(nullptr, &binHeader, binSize, false, &retVal));
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
    cl_int retVal = CL_DEVICE_NOT_FOUND;
    auto defaultSetting = DebugManager.flags.DisableStatelessToStatefulOptimization.get();

    DebugManager.flags.DisableStatelessToStatefulOptimization.set(false);
    if (pDevice) {
        MockProgram program(pContext, false);
        char paramValue[32];
        pDevice->getDeviceInfo(CL_DEVICE_VERSION, 32, paramValue, 0);
        if (strstr(paramValue, "2.1")) {
            EXPECT_THAT(program.getInternalOptions(), testing::HasSubstr(std::string("-ocl-version=210")));
        } else if (strstr(paramValue, "2.0")) {
            EXPECT_THAT(program.getInternalOptions(), testing::HasSubstr(std::string("-ocl-version=200")));
        } else if (strstr(paramValue, "1.2")) {
            EXPECT_THAT(program.getInternalOptions(), testing::HasSubstr(std::string("-ocl-version=120")));
        } else {
            EXPECT_THAT(program.getInternalOptions(), testing::HasSubstr(std::string("-ocl-version=000")));
        }
    } else {
        EXPECT_NE(CL_DEVICE_NOT_FOUND, retVal);
    }
    DebugManager.flags.DisableStatelessToStatefulOptimization.set(defaultSetting);
}

TEST_F(ProgramTests, ProgramCtorSetsProperInternalOptionsForced20) {
    auto defaultVersion = pDevice->getMutableDeviceInfo()->clVersion;

    pDevice->getMutableDeviceInfo()->clVersion = "OpenCL 2.0 ";
    if (pDevice) {
        MockProgram program(pContext, false);
        char paramValue[32];
        pDevice->getDeviceInfo(CL_DEVICE_VERSION, 32, paramValue, 0);
        ASSERT_EQ(std::string(paramValue), "OpenCL 2.0 ");
        EXPECT_THAT(program.getInternalOptions(), testing::HasSubstr(std::string("-ocl-version=200")));
    }
    pDevice->getMutableDeviceInfo()->clVersion = defaultVersion;
}

TEST_F(ProgramTests, ProgramCtorSetsProperInternalOptionsWhenStatelessToStatefulIsDisabled) {
    cl_int retVal = CL_DEVICE_NOT_FOUND;
    auto defaultSetting = DebugManager.flags.DisableStatelessToStatefulOptimization.get();

    DebugManager.flags.DisableStatelessToStatefulOptimization.set(true);
    if (pDevice) {
        MockProgram program(pContext, false);
        char paramValue[32];
        pDevice->getDeviceInfo(CL_DEVICE_VERSION, 32, paramValue, 0);
        if (strstr(paramValue, "2.1")) {
            EXPECT_THAT(program.getInternalOptions(), testing::HasSubstr(std::string("-ocl-version=210")));
            EXPECT_THAT(program.getInternalOptions(), testing::HasSubstr(std::string("-cl-intel-greater-than-4GB-buffer-required")));
        } else if (strstr(paramValue, "2.0")) {
            EXPECT_THAT(program.getInternalOptions(), testing::HasSubstr(std::string("-ocl-version=200")));
            EXPECT_THAT(program.getInternalOptions(), testing::HasSubstr(std::string("-cl-intel-greater-than-4GB-buffer-required")));
        } else if (strstr(paramValue, "1.2")) {
            EXPECT_THAT(program.getInternalOptions(), testing::HasSubstr(std::string("-ocl-version=120")));
            EXPECT_THAT(program.getInternalOptions(), testing::HasSubstr(std::string("-cl-intel-greater-than-4GB-buffer-required")));
        } else {
            EXPECT_THAT(program.getInternalOptions(), testing::HasSubstr(std::string("-ocl-version=000")));
            EXPECT_THAT(program.getInternalOptions(), testing::HasSubstr(std::string("-cl-intel-greater-than-4GB-buffer-required")));
        }
    } else {
        EXPECT_NE(CL_DEVICE_NOT_FOUND, retVal);
    }
    DebugManager.flags.DisableStatelessToStatefulOptimization.set(defaultSetting);
}

TEST_F(ProgramTests, ProgramCtorSetsProperInternalOptionsWhenForcing32BitAddressess) {
    cl_int retVal = CL_DEVICE_NOT_FOUND;
    auto defaultSetting = DebugManager.flags.DisableStatelessToStatefulOptimization.get();

    DebugManager.flags.DisableStatelessToStatefulOptimization.set(false);
    if (pDevice) {
        const_cast<DeviceInfo *>(&pDevice->getDeviceInfo())->force32BitAddressess = true;
        MockProgram program(pContext, false);
        char paramValue[32];
        pDevice->getDeviceInfo(CL_DEVICE_VERSION, 32, paramValue, 0);
        if (strstr(paramValue, "2.1")) {
            EXPECT_THAT(program.getInternalOptions(), testing::HasSubstr(std::string("-ocl-version=210")));
        } else if (strstr(paramValue, "2.0")) {
            EXPECT_THAT(program.getInternalOptions(), testing::HasSubstr(std::string("-ocl-version=200")));
        } else if (strstr(paramValue, "1.2")) {
            EXPECT_THAT(program.getInternalOptions(), testing::HasSubstr(std::string("-ocl-version=120")));
        } else {
            EXPECT_THAT(program.getInternalOptions(), testing::HasSubstr(std::string("-ocl-version=000")));
        }
        EXPECT_THAT(program.getInternalOptions(), testing::Not(testing::HasSubstr(std::string("-cl-intel-greater-than-4GB-buffer-required"))));
    } else {
        EXPECT_NE(CL_DEVICE_NOT_FOUND, retVal);
    }
    DebugManager.flags.DisableStatelessToStatefulOptimization.set(defaultSetting);
}

TEST_F(ProgramTests, BuiltinProgramCreateSetsProperInternalOptions) {
    cl_int retVal = CL_DEVICE_NOT_FOUND;
    auto defaultSetting = DebugManager.flags.DisableStatelessToStatefulOptimization.get();

    DebugManager.flags.DisableStatelessToStatefulOptimization.set(false);
    if (pDevice) {
        MockProgram *pProgram = Program::create<MockProgram>("", pContext, *pDevice, true, nullptr);
        EXPECT_THAT(pProgram->getInternalOptions(), testing::HasSubstr(std::string("")));
        delete pProgram;

    } else {
        EXPECT_NE(CL_DEVICE_NOT_FOUND, retVal);
    }
    DebugManager.flags.DisableStatelessToStatefulOptimization.set(defaultSetting);
}

TEST_F(ProgramTests, BuiltinProgramCreateSetsProperInternalOptionsWhenStatelessToStatefulIsDisabled) {
    cl_int retVal = CL_DEVICE_NOT_FOUND;
    auto defaultSetting = DebugManager.flags.DisableStatelessToStatefulOptimization.get();

    DebugManager.flags.DisableStatelessToStatefulOptimization.set(true);
    if (pDevice) {
        MockProgram *pProgram = Program::create<MockProgram>("", pContext, *pDevice, true, nullptr);
        EXPECT_THAT(pProgram->getInternalOptions(), testing::HasSubstr(std::string("-cl-intel-greater-than-4GB-buffer-required")));
        delete pProgram;

    } else {
        EXPECT_NE(CL_DEVICE_NOT_FOUND, retVal);
    }
    DebugManager.flags.DisableStatelessToStatefulOptimization.set(defaultSetting);
}

TEST_F(ProgramTests, givenProgramWhenItIsCompiledThenItAlwyasHavePreserveVec3TypeInternalOptionSet) {
    std::unique_ptr<MockProgram> pProgram(Program::create<MockProgram>("", pContext, *pDevice, true, nullptr));
    EXPECT_THAT(pProgram->getInternalOptions(), testing::HasSubstr(std::string("-fpreserve-vec3-type ")));
}

TEST_F(ProgramTests, BuiltinProgramCreateSetsProperInternalOptionsWhenForcing32BitAddressess) {
    cl_int retVal = CL_DEVICE_NOT_FOUND;
    auto defaultSetting = DebugManager.flags.DisableStatelessToStatefulOptimization.get();

    DebugManager.flags.DisableStatelessToStatefulOptimization.set(false);
    if (pDevice) {
        const_cast<DeviceInfo *>(&pDevice->getDeviceInfo())->force32BitAddressess = true;
        MockProgram *pProgram = Program::create<MockProgram>("", pContext, *pDevice, true, nullptr);
        if (is32bit) {
            EXPECT_THAT(pProgram->getInternalOptions(), testing::HasSubstr(std::string("-cl-intel-greater-than-4GB-buffer-required")));
        } else {
            EXPECT_THAT(pProgram->getInternalOptions(), testing::HasSubstr(std::string("")));
        }
        delete pProgram;

    } else {
        EXPECT_NE(CL_DEVICE_NOT_FOUND, retVal);
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
    std::unique_ptr<MockProgram> pProgram(Program::create<MockProgram>(pContext, 1u, programSources, &length, errorCode));

    EXPECT_THAT(pProgram->getInternalOptions(), testing::HasSubstr(std::string("-cl-intel-has-buffer-offset-arg ")));
}

TEST_F(ProgramTests, givenStatelessToStatefullOptimizationOffWHenProgramIsCreatedThenOptimizationStringIsNotPresent) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.set(0);
    cl_int errorCode = CL_SUCCESS;
    const char programSource[] = "program";
    const char *programPointer = programSource;
    const char **programSources = reinterpret_cast<const char **>(&programPointer);
    size_t length = sizeof(programSource);
    std::unique_ptr<MockProgram> pProgram(Program::create<MockProgram>(pContext, 1u, programSources, &length, errorCode));
    EXPECT_THAT(pProgram->getInternalOptions(), Not(testing::HasSubstr(std::string("-cl-intel-has-buffer-offset-arg "))));
}

TEST_F(ProgramTests, ProgramCtorSetsProperProgramScopePatchListSize) {

    MockProgram program(pContext, false);
    EXPECT_EQ((size_t)0, program.getProgramScopePatchListSize());
}

TEST_F(ProgramTests, GivenContextWhenCreateProgramThenIncrementContextRefCount) {
    auto initialApiRefCount = pContext->getReference();
    auto initialInternalRefCount = pContext->getRefInternalCount();
    MockProgram tempProgram;

    MockProgram *program = new MockProgram(pContext, false);

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
    Program *pProgram = Program::createFromGenBinary(pContext, nullptr, 0, false, &retVal);
    EXPECT_EQ(nullptr, pProgram);
    EXPECT_NE(CL_SUCCESS, retVal);
}

TEST_F(ProgramTests, ProgramFromGenBinary) {
    cl_int retVal = CL_INVALID_BINARY;
    char binary[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, '\0'};
    size_t size = 10;

    Program *pProgram = Program::createFromGenBinary(pContext, binary, size, false, &retVal);
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

    Program *pProgram = Program::createFromGenBinary(pContext, binary, size, true, &retVal);
    EXPECT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(pProgram->getIsBuiltIn());

    delete pProgram;
}

TEST_F(ProgramTests, ProgramFromGenBinaryWithoutRetVal) {
    char binary[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, '\0'};
    size_t size = 10;

    Program *pProgram = Program::createFromGenBinary(pContext, binary, size, false, nullptr);
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

    Program *pProgram = Program::createFromGenBinary(nullptr, binary, size, false, &retVal);
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

    Program *pProgram = Program::createFromGenBinary(nullptr, &genBin[0], binSize, false, &retVal);
    EXPECT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ((uint32_t)CL_PROGRAM_BINARY_TYPE_EXECUTABLE, (uint32_t)pProgram->getProgramBinaryType());

    cl_device_id deviceId = pContext->getDevice(0);
    Device *pDevice = castToObject<Device>(deviceId);
    char *pBin = &genBin[0];
    retVal = CL_INVALID_BINARY;
    binSize = 0;

    if (pDevice != nullptr) {
        // Prepare simple program binary containing patch token PATCH_TOKEN_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT
        SProgramBinaryHeader *pBHdr = (SProgramBinaryHeader *)pBin;
        pBHdr->Magic = iOpenCL::MAGIC_CL;
        pBHdr->Version = iOpenCL::CURRENT_ICBE_VERSION;
        pBHdr->Device = pDevice->getHardwareInfo().pPlatform->eRenderCoreFamily;
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
        pKHdr->PatchListSize = 24;
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
        binSize += sizeof(SPatchGlobalMemoryObjectKernelArgument);

        // Decode prepared program binary
        pProgram->storeGenBinary(&genBin[0], binSize);
        retVal = pProgram->processGenBinary();
    }
    ASSERT_EQ(CL_SUCCESS, retVal);

    delete pProgram;
}

TEST_F(ProgramTests, ProgramFromGenBinaryWithPATCH_TOKEN_GTPIN_FREE_GRF_INFO) {
#define GRF_INFO_SIZE 44u
    cl_int retVal = CL_INVALID_BINARY;
    char genBin[1024] = {1, 2, 3, 4, 5, 6, 7, 8, 9, '\0'};
    size_t binSize = 10;

    Program *pProgram = Program::createFromGenBinary(nullptr, &genBin[0], binSize, false, &retVal);
    EXPECT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ((uint32_t)CL_PROGRAM_BINARY_TYPE_EXECUTABLE, (uint32_t)pProgram->getProgramBinaryType());

    cl_device_id deviceId = pContext->getDevice(0);
    Device *pDevice = castToObject<Device>(deviceId);
    char *pBin = &genBin[0];
    retVal = CL_INVALID_BINARY;
    binSize = 0;

    if (pDevice != nullptr) {
        // Prepare simple program binary containing patch token PATCH_TOKEN_GTPIN_FREE_GRF_INFO
        SProgramBinaryHeader *pBHdr = (SProgramBinaryHeader *)pBin;
        pBHdr->Magic = iOpenCL::MAGIC_CL;
        pBHdr->Version = iOpenCL::CURRENT_ICBE_VERSION;
        pBHdr->Device = pDevice->getHardwareInfo().pPlatform->eRenderCoreFamily;
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
        pKHdr->PatchListSize = 24;
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
        pPatch->Size = sizeof(iOpenCL::SPatchGtpinFreeGRFInfo) + GRF_INFO_SIZE;
        pPatch->BufferSize = GRF_INFO_SIZE;
        binSize += pPatch->Size;

        // Decode prepared program binary
        pProgram->storeGenBinary(&genBin[0], binSize);
        retVal = pProgram->processGenBinary();
    }
    ASSERT_EQ(CL_SUCCESS, retVal);

    delete pProgram;
#undef GRF_INFO_SIZE
}

TEST_F(ProgramTests, GetGenBinaryReturnsBinaryStoreInProgram) {
    char genBin[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    MockProgram mp;
    mp.storeGenBinary(genBin, sizeof(genBin));

    size_t binarySize = 0;
    const char *binary = mp.getGenBinary(binarySize);
    ASSERT_EQ(sizeof(genBin), binarySize);
    EXPECT_EQ(0, memcmp(genBin, binary, sizeof(genBin)));
}

TEST_F(ProgramTests, ValidBinaryWithIGCVersionEqual0) {
    class MyProgram3 : public Program {
      public:
        MyProgram3(){};
        cl_int createProgramFromBinaryPub(const void *pBinary, size_t binarySize) { return createProgramFromBinary(pBinary, binarySize); }
        void setDevice(Device *device) { pDevice = device; }
        using Program::rebuildProgramFromIr;
        char *getIrBinary() { return irBinary; };
    };

    cl_int retVal;
    CompilerInterface::getInstance();

    MyProgram3 *pProgram = new MyProgram3();
    EXPECT_NE(nullptr, pProgram);
    cl_device_id deviceId = pContext->getDevice(0);
    Device *pDevice = castToObject<Device>(deviceId);
    pProgram->setDevice(pDevice);

    // Load a binary program file
    void *pBinary = nullptr;
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "CopyBuffer_simd8_", ".bin");
    size_t binarySize = loadDataFromFile(filePath.c_str(), pBinary);
    EXPECT_NE(0u, binarySize);

    // Find its OpenCL program data and mark that the data were created with unknown compiler version,
    // which means that the program has to be rebuild from its IR binary
    CLElfLib::CElfReader *pElfReader = nullptr;
    pElfReader = CLElfLib::CElfReader::create((const char *)pBinary, binarySize);
    EXPECT_NE(nullptr, pElfReader);
    EXPECT_TRUE(CLElfLib::CElfReader::isValidElf64(pBinary, binarySize));
    const CLElfLib::SElf64Header *pElfHeader = pElfReader->getElfHeader();
    char *pSectionData = nullptr;
    size_t sectionDataSize = 0;
    SProgramBinaryHeader *pBHdr = nullptr;
    EXPECT_NE(nullptr, pElfHeader);
    EXPECT_EQ(pElfHeader->Type, CLElfLib::E_EH_TYPE::EH_TYPE_OPENCL_EXECUTABLE);

    for (uint32_t i = 1; i < pElfHeader->NumSectionHeaderEntries; i++) {
        const CLElfLib::SElf64SectionHeader *pSectionHeader = pElfReader->getSectionHeader(i);
        if (pSectionHeader->Type != CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_DEV_BINARY) {
            continue;
        }
        pElfReader->getSectionData(i, pSectionData, sectionDataSize);
        EXPECT_NE(nullptr, pSectionData);
        EXPECT_NE(0u, sectionDataSize);
        pBHdr = (SProgramBinaryHeader *)pSectionData;
        pBHdr->Version = 0; // Simulate compiler Version = 0
        break;
    }
    EXPECT_NE(nullptr, pBHdr);

    // Create program from modified binary, is should be successfully rebuilt
    retVal = pProgram->createProgramFromBinaryPub(pBinary, binarySize);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Get IR binary and modify its header magic,
    // then ask to rebuild program from its IR binary - it should fail
    char *pIrBinary = pProgram->getIrBinary();
    (*pIrBinary)--;
    retVal = pProgram->rebuildProgramFromIr();
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);

    // Cleanup
    CLElfLib::CElfReader::destroy(pElfReader);
    deleteDataReadFromFile(pBinary);
    delete pProgram;
    CompilerInterface::shutdown();
}

TEST_F(ProgramTests, RebuildBinaryButNoCompilerInterface) {
    class MyProgram2 : public Program {
      public:
        MyProgram2(){};

        cl_int createProgramFromBinaryPub(const void *pBinary, size_t binarySize) { return createProgramFromBinary(pBinary, binarySize); }
        void setDevice(Device *device) { pDevice = device; }
        using Program::rebuildProgramFromIr;

      protected:
        CompilerInterface *getCompilerInterface() const override { return nullptr; }
    };

    MyProgram2 *pProgram = new MyProgram2();
    EXPECT_NE(nullptr, pProgram);
    cl_device_id deviceId = pContext->getDevice(0);
    Device *pDevice = castToObject<Device>(deviceId);
    pProgram->setDevice(pDevice);

    // Load a binary program file
    void *pBinary = nullptr;
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "CopyBuffer_simd8_", ".bin");
    size_t binarySize = loadDataFromFile(filePath.c_str(), pBinary);
    EXPECT_NE(0u, binarySize);

    // Create program from loaded binary
    cl_int retVal = pProgram->createProgramFromBinaryPub(pBinary, binarySize);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Ask to rebuild program from its IR binary - it should fail (no Compiler Interface)
    retVal = pProgram->rebuildProgramFromIr();
    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);

    // Cleanup
    deleteDataReadFromFile(pBinary);
    delete pProgram;
}

TEST_F(ProgramTests, RebuildBinaryWithRebuildError) {
    class MyCompilerInterface2 : public CompilerInterface {
      public:
        MyCompilerInterface2(){};
        ~MyCompilerInterface2() override{};

        cl_int link(Program &program, const TranslationArgs &pInputArgs) override { return CL_LINK_PROGRAM_FAILURE; }
    };

    class MyProgram2 : public Program {
      public:
        MyProgram2() { cip = nullptr; };

        cl_int createProgramFromBinaryPub(const void *pBinary, size_t binarySize) { return createProgramFromBinary(pBinary, binarySize); }
        void setDevice(Device *device) { pDevice = device; }
        using Program::rebuildProgramFromIr;
        void releaseCompilerInterface() {
            delete cip;
            cip = nullptr;
        }

      protected:
        CompilerInterface *getCompilerInterface() const override {
            cip = new MyCompilerInterface2;
            return cip;
        }
        mutable MyCompilerInterface2 *cip;
    };

    MyProgram2 *pProgram = new MyProgram2();
    EXPECT_NE(nullptr, pProgram);
    cl_device_id deviceId = pContext->getDevice(0);
    Device *pDevice = castToObject<Device>(deviceId);
    pProgram->setDevice(pDevice);

    // Load a binary program file
    void *pBinary = nullptr;
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "CopyBuffer_simd8_", ".bin");
    size_t binarySize = loadDataFromFile(filePath.c_str(), pBinary);
    EXPECT_NE(0u, binarySize);

    // Create program from loaded binary
    cl_int retVal = pProgram->createProgramFromBinaryPub(pBinary, binarySize);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Ask to rebuild program from its IR binary - it should fail (linking error)
    retVal = pProgram->rebuildProgramFromIr();
    EXPECT_EQ(CL_LINK_PROGRAM_FAILURE, retVal);

    // Cleanup
    pProgram->releaseCompilerInterface();
    deleteDataReadFromFile(pBinary);
    delete pProgram;
}

TEST_F(ProgramTests, BuildProgramWithReraFlag) {
    class MyCompilerInterface2 : public CompilerInterface {
      public:
        MyCompilerInterface2() { buildOptions[0] = buildInternalOptions[0] = '\0'; };
        ~MyCompilerInterface2() override{};

        cl_int build(Program &program, const TranslationArgs &inputArgs, bool enableCaching) override {
            strcpy_s(&buildOptions[0], sizeof(buildOptions), inputArgs.pOptions);
            strcpy_s(&buildInternalOptions[0], sizeof(buildInternalOptions), inputArgs.pInternalOptions);
            return CL_SUCCESS;
        }
        void getBuildOptions(std::string &s) { s = buildOptions; }
        void getBuildInternalOptions(std::string &s) { s = buildInternalOptions; }

      protected:
        char buildOptions[256];
        char buildInternalOptions[1024];
    };

    class MyProgram2 : public Program {
      public:
        MyProgram2() { cip = nullptr; };

        void setDevice(Device *device) { pDevice = device; }
        cl_int processGenBinary() override { return CL_SUCCESS; }
        void releaseCompilerInterface() {
            delete cip;
            cip = nullptr;
        }
        MyCompilerInterface2 *getCompilerInterfacePub() {
            getCompilerInterface();
            return cip;
        }

      protected:
        CompilerInterface *getCompilerInterface() const override {
            if (cip == nullptr) {
                cip = new MyCompilerInterface2;
            }
            return cip;
        }
        mutable MyCompilerInterface2 *cip;
    };

    MyProgram2 *pProgram = new MyProgram2();
    EXPECT_NE(nullptr, pProgram);
    cl_device_id deviceId = pContext->getDevice(0);
    Device *pDevice = castToObject<Device>(deviceId);
    pProgram->setDevice(pDevice);
    MyCompilerInterface2 *cip = pProgram->getCompilerInterfacePub();
    EXPECT_NE(nullptr, cip);
    pProgram->setSource((char *)"__kernel mock() {}");

    // Check default build options
    std::string s1;
    std::string s2;
    cip->getBuildOptions(s1);
    size_t pos = s1.find("-cl-fast-relaxed-math");
    EXPECT_EQ(pos, std::string::npos);
    cip->getBuildInternalOptions(s2);
    pos = s2.find("-cl-intel-gtpin-rera");
    EXPECT_EQ(pos, std::string::npos);

    // Ask to build created program without "-cl-intel-gtpin-rera" flag.
    s1.assign("");
    s2.assign("");
    cl_int retVal = pProgram->build(0, nullptr, "-cl-fast-relaxed-math", nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Check build options that were applied
    cip->getBuildOptions(s1);
    pos = s1.find("-cl-fast-relaxed-math");
    EXPECT_NE(pos, std::string::npos);
    cip->getBuildInternalOptions(s2);
    pos = s2.find("-cl-intel-gtpin-rera");
    EXPECT_EQ(pos, std::string::npos);

    // Ask to build created program with "-cl-intel-gtpin-rera" flag.
    s1.assign("");
    s2.assign("");
    retVal = pProgram->build(0, nullptr, "-cl-intel-gtpin-rera -cl-finite-math-only", nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Check build options that were applied
    cip->getBuildOptions(s1);
    pos = s1.find("-cl-fast-relaxed-math");
    EXPECT_EQ(pos, std::string::npos);
    pos = s1.find("-cl-finite-math-only");
    EXPECT_NE(pos, std::string::npos);
    cip->getBuildInternalOptions(s2);
    pos = s2.find("-cl-intel-gtpin-rera");
    EXPECT_NE(pos, std::string::npos);

    // Cleanup
    pProgram->releaseCompilerInterface();
    delete pProgram;
}

TEST_F(ProgramTests, RebuildBinaryWithProcessGenBinaryError) {
    class MyProgram3 : public Program {
      public:
        MyProgram3(){};
        cl_int createProgramFromBinaryPub(const void *pBinary, size_t binarySize) { return createProgramFromBinary(pBinary, binarySize); }
        void setDevice(Device *device) { pDevice = device; }
        using Program::rebuildProgramFromIr;
        cl_int processGenBinary() override { return CL_INVALID_BINARY; }
    };

    cl_int retVal;
    CompilerInterface::getInstance();

    MyProgram3 *pProgram = new MyProgram3();
    EXPECT_NE(nullptr, pProgram);
    cl_device_id deviceId = pContext->getDevice(0);
    Device *pDevice = castToObject<Device>(deviceId);
    pProgram->setDevice(pDevice);

    // Load a binary program file
    void *pBinary = nullptr;
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "CopyBuffer_simd8_", ".bin");
    size_t binarySize = loadDataFromFile(filePath.c_str(), pBinary);
    EXPECT_NE(0u, binarySize);

    // Create program from loaded binary
    retVal = pProgram->createProgramFromBinaryPub(pBinary, binarySize);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Ask to rebuild program from its IR binary - it should fail (simulated invalid binary)
    retVal = pProgram->rebuildProgramFromIr();
    EXPECT_EQ(CL_INVALID_BINARY, retVal);

    // Cleanup
    deleteDataReadFromFile(pBinary);
    delete pProgram;
    CompilerInterface::shutdown();
}

TEST_F(ProgramTests, GetProgramCompilerVersion) {
    class MyProgram2 : public Program {
      public:
        MyProgram2(){};

        void getProgramCompilerVersionPub(SProgramBinaryHeader *pSectionData, uint32_t &binaryVersion) { getProgramCompilerVersion(pSectionData, binaryVersion); }
    };

    MyProgram2 *pProgram = new MyProgram2();
    EXPECT_NE(nullptr, pProgram);

    // Create example header of OpenCL Program Binary
    cl_device_id deviceId = pContext->getDevice(0);
    Device *pDevice = castToObject<Device>(deviceId);
    struct SProgramBinaryHeader prgHdr;
    prgHdr.Magic = iOpenCL::MAGIC_CL;
    prgHdr.Version = 12;
    prgHdr.Device = pDevice->getHardwareInfo().pPlatform->eRenderCoreFamily;
    prgHdr.GPUPointerSizeInBytes = 8;
    prgHdr.NumberOfKernels = 1;
    prgHdr.SteppingId = 0;
    prgHdr.PatchListSize = 0;

    // Check whether Program Binary version is returned correctly
    uint32_t binaryVersion = 0;
    pProgram->getProgramCompilerVersionPub(&prgHdr, binaryVersion);
    EXPECT_EQ(binaryVersion, 12u);

    // Check whether Program Binary version is left intact
    binaryVersion = 1;
    pProgram->getProgramCompilerVersionPub(nullptr, binaryVersion);
    EXPECT_EQ(binaryVersion, 1u);

    // Cleanup
    delete pProgram;
}

TEST_F(ProgramTests, GivenZeroPrivateSizeInBlockWhenAllocateBlockProvateSurfacesCalledThenNoSurfaceIsCreated) {
    MockProgram *program = new MockProgram(pContext, false);

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

    program->addBlockKernel(infoBlock);

    program->allocateBlockPrivateSurfaces();

    EXPECT_EQ(nullptr, program->getBlockKernelManager()->getPrivateSurface(0));

    delete privateSurfaceBlock;
    delete program;
}

TEST_F(ProgramTests, GivenNonZeroPrivateSizeInBlockWhenAllocateBlockProvateSurfacesCalledThenSurfaceIsCreated) {
    MockProgram *program = new MockProgram(pContext, false);

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

    program->addBlockKernel(infoBlock);

    program->allocateBlockPrivateSurfaces();

    EXPECT_NE(nullptr, program->getBlockKernelManager()->getPrivateSurface(0));

    delete privateSurfaceBlock;
    delete program;
}

TEST_F(ProgramTests, GivenNonZeroPrivateSizeInBlockWhenAllocateBlockProvateSurfacesCalledThenSecondSurfaceIsNotCreated) {
    MockProgram *program = new MockProgram(pContext, false);

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

    program->addBlockKernel(infoBlock);

    program->allocateBlockPrivateSurfaces();

    GraphicsAllocation *privateSurface = program->getBlockKernelManager()->getPrivateSurface(0);

    EXPECT_NE(nullptr, privateSurface);

    program->allocateBlockPrivateSurfaces();

    GraphicsAllocation *privateSurface2 = program->getBlockKernelManager()->getPrivateSurface(0);

    EXPECT_EQ(privateSurface, privateSurface2);

    delete privateSurfaceBlock;
    delete program;
}

TEST_F(ProgramTests, givenProgramWithBlockKernelsWhenfreeBlockResourcesisCalledThenFreeGraphhicsAllocationsFromBlockKernelManagerIsCalled) {
    MockProgram *program = new MockProgram(pContext, false);

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

    program->addBlockKernel(infoBlock);

    GraphicsAllocation *privateSurface = program->getDevice(0).getMemoryManager()->allocateGraphicsMemory(4096);
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

TEST_F(Program32BitTests, givenDeviceWithForce32BitAddressingOnWhenBultinIsCreatedThenNoFlagsArePassedAsInternalOptions) {
    MockProgram pProgram;
    auto &internalOptions = pProgram.getInternalOptions();
    EXPECT_THAT(internalOptions, testing::HasSubstr(std::string("")));
}

TEST_F(Program32BitTests, givenDeviceWithForce32BitAddressingOnWhenProgramIsCreatedThen32bitFlagIsPassedAsInternalOption) {
    MockProgram pProgram(pContext, false);
    auto &internalOptions = pProgram.getInternalOptions();
    std::string s1 = internalOptions;
    size_t pos = s1.find("-m32");
    if (is64bit) {
        EXPECT_NE(pos, std::string::npos);
    } else {
        EXPECT_EQ(pos, std::string::npos);
    }
}

TEST_F(Program32BitTests, givenDeviceWhenProgramIsCreatedThenProgramCountInDeviceIncreases) {
    auto device = pContext->getDevice(0);
    EXPECT_EQ(0u, device->getProgramCount());
    MockProgram pProgram(pContext, false);
    EXPECT_EQ(1u, device->getProgramCount());
}

TEST_F(ProgramTests, givenNewProgramTheStatelessToStatefulBufferOffsetOtimizationIsMatchingThePlatformEnablingStatus) {
    MockProgram prog(pContext, false);
    auto &internalOpts = prog.getInternalOptions();
    auto it = internalOpts.find("-cl-intel-has-buffer-offset-arg ");

    HardwareCapabilities hwCaps = {0};
    HwHelper::get(prog.getDevice(0).getHardwareInfo().pPlatform->eRenderCoreFamily).setupHardwareCapabilities(&hwCaps);
    if (hwCaps.isStatelesToStatefullWithOffsetSupported) {
        EXPECT_NE(std::string::npos, it);
    } else {
        EXPECT_EQ(std::string::npos, it);
    }
}

template <int32_t ErrCodeToReturn, bool spirv = true>
struct CreateProgramFromBinaryMock : MockProgram {
    using MockProgram::MockProgram;
    CreateProgramFromBinaryMock(Context *context, bool isBuiltIn)
        : MockProgram(context, isBuiltIn) {
    }

    cl_int createProgramFromBinary(const void *pBinary,
                                   size_t binarySize) override {
        this->irBinary = new char[binarySize];
        this->irBinarySize = binarySize;
        this->isSpirV = spirv;
        memcpy_s(this->irBinary, binarySize, pBinary, binarySize);
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
    MockProgram program(pContext, false);

    EXPECT_EQ(0u, program.getKernelInfoArray().size());
    EXPECT_EQ(0u, program.getParentKernelInfoArray().size());
    EXPECT_EQ(0u, program.getSubgroupKernelInfoArray().size());

    program.separateBlockKernels();

    EXPECT_EQ(0u, program.getKernelInfoArray().size());
    EXPECT_EQ(0u, program.getBlockKernelManager()->getCount());
}

TEST_F(ProgramTests, givenSeparateBlockKernelsWhenRegularKernelsThenSeparateNoneKernel) {
    MockProgram program(pContext, false);

    auto pRegularKernel1Info = KernelInfo::create();
    pRegularKernel1Info->name = "regular_kernel_1";
    program.getKernelInfoArray().push_back(pRegularKernel1Info);

    auto pRegularKernel2Info = KernelInfo::create();
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
    MockProgram program(pContext, false);

    auto pParentKernelInfo = KernelInfo::create();
    pParentKernelInfo->name = "another_parent_kernel";
    program.getKernelInfoArray().push_back(pParentKernelInfo);
    program.getParentKernelInfoArray().push_back(pParentKernelInfo);

    auto pChildKernelInfo = KernelInfo::create();
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
    MockProgram program(pContext, false);

    auto pSubgroupKernelInfo = KernelInfo::create();
    pSubgroupKernelInfo->name = "another_subgroup_kernel";
    program.getKernelInfoArray().push_back(pSubgroupKernelInfo);
    program.getSubgroupKernelInfoArray().push_back(pSubgroupKernelInfo);

    auto pChildKernelInfo = KernelInfo::create();
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
    MockProgram program(pContext, false);

    auto pParentKernelInfo = KernelInfo::create();
    pParentKernelInfo->name = "parent_kernel";
    program.getKernelInfoArray().push_back(pParentKernelInfo);
    program.getParentKernelInfoArray().push_back(pParentKernelInfo);

    auto pChildKernelInfo = KernelInfo::create();
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
    MockProgram program(pContext, false);

    auto pSubgroupKernelInfo = KernelInfo::create();
    pSubgroupKernelInfo->name = "subgroup_kernel";
    program.getKernelInfoArray().push_back(pSubgroupKernelInfo);
    program.getSubgroupKernelInfoArray().push_back(pSubgroupKernelInfo);

    auto pChildKernelInfo = KernelInfo::create();
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
    MockProgram pProgram(nullptr, false);
    EXPECT_EQ(nullptr, pProgram.getDevicePtr());
    auto dummyDevice = (Device *)0x1337;
    pProgram.SetDevice(dummyDevice);
    EXPECT_EQ(dummyDevice, pProgram.getDevicePtr());
    pProgram.SetDevice(nullptr);
    EXPECT_EQ(nullptr, pProgram.getDevicePtr());
}

TEST(ProgramDestructionTests, givenProgramUsingDeviceWhenItIsDestroyedAfterPlatfromCleanupThenItIsCleanedUpProperly) {
    platformImpl->initialize();
    auto device = platformImpl->getDevice(0);
    MockContext *context = new MockContext(device, false);
    MockProgram *pProgram = new MockProgram(context, false);
    auto globalAllocation = device->getMemoryManager()->allocateGraphicsMemory(MemoryConstants::pageSize);
    pProgram->setGlobalSurface(globalAllocation);

    platformImpl.reset(nullptr);
    EXPECT_EQ(1, device->getRefInternalCount());
    EXPECT_EQ(1, pProgram->getRefInternalCount());
    context->decRefInternal();
    pProgram->decRefInternal();
}

TEST_F(ProgramTests, givenCompilerInterfaceWhenCompileIsCalledThenProperIntermediateRepresentationTypeIsUsed) {
    struct SmallMockCompilerInterface : MockCompilerInterface {
        using CompilerInterface::initialize;
        using CompilerInterface::useLlvmText;
        IGC::CodeType::CodeType_t intermediateRepresentation;
        IGC::CodeType::CodeType_t getPreferredIntermediateRepresentation(const Device &device) override {
            return intermediateRepresentation;
        }
    };

    struct SmallMockProgram : public Program {};
    using ProgramAutoPtr = std::unique_ptr<SmallMockProgram, void (*)(SmallMockProgram *)>;

    auto device = castToObject<Device>(pContext->getDevice(0));

    TranslationArgs input = {};
    char inputData = 0;
    input.pInput = &inputData;
    input.InputSize = 1;

    SmallMockCompilerInterface compilerInterface;
    auto compilerMain = new MockCIFMain();
    compilerInterface.overrideGlobalCompilerInterface();
    compilerInterface.SetFclMain(compilerMain);
    compilerMain->Retain();
    compilerInterface.SetIgcMain(compilerMain);
    compilerMain->setDefaultCreatorFunc<OCLRT::MockIgcOclDeviceCtx>(OCLRT::MockIgcOclDeviceCtx::Create);
    compilerMain->setDefaultCreatorFunc<OCLRT::MockFclOclDeviceCtx>(OCLRT::MockFclOclDeviceCtx::Create);

    compilerInterface.useLlvmText = true;
    ProgramAutoPtr programLlvmText{new SmallMockProgram(), [](SmallMockProgram *p) { p->release(); }};
    programLlvmText->setDevice(device);
    compilerInterface.intermediateRepresentation = IGC::CodeType::spirV;
    compilerInterface.compile(*programLlvmText, input);
    EXPECT_FALSE(programLlvmText->getIsSpirV());

    compilerInterface.useLlvmText = false;
    ProgramAutoPtr programSpirV{new SmallMockProgram(), [](SmallMockProgram *p) { p->release(); }};
    programSpirV->setDevice(device);
    compilerInterface.intermediateRepresentation = IGC::CodeType::spirV;
    compilerInterface.compile(*programSpirV, input);
    EXPECT_TRUE(programSpirV->getIsSpirV());

    ProgramAutoPtr programLlvmBc{new SmallMockProgram(), [](SmallMockProgram *p) { p->release(); }};
    programLlvmBc->setDevice(device);
    compilerInterface.intermediateRepresentation = IGC::CodeType::llvmBc;
    compilerInterface.compile(*programLlvmBc, input);
    EXPECT_FALSE(programLlvmBc->getIsSpirV());
}

TEST_F(ProgramTests, givenProgramWithSpirvWhenRebuildProgramIsCalledThenSpirvPathIsTaken) {
    struct SmallMockProgram : public Program {
        using Program::rebuildProgramFromIr;
    };
    using ProgramAutoPtr = std::unique_ptr<SmallMockProgram, void (*)(SmallMockProgram *)>;

    auto device = castToObject<Device>(pContext->getDevice(0));

    MockCompilerInterface compilerInterface;
    auto compilerMain = new MockCIFMain();
    compilerInterface.overrideGlobalCompilerInterface();
    compilerInterface.SetFclMain(compilerMain);
    compilerMain->Retain();
    compilerInterface.SetIgcMain(compilerMain);
    compilerMain->setDefaultCreatorFunc<OCLRT::MockIgcOclDeviceCtx>(OCLRT::MockIgcOclDeviceCtx::Create);
    compilerMain->setDefaultCreatorFunc<OCLRT::MockFclOclDeviceCtx>(OCLRT::MockFclOclDeviceCtx::Create);

    std::string receivedInput;
    MockCompilerDebugVars debugVars = {};
    debugVars.receivedInput = &receivedInput;
    debugVars.forceBuildFailure = true;
    gEnvironment->igcPushDebugVars(debugVars);
    std::unique_ptr<void, void (*)(void *)> igcDebugVarsAutoPop{&gEnvironment, [](void *) { gEnvironment->igcPopDebugVars(); }};

    ProgramAutoPtr program{new SmallMockProgram(), [](SmallMockProgram *p) { p->release(); }};
    program->setDevice(device);
    uint32_t spirv[16] = {0x03022307, 0x23471113, 0x17192329};
    program->storeIrBinary(spirv, sizeof(spirv), true);
    auto buildRet = program->rebuildProgramFromIr();
    EXPECT_NE(CL_SUCCESS, buildRet);

    using namespace CLElfLib;
    using AutoElfReader = std::unique_ptr<CElfReader, void (*)(CElfReader *)>;
    AutoElfReader elfReader{CElfReader::create(receivedInput.data(), receivedInput.size()), [](CElfReader *r) { CElfReader::destroy(r); }};
    const SElf64SectionHeader *spvSection = nullptr;
    char *spvSectionData = nullptr;
    size_t spvSectionDataSize = 0;
    for (uint32_t i = 0; i < elfReader->getElfHeader()->NumSectionHeaderEntries; i++) {
        const SElf64SectionHeader *section = elfReader->getSectionHeader(i);
        if (section->Type == CLElfLib::E_SH_TYPE::SH_TYPE_SPIRV) {
            EXPECT_EQ(nullptr, spvSection);
            elfReader->getSectionData(i, spvSectionData, spvSectionDataSize);
            spvSection = section;
        }
    }
    ASSERT_NE(nullptr, spvSection);
    EXPECT_EQ(sizeof(spirv), spvSectionDataSize);
    EXPECT_EQ(0, memcmp(spirv, spvSectionData, spvSectionDataSize));
}
