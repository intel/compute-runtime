/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/zebin/zebin_elf.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/api/leo_dispatch.h"
#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/kernel/leo_kernel.h"
#include "level_zero/api/opencl/source/program/leo_program.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

#include <limits>
#include <map>

namespace NEO {
namespace LEO {
namespace ult {

struct MockModuleForKernelNames : public L0::Module {
    std::vector<std::string> kernelNamesStorage;
    std::vector<const char *> kernelNamePtrs;

    MockModuleForKernelNames(std::vector<std::string> names) : kernelNamesStorage(std::move(names)) {
        for (auto &name : kernelNamesStorage) {
            kernelNamePtrs.push_back(name.c_str());
        }
    }

    ze_result_t getKernelNames(uint32_t *pCount, const char **pNames) override {
        if (*pCount == 0) {
            *pCount = static_cast<uint32_t>(kernelNamePtrs.size());
            return ZE_RESULT_SUCCESS;
        }
        if (*pCount > static_cast<uint32_t>(kernelNamePtrs.size())) {
            *pCount = static_cast<uint32_t>(kernelNamePtrs.size());
        }
        for (uint32_t i = 0; i < *pCount; i++) {
            pNames[i] = kernelNamePtrs[i];
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t initialize(const ze_module_desc_t *, NEO::Device *) override { return ZE_RESULT_SUCCESS; }
    L0::Device *getDevice() const override { return nullptr; }
    ze_result_t createKernel(const ze_kernel_desc_t *, ze_kernel_handle_t *) override { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; }
    ze_result_t destroy() override { return ZE_RESULT_SUCCESS; }
    ze_result_t getNativeBinary(size_t *, uint8_t *) override { return ZE_RESULT_SUCCESS; }
    ze_result_t getIrBinary(size_t *, uint8_t *) override { return ZE_RESULT_SUCCESS; }
    ze_result_t getFunctionPointer(const char *, void **) override { return ZE_RESULT_SUCCESS; }
    ze_result_t getGlobalPointer(const char *, size_t *, void **) override { return ZE_RESULT_SUCCESS; }
    ze_result_t getDebugInfo(size_t *, uint8_t *) override { return ZE_RESULT_SUCCESS; }
    ze_result_t getProperties(ze_module_properties_t *) override { return ZE_RESULT_SUCCESS; }
    ze_result_t performDynamicLink(uint32_t, ze_module_handle_t *, ze_module_build_log_handle_t *) override { return ZE_RESULT_SUCCESS; }
    ze_result_t inspectLinkage(ze_linkage_inspection_ext_desc_t *, uint32_t, ze_module_handle_t *, ze_module_build_log_handle_t *) override { return ZE_RESULT_SUCCESS; }
    const L0::KernelImmutableData *getKernelImmutableData(const char *) const override { return nullptr; }
    const std::vector<std::unique_ptr<L0::KernelImmutableData>> &getKernelImmutableDataVector() const override { return emptyKernelImmData; }
    uint32_t getMaxGroupSize(const NEO::KernelDescriptor &) const override { return 0; }
    bool shouldAllocatePrivateMemoryPerDispatch() const override { return false; }
    uint32_t getProfileFlags() const override { return 0; }
    void checkIfPrivateMemoryPerDispatchIsNeeded() override {}
    void populateZebinExtendedArgsMetadata() override {}
    void generateDefaultExtendedArgsMetadata() override {}
    bool isModulesPackage() const override { return false; }

    std::vector<std::unique_ptr<L0::KernelImmutableData>> emptyKernelImmData;
};

struct MockContextForKernelNames : public Context {
    MockContextForKernelNames() : Context(nullptr, nullptr, 0, nullptr, false) {}
    ~MockContextForKernelNames() override = default;
};

struct MockProgramForKernelNames : public Program {
    MockProgramForKernelNames(MockContextForKernelNames *ctx, ze_module_handle_t moduleHandle) : Program(ctx) {
        this->setModuleHandle(0u, moduleHandle);
    }
    ~MockProgramForKernelNames() override {
        this->moduleHandles.clear();
    }
};

struct MockL0KernelForSuggestedLocalWorkSize : public L0::ult::Mock<L0::KernelImp> {
    MockL0KernelForSuggestedLocalWorkSize() {
        this->privateState.kernelArgHandlers.resize(1);
    }

    ze_result_t suggestGroupSize(uint32_t, uint32_t, uint32_t,
                                 uint32_t *groupSizeX,
                                 uint32_t *groupSizeY,
                                 uint32_t *groupSizeZ) override {
        *groupSizeX = 1;
        *groupSizeY = 1;
        *groupSizeZ = 1;
        return ZE_RESULT_SUCCESS;
    }
};

struct GetKernelSuggestedLocalWorkSizeFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        device = platform->getDevices()[0].get();
        cl_device_id clDevice = device;
        context = std::make_unique<Context>(nullptr, nullptr, 1, &clDevice, true);
        commandQueue = std::make_unique<CommandQueue>(context.get(), device, nullptr, nullptr);
        l0Kernel = std::make_unique<MockL0KernelForSuggestedLocalWorkSize>();
        program = std::make_unique<Program>(context.get());
        std::map<uint32_t, ze_kernel_handle_t> kernelHandles{{0u, l0Kernel->toHandle()}};
        kernel = std::make_unique<Kernel>(std::move(kernelHandles), program.get());
    }

    void TearDown() override {
        kernel.reset();
        program.reset();
        l0Kernel.release();
        commandQueue.reset();
        context.reset();
        Test<OclFixture>::TearDown();
    }

    ClDevice *device = nullptr;
    std::unique_ptr<Context> context;
    std::unique_ptr<CommandQueue> commandQueue;
    std::unique_ptr<MockL0KernelForSuggestedLocalWorkSize> l0Kernel;
    std::unique_ptr<Program> program;
    std::unique_ptr<Kernel> kernel;
};

TEST(CreateKernelTests, givenNullProgramWhenCreateKernelThenReturnsCLInvalidProgram) {
    cl_int errcode = CL_SUCCESS;
    auto kernel = clCreateKernel(nullptr, "kernelName", &errcode);
    EXPECT_EQ(nullptr, kernel);
    EXPECT_EQ(CL_INVALID_PROGRAM, errcode);
}

TEST(RetainReleaseKernelTests, givenNullKernelWhenRetainKernelThenReturnsCLInvalidKernel) {
    auto retVal = clRetainKernel(nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST(RetainReleaseKernelTests, givenNullKernelWhenReleaseKernelThenReturnsCLInvalidKernel) {
    auto retVal = clReleaseKernel(nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST(GetKernelInfoTests, givenNullKernelWhenGetKernelInfoThenReturnsCLInvalidKernel) {
    auto retVal = clGetKernelInfo(nullptr, CL_KERNEL_FUNCTION_NAME, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST(GetKernelArgInfoTests, givenNullKernelWhenGetKernelArgInfoThenReturnsCLInvalidKernel) {
    auto retVal = clGetKernelArgInfo(nullptr, 0, CL_KERNEL_ARG_NAME, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST(GetKernelWorkGroupInfoTests, givenNullKernelWhenGetKernelWorkGroupInfoThenReturnsCLInvalidKernel) {
    auto retVal = clGetKernelWorkGroupInfo(nullptr, nullptr, CL_KERNEL_WORK_GROUP_SIZE, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST(GetKernelSubGroupInfoTests, givenNullKernelWhenGetKernelSubGroupInfoThenReturnsCLInvalidKernel) {
    auto retVal = clGetKernelSubGroupInfo(nullptr, nullptr, CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE, 0, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST(SetKernelArgTests, givenNullKernelWhenSetKernelArgThenReturnsCLInvalidKernel) {
    auto retVal = clSetKernelArg(nullptr, 0, 0, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST(SetKernelArgSVMPointerTests, givenNullKernelWhenSetKernelArgSVMPointerThenReturnsCLInvalidKernel) {
    auto retVal = clSetKernelArgSVMPointer(nullptr, 0, nullptr);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST(SetKernelExecInfoTests, givenNullKernelWhenSetKernelExecInfoThenReturnsCLInvalidKernel) {
    cl_bool value = CL_TRUE;
    auto retVal = clSetKernelExecInfo(nullptr, CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL, sizeof(cl_bool), &value);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST(CreateKernelsInProgramTests, givenNullProgramWhenCreateKernelsInProgramThenReturnsCLInvalidProgram) {
    auto retVal = clCreateKernelsInProgram(nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
}

TEST(CreateKernelsInProgramTests, givenProgramWithExternalFunctionsWhenCreateKernelsInProgramThenExternalFunctionsAreFilteredOut) {
    std::string externalFunctionsName{NEO::Zebin::Elf::SectionNames::externalFunctions};
    MockModuleForKernelNames mockModule({"kernel0", externalFunctionsName, "kernel1"});

    MockContextForKernelNames context;
    MockProgramForKernelNames program(&context, mockModule.toHandle());

    cl_uint numKernels = 0;
    auto retVal = clCreateKernelsInProgram(&program, 0, nullptr, &numKernels);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2u, numKernels);
}

TEST(GetProgramInfoTests, givenProgramWithExternalFunctionsWhenGetProgramInfoWithNumKernelsThenExternalFunctionsAreFilteredOut) {
    std::string externalFunctionsName{NEO::Zebin::Elf::SectionNames::externalFunctions};
    MockModuleForKernelNames mockModule({"kernel0", externalFunctionsName, "kernel1"});

    MockContextForKernelNames context;
    MockProgramForKernelNames program(&context, mockModule.toHandle());

    uint32_t numKernels = 0;
    auto retVal = clGetProgramInfo(&program, CL_PROGRAM_NUM_KERNELS, sizeof(numKernels), &numKernels, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2u, numKernels);
}

TEST(GetProgramInfoTests, givenProgramWithExternalFunctionsWhenGetProgramInfoWithKernelNamesThenExternalFunctionsAreFilteredOut) {
    std::string externalFunctionsName{NEO::Zebin::Elf::SectionNames::externalFunctions};
    MockModuleForKernelNames mockModule({"kernel0", externalFunctionsName, "kernel1"});

    MockContextForKernelNames context;
    MockProgramForKernelNames program(&context, mockModule.toHandle());

    char kernelNames[64] = {};
    auto retVal = clGetProgramInfo(&program, CL_PROGRAM_KERNEL_NAMES, sizeof(kernelNames), kernelNames, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_STREQ("kernel0;kernel1", kernelNames);
}

TEST(CloneKernelTests, givenNullKernelWhenCloneKernelThenReturnsCLInvalidKernel) {
    cl_int errcode = CL_SUCCESS;
    auto kernel = clCloneKernel(nullptr, &errcode);
    EXPECT_EQ(nullptr, kernel);
    EXPECT_EQ(CL_INVALID_KERNEL, errcode);
}

TEST_F(GetKernelSuggestedLocalWorkSizeFixture, givenNullKernelWhenGetKernelSuggestedLocalWorkSizeThenReturnsCLInvalidKernel) {
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t suggestedLocalWorkSize[3] = {};

    auto retVal = clGetKernelSuggestedLocalWorkSize(commandQueue.get(), nullptr, 1, nullptr, globalWorkSize, suggestedLocalWorkSize);

    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST(GetKernelSuggestedLocalWorkSizeTests, GivenIcdDispatchTableWhenCheckingFunctionPointerThenCoreFunctionIsRegistered) {
    auto icdStoredFunction = icdGlobalDispatchTable.clGetKernelSuggestedLocalWorkSize;
    auto pFunction = &clGetKernelSuggestedLocalWorkSize;
    EXPECT_EQ(icdStoredFunction, pFunction);
}

TEST(GetKernelSuggestedLocalWorkSizeTests, GivenClGetKernelSuggestedLocalWorkSizeWhenGettingExtensionFunctionThenCorrectAddressIsReturned) {
    auto retVal = clGetExtensionFunctionAddress("clGetKernelSuggestedLocalWorkSize");
    EXPECT_EQ(retVal, reinterpret_cast<void *>(clGetKernelSuggestedLocalWorkSize));
}

TEST_F(GetKernelSuggestedLocalWorkSizeFixture, givenNullCommandQueueWhenGetKernelSuggestedLocalWorkSizeThenReturnsCLInvalidCommandQueue) {
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t suggestedLocalWorkSize[3] = {};

    auto retVal = clGetKernelSuggestedLocalWorkSize(nullptr, kernel.get(), 1, nullptr, globalWorkSize, suggestedLocalWorkSize);

    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(GetKernelSuggestedLocalWorkSizeFixture, givenNullSuggestedLocalWorkSizeWhenGetKernelSuggestedLocalWorkSizeThenReturnsCLInvalidValue) {
    size_t globalWorkSize[3] = {1, 1, 1};

    auto retVal = clGetKernelSuggestedLocalWorkSize(commandQueue.get(), kernel.get(), 1, nullptr, globalWorkSize, nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(GetKernelSuggestedLocalWorkSizeFixture, givenNullGlobalWorkSizeWhenGetKernelSuggestedLocalWorkSizeThenReturnsCLInvalidGlobalWorkSize) {
    size_t suggestedLocalWorkSize[3] = {};

    auto retVal = clGetKernelSuggestedLocalWorkSize(commandQueue.get(), kernel.get(), 1, nullptr, nullptr, suggestedLocalWorkSize);

    EXPECT_EQ(CL_INVALID_GLOBAL_WORK_SIZE, retVal);
}

TEST_F(GetKernelSuggestedLocalWorkSizeFixture, givenZeroGlobalWorkSizeWhenGetKernelSuggestedLocalWorkSizeThenReturnsCLInvalidGlobalWorkSize) {
    size_t globalWorkSize[3] = {1, 0, 1};
    size_t suggestedLocalWorkSize[3] = {};

    auto retVal = clGetKernelSuggestedLocalWorkSize(commandQueue.get(), kernel.get(), 2, nullptr, globalWorkSize, suggestedLocalWorkSize);

    EXPECT_EQ(CL_INVALID_GLOBAL_WORK_SIZE, retVal);
}

TEST_F(GetKernelSuggestedLocalWorkSizeFixture, givenInvalidWorkDimWhenGetKernelSuggestedLocalWorkSizeThenReturnsCLInvalidWorkDimension) {
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t suggestedLocalWorkSize[3] = {};

    auto retVal = clGetKernelSuggestedLocalWorkSize(commandQueue.get(), kernel.get(), 0, nullptr, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_INVALID_WORK_DIMENSION, retVal);

    retVal = clGetKernelSuggestedLocalWorkSize(commandQueue.get(), kernel.get(), 4, nullptr, globalWorkSize, suggestedLocalWorkSize);
    EXPECT_EQ(CL_INVALID_WORK_DIMENSION, retVal);
}

TEST_F(GetKernelSuggestedLocalWorkSizeFixture, givenGlobalWorkOffsetOverflowWhenGetKernelSuggestedLocalWorkSizeThenReturnsCLInvalidGlobalOffset) {
    size_t globalWorkOffset[3] = {std::numeric_limits<size_t>::max(), 0, 0};
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t suggestedLocalWorkSize[3] = {};

    auto retVal = clGetKernelSuggestedLocalWorkSize(commandQueue.get(), kernel.get(), 1, globalWorkOffset, globalWorkSize, suggestedLocalWorkSize);

    EXPECT_EQ(CL_INVALID_GLOBAL_OFFSET, retVal);
}

TEST_F(GetKernelSuggestedLocalWorkSizeFixture, givenValidInputWhenGetKernelSuggestedLocalWorkSizeThenReturnsSuggestedSize) {
    size_t globalWorkSize[3] = {8, 1, 1};
    size_t suggestedLocalWorkSize[3] = {};

    auto retVal = clGetKernelSuggestedLocalWorkSize(commandQueue.get(), kernel.get(), 1, nullptr, globalWorkSize, suggestedLocalWorkSize);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, suggestedLocalWorkSize[0]);
}

TEST_F(GetKernelSuggestedLocalWorkSizeFixture, givenConcurrentTypeWhenSetKernelExecInfoKernelTypeThenExecutionTypeIsStored) {
    cl_execution_info_kernel_type_intel type = CL_KERNEL_EXEC_INFO_CONCURRENT_TYPE_INTEL;

    auto retVal = clSetKernelExecInfo(kernel.get(), CL_KERNEL_EXEC_INFO_KERNEL_TYPE_INTEL, sizeof(type), &type);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(NEO::KernelExecutionType::concurrent, kernel->getExecutionType());
}

TEST_F(GetKernelSuggestedLocalWorkSizeFixture, givenDefaultTypeWhenSetKernelExecInfoKernelTypeThenExecutionTypeIsStored) {
    cl_execution_info_kernel_type_intel type = CL_KERNEL_EXEC_INFO_DEFAULT_TYPE_INTEL;

    auto retVal = clSetKernelExecInfo(kernel.get(), CL_KERNEL_EXEC_INFO_KERNEL_TYPE_INTEL, sizeof(type), &type);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(NEO::KernelExecutionType::defaultType, kernel->getExecutionType());
}

TEST_F(GetKernelSuggestedLocalWorkSizeFixture, givenInvalidTypeWhenSetKernelExecInfoKernelTypeThenReturnsCLInvalidValue) {
    cl_execution_info_kernel_type_intel type = CL_KERNEL_EXEC_INFO_CONCURRENT_TYPE_INTEL + 0xff;

    auto retVal = clSetKernelExecInfo(kernel.get(), CL_KERNEL_EXEC_INFO_KERNEL_TYPE_INTEL, sizeof(type), &type);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(GetKernelSuggestedLocalWorkSizeFixture, givenInvalidParamValueSizeWhenSetKernelExecInfoKernelTypeThenReturnsCLInvalidValue) {
    cl_execution_info_kernel_type_intel type = CL_KERNEL_EXEC_INFO_CONCURRENT_TYPE_INTEL;

    auto retVal = clSetKernelExecInfo(kernel.get(), CL_KERNEL_EXEC_INFO_KERNEL_TYPE_INTEL, sizeof(type) - 1, &type);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(GetKernelSuggestedLocalWorkSizeFixture, givenNullParamValueWhenSetKernelExecInfoKernelTypeThenReturnsCLInvalidValue) {
    auto retVal = clSetKernelExecInfo(kernel.get(), CL_KERNEL_EXEC_INFO_KERNEL_TYPE_INTEL, sizeof(cl_execution_info_kernel_type_intel), nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(GetKernelSuggestedLocalWorkSizeFixture, givenNullCommandQueueWhenGetKernelSuggestedLocalWorkSizeINTELThenReturnsCLInvalidCommandQueue) {
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t suggestedLocalWorkSize[3] = {};

    auto retVal = clGetKernelSuggestedLocalWorkSizeINTEL(nullptr, kernel.get(), 1, nullptr, globalWorkSize, suggestedLocalWorkSize);

    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(GetKernelSuggestedLocalWorkSizeFixture, givenNullCommandQueueWhenGetKernelSuggestedLocalWorkSizeKHRThenReturnsCLInvalidCommandQueue) {
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t suggestedLocalWorkSize[3] = {};

    auto retVal = clGetKernelSuggestedLocalWorkSizeKHR(nullptr, kernel.get(), 1, nullptr, globalWorkSize, suggestedLocalWorkSize);

    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

struct DriverBackedL0Context : public L0::ult::Mock<L0::Context> {
    DriverBackedL0Context(L0::DriverHandle *driverHandle) : L0::ult::Mock<L0::Context>(driverHandle), backingDriver(driverHandle) {}
    L0::DriverHandle *getDriverHandle() override { return backingDriver; }
    L0::DriverHandle *backingDriver = nullptr;
};

struct SvmKernelValidationFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        cl_device_id clDeviceId = clDevice;
        l0Context = std::make_unique<DriverBackedL0Context>(driverHandle.get());
        context = std::make_unique<Context>(nullptr, l0Context->toHandle(), 1, &clDeviceId, true);
        program = std::make_unique<Program>(context.get());
        l0Kernel = std::make_unique<L0::ult::Mock<L0::KernelImp>>();
        l0Kernel->privateState.kernelArgHandlers.resize(1);
        l0Kernel->checkPassedArgumentValues = true;
        l0Kernel->passedArgumentValues.resize(1);
        auto &explicitArgs = l0Kernel->descriptor.payloadMappings.explicitArgs;
        explicitArgs.resize(1);
        explicitArgs[0] = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
        std::map<uint32_t, ze_kernel_handle_t> kernelHandles{{0u, l0Kernel->toHandle()}};
        kernel = std::make_unique<Kernel>(std::move(kernelHandles), program.get());
    }

    void TearDown() override {
        kernel.reset();
        l0Kernel.release();
        program.reset();
        context.reset();
        l0Context.reset();
        Test<OclFixture>::TearDown();
    }

    void setArgAddressQualifier(NEO::KernelArgMetadata::AddressSpaceQualifier addressQualifier) {
        l0Kernel->descriptor.payloadMappings.explicitArgs[0].getTraits().addressQualifier = addressQualifier;
    }

    ClDevice *clDevice = nullptr;
    std::unique_ptr<DriverBackedL0Context> l0Context;
    std::unique_ptr<Context> context;
    std::unique_ptr<Program> program;
    std::unique_ptr<L0::ult::Mock<L0::KernelImp>> l0Kernel;
    std::unique_ptr<Kernel> kernel;
};

TEST_F(SvmKernelValidationFixture, givenLocalAddressQualifierArgWhenSetKernelArgSVMPointerThenReturnsInvalidArgValue) {
    setArgAddressQualifier(NEO::KernelArgMetadata::AddrLocal);
    int dummy = 0;
    auto retVal = clSetKernelArgSVMPointer(kernel.get(), 0, &dummy);
    EXPECT_EQ(CL_INVALID_ARG_VALUE, retVal);
}

TEST_F(SvmKernelValidationFixture, givenGlobalAddressQualifierArgWhenSetKernelArgSVMPointerThenReturnsSuccess) {
    setArgAddressQualifier(NEO::KernelArgMetadata::AddrGlobal);
    int dummy = 0;
    auto retVal = clSetKernelArgSVMPointer(kernel.get(), 0, &dummy);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(SvmKernelValidationFixture, givenArgIndexOutOfRangeWhenSetKernelArgSVMPointerThenReturnsInvalidArgIndex) {
    int dummy = 0;
    auto retVal = clSetKernelArgSVMPointer(kernel.get(), 1, &dummy);
    EXPECT_EQ(CL_INVALID_ARG_INDEX, retVal);
}

TEST_F(SvmKernelValidationFixture, givenSvmPtrsWithSizeNotMultipleOfPointerSizeWhenSetKernelExecInfoThenReturnsInvalidValue) {
    int dummy = 0;
    void *ptr = &dummy;
    auto retVal = clSetKernelExecInfo(kernel.get(), CL_KERNEL_EXEC_INFO_SVM_PTRS, 3, &ptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(SvmKernelValidationFixture, givenSvmPtrsWithUnknownPointerWhenSetKernelExecInfoThenReturnsInvalidValue) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EnableSharedSystemUsmSupport.set(0);

    void *unknownPtr = reinterpret_cast<void *>(0x1000);
    auto retVal = clSetKernelExecInfo(kernel.get(), CL_KERNEL_EXEC_INFO_SVM_PTRS, sizeof(void *), &unknownPtr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(SvmKernelValidationFixture, givenInvalidParamNameWhenSetKernelExecInfoThenReturnsInvalidValue) {
    int dummy = 0;
    void *ptr = &dummy;
    auto retVal = clSetKernelExecInfo(kernel.get(), 0, sizeof(void *), &ptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(SvmKernelValidationFixture, givenFineGrainSystemWhenSetKernelExecInfoThenReturnsInvalidOperation) {
    cl_bool enabled = CL_TRUE;
    auto retVal = clSetKernelExecInfo(kernel.get(), CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM, sizeof(cl_bool), &enabled);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

TEST_F(SvmKernelValidationFixture, givenKnownSvmPointerWhenSetKernelExecInfoSvmPtrsThenSucceedsAndEnablesIndirectAccess) {
    constexpr uint64_t gpuAddress = 0x1000u;
    void *svmPtr = reinterpret_cast<void *>(gpuAddress);
    NEO::MockGraphicsAllocation mockAllocation(svmPtr, gpuAddress, 0x1000u);
    NEO::SvmAllocationData allocData(0u);
    allocData.size = 0x1000u;
    allocData.gpuAllocations.addAllocation(&mockAllocation);
    context->getL0Object()->getDriverHandle()->getSvmAllocsManager()->insertSVMAlloc(allocData);

    auto retVal = clSetKernelExecInfo(kernel.get(), CL_KERNEL_EXEC_INFO_SVM_PTRS, sizeof(void *), &svmPtr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto unifiedMemoryControls = l0Kernel->getUnifiedMemoryControls();
    EXPECT_TRUE(unifiedMemoryControls.indirectDeviceAllocationsAllowed);
    EXPECT_TRUE(unifiedMemoryControls.indirectHostAllocationsAllowed);
    EXPECT_TRUE(unifiedMemoryControls.indirectSharedAllocationsAllowed);

    context->getL0Object()->getDriverHandle()->getSvmAllocsManager()->removeSVMAlloc(allocData);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
