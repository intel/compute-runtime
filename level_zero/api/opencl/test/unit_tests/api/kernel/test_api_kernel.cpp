/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/zebin/zebin_elf.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/api/api.h"
#include "level_zero/api/opencl/source/api/dispatch.h"
#include "level_zero/api/opencl/source/command_queue/command_queue.h"
#include "level_zero/api/opencl/source/context/context.h"
#include "level_zero/api/opencl/source/kernel/kernel.h"
#include "level_zero/api/opencl/source/program/program.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

#include "CL/cl.h"

#include <limits>

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
        this->moduleHandle = moduleHandle;
    }
    ~MockProgramForKernelNames() override {
        this->moduleHandle = nullptr;
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
        kernel = std::make_unique<Kernel>(l0Kernel->toHandle(), program.get());
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

} // namespace ult
} // namespace LEO
} // namespace NEO
