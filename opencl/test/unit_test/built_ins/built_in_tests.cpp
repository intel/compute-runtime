/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hash.h"
#include "shared/source/helpers/path.h"
#include "shared/source/helpers/string.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/libult/global_environment.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_builtinslib.h"
#include "shared/test/common/mocks/mock_compiler_interface.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/accelerators/intel_motion_estimation.h"
#include "opencl/source/built_ins/aux_translation_builtin.h"
#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/built_ins/vme_builtin.h"
#include "opencl/source/built_ins/vme_dispatch_builder.h"
#include "opencl/source/helpers/dispatch_info_builder.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/built_ins/built_ins_file_names.h"
#include "opencl/test/unit_test/fixtures/built_in_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/fixtures/run_kernel_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_cl_execution_environment.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "gtest/gtest.h"
#include "os_inc.h"
#include "test_traits_common.h"

#include <string>

using namespace NEO;

class BuiltInTests
    : public BuiltInFixture,
      public ClDeviceFixture,
      public ContextFixture,
      public ::testing::Test {

    using BuiltInFixture::setUp;
    using ContextFixture::setUp;

  public:
    BuiltInTests() {
        // reserving space here to avoid the appearance of a memory management
        // leak being reported
        allBuiltIns.reserve(5000);
    }

    void SetUp() override {
        debugManager.flags.ForceAuxTranslationMode.set(static_cast<int32_t>(AuxTranslationMode::builtin));
        ClDeviceFixture::setUp();
        cl_device_id device = pClDevice;
        ContextFixture::setUp(1, &device);
        BuiltInFixture::setUp(pDevice);
    }

    void TearDown() override {
        allBuiltIns.clear();
        BuiltInFixture::tearDown();
        ContextFixture::tearDown();
        ClDeviceFixture::tearDown();
    }

    void appendBuiltInStringFromFile(std::string builtInFile, size_t &size) {
        std::string src;
        auto pData = loadDataFromFile(
            builtInFile.c_str(),
            size);

        ASSERT_NE(nullptr, pData);

        src = (const char *)pData.get();
        size_t start = src.find("R\"===(");
        size_t stop = src.find(")===\"");

        // assert that pattern was found
        ASSERT_NE(std::string::npos, start);
        ASSERT_NE(std::string::npos, stop);

        start += strlen("R\"===(");
        size = stop - start;
        allBuiltIns.append(src, start, size);
    }

    bool compareBuiltinOpParams(const BuiltinOpParams &left,
                                const BuiltinOpParams &right) {
        return left.srcPtr == right.srcPtr &&
               left.dstPtr == right.dstPtr &&
               left.size == right.size &&
               left.srcOffset == right.srcOffset &&
               left.dstOffset == right.dstOffset &&
               left.dstMemObj == right.dstMemObj &&
               left.srcMemObj == right.srcMemObj;
    }

    DebugManagerStateRestore restore;
    std::string allBuiltIns;
};

struct VmeBuiltInTests : BuiltInTests {
    void SetUp() override {
        BuiltInTests::SetUp();
        if (!pDevice->getHardwareInfo().capabilityTable.supportsVme) {
            GTEST_SKIP();
        }
    }
};

struct AuxBuiltInTests : BuiltInTests, public ::testing::WithParamInterface<KernelObjForAuxTranslation::Type> {
    void SetUp() override {
        BuiltInTests::SetUp();
        kernelObjType = GetParam();
    }
    KernelObjForAuxTranslation::Type kernelObjType;
};

struct AuxBuiltinsMatcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::auxBuiltinsSupported;
    }
};

struct HeaplessSupportedMatcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::heaplessAllowed;
    }
};

HWTEST2_F(BuiltInTests, GivenBuiltinTypeBinaryWhenGettingAuxTranslationBuiltinThenResourceSizeIsNonZero, MatchAny) {
    auto mockBuiltinsLib = std::unique_ptr<MockBuiltinsLib>(new MockBuiltinsLib());

    EXPECT_EQ(TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::auxBuiltinsSupported,
              mockBuiltinsLib->getBuiltinResource(EBuiltInOps::auxTranslation, BuiltinCode::ECodeType::binary, *pDevice).size() != 0);
}

class MockAuxBuilInOp : public BuiltInOp<EBuiltInOps::auxTranslation> {
  public:
    using BuiltinDispatchInfoBuilder::populate;
    using BaseClass = BuiltInOp<EBuiltInOps::auxTranslation>;
    using BaseClass::baseKernel;
    using BaseClass::convertToAuxKernel;
    using BaseClass::convertToNonAuxKernel;
    using BaseClass::resizeKernelInstances;
    using BaseClass::usedKernels;

    using BaseClass::BuiltInOp;
};

INSTANTIATE_TEST_SUITE_P(,
                         AuxBuiltInTests,
                         testing::ValuesIn({KernelObjForAuxTranslation::Type::memObj, KernelObjForAuxTranslation::Type::gfxAlloc}));

HWCMDTEST_P(IGFX_XE_HP_CORE, AuxBuiltInTests, givenXeHpCoreCommandsAndAuxTranslationKernelWhenSettingKernelArgsThenSetValidMocs) {

    const auto &compilerProductHelper = pDevice->getRootDeviceEnvironment().getHelper<CompilerProductHelper>();
    if (compilerProductHelper.isForceToStatelessRequired()) {
        GTEST_SKIP();
    }
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    MockAuxBuilInOp mockAuxBuiltInOp(*pBuiltIns, *pClDevice);

    BuiltinOpParams builtinOpParamsToAux;
    builtinOpParamsToAux.auxTranslationDirection = AuxTranslationDirection::nonAuxToAux;

    BuiltinOpParams builtinOpParamsToNonAux;
    builtinOpParamsToNonAux.auxTranslationDirection = AuxTranslationDirection::auxToNonAux;

    std::unique_ptr<Buffer> buffer = nullptr;
    std::unique_ptr<GraphicsAllocation> gfxAllocation = nullptr;

    auto kernelObjsForAuxTranslation = std::make_unique<KernelObjsForAuxTranslation>();
    if (kernelObjType == MockKernelObjForAuxTranslation::Type::memObj) {
        cl_int retVal = CL_SUCCESS;
        buffer.reset(Buffer::create(pContext, 0, MemoryConstants::pageSize, nullptr, retVal));
        kernelObjsForAuxTranslation->insert({KernelObjForAuxTranslation::Type::memObj, buffer.get()});
    } else {
        gfxAllocation.reset(new MockGraphicsAllocation(nullptr, MemoryConstants::pageSize));
        kernelObjsForAuxTranslation->insert({KernelObjForAuxTranslation::Type::gfxAlloc, gfxAllocation.get()});
    }

    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.setKernelObjsForAuxTranslation(std::move(kernelObjsForAuxTranslation));

    mockAuxBuiltInOp.buildDispatchInfosForAuxTranslation<FamilyType>(multiDispatchInfo, builtinOpParamsToAux);
    mockAuxBuiltInOp.buildDispatchInfosForAuxTranslation<FamilyType>(multiDispatchInfo, builtinOpParamsToNonAux);

    {
        // read args
        auto argNum = 0;
        auto expectedMocs = pDevice->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED);
        auto sshBase = mockAuxBuiltInOp.convertToAuxKernel[0]->getSurfaceStateHeap();
        auto sshOffset = mockAuxBuiltInOp.convertToAuxKernel[0]->getKernelInfo().getArgDescriptorAt(argNum).as<ArgDescPointer>().bindful;
        auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(sshBase, sshOffset));
        EXPECT_EQ(expectedMocs, surfaceState->getMemoryObjectControlState());

        sshBase = mockAuxBuiltInOp.convertToNonAuxKernel[0]->getSurfaceStateHeap();
        sshOffset = mockAuxBuiltInOp.convertToNonAuxKernel[0]->getKernelInfo().getArgDescriptorAt(argNum).as<ArgDescPointer>().bindful;
        surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(sshBase, sshOffset));
        EXPECT_EQ(expectedMocs, surfaceState->getMemoryObjectControlState());
    }

    {
        // write args
        auto argNum = 1;
        auto expectedMocs = pDevice->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST);
        auto sshBase = mockAuxBuiltInOp.convertToAuxKernel[0]->getSurfaceStateHeap();
        auto sshOffset = mockAuxBuiltInOp.convertToAuxKernel[0]->getKernelInfo().getArgDescriptorAt(argNum).as<ArgDescPointer>().bindful;
        auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(sshBase, sshOffset));
        EXPECT_EQ(expectedMocs, surfaceState->getMemoryObjectControlState());

        sshBase = mockAuxBuiltInOp.convertToNonAuxKernel[0]->getSurfaceStateHeap();
        sshOffset = mockAuxBuiltInOp.convertToNonAuxKernel[0]->getKernelInfo().getArgDescriptorAt(argNum).as<ArgDescPointer>().bindful;
        surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(sshBase, sshOffset));
        EXPECT_EQ(expectedMocs, surfaceState->getMemoryObjectControlState());
    }
}

TEST_F(BuiltInTests, WhenBuildingListOfBuiltinsThenBuiltinsHaveBeenGenerated) {
    for (auto supportsImages : ::testing::Bool()) {
        allBuiltIns.clear();
        size_t size = 0;

        for (auto &fileName : getBuiltInFileNames(supportsImages)) {
            appendBuiltInStringFromFile(sharedBuiltinsDir + "/" + fileName, size);
            ASSERT_NE(0u, size);
        }

        // convert /r/n to /n
        size_t startPos = 0;
        while ((startPos = allBuiltIns.find("\r\n", startPos)) != std::string::npos) {
            allBuiltIns.replace(startPos, 2, "\n");
        }

        // convert /r to /n
        startPos = 0;
        while ((startPos = allBuiltIns.find("\r", startPos)) != std::string::npos) {
            allBuiltIns.replace(startPos, 1, "\n");
        }

        uint64_t hash = Hash::hash(allBuiltIns.c_str(), allBuiltIns.length());
        auto hashName = getBuiltInHashFileName(hash, supportsImages);

        // First fail, if we are inconsistent
        EXPECT_EQ(true, fileExists(hashName)) << "**********\nBuilt in kernels need to be regenerated for the mock compilers!\n**********";

        // then write to file if needed
#define GENERATE_NEW_HASH_FOR_BUILT_INS 0
#if GENERATE_NEW_HASH_FOR_BUILT_INS
        std::cout << "writing builtins to file: " << hashName << std::endl;
        const char *pData = allBuiltIns.c_str();
        writeDataToFile(hashName.c_str(), pData, allBuiltIns.length());
#endif
    }
}

TEST_F(BuiltInTests, GivenCopyBufferToSystemMemoryBufferWhenDispatchInfoIsCreatedThenParamsAreCorrect) {
    BuiltinDispatchInfoBuilder &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferToBuffer, *pClDevice);

    MockBuffer *srcPtr = new MockBuffer();
    MockBuffer *dstPtr = new MockBuffer();

    MockBuffer &src = *srcPtr;
    MockBuffer &dst = *dstPtr;

    srcPtr->mockGfxAllocation.setAllocationType(AllocationType::buffer);
    dstPtr->mockGfxAllocation.setAllocationType(AllocationType::bufferHostMemory);

    BuiltinOpParams builtinOpsParams;

    builtinOpsParams.srcMemObj = &src;
    builtinOpsParams.dstMemObj = &dst;
    builtinOpsParams.srcPtr = src.getCpuAddress();
    builtinOpsParams.dstPtr = dst.getCpuAddress();
    builtinOpsParams.size = {dst.getSize(), 0, 0};

    MultiDispatchInfo multiDispatchInfo(builtinOpsParams);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));

    size_t leftSize = reinterpret_cast<uintptr_t>(dst.getCpuAddress()) % MemoryConstants::cacheLineSize;
    if (leftSize > 0) {
        leftSize = MemoryConstants::cacheLineSize - leftSize;
    }
    size_t rightSize = (reinterpret_cast<uintptr_t>(dst.getCpuAddress()) + dst.getSize()) % MemoryConstants::cacheLineSize;
    size_t middleSize = (dst.getSize() - leftSize - rightSize) / (sizeof(uint32_t) * 4);

    int i = 0;
    int leftKernel = 0;
    int middleKernel = 0;
    int rightKernel = 0;

    if (leftSize > 0) {
        middleKernel++;
        rightKernel++;
    } else {
        leftKernel = -1;
    }
    if (middleSize > 0) {
        rightKernel++;
    } else {
        middleKernel = -1;
    }
    if (rightSize == 0) {
        rightKernel = -1;
    }

    for (auto &dispatchInfo : multiDispatchInfo) {
        EXPECT_EQ(1u, dispatchInfo.getDim());
        if (i == leftKernel) {
            EXPECT_EQ(Vec3<size_t>(leftSize, 1, 1), dispatchInfo.getGWS());
        } else if (i == middleKernel) {
            EXPECT_EQ(Vec3<size_t>(middleSize, 1, 1), dispatchInfo.getGWS());
        } else if (i == rightKernel) {
            EXPECT_EQ(Vec3<size_t>(rightSize, 1, 1), dispatchInfo.getGWS());
        }
        i++;
        EXPECT_TRUE(dispatchInfo.getKernel()->getDestinationAllocationInSystemMemory());
    }
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), builtinOpsParams));
    delete srcPtr;
    delete dstPtr;
}

TEST_F(BuiltInTests, GivenCopyBufferToLocalMemoryBufferWhenDispatchInfoIsCreatedThenParamsAreCorrect) {
    BuiltinDispatchInfoBuilder &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferToBuffer, *pClDevice);

    MockBuffer *srcPtr = new MockBuffer();
    MockBuffer *dstPtr = new MockBuffer();

    MockBuffer &src = *srcPtr;
    MockBuffer &dst = *dstPtr;

    srcPtr->mockGfxAllocation.setAllocationType(AllocationType::bufferHostMemory);
    dstPtr->mockGfxAllocation.setAllocationType(AllocationType::buffer);

    BuiltinOpParams builtinOpsParams;

    builtinOpsParams.srcMemObj = &src;
    builtinOpsParams.dstMemObj = &dst;
    builtinOpsParams.srcPtr = src.getCpuAddress();
    builtinOpsParams.dstPtr = dst.getCpuAddress();
    builtinOpsParams.size = {dst.getSize(), 0, 0};

    MultiDispatchInfo multiDispatchInfo(builtinOpsParams);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));

    for (auto &dispatchInfo : multiDispatchInfo) {
        EXPECT_FALSE(dispatchInfo.getKernel()->getDestinationAllocationInSystemMemory());
    }

    delete srcPtr;
    delete dstPtr;
}

HWTEST2_P(AuxBuiltInTests, givenInputBufferWhenBuildingNonAuxDispatchInfoForAuxTranslationThenPickAndSetupCorrectKernels, AuxBuiltinsMatcher) {
    BuiltinDispatchInfoBuilder &baseBuilder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::auxTranslation, *pClDevice);
    auto &builder = static_cast<BuiltInOp<EBuiltInOps::auxTranslation> &>(baseBuilder);

    std::vector<Kernel *> builtinKernels;
    std::vector<MockKernelObjForAuxTranslation> mockKernelObjForAuxTranslation;
    mockKernelObjForAuxTranslation.push_back(MockKernelObjForAuxTranslation(kernelObjType, 0x1000));
    mockKernelObjForAuxTranslation.push_back(MockKernelObjForAuxTranslation(kernelObjType, 0x20000));
    mockKernelObjForAuxTranslation.push_back(MockKernelObjForAuxTranslation(kernelObjType, 0x30000));

    BuiltinOpParams builtinOpsParams;
    builtinOpsParams.auxTranslationDirection = AuxTranslationDirection::auxToNonAux;

    auto kernelObjsForAuxTranslation = std::make_unique<KernelObjsForAuxTranslation>();
    for (auto &kernelObj : mockKernelObjForAuxTranslation) {
        kernelObjsForAuxTranslation->insert(kernelObj);
    }
    auto kernelObjsForAuxTranslationPtr = kernelObjsForAuxTranslation.get();

    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.setKernelObjsForAuxTranslation(std::move(kernelObjsForAuxTranslation));

    EXPECT_TRUE(builder.buildDispatchInfosForAuxTranslation<FamilyType>(multiDispatchInfo, builtinOpsParams));
    EXPECT_EQ(3u, multiDispatchInfo.size());

    for (auto &dispatchInfo : multiDispatchInfo) {
        auto kernel = dispatchInfo.getKernel();
        builtinKernels.push_back(kernel);

        if (kernelObjType == KernelObjForAuxTranslation::Type::memObj) {
            auto buffer = castToObject<Buffer>(kernel->getKernelArguments().at(0).object);
            auto kernelObj = *kernelObjsForAuxTranslationPtr->find({KernelObjForAuxTranslation::Type::memObj, buffer});
            EXPECT_NE(nullptr, kernelObj.object);
            EXPECT_EQ(KernelObjForAuxTranslation::Type::memObj, kernelObj.type);
            kernelObjsForAuxTranslationPtr->erase(kernelObj);

            cl_mem clMem = buffer;
            EXPECT_EQ(clMem, kernel->getKernelArguments().at(0).object);
            EXPECT_EQ(clMem, kernel->getKernelArguments().at(1).object);

            EXPECT_EQ(1u, dispatchInfo.getDim());
            size_t xGws = alignUp(buffer->getSize(), 512) / 16;
            Vec3<size_t> gws = {xGws, 1, 1};
            EXPECT_EQ(gws, dispatchInfo.getGWS());
        } else {
            auto gfxAllocation = static_cast<GraphicsAllocation *>(kernel->getKernelArguments().at(0).object);
            auto kernelObj = *kernelObjsForAuxTranslationPtr->find({KernelObjForAuxTranslation::Type::gfxAlloc, gfxAllocation});
            EXPECT_NE(nullptr, kernelObj.object);
            EXPECT_EQ(KernelObjForAuxTranslation::Type::gfxAlloc, kernelObj.type);
            kernelObjsForAuxTranslationPtr->erase(kernelObj);

            EXPECT_EQ(gfxAllocation, kernel->getKernelArguments().at(0).object);
            EXPECT_EQ(gfxAllocation, kernel->getKernelArguments().at(1).object);

            EXPECT_EQ(1u, dispatchInfo.getDim());
            size_t xGws = alignUp(gfxAllocation->getUnderlyingBufferSize(), 512) / 16;
            Vec3<size_t> gws = {xGws, 1, 1};
            EXPECT_EQ(gws, dispatchInfo.getGWS());
        }
    }
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), builtinOpsParams));
    // always pick different kernel
    EXPECT_EQ(3u, builtinKernels.size());
    EXPECT_NE(builtinKernels[0], builtinKernels[1]);
    EXPECT_NE(builtinKernels[0], builtinKernels[2]);
    EXPECT_NE(builtinKernels[1], builtinKernels[2]);
}

HWTEST2_P(AuxBuiltInTests, givenInputBufferWhenBuildingAuxDispatchInfoForAuxTranslationThenPickAndSetupCorrectKernels, AuxBuiltinsMatcher) {
    BuiltinDispatchInfoBuilder &baseBuilder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::auxTranslation, *pClDevice);
    auto &builder = static_cast<BuiltInOp<EBuiltInOps::auxTranslation> &>(baseBuilder);

    std::vector<Kernel *> builtinKernels;
    std::vector<MockKernelObjForAuxTranslation> mockKernelObjForAuxTranslation;
    mockKernelObjForAuxTranslation.push_back(MockKernelObjForAuxTranslation(kernelObjType, 0x1000));
    mockKernelObjForAuxTranslation.push_back(MockKernelObjForAuxTranslation(kernelObjType, 0x20000));
    mockKernelObjForAuxTranslation.push_back(MockKernelObjForAuxTranslation(kernelObjType, 0x30000));

    BuiltinOpParams builtinOpsParams;
    builtinOpsParams.auxTranslationDirection = AuxTranslationDirection::nonAuxToAux;

    auto kernelObjsForAuxTranslation = std::make_unique<KernelObjsForAuxTranslation>();
    auto kernelObjsForAuxTranslationPtr = kernelObjsForAuxTranslation.get();
    for (auto &kernelObj : mockKernelObjForAuxTranslation) {
        kernelObjsForAuxTranslation->insert(kernelObj);
    }
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.setKernelObjsForAuxTranslation(std::move(kernelObjsForAuxTranslation));

    EXPECT_TRUE(builder.buildDispatchInfosForAuxTranslation<FamilyType>(multiDispatchInfo, builtinOpsParams));
    EXPECT_EQ(3u, multiDispatchInfo.size());

    for (auto &dispatchInfo : multiDispatchInfo) {
        auto kernel = dispatchInfo.getKernel();
        builtinKernels.push_back(kernel);

        if (kernelObjType == KernelObjForAuxTranslation::Type::memObj) {
            auto buffer = castToObject<Buffer>(kernel->getKernelArguments().at(0).object);
            auto kernelObj = *kernelObjsForAuxTranslationPtr->find({KernelObjForAuxTranslation::Type::memObj, buffer});
            EXPECT_NE(nullptr, kernelObj.object);
            EXPECT_EQ(KernelObjForAuxTranslation::Type::memObj, kernelObj.type);
            kernelObjsForAuxTranslationPtr->erase(kernelObj);

            cl_mem clMem = buffer;
            EXPECT_EQ(clMem, kernel->getKernelArguments().at(0).object);
            EXPECT_EQ(clMem, kernel->getKernelArguments().at(1).object);

            EXPECT_EQ(1u, dispatchInfo.getDim());
            size_t xGws = alignUp(buffer->getSize(), 4) / 4;
            Vec3<size_t> gws = {xGws, 1, 1};
            EXPECT_EQ(gws, dispatchInfo.getGWS());
        } else {
            auto gfxAllocation = static_cast<GraphicsAllocation *>(kernel->getKernelArguments().at(0).object);
            auto kernelObj = *kernelObjsForAuxTranslationPtr->find({KernelObjForAuxTranslation::Type::gfxAlloc, gfxAllocation});
            EXPECT_NE(nullptr, kernelObj.object);
            EXPECT_EQ(KernelObjForAuxTranslation::Type::gfxAlloc, kernelObj.type);
            kernelObjsForAuxTranslationPtr->erase(kernelObj);

            EXPECT_EQ(gfxAllocation, kernel->getKernelArguments().at(0).object);
            EXPECT_EQ(gfxAllocation, kernel->getKernelArguments().at(1).object);

            EXPECT_EQ(1u, dispatchInfo.getDim());
            size_t xGws = alignUp(gfxAllocation->getUnderlyingBufferSize(), 512) / 16;
            Vec3<size_t> gws = {xGws, 1, 1};
            EXPECT_EQ(gws, dispatchInfo.getGWS());
        }
    }
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), builtinOpsParams));
    // always pick different kernel
    EXPECT_EQ(3u, builtinKernels.size());
    EXPECT_NE(builtinKernels[0], builtinKernels[1]);
    EXPECT_NE(builtinKernels[0], builtinKernels[2]);
    EXPECT_NE(builtinKernels[1], builtinKernels[2]);
}

HWTEST2_P(AuxBuiltInTests, givenInputBufferWhenBuildingAuxTranslationDispatchThenPickDifferentKernelsDependingOnRequest, AuxBuiltinsMatcher) {
    BuiltinDispatchInfoBuilder &baseBuilder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::auxTranslation, *pClDevice);
    auto &builder = static_cast<BuiltInOp<EBuiltInOps::auxTranslation> &>(baseBuilder);

    std::vector<MockKernelObjForAuxTranslation> mockKernelObjForAuxTranslation;
    for (int i = 0; i < 3; i++) {
        mockKernelObjForAuxTranslation.push_back(MockKernelObjForAuxTranslation(kernelObjType));
    }
    std::vector<Kernel *> builtinKernels;

    BuiltinOpParams builtinOpsParams;

    auto kernelObjsForAuxTranslation = std::make_unique<KernelObjsForAuxTranslation>();
    for (auto &kernelObj : mockKernelObjForAuxTranslation) {
        kernelObjsForAuxTranslation->insert(kernelObj);
    }
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.setKernelObjsForAuxTranslation(std::move(kernelObjsForAuxTranslation));

    builtinOpsParams.auxTranslationDirection = AuxTranslationDirection::auxToNonAux;
    EXPECT_TRUE(builder.buildDispatchInfosForAuxTranslation<FamilyType>(multiDispatchInfo, builtinOpsParams));

    builtinOpsParams.auxTranslationDirection = AuxTranslationDirection::nonAuxToAux;
    EXPECT_TRUE(builder.buildDispatchInfosForAuxTranslation<FamilyType>(multiDispatchInfo, builtinOpsParams));

    EXPECT_EQ(6u, multiDispatchInfo.size());

    for (auto &dispatchInfo : multiDispatchInfo) {
        builtinKernels.push_back(dispatchInfo.getKernel());
    }
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), builtinOpsParams));
    // nonAux vs Aux instance
    EXPECT_EQ(6u, builtinKernels.size());
    EXPECT_NE(builtinKernels[0], builtinKernels[3]);
    EXPECT_NE(builtinKernels[1], builtinKernels[4]);
    EXPECT_NE(builtinKernels[2], builtinKernels[5]);
}

HWTEST2_P(AuxBuiltInTests, givenInvalidAuxTranslationDirectionWhenBuildingDispatchInfosThenAbort, AuxBuiltinsMatcher) {
    BuiltinDispatchInfoBuilder &baseBuilder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::auxTranslation, *pClDevice);
    auto &builder = static_cast<BuiltInOp<EBuiltInOps::auxTranslation> &>(baseBuilder);

    auto kernelObjsForAuxTranslation = std::make_unique<KernelObjsForAuxTranslation>();
    auto kernelObjsForAuxTranslationPtr = kernelObjsForAuxTranslation.get();
    MockKernelObjForAuxTranslation mockKernelObjForAuxTranslation(kernelObjType);

    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.setKernelObjsForAuxTranslation(std::move(kernelObjsForAuxTranslation));
    BuiltinOpParams builtinOpsParams;

    kernelObjsForAuxTranslationPtr->insert(mockKernelObjForAuxTranslation);

    builtinOpsParams.auxTranslationDirection = AuxTranslationDirection::none;
    EXPECT_THROW(builder.buildDispatchInfosForAuxTranslation<FamilyType>(multiDispatchInfo, builtinOpsParams), std::exception);
}

TEST_F(BuiltInTests, whenAuxBuiltInIsConstructedThenResizeKernelInstancedTo5) {
    MockAuxBuilInOp mockAuxBuiltInOp(*pBuiltIns, *pClDevice);
    EXPECT_EQ(5u, mockAuxBuiltInOp.convertToAuxKernel.size());
    EXPECT_EQ(5u, mockAuxBuiltInOp.convertToNonAuxKernel.size());
}

HWTEST2_P(AuxBuiltInTests, givenMoreKernelObjectsForAuxTranslationThanKernelInstancesWhenDispatchingThenResize, AuxBuiltinsMatcher) {
    MockAuxBuilInOp mockAuxBuiltInOp(*pBuiltIns, *pClDevice);
    EXPECT_EQ(5u, mockAuxBuiltInOp.convertToAuxKernel.size());
    EXPECT_EQ(5u, mockAuxBuiltInOp.convertToNonAuxKernel.size());

    std::vector<MockKernelObjForAuxTranslation> mockKernelObjForAuxTranslation;
    for (int i = 0; i < 7; i++) {
        mockKernelObjForAuxTranslation.push_back(MockKernelObjForAuxTranslation(kernelObjType));
    }

    BuiltinOpParams builtinOpsParams;
    builtinOpsParams.auxTranslationDirection = AuxTranslationDirection::auxToNonAux;

    auto kernelObjsForAuxTranslation = std::make_unique<KernelObjsForAuxTranslation>();
    for (auto &kernelObj : mockKernelObjForAuxTranslation) {
        kernelObjsForAuxTranslation->insert(kernelObj);
    }
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.setKernelObjsForAuxTranslation(std::move(kernelObjsForAuxTranslation));

    EXPECT_TRUE(mockAuxBuiltInOp.buildDispatchInfosForAuxTranslation<FamilyType>(multiDispatchInfo, builtinOpsParams));
    EXPECT_EQ(7u, mockAuxBuiltInOp.convertToAuxKernel.size());
    EXPECT_EQ(7u, mockAuxBuiltInOp.convertToNonAuxKernel.size());
}

TEST_F(BuiltInTests, givenkAuxBuiltInWhenResizeIsCalledThenCloneAllNewInstancesFromBaseKernel) {
    MockAuxBuilInOp mockAuxBuiltInOp(*pBuiltIns, *pClDevice);
    size_t newSize = mockAuxBuiltInOp.convertToAuxKernel.size() + 3;
    mockAuxBuiltInOp.resizeKernelInstances(newSize);

    EXPECT_EQ(newSize, mockAuxBuiltInOp.convertToAuxKernel.size());
    for (auto &convertToAuxKernel : mockAuxBuiltInOp.convertToAuxKernel) {
        EXPECT_EQ(&mockAuxBuiltInOp.baseKernel->getKernelInfo(), &convertToAuxKernel->getKernelInfo());
    }

    EXPECT_EQ(newSize, mockAuxBuiltInOp.convertToNonAuxKernel.size());
    for (auto &convertToNonAuxKernel : mockAuxBuiltInOp.convertToNonAuxKernel) {
        EXPECT_EQ(&mockAuxBuiltInOp.baseKernel->getKernelInfo(), &convertToNonAuxKernel->getKernelInfo());
    }
}

HWTEST2_P(AuxBuiltInTests, givenKernelWithAuxTranslationRequiredWhenEnqueueCalledThenLockOnBuiltin, AuxBuiltinsMatcher) {
    BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::auxTranslation, *pClDevice);
    auto mockAuxBuiltInOp = new MockAuxBuilInOp(*pBuiltIns, *pClDevice);
    pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(rootDeviceIndex, EBuiltInOps::auxTranslation, std::unique_ptr<MockAuxBuilInOp>(mockAuxBuiltInOp));

    auto mockProgram = clUniquePtr(new MockProgram(toClDeviceVector(*pClDevice)));
    auto mockBuiltinKernel = MockKernel::create(*pDevice, mockProgram.get());
    auto kernelInfos = MockKernel::toKernelInfoContainer(mockBuiltinKernel->getKernelInfo(), rootDeviceIndex);
    auto pMultiDeviceKernel = new MockMultiDeviceKernel(MockMultiDeviceKernel::toKernelVector(mockBuiltinKernel), kernelInfos);
    mockAuxBuiltInOp->usedKernels.at(0).reset(pMultiDeviceKernel);

    MockKernelWithInternals mockKernel(*pClDevice, pContext);
    MockCommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, nullptr);
    size_t gws[3] = {1, 0, 0};

    mockKernel.kernelInfo.addArgBuffer(0, 0, sizeof(void *));
    mockKernel.mockKernel->initialize();

    std::unique_ptr<Gmm> gmm;

    MockKernelObjForAuxTranslation mockKernelObjForAuxTranslation(kernelObjType);
    if (kernelObjType == KernelObjForAuxTranslation::Type::memObj) {
        MockBuffer::setAllocationType(mockKernelObjForAuxTranslation.mockBuffer->getGraphicsAllocation(0), pDevice->getRootDeviceEnvironment().getGmmHelper(), true);

        cl_mem clMem = mockKernelObjForAuxTranslation.mockBuffer.get();
        mockKernel.mockKernel->setArgBuffer(0, sizeof(cl_mem *), &clMem);
    } else {
        auto gfxAllocation = mockKernelObjForAuxTranslation.mockGraphicsAllocation.get();

        MockBuffer::setAllocationType(gfxAllocation, pDevice->getRootDeviceEnvironment().getGmmHelper(), true);

        auto ptr = reinterpret_cast<void *>(gfxAllocation->getGpuAddressToPatch());
        mockKernel.mockKernel->setArgSvmAlloc(0, ptr, gfxAllocation, 0u);

        gmm.reset(gfxAllocation->getDefaultGmm());
    }

    mockKernel.mockKernel->auxTranslationRequired = false;
    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(0u, pMultiDeviceKernel->takeOwnershipCalls);
    EXPECT_EQ(0u, pMultiDeviceKernel->releaseOwnershipCalls);

    mockKernel.mockKernel->auxTranslationRequired = true;
    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(1u, pMultiDeviceKernel->takeOwnershipCalls);
    EXPECT_EQ(1u, pMultiDeviceKernel->releaseOwnershipCalls);
}

HWCMDTEST_P(IGFX_GEN8_CORE, AuxBuiltInTests, givenAuxTranslationKernelWhenSettingKernelArgsThenSetValidMocs) {
    if (this->pDevice->areSharedSystemAllocationsAllowed()) {
        GTEST_SKIP();
    }

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    MockAuxBuilInOp mockAuxBuiltInOp(*pBuiltIns, *pClDevice);

    BuiltinOpParams builtinOpParamsToAux;
    builtinOpParamsToAux.auxTranslationDirection = AuxTranslationDirection::nonAuxToAux;

    BuiltinOpParams builtinOpParamsToNonAux;
    builtinOpParamsToNonAux.auxTranslationDirection = AuxTranslationDirection::auxToNonAux;

    std::unique_ptr<Buffer> buffer = nullptr;
    std::unique_ptr<GraphicsAllocation> gfxAllocation = nullptr;

    auto kernelObjsForAuxTranslation = std::make_unique<KernelObjsForAuxTranslation>();

    if (kernelObjType == MockKernelObjForAuxTranslation::Type::memObj) {
        cl_int retVal = CL_SUCCESS;
        buffer.reset(Buffer::create(pContext, 0, MemoryConstants::pageSize, nullptr, retVal));
        kernelObjsForAuxTranslation->insert({KernelObjForAuxTranslation::Type::memObj, buffer.get()});
    } else {
        gfxAllocation.reset(new MockGraphicsAllocation(nullptr, MemoryConstants::pageSize));
        kernelObjsForAuxTranslation->insert({KernelObjForAuxTranslation::Type::gfxAlloc, gfxAllocation.get()});
    }

    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.setKernelObjsForAuxTranslation(std::move(kernelObjsForAuxTranslation));

    mockAuxBuiltInOp.buildDispatchInfosForAuxTranslation<FamilyType>(multiDispatchInfo, builtinOpParamsToAux);
    mockAuxBuiltInOp.buildDispatchInfosForAuxTranslation<FamilyType>(multiDispatchInfo, builtinOpParamsToNonAux);

    {
        // read args
        auto argNum = 0;
        auto expectedMocs = pDevice->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED);
        auto sshBase = mockAuxBuiltInOp.convertToAuxKernel[0]->getSurfaceStateHeap();
        auto sshOffset = mockAuxBuiltInOp.convertToAuxKernel[0]->getKernelInfo().getArgDescriptorAt(argNum).as<ArgDescPointer>().bindful;
        auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(sshBase, sshOffset));
        EXPECT_EQ(expectedMocs, surfaceState->getMemoryObjectControlState());

        sshBase = mockAuxBuiltInOp.convertToNonAuxKernel[0]->getSurfaceStateHeap();
        sshOffset = mockAuxBuiltInOp.convertToNonAuxKernel[0]->getKernelInfo().getArgDescriptorAt(argNum).as<ArgDescPointer>().bindful;
        surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(sshBase, sshOffset));
        EXPECT_EQ(expectedMocs, surfaceState->getMemoryObjectControlState());
    }

    {
        // write args
        auto argNum = 1;
        auto expectedMocs = pDevice->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
        auto sshBase = mockAuxBuiltInOp.convertToAuxKernel[0]->getSurfaceStateHeap();
        auto sshOffset = mockAuxBuiltInOp.convertToAuxKernel[0]->getKernelInfo().getArgDescriptorAt(argNum).as<ArgDescPointer>().bindful;
        auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(sshBase, sshOffset));
        EXPECT_EQ(expectedMocs, surfaceState->getMemoryObjectControlState());

        sshBase = mockAuxBuiltInOp.convertToNonAuxKernel[0]->getSurfaceStateHeap();
        sshOffset = mockAuxBuiltInOp.convertToNonAuxKernel[0]->getKernelInfo().getArgDescriptorAt(argNum).as<ArgDescPointer>().bindful;
        surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(sshBase, sshOffset));
        EXPECT_EQ(expectedMocs, surfaceState->getMemoryObjectControlState());
    }
}

HWTEST2_P(AuxBuiltInTests, givenAuxToNonAuxTranslationWhenSettingSurfaceStateThenSetValidAuxMode, AuxBuiltinsMatcher) {
    const auto &compilerProductHelper = pDevice->getRootDeviceEnvironment().getHelper<CompilerProductHelper>();
    if (compilerProductHelper.isForceToStatelessRequired()) {
        GTEST_SKIP();
    }

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    MockAuxBuilInOp mockAuxBuiltInOp(*pBuiltIns, *pClDevice);

    BuiltinOpParams builtinOpParams;
    builtinOpParams.auxTranslationDirection = AuxTranslationDirection::auxToNonAux;

    std::unique_ptr<Buffer> buffer = nullptr;
    std::unique_ptr<GraphicsAllocation> gfxAllocation = nullptr;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::unique_ptr<Gmm>(new Gmm(pDevice->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    gmm->setCompressionEnabled(true);

    auto kernelObjsForAuxTranslation = std::make_unique<KernelObjsForAuxTranslation>();

    if (kernelObjType == MockKernelObjForAuxTranslation::Type::memObj) {
        cl_int retVal = CL_SUCCESS;
        buffer.reset(Buffer::create(pContext, 0, MemoryConstants::pageSize, nullptr, retVal));
        buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->setDefaultGmm(gmm.release());

        kernelObjsForAuxTranslation->insert({KernelObjForAuxTranslation::Type::memObj, buffer.get()});
    } else {
        gfxAllocation.reset(new MockGraphicsAllocation(nullptr, MemoryConstants::pageSize));
        gfxAllocation->setDefaultGmm(gmm.get());

        kernelObjsForAuxTranslation->insert({KernelObjForAuxTranslation::Type::gfxAlloc, gfxAllocation.get()});
    }
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.setKernelObjsForAuxTranslation(std::move(kernelObjsForAuxTranslation));
    mockAuxBuiltInOp.buildDispatchInfosForAuxTranslation<FamilyType>(multiDispatchInfo, builtinOpParams);

    {
        // read arg
        auto argNum = 0;
        auto sshBase = mockAuxBuiltInOp.convertToNonAuxKernel[0]->getSurfaceStateHeap();
        auto sshOffset = mockAuxBuiltInOp.convertToNonAuxKernel[0]->getKernelInfo().getArgDescriptorAt(argNum).as<ArgDescPointer>().bindful;
        auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(sshBase, sshOffset));
        EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E, surfaceState->getAuxiliarySurfaceMode());
    }

    {
        // write arg
        auto argNum = 1;
        auto sshBase = mockAuxBuiltInOp.convertToNonAuxKernel[0]->getSurfaceStateHeap();
        auto sshOffset = mockAuxBuiltInOp.convertToNonAuxKernel[0]->getKernelInfo().getArgDescriptorAt(argNum).as<ArgDescPointer>().bindful;
        auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(sshBase, sshOffset));
        EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE, surfaceState->getAuxiliarySurfaceMode());
    }
}

HWTEST2_P(AuxBuiltInTests, givenNonAuxToAuxTranslationWhenSettingSurfaceStateThenSetValidAuxMode, AuxBuiltinsMatcher) {
    const auto &compilerProductHelper = pDevice->getRootDeviceEnvironment().getHelper<CompilerProductHelper>();
    if (compilerProductHelper.isForceToStatelessRequired()) {
        GTEST_SKIP();
    }

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    MockAuxBuilInOp mockAuxBuiltInOp(*pBuiltIns, *pClDevice);

    BuiltinOpParams builtinOpParams;
    builtinOpParams.auxTranslationDirection = AuxTranslationDirection::nonAuxToAux;

    MockKernelObjForAuxTranslation mockKernelObjForAuxTranslation(kernelObjType);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(pDevice->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, StorageInfo{}, gmmRequirements);
    gmm->setCompressionEnabled(true);
    if (kernelObjType == MockKernelObjForAuxTranslation::Type::memObj) {
        mockKernelObjForAuxTranslation.mockBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->setDefaultGmm(gmm.release());
    } else {
        mockKernelObjForAuxTranslation.mockGraphicsAllocation->setDefaultGmm(gmm.get());
    }
    auto kernelObjsForAuxTranslation = std::make_unique<KernelObjsForAuxTranslation>();
    kernelObjsForAuxTranslation->insert(mockKernelObjForAuxTranslation);

    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.setKernelObjsForAuxTranslation(std::move(kernelObjsForAuxTranslation));
    mockAuxBuiltInOp.buildDispatchInfosForAuxTranslation<FamilyType>(multiDispatchInfo, builtinOpParams);

    {
        // read arg
        auto argNum = 0;
        auto sshBase = mockAuxBuiltInOp.convertToAuxKernel[0]->getSurfaceStateHeap();
        auto sshOffset = mockAuxBuiltInOp.convertToAuxKernel[0]->getKernelInfo().getArgDescriptorAt(argNum).as<ArgDescPointer>().bindful;
        auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(sshBase, sshOffset));
        EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE, surfaceState->getAuxiliarySurfaceMode());
    }

    {
        // write arg
        auto argNum = 1;
        auto sshBase = mockAuxBuiltInOp.convertToAuxKernel[0]->getSurfaceStateHeap();
        auto sshOffset = mockAuxBuiltInOp.convertToAuxKernel[0]->getKernelInfo().getArgDescriptorAt(argNum).as<ArgDescPointer>().bindful;
        auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(sshBase, sshOffset));
        EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E, surfaceState->getAuxiliarySurfaceMode());
    }
}

TEST_F(BuiltInTests, GivenCopyBufferToBufferWhenDispatchInfoIsCreatedThenSizeIsAlignedToCachLineSize) {
    BuiltinDispatchInfoBuilder &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferToBuffer, *pClDevice);

    AlignedBuffer src;
    AlignedBuffer dst;

    BuiltinOpParams builtinOpsParams;

    builtinOpsParams.srcMemObj = &src;
    builtinOpsParams.dstMemObj = &dst;
    builtinOpsParams.size = {src.getSize(), 0, 0};

    MultiDispatchInfo multiDispatchInfo(builtinOpsParams);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));

    EXPECT_EQ(1u, multiDispatchInfo.size());

    const DispatchInfo *dispatchInfo = multiDispatchInfo.begin();

    EXPECT_EQ(1u, dispatchInfo->getDim());

    size_t leftSize = reinterpret_cast<uintptr_t>(dst.getCpuAddress()) % MemoryConstants::cacheLineSize;
    EXPECT_EQ(0u, leftSize);

    size_t rightSize = (reinterpret_cast<uintptr_t>(dst.getCpuAddress()) + dst.getSize()) % MemoryConstants::cacheLineSize;
    EXPECT_EQ(0u, rightSize);

    size_t middleElSize = sizeof(uint32_t) * 4;
    size_t middleSize = dst.getSize() / middleElSize;
    EXPECT_EQ(Vec3<size_t>(middleSize, 1, 1), dispatchInfo->getGWS());

    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), builtinOpsParams));
}

TEST_F(BuiltInTests, givenBigOffsetAndSizeWhenBuilderCopyBufferToBufferStatelessIsUsedThenParamsAreCorrect) {

    if (is32bit) {
        GTEST_SKIP();
    }

    BuiltinDispatchInfoBuilder &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferToBufferStateless, *pClDevice);

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t bigOffset = 4ull * MemoryConstants::gigaByte;
    uint64_t size = 4ull * MemoryConstants::gigaByte;

    MockBuffer srcBuffer;
    srcBuffer.size = static_cast<size_t>(bigSize);
    MockBuffer dstBuffer;
    dstBuffer.size = static_cast<size_t>(bigSize);

    BuiltinOpParams builtinOpsParams;

    builtinOpsParams.srcMemObj = &srcBuffer;
    builtinOpsParams.srcOffset = {static_cast<size_t>(bigOffset), 0, 0};
    builtinOpsParams.dstMemObj = &dstBuffer;
    builtinOpsParams.dstOffset = {0, 0, 0};
    builtinOpsParams.size = {static_cast<size_t>(size), 0, 0};

    MultiDispatchInfo multiDispatchInfo(builtinOpsParams);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), builtinOpsParams));
}

TEST_F(BuiltInTests, givenBigOffsetAndSizeWhenBuilderCopyBufferToSystemBufferRectStatelessIsUsedThenParamsAreCorrect) {
    if (is32bit) {
        GTEST_SKIP();
    }

    BuiltinDispatchInfoBuilder &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferRectStateless, *pClDevice);

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t bigOffset = 4ull * MemoryConstants::gigaByte;
    uint64_t size = 4ull * MemoryConstants::gigaByte;

    MockBuffer srcBuffer;
    srcBuffer.size = static_cast<size_t>(bigSize);
    MockBuffer dstBuffer;
    dstBuffer.size = static_cast<size_t>(bigSize);

    srcBuffer.mockGfxAllocation.setAllocationType(AllocationType::buffer);
    dstBuffer.mockGfxAllocation.setAllocationType(AllocationType::bufferHostMemory);

    BuiltinOpParams dc;
    dc.srcMemObj = &srcBuffer;
    dc.dstMemObj = &dstBuffer;
    dc.srcOffset = {static_cast<size_t>(bigOffset), 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {static_cast<size_t>(size), 1, 1};
    dc.srcRowPitch = static_cast<size_t>(size);
    dc.srcSlicePitch = 0;
    dc.dstRowPitch = static_cast<size_t>(size);
    dc.dstSlicePitch = 0;

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), dc));

    for (auto &dispatchInfo : multiDispatchInfo) {
        EXPECT_TRUE(dispatchInfo.getKernel()->getDestinationAllocationInSystemMemory());
    }
}

TEST_F(BuiltInTests, givenBigOffsetAndSizeWhenBuilderCopyBufferToLocalBufferRectStatelessIsUsedThenParamsAreCorrect) {
    if (is32bit) {
        GTEST_SKIP();
    }

    BuiltinDispatchInfoBuilder &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferRectStateless, *pClDevice);

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t bigOffset = 4ull * MemoryConstants::gigaByte;
    uint64_t size = 4ull * MemoryConstants::gigaByte;

    MockBuffer srcBuffer;
    srcBuffer.size = static_cast<size_t>(bigSize);
    MockBuffer dstBuffer;
    dstBuffer.size = static_cast<size_t>(bigSize);

    srcBuffer.mockGfxAllocation.setAllocationType(AllocationType::bufferHostMemory);
    dstBuffer.mockGfxAllocation.setAllocationType(AllocationType::buffer);

    BuiltinOpParams dc;
    dc.srcMemObj = &srcBuffer;
    dc.dstMemObj = &dstBuffer;
    dc.srcOffset = {static_cast<size_t>(bigOffset), 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {static_cast<size_t>(size), 1, 1};
    dc.srcRowPitch = static_cast<size_t>(size);
    dc.srcSlicePitch = 0;
    dc.dstRowPitch = static_cast<size_t>(size);
    dc.dstSlicePitch = 0;

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), dc));

    for (auto &dispatchInfo : multiDispatchInfo) {
        EXPECT_FALSE(dispatchInfo.getKernel()->getDestinationAllocationInSystemMemory());
    }
}

TEST_F(BuiltInTests, givenMisalignedDstPitchWhenBuilderCopyBufferRectSplitIsUsedThenParamsAreCorrect) {
    if (is32bit || !pClDevice->getProductHelper().isCopyBufferRectSplitSupported()) {
        GTEST_SKIP();
    }

    BuiltinDispatchInfoBuilder &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferRectStateless, *pClDevice);

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t size = 4ull * MemoryConstants::gigaByte;

    MockBuffer srcBuffer;
    srcBuffer.size = static_cast<size_t>(bigSize);
    MockBuffer dstBuffer;
    dstBuffer.size = static_cast<size_t>(bigSize);

    srcBuffer.mockGfxAllocation.setAllocationType(AllocationType::buffer);
    dstBuffer.mockGfxAllocation.setAllocationType(AllocationType::buffer);

    BuiltinOpParams dc;
    dc.srcMemObj = &srcBuffer;
    dc.dstMemObj = &dstBuffer;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {static_cast<size_t>(size), 1, 1};
    dc.srcRowPitch = static_cast<size_t>(size);
    dc.srcSlicePitch = 0;
    dc.dstRowPitch = static_cast<size_t>(size);
    dc.dstSlicePitch = 1;

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), dc));
}

TEST_F(BuiltInTests, givenBigOffsetAndSizeWhenBuilderFillSystemBufferStatelessIsUsedThenParamsAreCorrect) {
    if (is32bit) {
        GTEST_SKIP();
    }

    BuiltinDispatchInfoBuilder &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::fillBufferStateless, *pClDevice);

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t bigOffset = 4ull * MemoryConstants::gigaByte;
    uint64_t size = 4ull * MemoryConstants::gigaByte;

    MockBuffer srcBuffer;
    srcBuffer.size = static_cast<size_t>(bigSize);
    MockBuffer dstBuffer;
    dstBuffer.size = static_cast<size_t>(bigSize);

    srcBuffer.mockGfxAllocation.setAllocationType(AllocationType::buffer);
    dstBuffer.mockGfxAllocation.setAllocationType(AllocationType::bufferHostMemory);

    BuiltinOpParams dc;
    dc.srcMemObj = &srcBuffer;
    dc.dstMemObj = &dstBuffer;
    dc.dstOffset = {static_cast<size_t>(bigOffset), 0, 0};
    dc.size = {static_cast<size_t>(size), 0, 0};

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), dc));

    for (auto &dispatchInfo : multiDispatchInfo) {
        EXPECT_TRUE(dispatchInfo.getKernel()->getDestinationAllocationInSystemMemory());
    }
}

TEST_F(BuiltInTests, givenBigOffsetAndSizeWhenBuilderFillLocalBufferStatelessIsUsedThenParamsAreCorrect) {
    if (is32bit) {
        GTEST_SKIP();
    }

    BuiltinDispatchInfoBuilder &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::fillBufferStateless, *pClDevice);

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t bigOffset = 4ull * MemoryConstants::gigaByte;
    uint64_t size = 4ull * MemoryConstants::gigaByte;

    MockBuffer srcBuffer;
    srcBuffer.size = static_cast<size_t>(bigSize);
    MockBuffer dstBuffer;
    dstBuffer.size = static_cast<size_t>(bigSize);

    srcBuffer.mockGfxAllocation.setAllocationType(AllocationType::bufferHostMemory);
    dstBuffer.mockGfxAllocation.setAllocationType(AllocationType::buffer);

    BuiltinOpParams dc;
    dc.srcMemObj = &srcBuffer;
    dc.dstMemObj = &dstBuffer;
    dc.dstOffset = {static_cast<size_t>(bigOffset), 0, 0};
    dc.size = {static_cast<size_t>(size), 0, 0};

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), dc));

    for (auto &dispatchInfo : multiDispatchInfo) {
        EXPECT_FALSE(dispatchInfo.getKernel()->getDestinationAllocationInSystemMemory());
    }
}

HWTEST_F(BuiltInTests, givenBigOffsetAndSizeWhenBuilderCopyBufferToImageStatelessIsUsedThenParamsAreCorrect) {
    REQUIRE_64BIT_OR_SKIP();
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t bigOffset = 4ull * MemoryConstants::gigaByte;

    MockBuffer srcBuffer;
    srcBuffer.size = static_cast<size_t>(bigSize);
    std ::unique_ptr<Image> pDstImage(Image2dHelper<>::create(pContext));
    ASSERT_NE(nullptr, pDstImage.get());

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferToImage3dStateless, *pClDevice);

    BuiltinOpParams dc;
    dc.srcPtr = &srcBuffer;
    dc.dstMemObj = pDstImage.get();
    dc.srcOffset = {static_cast<size_t>(bigOffset), 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};
    dc.dstRowPitch = 0;
    dc.dstSlicePitch = 0;

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), dc));

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelAttributes.supportsBuffersBiggerThan4Gb());
    EXPECT_FALSE(kernel->getKernelInfo().getArgDescriptorAt(0).as<ArgDescPointer>().isPureStateful());
}

HWTEST_F(BuiltInTests, givenBigOffsetAndSizeWhenBuilderCopyImageToSystemBufferStatelessIsUsedThenParamsAreCorrect) {
    if (is32bit) {
        GTEST_SKIP();
    }

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t bigOffset = 4ull * MemoryConstants::gigaByte;

    MockBuffer dstBuffer;
    dstBuffer.size = static_cast<size_t>(bigSize);
    dstBuffer.mockGfxAllocation.setAllocationType(AllocationType::bufferHostMemory);
    std ::unique_ptr<Image> pSrcImage(Image2dHelper<>::create(pContext));
    ASSERT_NE(nullptr, pSrcImage.get());

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyImage3dToBufferStateless, *pClDevice);

    BuiltinOpParams dc;
    dc.srcMemObj = pSrcImage.get();
    dc.dstMemObj = &dstBuffer;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {static_cast<size_t>(bigOffset), 0, 0};
    dc.size = {1, 1, 1};

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), dc));

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelAttributes.supportsBuffersBiggerThan4Gb());

    for (auto &dispatchInfo : multiDispatchInfo) {
        EXPECT_TRUE(dispatchInfo.getKernel()->getDestinationAllocationInSystemMemory());
    }
}

HWTEST_F(BuiltInTests, givenBigOffsetAndSizeWhenBuilderCopyImageToLocalBufferStatelessIsUsedThenParamsAreCorrect) {

    if (is32bit) {
        GTEST_SKIP();
    }

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t bigOffset = 4ull * MemoryConstants::gigaByte;

    MockBuffer dstBuffer;
    dstBuffer.size = static_cast<size_t>(bigSize);
    dstBuffer.mockGfxAllocation.setAllocationType(AllocationType::buffer);
    std ::unique_ptr<Image> pSrcImage(Image2dHelper<>::create(pContext));
    ASSERT_NE(nullptr, pSrcImage.get());

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyImage3dToBufferStateless, *pClDevice);

    BuiltinOpParams dc;
    dc.srcMemObj = pSrcImage.get();
    dc.dstMemObj = &dstBuffer;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {static_cast<size_t>(bigOffset), 0, 0};
    dc.size = {1, 1, 1};

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), dc));

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelAttributes.supportsBuffersBiggerThan4Gb());

    for (auto &dispatchInfo : multiDispatchInfo) {
        EXPECT_FALSE(dispatchInfo.getKernel()->getDestinationAllocationInSystemMemory());
    }
}

TEST_F(BuiltInTests, GivenUnalignedCopyBufferToBufferWhenDispatchInfoIsCreatedThenParamsAreCorrect) {
    BuiltinDispatchInfoBuilder &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferToBuffer, *pClDevice);

    AlignedBuffer src;
    AlignedBuffer dst;

    BuiltinOpParams builtinOpsParams;

    builtinOpsParams.srcMemObj = &src;
    builtinOpsParams.srcOffset.x = 5; // causes misalignment from 4-byte boundary by 1 byte (8 bits)
    builtinOpsParams.dstMemObj = &dst;
    builtinOpsParams.size = {src.getSize(), 0, 0};

    MultiDispatchInfo multiDispatchInfo(builtinOpsParams);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));

    EXPECT_EQ(1u, multiDispatchInfo.size());

    const Kernel *kernel = multiDispatchInfo.begin()->getKernel();

    EXPECT_EQ(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName, "CopyBufferToBufferMiddleMisaligned");

    const auto crossThreadData = kernel->getCrossThreadData();
    const auto crossThreadOffset = kernel->getKernelInfo().getArgDescriptorAt(4).as<ArgDescValue>().elements[0].offset;
    EXPECT_EQ(8u, *reinterpret_cast<uint32_t *>(ptrOffset(crossThreadData, crossThreadOffset)));

    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), builtinOpsParams));
}

TEST_F(BuiltInTests, GivenReadBufferAlignedWhenDispatchInfoIsCreatedThenParamsAreCorrect) {
    BuiltinDispatchInfoBuilder &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferToBuffer, *pClDevice);

    AlignedBuffer srcMemObj;
    auto size = 10 * MemoryConstants::cacheLineSize;
    auto dstPtr = alignedMalloc(size, MemoryConstants::cacheLineSize);

    BuiltinOpParams builtinOpsParams;

    builtinOpsParams.srcMemObj = &srcMemObj;
    builtinOpsParams.dstPtr = dstPtr;
    builtinOpsParams.size = {size, 0, 0};

    MultiDispatchInfo multiDispatchInfo(builtinOpsParams);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));

    EXPECT_EQ(1u, multiDispatchInfo.size());

    const DispatchInfo *dispatchInfo = multiDispatchInfo.begin();

    EXPECT_EQ(1u, dispatchInfo->getDim());

    size_t leftSize = reinterpret_cast<uintptr_t>(dstPtr) % MemoryConstants::cacheLineSize;
    EXPECT_EQ(0u, leftSize);

    size_t rightSize = (reinterpret_cast<uintptr_t>(dstPtr) + size) % MemoryConstants::cacheLineSize;
    EXPECT_EQ(0u, rightSize);

    size_t middleElSize = sizeof(uint32_t) * 4;
    size_t middleSize = size / middleElSize;
    EXPECT_EQ(Vec3<size_t>(middleSize, 1, 1), dispatchInfo->getGWS());
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), builtinOpsParams));

    for (auto &dispatchInfo : multiDispatchInfo) {
        EXPECT_TRUE(dispatchInfo.getKernel()->getDestinationAllocationInSystemMemory());
    }
    alignedFree(dstPtr);
}

TEST_F(BuiltInTests, GivenWriteBufferAlignedWhenDispatchInfoIsCreatedThenParamsAreCorrect) {
    BuiltinDispatchInfoBuilder &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferToBuffer, *pClDevice);

    auto size = 10 * MemoryConstants::cacheLineSize;
    auto srcPtr = alignedMalloc(size, MemoryConstants::cacheLineSize);
    AlignedBuffer dstMemObj;

    BuiltinOpParams builtinOpsParams;

    builtinOpsParams.srcPtr = srcPtr;
    builtinOpsParams.dstMemObj = &dstMemObj;
    builtinOpsParams.size = {size, 0, 0};

    MultiDispatchInfo multiDispatchInfo(builtinOpsParams);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));

    EXPECT_EQ(1u, multiDispatchInfo.size());

    const DispatchInfo *dispatchInfo = multiDispatchInfo.begin();

    EXPECT_EQ(1u, dispatchInfo->getDim());

    size_t leftSize = reinterpret_cast<uintptr_t>(srcPtr) % MemoryConstants::cacheLineSize;
    EXPECT_EQ(0u, leftSize);

    size_t rightSize = (reinterpret_cast<uintptr_t>(srcPtr) + size) % MemoryConstants::cacheLineSize;
    EXPECT_EQ(0u, rightSize);

    size_t middleElSize = sizeof(uint32_t) * 4;
    size_t middleSize = size / middleElSize;
    EXPECT_EQ(Vec3<size_t>(middleSize, 1, 1), dispatchInfo->getGWS());
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), builtinOpsParams));
    alignedFree(srcPtr);
}

TEST_F(BuiltInTests, WhenGettingBuilderInfoTwiceThenPointerIsSame) {
    BuiltinDispatchInfoBuilder &builder1 = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferToBuffer, *pClDevice);
    BuiltinDispatchInfoBuilder &builder2 = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferToBuffer, *pClDevice);

    EXPECT_EQ(&builder1, &builder2);
}

TEST_F(BuiltInTests, GivenUnknownBuiltInOpWhenGettingBuilderInfoThenExceptionThrown) {
    EXPECT_THROW(
        BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::count, *pClDevice),
        std::runtime_error);
}

TEST_F(BuiltInTests, GivenUnsupportedBuildTypeWhenBuildingDispatchInfoThenFalseIsReturned) {
    auto &builtIns = *pDevice->getBuiltIns();
    BuiltinDispatchInfoBuilder dispatchInfoBuilder{builtIns, *pClDevice};
    BuiltinOpParams params;

    MultiDispatchInfo multiDispatchInfo(params);
    auto ret = dispatchInfoBuilder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_FALSE(ret);
    ASSERT_EQ(0U, multiDispatchInfo.size());

    ret = dispatchInfoBuilder.buildDispatchInfos(multiDispatchInfo, nullptr, 0, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
    EXPECT_FALSE(ret);
    EXPECT_EQ(0U, multiDispatchInfo.size());
}

TEST_F(BuiltInTests, GivenDefaultBuiltinDispatchInfoBuilderWhenValidateDispatchIsCalledThenClSuccessIsReturned) {
    auto &builtIns = *pDevice->getBuiltIns();
    BuiltinDispatchInfoBuilder dispatchInfoBuilder{builtIns, *pClDevice};
    auto ret = dispatchInfoBuilder.validateDispatch(nullptr, 1, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
    EXPECT_EQ(CL_SUCCESS, ret);
}

TEST_F(BuiltInTests, WhenSettingExplictArgThenTrueIsReturned) {
    auto &builtIns = *pDevice->getBuiltIns();
    BuiltinDispatchInfoBuilder dispatchInfoBuilder{builtIns, *pClDevice};
    MultiDispatchInfo multiDispatchInfo;
    BuiltinOpParams params;

    cl_int err;
    auto ret = dispatchInfoBuilder.setExplicitArg(1, 5, nullptr, err);
    EXPECT_TRUE(ret);
}

TEST_F(VmeBuiltInTests, GivenVmeBuilderWhenGettingDispatchInfoThenValidPointerIsReturned) {
    overwriteBuiltInBinaryName("media_kernels_backend");

    EBuiltInOps::Type vmeOps[] = {EBuiltInOps::vmeBlockMotionEstimateIntel, EBuiltInOps::vmeBlockAdvancedMotionEstimateCheckIntel, EBuiltInOps::vmeBlockAdvancedMotionEstimateBidirectionalCheckIntel};
    for (auto op : vmeOps) {
        BuiltinDispatchInfoBuilder &builder = Vme::getBuiltinDispatchInfoBuilder(op, *pClDevice);
        EXPECT_NE(nullptr, &builder);
    }

    restoreBuiltInBinaryName();
}

TEST_F(VmeBuiltInTests, givenInvalidBuiltInOpWhenGetVmeBuilderInfoThenExceptionIsThrown) {
    EXPECT_THROW(Vme::getBuiltinDispatchInfoBuilder(EBuiltInOps::count, *pClDevice), std::exception);
}

TEST_F(VmeBuiltInTests, GivenVmeBuilderAndInvalidParamsWhenGettingDispatchInfoThenEmptyKernelIsReturned) {
    overwriteBuiltInBinaryName("media_kernels_backend");
    EBuiltInOps::Type vmeOps[] = {EBuiltInOps::vmeBlockMotionEstimateIntel, EBuiltInOps::vmeBlockAdvancedMotionEstimateCheckIntel, EBuiltInOps::vmeBlockAdvancedMotionEstimateBidirectionalCheckIntel};
    for (auto op : vmeOps) {
        BuiltinDispatchInfoBuilder &builder = Vme::getBuiltinDispatchInfoBuilder(op, *pClDevice);

        MultiDispatchInfo outMdi;
        Vec3<size_t> gws{352, 288, 0};
        Vec3<size_t> elws{0, 0, 0};
        Vec3<size_t> offset{0, 0, 0};
        auto ret = builder.buildDispatchInfos(outMdi, nullptr, 0, gws, elws, offset);
        EXPECT_FALSE(ret);
        EXPECT_EQ(0U, outMdi.size());
    }
    restoreBuiltInBinaryName();
}

TEST_F(VmeBuiltInTests, GivenVmeBuilderWhenGettingDispatchInfoThenParamsAreCorrect) {
    MockKernelWithInternals mockKernel{*pClDevice};
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.simdSize = 16;
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0] = 16;
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1] = 0;
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2] = 0;

    overwriteBuiltInBinaryName("media_kernels_backend");
    BuiltinDispatchInfoBuilder &builder = Vme::getBuiltinDispatchInfoBuilder(EBuiltInOps::vmeBlockMotionEstimateIntel, *pClDevice);
    restoreBuiltInBinaryName();

    MultiDispatchInfo outMdi;
    Vec3<size_t> gws{352, 288, 0};
    Vec3<size_t> elws{0, 0, 0};
    Vec3<size_t> offset{16, 0, 0};

    MockBuffer mb;
    cl_mem bufferArg = static_cast<cl_mem>(&mb);

    cl_int err;
    constexpr uint32_t bufferArgNum = 3;
    bool ret = builder.setExplicitArg(bufferArgNum, sizeof(cl_mem), &bufferArg, err);
    EXPECT_FALSE(ret);
    EXPECT_EQ(CL_SUCCESS, err);

    ret = builder.buildDispatchInfos(outMdi, mockKernel.mockKernel, 0, gws, elws, offset);
    EXPECT_TRUE(ret);
    EXPECT_EQ(1U, outMdi.size());

    auto outDi = outMdi.begin();
    EXPECT_EQ(Vec3<size_t>(352, 1, 1), outDi->getGWS());
    EXPECT_EQ(Vec3<size_t>(16, 1, 1), outDi->getEnqueuedWorkgroupSize());
    EXPECT_EQ(Vec3<size_t>(16, 0, 0), outDi->getOffset());
    EXPECT_NE(mockKernel.mockKernel, outDi->getKernel());

    EXPECT_EQ(bufferArg, outDi->getKernel()->getKernelArg(bufferArgNum));

    constexpr uint32_t vmeImplicitArgsBase = 6;
    constexpr uint32_t vmeImplicitArgs = 3;
    ASSERT_EQ(vmeImplicitArgsBase + vmeImplicitArgs, outDi->getKernel()->getKernelInfo().kernelDescriptor.payloadMappings.explicitArgs.size());
    uint32_t vmeExtraArgsExpectedVals[] = {18, 22, 18}; // height, width, stride
    for (uint32_t i = 0; i < vmeImplicitArgs; ++i) {
        auto &argAsVal = outDi->getKernel()->getKernelInfo().getArgDescriptorAt(vmeImplicitArgsBase + i).as<ArgDescValue>();
        EXPECT_EQ(vmeExtraArgsExpectedVals[i], *((uint32_t *)(outDi->getKernel()->getCrossThreadData() + argAsVal.elements[0].offset)));
    }
}

TEST_F(VmeBuiltInTests, GivenAdvancedVmeBuilderWhenGettingDispatchInfoThenParamsAreCorrect) {
    MockKernelWithInternals mockKernel{*pClDevice};
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.simdSize = 16;
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0] = 16;
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1] = 0;
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2] = 0;

    Vec3<size_t> gws{352, 288, 0};
    Vec3<size_t> elws{0, 0, 0};
    Vec3<size_t> offset{0, 0, 0};

    cl_int err;

    constexpr uint32_t bufferArgNum = 7;
    MockBuffer mb;
    cl_mem bufferArg = static_cast<cl_mem>(&mb);

    constexpr uint32_t srcImageArgNum = 1;
    auto image = std::unique_ptr<Image>(Image2dHelper<>::create(pContext));
    cl_mem srcImageArg = static_cast<cl_mem>(image.get());

    EBuiltInOps::Type vmeOps[] = {EBuiltInOps::vmeBlockAdvancedMotionEstimateCheckIntel, EBuiltInOps::vmeBlockAdvancedMotionEstimateBidirectionalCheckIntel};
    for (auto op : vmeOps) {
        MultiDispatchInfo outMdi;
        overwriteBuiltInBinaryName("media_kernels_backend");
        BuiltinDispatchInfoBuilder &builder = Vme::getBuiltinDispatchInfoBuilder(op, *pClDevice);
        restoreBuiltInBinaryName();

        bool ret = builder.setExplicitArg(srcImageArgNum, sizeof(cl_mem), &srcImageArg, err);
        EXPECT_FALSE(ret);
        EXPECT_EQ(CL_SUCCESS, err);

        ret = builder.setExplicitArg(bufferArgNum, sizeof(cl_mem), &bufferArg, err);
        EXPECT_FALSE(ret);
        EXPECT_EQ(CL_SUCCESS, err);

        ret = builder.buildDispatchInfos(outMdi, mockKernel.mockKernel, 0, gws, elws, offset);
        EXPECT_TRUE(ret);
        EXPECT_EQ(1U, outMdi.size());

        auto outDi = outMdi.begin();
        EXPECT_EQ(Vec3<size_t>(352, 1, 1), outDi->getGWS());
        EXPECT_EQ(Vec3<size_t>(16, 1, 1), outDi->getEnqueuedWorkgroupSize());
        EXPECT_NE(mockKernel.mockKernel, outDi->getKernel());

        EXPECT_EQ(srcImageArg, outDi->getKernel()->getKernelArg(srcImageArgNum));

        uint32_t vmeImplicitArgsBase = outDi->getKernel()->getKernelInfo().getArgNumByName("intraSrcImg");
        uint32_t vmeImplicitArgs = 4;
        ASSERT_EQ(vmeImplicitArgsBase + vmeImplicitArgs, outDi->getKernel()->getKernelInfo().getExplicitArgs().size());
        EXPECT_EQ(srcImageArg, outDi->getKernel()->getKernelArg(vmeImplicitArgsBase));
        ++vmeImplicitArgsBase;
        --vmeImplicitArgs;
        uint32_t vmeExtraArgsExpectedVals[] = {18, 22, 18}; // height, width, stride
        for (uint32_t i = 0; i < vmeImplicitArgs; ++i) {
            auto &argAsVal = outDi->getKernel()->getKernelInfo().getArgDescriptorAt(vmeImplicitArgsBase + i).as<ArgDescValue>();
            EXPECT_EQ(vmeExtraArgsExpectedVals[i], *((uint32_t *)(outDi->getKernel()->getCrossThreadData() + argAsVal.elements[0].offset)));
        }
    }
}

TEST_F(VmeBuiltInTests, WhenGettingBuiltinAsStringThenCorrectStringIsReturned) {
    EXPECT_EQ(0, strcmp("aux_translation.builtin_kernel", getBuiltinAsString(EBuiltInOps::auxTranslation)));
    EXPECT_EQ(0, strcmp("copy_buffer_to_buffer.builtin_kernel", getBuiltinAsString(EBuiltInOps::copyBufferToBuffer)));
    EXPECT_EQ(0, strcmp("copy_buffer_rect.builtin_kernel", getBuiltinAsString(EBuiltInOps::copyBufferRect)));
    EXPECT_EQ(0, strcmp("fill_buffer.builtin_kernel", getBuiltinAsString(EBuiltInOps::fillBuffer)));
    EXPECT_EQ(0, strcmp("copy_buffer_to_image3d.builtin_kernel", getBuiltinAsString(EBuiltInOps::copyBufferToImage3d)));
    EXPECT_EQ(0, strcmp("copy_image3d_to_buffer.builtin_kernel", getBuiltinAsString(EBuiltInOps::copyImage3dToBuffer)));
    EXPECT_EQ(0, strcmp("copy_image_to_image1d.builtin_kernel", getBuiltinAsString(EBuiltInOps::copyImageToImage1d)));
    EXPECT_EQ(0, strcmp("copy_image_to_image2d.builtin_kernel", getBuiltinAsString(EBuiltInOps::copyImageToImage2d)));
    EXPECT_EQ(0, strcmp("copy_image_to_image3d.builtin_kernel", getBuiltinAsString(EBuiltInOps::copyImageToImage3d)));
    EXPECT_EQ(0, strcmp("copy_kernel_timestamps.builtin_kernel", getBuiltinAsString(EBuiltInOps::queryKernelTimestamps)));
    EXPECT_EQ(0, strcmp("fill_image1d.builtin_kernel", getBuiltinAsString(EBuiltInOps::fillImage1d)));
    EXPECT_EQ(0, strcmp("fill_image2d.builtin_kernel", getBuiltinAsString(EBuiltInOps::fillImage2d)));
    EXPECT_EQ(0, strcmp("fill_image3d.builtin_kernel", getBuiltinAsString(EBuiltInOps::fillImage3d)));
    EXPECT_EQ(0, strcmp("vme_block_motion_estimate_intel.builtin_kernel", getBuiltinAsString(EBuiltInOps::vmeBlockMotionEstimateIntel)));
    EXPECT_EQ(0, strcmp("vme_block_advanced_motion_estimate_check_intel.builtin_kernel", getBuiltinAsString(EBuiltInOps::vmeBlockAdvancedMotionEstimateCheckIntel)));
    EXPECT_EQ(0, strcmp("vme_block_advanced_motion_estimate_bidirectional_check_intel", getBuiltinAsString(EBuiltInOps::vmeBlockAdvancedMotionEstimateBidirectionalCheckIntel)));
    EXPECT_EQ(0, strcmp("unknown", getBuiltinAsString(EBuiltInOps::count)));
}

TEST_F(BuiltInTests, GivenEncodeTypeWhenGettingExtensionThenCorrectStringIsReturned) {
    EXPECT_EQ(0, strcmp("", BuiltinCode::getExtension(BuiltinCode::ECodeType::any)));
    EXPECT_EQ(0, strcmp(".bin", BuiltinCode::getExtension(BuiltinCode::ECodeType::binary)));
    EXPECT_EQ(0, strcmp(".spv", BuiltinCode::getExtension(BuiltinCode::ECodeType::intermediate)));
    EXPECT_EQ(0, strcmp(".cl", BuiltinCode::getExtension(BuiltinCode::ECodeType::source)));
    EXPECT_EQ(0, strcmp("", BuiltinCode::getExtension(BuiltinCode::ECodeType::count)));
    EXPECT_EQ(0, strcmp("", BuiltinCode::getExtension(BuiltinCode::ECodeType::invalid)));
}

TEST_F(BuiltInTests, GivenBuiltinResourceWhenCreatingBuiltinResourceThenSizesAreEqual) {
    std::string resource = "__kernel";

    auto br1 = createBuiltinResource(resource.data(), resource.size());
    EXPECT_NE(0u, br1.size());
    auto br2 = createBuiltinResource(br1);
    EXPECT_NE(0u, br2.size());

    EXPECT_EQ(br1, br2);
}

TEST_F(BuiltInTests, WhenJoiningPathThenPathsAreJoinedWithCorrectSeparator) {
    std::string resourceName = "copy_buffer_to_buffer.builtin_kernel.cl";
    std::string resourcePath = "path";

    EXPECT_EQ(0, strcmp(resourceName.c_str(), joinPath("", resourceName).c_str()));
    EXPECT_EQ(0, strcmp(resourcePath.c_str(), joinPath(resourcePath, "").c_str()));
    EXPECT_EQ(0, strcmp((resourcePath + PATH_SEPARATOR + resourceName).c_str(), joinPath(resourcePath + PATH_SEPARATOR, resourceName).c_str()));
    EXPECT_EQ(0, strcmp((resourcePath + PATH_SEPARATOR + resourceName).c_str(), joinPath(resourcePath, resourceName).c_str()));
}

TEST_F(BuiltInTests, GivenFileNameWhenGettingKernelFromEmbeddedStorageRegistryThenValidPtrIsReturnedForExisitngKernels) {
    class MockEmbeddedStorageRegistry : public EmbeddedStorageRegistry {
        using EmbeddedStorageRegistry::EmbeddedStorageRegistry;
    };
    MockEmbeddedStorageRegistry storageRegistry;

    std::string resource = "__kernel";
    storageRegistry.store("kernel.cl", createBuiltinResource(resource.data(), resource.size() + 1));

    const BuiltinResourceT *br = storageRegistry.get("kernel.cl");
    EXPECT_NE(nullptr, br);
    EXPECT_EQ(0, strcmp(resource.data(), br->data()));

    const BuiltinResourceT *bnr = storageRegistry.get("unknown.cl");
    EXPECT_EQ(nullptr, bnr);
}

TEST_F(BuiltInTests, WhenStoringRootPathThenPathIsSavedCorrectly) {
    class MockStorage : Storage {
      public:
        MockStorage(const std::string &rootPath) : Storage(rootPath){};
        std::string &getRootPath() {
            return Storage::rootPath;
        }

      protected:
        BuiltinResourceT loadImpl(const std::string &fullResourceName) override {
            BuiltinResourceT ret;
            return ret;
        }
    };
    const std::string rootPath("root");
    MockStorage mockStorage(rootPath);
    EXPECT_EQ(0, strcmp(rootPath.data(), mockStorage.getRootPath().data()));
}

TEST_F(BuiltInTests, GivenFiledNameWhenLoadingImplKernelFromEmbeddedStorageRegistryThenValidPtrIsReturnedForExisitngKernels) {
    class MockEmbeddedStorage : EmbeddedStorage {
      public:
        MockEmbeddedStorage(const std::string &rootPath) : EmbeddedStorage(rootPath){};
        BuiltinResourceT loadImpl(const std::string &fullResourceName) override {
            return EmbeddedStorage::loadImpl(fullResourceName);
        }
    };
    MockEmbeddedStorage mockEmbeddedStorage("root");

    BuiltinResourceT br = mockEmbeddedStorage.loadImpl("copy_buffer_to_buffer.builtin_kernel.cl");
    EXPECT_NE(0u, br.size());

    BuiltinResourceT bnr = mockEmbeddedStorage.loadImpl("unknown.cl");
    EXPECT_EQ(0u, bnr.size());
}

TEST_F(BuiltInTests, GivenFiledNameWhenLoadingImplKernelFromFileStorageThenValidPtrIsReturnedForExisitngKernels) {
    class MockFileStorage : FileStorage {
      public:
        MockFileStorage(const std::string &rootPath) : FileStorage(rootPath){};
        BuiltinResourceT loadImpl(const std::string &fullResourceName) override {
            return FileStorage::loadImpl(fullResourceName);
        }
    };
    MockFileStorage mockEmbeddedStorage("root");

    BuiltinResourceT br = mockEmbeddedStorage.loadImpl(clFiles + "copybuffer.cl");
    EXPECT_NE(0u, br.size());

    BuiltinResourceT bnr = mockEmbeddedStorage.loadImpl("unknown.cl");
    EXPECT_EQ(0u, bnr.size());
}

TEST_F(BuiltInTests, WhenBuiltinsLibIsCreatedThenAllStoragesSizeIsTwo) {
    auto mockBuiltinsLib = std::unique_ptr<MockBuiltinsLib>(new MockBuiltinsLib());
    EXPECT_EQ(2u, mockBuiltinsLib->allStorages.size());
}

TEST_F(BuiltInTests, GivenTypeAnyWhenGettingBuiltinCodeThenCorrectBuiltinReturned) {
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    BuiltinCode code = builtinsLib->getBuiltinCode(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::any, *pDevice);
    EXPECT_EQ(BuiltinCode::ECodeType::binary, code.type);
    EXPECT_NE(0u, code.resource.size());
    EXPECT_EQ(pDevice, code.targetDevice);
}

TEST_F(BuiltInTests, GivenTypeBinaryWhenGettingBuiltinCodeThenCorrectBuiltinReturned) {
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    BuiltinCode code = builtinsLib->getBuiltinCode(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::binary, *pDevice);
    EXPECT_EQ(BuiltinCode::ECodeType::binary, code.type);
    EXPECT_NE(0u, code.resource.size());
    EXPECT_EQ(pDevice, code.targetDevice);
}

TEST_F(BuiltInTests, GivenTypeIntermediateWhenGettingBuiltinCodeThenCorrectBuiltinReturned) {
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    BuiltinCode code = builtinsLib->getBuiltinCode(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::intermediate, *pDevice);
    EXPECT_EQ(BuiltinCode::ECodeType::intermediate, code.type);
    EXPECT_EQ(0u, code.resource.size());
    EXPECT_EQ(pDevice, code.targetDevice);
}

TEST_F(BuiltInTests, GivenTypeSourceWhenGettingBuiltinCodeThenCorrectBuiltinReturned) {
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    BuiltinCode code = builtinsLib->getBuiltinCode(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::source, *pDevice);
    EXPECT_EQ(BuiltinCode::ECodeType::source, code.type);
    EXPECT_NE(0u, code.resource.size());
    EXPECT_EQ(pDevice, code.targetDevice);
}

TEST_F(BuiltInTests, GivenTypeInvalidWhenGettingBuiltinCodeThenKernelIsEmpty) {
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    BuiltinCode code = builtinsLib->getBuiltinCode(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::invalid, *pDevice);
    EXPECT_EQ(BuiltinCode::ECodeType::invalid, code.type);
    EXPECT_EQ(0u, code.resource.size());
    EXPECT_EQ(pDevice, code.targetDevice);
}

TEST_F(BuiltInTests, GivenBuiltinTypeSourceWhenGettingBuiltinResourceThenResourceSizeIsNonZero) {
    auto mockBuiltinsLib = std::unique_ptr<MockBuiltinsLib>(new MockBuiltinsLib());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::auxTranslation, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::copyBufferRect, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::fillBuffer, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::copyBufferToImage3d, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::copyImage3dToBuffer, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::copyImageToImage1d, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::copyImageToImage2d, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::copyImageToImage3d, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::fillImage1d, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::fillImage2d, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::fillImage3d, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::vmeBlockMotionEstimateIntel, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::vmeBlockAdvancedMotionEstimateCheckIntel, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::vmeBlockAdvancedMotionEstimateBidirectionalCheckIntel, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::count, BuiltinCode::ECodeType::source, *pDevice).size());
}

HWCMDTEST_F(IGFX_GEN8_CORE, BuiltInTests, GivenBuiltinTypeBinaryWhenGettingBuiltinResourceThenResourceSizeIsNonZero) {
    auto mockBuiltinsLib = std::unique_ptr<MockBuiltinsLib>(new MockBuiltinsLib());

    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::copyBufferRect, BuiltinCode::ECodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::fillBuffer, BuiltinCode::ECodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::copyBufferToImage3d, BuiltinCode::ECodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::copyImage3dToBuffer, BuiltinCode::ECodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::copyImageToImage1d, BuiltinCode::ECodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::copyImageToImage2d, BuiltinCode::ECodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::copyImageToImage3d, BuiltinCode::ECodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::fillImage1d, BuiltinCode::ECodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::fillImage2d, BuiltinCode::ECodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::fillImage3d, BuiltinCode::ECodeType::binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::vmeBlockMotionEstimateIntel, BuiltinCode::ECodeType::binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::vmeBlockAdvancedMotionEstimateCheckIntel, BuiltinCode::ECodeType::binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::vmeBlockAdvancedMotionEstimateBidirectionalCheckIntel, BuiltinCode::ECodeType::binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::count, BuiltinCode::ECodeType::binary, *pDevice).size());
}

TEST_F(BuiltInTests, GivenBuiltinTypeSourceWhenGettingBuiltinResourceForNotRegisteredRevisionThenResourceSizeIsNonZero) {
    pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->platform.usRevId += 0xdead;
    auto mockBuiltinsLib = std::unique_ptr<MockBuiltinsLib>(new MockBuiltinsLib());

    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::copyBufferRect, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::fillBuffer, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::copyBufferToImage3d, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::copyImage3dToBuffer, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::copyImageToImage1d, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::copyImageToImage2d, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::copyImageToImage3d, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::fillImage1d, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::fillImage2d, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::fillImage3d, BuiltinCode::ECodeType::source, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::count, BuiltinCode::ECodeType::source, *pDevice).size());
}

TEST_F(BuiltInTests, GivenTypeAnyWhenCreatingProgramFromCodeThenValidPointerIsReturned) {
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    const BuiltinCode bc = builtinsLib->getBuiltinCode(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::any, *pDevice);
    EXPECT_NE(0u, bc.resource.size());
    auto program = std::unique_ptr<Program>(BuiltinDispatchInfoBuilder::createProgramFromCode(bc, toClDeviceVector(*pClDevice)));
    EXPECT_NE(nullptr, program.get());
}

TEST_F(BuiltInTests, GivenTypeSourceWhenCreatingProgramFromCodeThenValidPointerIsReturned) {
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    const BuiltinCode bc = builtinsLib->getBuiltinCode(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::source, *pDevice);
    EXPECT_NE(0u, bc.resource.size());
    auto program = std::unique_ptr<Program>(BuiltinDispatchInfoBuilder::createProgramFromCode(bc, toClDeviceVector(*pClDevice)));
    EXPECT_NE(nullptr, program.get());
}

TEST_F(BuiltInTests, givenCreateProgramFromSourceWhenForceToStatelessRequiredOr32BitThenInternalOptionsHasGreaterThan4gbBuffersRequired) {
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());

    const BuiltinCode bc = builtinsLib->getBuiltinCode(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::source, *pDevice);
    EXPECT_NE(0u, bc.resource.size());
    auto program = std::unique_ptr<Program>(BuiltinDispatchInfoBuilder::createProgramFromCode(bc, toClDeviceVector(*pClDevice)));
    EXPECT_NE(nullptr, program.get());
    auto builtinInternalOptions = program->getInternalOptions();

    const auto &compilerProductHelper = pDevice->getRootDeviceEnvironment().getHelper<CompilerProductHelper>();
    if (compilerProductHelper.isForceToStatelessRequired() || is32bit) {
        EXPECT_TRUE(hasSubstr(builtinInternalOptions, std::string(CompilerOptions::greaterThan4gbBuffersRequired)));
    } else {
        EXPECT_FALSE(hasSubstr(builtinInternalOptions, std::string(CompilerOptions::greaterThan4gbBuffersRequired)));
    }
}

TEST_F(BuiltInTests, GivenTypeIntermediateWhenCreatingProgramFromCodeThenNullPointerIsReturned) {
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    const BuiltinCode bc = builtinsLib->getBuiltinCode(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::intermediate, *pDevice);
    EXPECT_EQ(0u, bc.resource.size());
    auto program = std::unique_ptr<Program>(BuiltinDispatchInfoBuilder::createProgramFromCode(bc, toClDeviceVector(*pClDevice)));
    EXPECT_EQ(nullptr, program.get());
}

TEST_F(BuiltInTests, GivenTypeBinaryWhenCreatingProgramFromCodeThenValidPointerIsReturned) {
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    const BuiltinCode bc = builtinsLib->getBuiltinCode(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::binary, *pDevice);
    EXPECT_NE(0u, bc.resource.size());
    auto program = std::unique_ptr<Program>(BuiltinDispatchInfoBuilder::createProgramFromCode(bc, toClDeviceVector(*pClDevice)));
    EXPECT_NE(nullptr, program.get());
}

TEST_F(BuiltInTests, GivenTypeInvalidWhenCreatingProgramFromCodeThenNullPointerIsReturned) {
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    const BuiltinCode bc = builtinsLib->getBuiltinCode(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::invalid, *pDevice);
    EXPECT_EQ(0u, bc.resource.size());
    auto program = std::unique_ptr<Program>(BuiltinDispatchInfoBuilder::createProgramFromCode(bc, toClDeviceVector(*pClDevice)));
    EXPECT_EQ(nullptr, program.get());
}

TEST_F(BuiltInTests, GivenInvalidBuiltinWhenCreatingProgramFromCodeThenNullPointerIsReturned) {
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    const BuiltinCode bc = builtinsLib->getBuiltinCode(EBuiltInOps::count, BuiltinCode::ECodeType::any, *pDevice);
    EXPECT_EQ(0u, bc.resource.size());
    auto program = std::unique_ptr<Program>(BuiltinDispatchInfoBuilder::createProgramFromCode(bc, toClDeviceVector(*pClDevice)));
    EXPECT_EQ(nullptr, program.get());
}

TEST_F(BuiltInTests, GivenForce32bitWhenCreatingProgramThenCorrectKernelIsCreated) {
    bool force32BitAddressess = pDevice->getDeviceInfo().force32BitAddressess;
    const_cast<DeviceInfo *>(&pDevice->getDeviceInfo())->force32BitAddressess = true;

    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    const BuiltinCode bc = builtinsLib->getBuiltinCode(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::source, *pDevice);
    ASSERT_NE(0u, bc.resource.size());
    auto program = std::unique_ptr<Program>(BuiltinDispatchInfoBuilder::createProgramFromCode(bc, toClDeviceVector(*pClDevice)));
    ASSERT_NE(nullptr, program.get());
    auto builtinInternalOptions = program->getInternalOptions();
    auto it = builtinInternalOptions.find(NEO::CompilerOptions::arch32bit.data());
    EXPECT_EQ(std::string::npos, it);

    it = builtinInternalOptions.find(NEO::CompilerOptions::greaterThan4gbBuffersRequired.data());
    const auto &compilerProductHelper = pDevice->getRootDeviceEnvironment().getHelper<CompilerProductHelper>();

    if (is32bit || compilerProductHelper.isForceToStatelessRequired()) {
        EXPECT_NE(std::string::npos, it);
    } else {
        EXPECT_EQ(std::string::npos, it);
    }

    const_cast<DeviceInfo *>(&pDevice->getDeviceInfo())->force32BitAddressess = force32BitAddressess;
}

TEST_F(BuiltInTests, GivenVmeKernelWhenGettingDeviceInfoThenCorrectVmeVersionIsReturned) {
    if (!pDevice->getHardwareInfo().capabilityTable.supportsVme) {
        GTEST_SKIP();
    }
    cl_uint param;
    auto ret = pClDevice->getDeviceInfo(CL_DEVICE_ME_VERSION_INTEL, sizeof(param), &param, nullptr);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_EQ(static_cast<cl_uint>(CL_ME_VERSION_ADVANCED_VER_2_INTEL), param);
}

TEST_F(VmeBuiltInTests, WhenVmeKernelIsCreatedThenParamsAreCorrect) {
    overwriteBuiltInBinaryName("media_kernels_backend");
    BuiltInOp<EBuiltInOps::vmeBlockAdvancedMotionEstimateCheckIntel> vmeBuilder(*this->pBuiltIns, *pClDevice);
    restoreBuiltInBinaryName();

    cl_int err;

    {
        int32_t bufArgNum = 7;

        cl_mem mem = 0;
        vmeBuilder.setExplicitArg(bufArgNum, sizeof(cl_mem), &mem, err);
        EXPECT_TRUE(vmeBuilder.validateBufferSize(-1, 16));
        EXPECT_TRUE(vmeBuilder.validateBufferSize(bufArgNum, 16));

        MockBuffer mb;
        mem = &mb;
        vmeBuilder.setExplicitArg(bufArgNum, sizeof(cl_mem), &mem, err);
        EXPECT_TRUE(vmeBuilder.validateBufferSize(bufArgNum, mb.getSize()));
        EXPECT_TRUE(vmeBuilder.validateBufferSize(bufArgNum, mb.getSize() / 2));
        EXPECT_FALSE(vmeBuilder.validateBufferSize(bufArgNum, mb.getSize() * 2));

        mem = 0;
        vmeBuilder.setExplicitArg(bufArgNum, sizeof(cl_mem), &mem, err);
    }

    {
        EXPECT_TRUE(vmeBuilder.validateEnumVal(1, 1, 2, 3, 4));
        EXPECT_TRUE(vmeBuilder.validateEnumVal(1, 1));
        EXPECT_TRUE(vmeBuilder.validateEnumVal(3, 1, 2, 3));

        EXPECT_FALSE(vmeBuilder.validateEnumVal(1, 3, 4));
        EXPECT_FALSE(vmeBuilder.validateEnumVal(1));
        EXPECT_FALSE(vmeBuilder.validateEnumVal(1, 2));

        int32_t valArgNum = 3;
        uint32_t val = 7;
        vmeBuilder.setExplicitArg(valArgNum, sizeof(val), &val, err);
        EXPECT_FALSE(vmeBuilder.validateEnumArg<uint32_t>(valArgNum, 3));
        EXPECT_TRUE(vmeBuilder.validateEnumArg<uint32_t>(valArgNum, 7));

        val = 0;
        vmeBuilder.setExplicitArg(valArgNum, sizeof(val), &val, err);
    }

    {
        int32_t valArgNum = 3;
        uint32_t val = 7;
        vmeBuilder.setExplicitArg(valArgNum, sizeof(val), &val, err);
        EXPECT_EQ(val, vmeBuilder.getKernelArgByValValue<uint32_t>(valArgNum));
        val = 11;
        vmeBuilder.setExplicitArg(valArgNum, sizeof(val), &val, err);
        EXPECT_EQ(val, vmeBuilder.getKernelArgByValValue<uint32_t>(valArgNum));

        val = 0;
        vmeBuilder.setExplicitArg(valArgNum, sizeof(val), &val, err);
    }
}

TEST_F(VmeBuiltInTests, WhenVmeKernelIsCreatedThenDispatchIsBidirectional) {
    overwriteBuiltInBinaryName("media_kernels_backend");
    BuiltInOp<EBuiltInOps::vmeBlockAdvancedMotionEstimateCheckIntel> avmeBuilder(*this->pBuiltIns, *pClDevice);
    BuiltInOp<EBuiltInOps::vmeBlockAdvancedMotionEstimateBidirectionalCheckIntel> avmeBidirBuilder(*this->pBuiltIns, *pClDevice);
    restoreBuiltInBinaryName();

    EXPECT_FALSE(avmeBuilder.isBidirKernel());
    EXPECT_TRUE(avmeBidirBuilder.isBidirKernel());
}

struct ImageVmeValidFormat : Image2dDefaults {
    static const cl_image_format imageFormat;
    static const cl_image_desc iamgeDesc;
};

const cl_image_format ImageVmeValidFormat::imageFormat = {
    CL_R,
    CL_UNORM_INT8};

const cl_image_desc ImageVmeValidFormat::iamgeDesc = {
    CL_MEM_OBJECT_IMAGE1D,
    8192,
    16,
    1,
    1,
    0,
    0,
    0,
    0,
    {nullptr}};

struct ImageVmeInvalidDataType : Image2dDefaults {
    static const cl_image_format imageFormat;
};

const cl_image_format ImageVmeInvalidDataType::imageFormat = {
    CL_R,
    CL_FLOAT};

struct ImageVmeInvalidChannelOrder : Image2dDefaults {
    static const cl_image_format imageFormat;
};

const cl_image_format ImageVmeInvalidChannelOrder::imageFormat = {
    CL_RGBA,
    CL_UNORM_INT8};

TEST_F(VmeBuiltInTests, WhenValidatingImagesThenCorrectResponses) {
    overwriteBuiltInBinaryName("media_kernels_backend");
    BuiltInOp<EBuiltInOps::vmeBlockMotionEstimateIntel> vmeBuilder(*this->pBuiltIns, *pClDevice);
    restoreBuiltInBinaryName();

    uint32_t srcImgArgNum = 1;
    uint32_t refImgArgNum = 2;

    cl_int err;
    { // validate images are not null
        std::unique_ptr<Image> image1(ImageHelper<ImageVmeValidFormat>::create(pContext));

        cl_mem srcImgMem = 0;
        EXPECT_EQ(CL_INVALID_KERNEL_ARGS, vmeBuilder.validateImages(Vec3<size_t>{3, 3, 0}, Vec3<size_t>{0, 0, 0}));

        srcImgMem = image1.get();
        vmeBuilder.setExplicitArg(srcImgArgNum, sizeof(srcImgMem), &srcImgMem, err);
        EXPECT_EQ(CL_INVALID_KERNEL_ARGS, vmeBuilder.validateImages(Vec3<size_t>{3, 3, 0}, Vec3<size_t>{0, 0, 0}));
    }

    { // validate image formats
        std::unique_ptr<Image> imageValid(ImageHelper<ImageVmeValidFormat>::create(pContext));
        std::unique_ptr<Image> imageInvalidDataType(ImageHelper<ImageVmeInvalidDataType>::create(pContext));
        std::unique_ptr<Image> imageChannelOrder(ImageHelper<ImageVmeInvalidChannelOrder>::create(pContext));

        Image *images[] = {imageValid.get(), imageInvalidDataType.get(), imageChannelOrder.get()};
        for (Image *srcImg : images) {
            for (Image *dstImg : images) {
                cl_mem srcImgMem = srcImg;
                cl_mem refImgMem = dstImg;
                vmeBuilder.setExplicitArg(srcImgArgNum, sizeof(srcImgMem), &srcImgMem, err);
                vmeBuilder.setExplicitArg(refImgArgNum, sizeof(refImgMem), &refImgMem, err);
                bool shouldSucceed = (srcImg == imageValid.get()) && (dstImg == imageValid.get());
                if (shouldSucceed) {
                    EXPECT_EQ(CL_SUCCESS, vmeBuilder.validateImages(Vec3<size_t>{1, 1, 0}, Vec3<size_t>{0, 0, 0}));
                } else {
                    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, vmeBuilder.validateImages(Vec3<size_t>{1, 1, 0}, Vec3<size_t>{0, 0, 0}));
                }
            }
        }
    }

    { // validate image tiling
        std::unique_ptr<Image> imageValid(ImageHelper<ImageVmeValidFormat>::create(pContext));
        DebugManagerStateRestore restorer;
        debugManager.flags.ForceLinearImages.set(true);
        std::unique_ptr<Image> imageLinear(ImageHelper<ImageVmeValidFormat>::create(pContext));
        Image *images[] = {imageValid.get(), imageLinear.get()};
        for (Image *srcImg : images) {
            for (Image *dstImg : images) {
                cl_mem srcImgMem = srcImg;
                cl_mem refImgMem = dstImg;
                vmeBuilder.setExplicitArg(srcImgArgNum, sizeof(srcImgMem), &srcImgMem, err);
                vmeBuilder.setExplicitArg(refImgArgNum, sizeof(refImgMem), &refImgMem, err);
                bool shouldSucceed = (srcImg == imageValid.get()) && (dstImg == imageValid.get());
                if (shouldSucceed) {
                    EXPECT_EQ(CL_SUCCESS, vmeBuilder.validateImages(Vec3<size_t>{1, 1, 0}, Vec3<size_t>{0, 0, 0}));
                } else {
                    EXPECT_EQ(CL_OUT_OF_RESOURCES, vmeBuilder.validateImages(Vec3<size_t>{1, 1, 0}, Vec3<size_t>{0, 0, 0}));
                }
            }
        }
    }

    { // validate region size
        std::unique_ptr<Image> imageValid(ImageHelper<ImageVmeValidFormat>::create(pContext));
        cl_mem imgValidMem = imageValid.get();
        vmeBuilder.setExplicitArg(srcImgArgNum, sizeof(imgValidMem), &imgValidMem, err);
        vmeBuilder.setExplicitArg(refImgArgNum, sizeof(imgValidMem), &imgValidMem, err);

        EXPECT_EQ(CL_INVALID_IMAGE_SIZE, vmeBuilder.validateImages(Vec3<size_t>{imageValid->getImageDesc().image_width + 1, 1, 0}, Vec3<size_t>{0, 0, 0}));
        EXPECT_EQ(CL_INVALID_IMAGE_SIZE, vmeBuilder.validateImages(Vec3<size_t>{1, imageValid->getImageDesc().image_height + 1, 0}, Vec3<size_t>{0, 0, 0}));
    }
}

TEST_F(VmeBuiltInTests, WhenValidatingFlagsThenValidFlagCombinationsReturnTrue) {
    overwriteBuiltInBinaryName("media_kernels_backend");
    BuiltInOp<EBuiltInOps::vmeBlockAdvancedMotionEstimateCheckIntel> vmeBuilder(*this->pBuiltIns, *pClDevice);
    restoreBuiltInBinaryName();

    uint32_t defaultSkipBlockVal = 8192;
    uint32_t flagsArgNum = 3;

    std::tuple<uint32_t, bool, uint32_t> flagsToTest[] = {
        std::make_tuple(CL_ME_CHROMA_INTRA_PREDICT_ENABLED_INTEL, false, defaultSkipBlockVal),
        std::make_tuple(CL_ME_SKIP_BLOCK_TYPE_16x16_INTEL, true, CL_ME_MB_TYPE_16x16_INTEL),
        std::make_tuple(CL_ME_SKIP_BLOCK_TYPE_8x8_INTEL, true, CL_ME_MB_TYPE_8x8_INTEL),
        std::make_tuple(defaultSkipBlockVal, true, defaultSkipBlockVal),
    };

    cl_int err;
    for (auto &conf : flagsToTest) {
        uint32_t skipBlock = defaultSkipBlockVal;
        vmeBuilder.setExplicitArg(flagsArgNum, sizeof(uint32_t), &std::get<0>(conf), err);
        bool validationResult = vmeBuilder.validateFlags(skipBlock);
        if (std::get<1>(conf)) {
            EXPECT_TRUE(validationResult);
        } else {
            EXPECT_FALSE(validationResult);
        }
        EXPECT_EQ(std::get<2>(conf), skipBlock);
    }
}

TEST_F(VmeBuiltInTests, WhenValidatingSkipBlockTypeThenCorrectResponses) {
    overwriteBuiltInBinaryName("media_kernels_backend");
    BuiltInOp<EBuiltInOps::vmeBlockAdvancedMotionEstimateBidirectionalCheckIntel> avmeBidirectionalBuilder(*this->pBuiltIns, *pClDevice);
    BuiltInOp<EBuiltInOps::vmeBlockAdvancedMotionEstimateCheckIntel> avmeBuilder(*this->pBuiltIns, *pClDevice);
    restoreBuiltInBinaryName();

    cl_int err;
    uint32_t skipBlockTypeArgNum = 4;

    uint32_t skipBlockType = 8192;
    bool ret = avmeBidirectionalBuilder.validateSkipBlockTypeArg(skipBlockType);
    EXPECT_TRUE(ret);
    EXPECT_EQ(8192U, skipBlockType);

    skipBlockType = 8192U;
    avmeBuilder.setExplicitArg(skipBlockTypeArgNum, sizeof(uint32_t), &skipBlockType, err);
    ret = avmeBuilder.validateSkipBlockTypeArg(skipBlockType);
    EXPECT_FALSE(ret);

    skipBlockType = CL_ME_MB_TYPE_16x16_INTEL;
    avmeBuilder.setExplicitArg(skipBlockTypeArgNum, sizeof(uint32_t), &skipBlockType, err);
    skipBlockType = 8192U;
    ret = avmeBuilder.validateSkipBlockTypeArg(skipBlockType);
    EXPECT_TRUE(ret);
    EXPECT_EQ(static_cast<uint32_t>(CL_ME_MB_TYPE_16x16_INTEL), skipBlockType);

    skipBlockType = CL_ME_MB_TYPE_8x8_INTEL;
    avmeBuilder.setExplicitArg(skipBlockTypeArgNum, sizeof(uint32_t), &skipBlockType, err);
    skipBlockType = 8192U;
    ret = avmeBuilder.validateSkipBlockTypeArg(skipBlockType);
    EXPECT_TRUE(ret);
    EXPECT_EQ(static_cast<uint32_t>(CL_ME_MB_TYPE_8x8_INTEL), skipBlockType);
}

TEST_F(VmeBuiltInTests, GivenAcceleratorWhenExplicitlySettingArgThenFalseIsReturned) {
    overwriteBuiltInBinaryName("media_kernels_backend");

    BuiltInOp<EBuiltInOps::vmeBlockAdvancedMotionEstimateCheckIntel> vmeBuilder(*this->pBuiltIns, *pClDevice);
    restoreBuiltInBinaryName();

    cl_int err;
    uint32_t aceleratorArgNum = 0;
    bool ret = vmeBuilder.setExplicitArg(aceleratorArgNum, sizeof(cl_accelerator_intel), nullptr, err);
    EXPECT_FALSE(ret);
    EXPECT_EQ(CL_INVALID_ACCELERATOR_INTEL, err);

    cl_motion_estimation_desc_intel acceleratorDesc;
    acceleratorDesc.subpixel_mode = CL_ME_SUBPIXEL_MODE_INTEGER_INTEL;
    acceleratorDesc.sad_adjust_mode = CL_ME_SAD_ADJUST_MODE_NONE_INTEL;
    acceleratorDesc.search_path_type = CL_ME_SEARCH_PATH_RADIUS_2_2_INTEL;
    acceleratorDesc.mb_block_type = CL_ME_MB_TYPE_16x16_INTEL;
    auto neoAccelerator = std::unique_ptr<VmeAccelerator>(VmeAccelerator::create(pContext, CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL, sizeof(acceleratorDesc), &acceleratorDesc, err));
    ASSERT_NE(nullptr, neoAccelerator.get());
    cl_accelerator_intel clAccel = neoAccelerator.get();
    ret = vmeBuilder.setExplicitArg(aceleratorArgNum, sizeof(cl_accelerator_intel), &clAccel, err);
    EXPECT_FALSE(ret);
    EXPECT_EQ(CL_SUCCESS, err);
}

TEST_F(VmeBuiltInTests, WhenValidatingDispatchThenCorrectReturns) {
    overwriteBuiltInBinaryName("media_kernels_backend");
    struct MockVmeBuilder : BuiltInOp<EBuiltInOps::vmeBlockMotionEstimateIntel> {
        using BuiltInOp<EBuiltInOps::vmeBlockMotionEstimateIntel>::BuiltInOp;

        cl_int validateVmeDispatch(const Vec3<size_t> &inputRegion, const Vec3<size_t> &offset, size_t blkNum, size_t blkMul) const override {
            receivedInputRegion = inputRegion;
            receivedOffset = offset;
            receivedBlkNum = blkNum;
            receivedBlkMul = blkMul;
            wasValidateVmeDispatchCalled = true;
            return valueToReturn;
        }

        mutable bool wasValidateVmeDispatchCalled = false;
        mutable Vec3<size_t> receivedInputRegion = {0, 0, 0};
        mutable Vec3<size_t> receivedOffset = {0, 0, 0};
        mutable size_t receivedBlkNum = 0;
        mutable size_t receivedBlkMul = 0;
        mutable cl_int valueToReturn = CL_SUCCESS;
    };

    uint32_t aaceleratorArgNum = 0;
    MockVmeBuilder vmeBuilder(*this->pBuiltIns, *pClDevice);
    restoreBuiltInBinaryName();

    cl_int ret = vmeBuilder.validateDispatch(nullptr, 1, Vec3<size_t>{16, 16, 0}, Vec3<size_t>{16, 1, 0}, Vec3<size_t>{0, 0, 0});
    EXPECT_EQ(CL_INVALID_WORK_DIMENSION, ret);

    ret = vmeBuilder.validateDispatch(nullptr, 3, Vec3<size_t>{16, 16, 0}, Vec3<size_t>{16, 1, 0}, Vec3<size_t>{0, 0, 0});
    EXPECT_EQ(CL_INVALID_WORK_DIMENSION, ret);

    ret = vmeBuilder.validateDispatch(nullptr, 2, Vec3<size_t>{16, 16, 0}, Vec3<size_t>{16, 1, 0}, Vec3<size_t>{0, 0, 0});
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, ret); // accelerator not set
    EXPECT_FALSE(vmeBuilder.wasValidateVmeDispatchCalled);

    cl_int err;
    cl_motion_estimation_desc_intel acceleratorDesc;
    acceleratorDesc.subpixel_mode = CL_ME_SUBPIXEL_MODE_INTEGER_INTEL;
    acceleratorDesc.sad_adjust_mode = CL_ME_SAD_ADJUST_MODE_NONE_INTEL;
    acceleratorDesc.search_path_type = CL_ME_SEARCH_PATH_RADIUS_2_2_INTEL;

    Vec3<size_t> gws{16, 16, 0};
    Vec3<size_t> lws{16, 1, 0};
    Vec3<size_t> off{0, 0, 0};
    size_t gwWidthInBlk = 0;
    size_t gwHeightInBlk = 0;
    vmeBuilder.getBlkTraits(gws, gwWidthInBlk, gwHeightInBlk);

    {
        acceleratorDesc.mb_block_type = CL_ME_MB_TYPE_16x16_INTEL;
        auto neoAccelerator = std::unique_ptr<VmeAccelerator>(VmeAccelerator::create(pContext, CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL, sizeof(acceleratorDesc), &acceleratorDesc, err));
        ASSERT_NE(nullptr, neoAccelerator.get());
        cl_accelerator_intel clAccel = neoAccelerator.get();
        vmeBuilder.setExplicitArg(aaceleratorArgNum, sizeof(clAccel), &clAccel, err);
        vmeBuilder.wasValidateVmeDispatchCalled = false;
        auto ret = vmeBuilder.validateDispatch(nullptr, 2, gws, lws, off);
        EXPECT_EQ(CL_SUCCESS, ret);
        EXPECT_TRUE(vmeBuilder.wasValidateVmeDispatchCalled);
        EXPECT_EQ(gws, vmeBuilder.receivedInputRegion);
        EXPECT_EQ(off, vmeBuilder.receivedOffset);

        EXPECT_EQ(gwWidthInBlk * gwHeightInBlk, vmeBuilder.receivedBlkNum);
        EXPECT_EQ(1U, vmeBuilder.receivedBlkMul);
    }

    {
        acceleratorDesc.mb_block_type = CL_ME_MB_TYPE_4x4_INTEL;
        auto neoAccelerator = std::unique_ptr<VmeAccelerator>(VmeAccelerator::create(pContext, CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL, sizeof(acceleratorDesc), &acceleratorDesc, err));
        ASSERT_NE(nullptr, neoAccelerator.get());
        cl_accelerator_intel clAccel = neoAccelerator.get();
        vmeBuilder.setExplicitArg(aaceleratorArgNum, sizeof(clAccel), &clAccel, err);
        vmeBuilder.wasValidateVmeDispatchCalled = false;
        auto ret = vmeBuilder.validateDispatch(nullptr, 2, gws, lws, off);
        EXPECT_EQ(CL_SUCCESS, ret);
        EXPECT_TRUE(vmeBuilder.wasValidateVmeDispatchCalled);
        EXPECT_EQ(gws, vmeBuilder.receivedInputRegion);
        EXPECT_EQ(off, vmeBuilder.receivedOffset);

        EXPECT_EQ(gwWidthInBlk * gwHeightInBlk, vmeBuilder.receivedBlkNum);
        EXPECT_EQ(16U, vmeBuilder.receivedBlkMul);
    }

    {
        acceleratorDesc.mb_block_type = CL_ME_MB_TYPE_8x8_INTEL;
        auto neoAccelerator = std::unique_ptr<VmeAccelerator>(VmeAccelerator::create(pContext, CL_ACCELERATOR_TYPE_MOTION_ESTIMATION_INTEL, sizeof(acceleratorDesc), &acceleratorDesc, err));
        ASSERT_NE(nullptr, neoAccelerator.get());
        cl_accelerator_intel clAccel = neoAccelerator.get();
        vmeBuilder.setExplicitArg(aaceleratorArgNum, sizeof(clAccel), &clAccel, err);
        vmeBuilder.wasValidateVmeDispatchCalled = false;
        vmeBuilder.valueToReturn = 37;
        auto ret = vmeBuilder.validateDispatch(nullptr, 2, gws, lws, off);
        EXPECT_EQ(37, ret);
        EXPECT_TRUE(vmeBuilder.wasValidateVmeDispatchCalled);
        EXPECT_EQ(gws, vmeBuilder.receivedInputRegion);
        EXPECT_EQ(off, vmeBuilder.receivedOffset);

        EXPECT_EQ(gwWidthInBlk * gwHeightInBlk, vmeBuilder.receivedBlkNum);
        EXPECT_EQ(4U, vmeBuilder.receivedBlkMul);
    }
}

TEST_F(VmeBuiltInTests, WhenValidatingVmeDispatchThenCorrectReturns) {
    overwriteBuiltInBinaryName("media_kernels_backend");
    BuiltInOp<EBuiltInOps::vmeBlockMotionEstimateIntel> vmeBuilder(*this->pBuiltIns, *pClDevice);
    restoreBuiltInBinaryName();

    cl_int err;

    // images not set
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, vmeBuilder.validateVmeDispatch(Vec3<size_t>{1, 1, 0}, Vec3<size_t>{0, 0, 0}, 64, 1));

    uint32_t srcImgArgNum = 1;
    uint32_t refImgArgNum = 2;

    std::unique_ptr<Image> imageValid(ImageHelper<ImageVmeValidFormat>::create(pContext));
    cl_mem srcImgMem = imageValid.get();

    vmeBuilder.setExplicitArg(srcImgArgNum, sizeof(srcImgMem), &srcImgMem, err);
    vmeBuilder.setExplicitArg(refImgArgNum, sizeof(srcImgMem), &srcImgMem, err);

    // null buffers are valid
    EXPECT_EQ(CL_SUCCESS, vmeBuilder.validateVmeDispatch(Vec3<size_t>{1, 1, 0}, Vec3<size_t>{0, 0, 0}, 64, 1));

    // too small buffers should fail
    MockBuffer mb;
    cl_mem mem = &mb;

    uint32_t predictionMotionVectorBufferArgNum = 3;
    uint32_t motionVectorBufferArgNum = 4;
    uint32_t residualsBufferArgNum = 5;
    for (uint32_t argNum : {predictionMotionVectorBufferArgNum, motionVectorBufferArgNum, residualsBufferArgNum}) {
        EXPECT_EQ(CL_SUCCESS, vmeBuilder.validateVmeDispatch(Vec3<size_t>{1, 1, 0}, Vec3<size_t>{0, 0, 0}, mb.getSize() * 2, 1));
        vmeBuilder.setExplicitArg(argNum, sizeof(cl_mem), &mem, err);
        EXPECT_EQ(CL_INVALID_BUFFER_SIZE, vmeBuilder.validateVmeDispatch(Vec3<size_t>{1, 1, 0}, Vec3<size_t>{0, 0, 0}, mb.getSize() * 2, 1));
        vmeBuilder.setExplicitArg(argNum, sizeof(cl_mem), nullptr, err);
    }
}

TEST_F(VmeBuiltInTests, GivenAdvancedVmeWhenValidatingVmeDispatchThenCorrectReturns) {
    overwriteBuiltInBinaryName("media_kernels_backend");
    BuiltInOp<EBuiltInOps::vmeBlockAdvancedMotionEstimateCheckIntel> avmeBuilder(*this->pBuiltIns, *pClDevice);
    restoreBuiltInBinaryName();

    cl_int err;
    // images not set
    ASSERT_EQ(CL_INVALID_KERNEL_ARGS, avmeBuilder.VmeBuiltinDispatchInfoBuilder::validateVmeDispatch(Vec3<size_t>{1, 1, 0}, Vec3<size_t>{0, 0, 0}, 64, 1));
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, avmeBuilder.validateVmeDispatch(Vec3<size_t>{1, 1, 0}, Vec3<size_t>{0, 0, 0}, 64, 1));

    uint32_t srcImgArgNum = 1;
    uint32_t refImgArgNum = 2;

    std::unique_ptr<Image> imageValid(ImageHelper<ImageVmeValidFormat>::create(pContext));
    cl_mem srcImgMem = imageValid.get();

    avmeBuilder.setExplicitArg(srcImgArgNum, sizeof(srcImgMem), &srcImgMem, err);
    avmeBuilder.setExplicitArg(refImgArgNum, sizeof(srcImgMem), &srcImgMem, err);

    ASSERT_EQ(CL_SUCCESS, avmeBuilder.VmeBuiltinDispatchInfoBuilder::validateVmeDispatch(Vec3<size_t>{1, 1, 0}, Vec3<size_t>{0, 0, 0}, 64, 1));

    uint32_t flagsArgNum = 3;
    uint32_t val = CL_ME_CHROMA_INTRA_PREDICT_ENABLED_INTEL;
    avmeBuilder.setExplicitArg(flagsArgNum, sizeof(val), &val, err);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, avmeBuilder.validateVmeDispatch(Vec3<size_t>{1, 1, 0}, Vec3<size_t>{0, 0, 0}, 64, 1));
    val = CL_ME_SKIP_BLOCK_TYPE_8x8_INTEL;
    avmeBuilder.setExplicitArg(flagsArgNum, sizeof(val), &val, err);

    uint32_t skipBlockTypeArgNum = 4;
    val = 8192;
    avmeBuilder.setExplicitArg(skipBlockTypeArgNum, sizeof(uint32_t), &val, err);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, avmeBuilder.validateVmeDispatch(Vec3<size_t>{1, 1, 0}, Vec3<size_t>{0, 0, 0}, 64, 1));
    val = CL_ME_MB_TYPE_16x16_INTEL;
    avmeBuilder.setExplicitArg(skipBlockTypeArgNum, sizeof(uint32_t), &val, err);

    uint32_t searchCostPenaltyArgNum = 5;
    val = 8192;
    avmeBuilder.setExplicitArg(searchCostPenaltyArgNum, sizeof(uint32_t), &val, err);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, avmeBuilder.validateVmeDispatch(Vec3<size_t>{1, 1, 0}, Vec3<size_t>{0, 0, 0}, 64, 1));
    val = CL_ME_COST_PENALTY_NONE_INTEL;
    avmeBuilder.setExplicitArg(searchCostPenaltyArgNum, sizeof(uint32_t), &val, err);

    uint32_t searchCostPrecisionArgNum = 6;
    val = 8192;
    avmeBuilder.setExplicitArg(searchCostPrecisionArgNum, sizeof(uint32_t), &val, err);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, avmeBuilder.validateVmeDispatch(Vec3<size_t>{1, 1, 0}, Vec3<size_t>{0, 0, 0}, 64, 1));
    val = CL_ME_COST_PRECISION_QPEL_INTEL;
    avmeBuilder.setExplicitArg(searchCostPrecisionArgNum, sizeof(uint32_t), &val, err);

    // for non-bidirectional avme kernel, countMotionVectorBuffer must be set
    uint32_t countMotionVectorBufferArgNum = 7;
    EXPECT_EQ(CL_INVALID_BUFFER_SIZE, avmeBuilder.validateVmeDispatch(Vec3<size_t>{1, 1, 0}, Vec3<size_t>{0, 0, 0}, 64, 1));

    MockBuffer mb;
    cl_mem mem = &mb;
    avmeBuilder.setExplicitArg(countMotionVectorBufferArgNum, sizeof(cl_mem), &mem, err);
    EXPECT_EQ(CL_SUCCESS, avmeBuilder.validateVmeDispatch(Vec3<size_t>{1, 1, 0}, Vec3<size_t>{0, 0, 0}, 1, 1));
}

TEST_F(VmeBuiltInTests, GivenAdvancedBidirectionalVmeWhenValidatingVmeDispatchThenCorrectReturns) {
    overwriteBuiltInBinaryName("media_kernels_backend");
    BuiltInOp<EBuiltInOps::vmeBlockAdvancedMotionEstimateBidirectionalCheckIntel> avmeBuilder(*this->pBuiltIns, *pClDevice);
    restoreBuiltInBinaryName();

    cl_int err;
    uint32_t srcImgArgNum = 1;
    uint32_t refImgArgNum = 2;

    std::unique_ptr<Image> imageValid(ImageHelper<ImageVmeValidFormat>::create(pContext));
    cl_mem srcImgMem = imageValid.get();

    avmeBuilder.setExplicitArg(srcImgArgNum, sizeof(srcImgMem), &srcImgMem, err);
    avmeBuilder.setExplicitArg(refImgArgNum, sizeof(srcImgMem), &srcImgMem, err);

    ASSERT_EQ(CL_SUCCESS, avmeBuilder.VmeBuiltinDispatchInfoBuilder::validateVmeDispatch(Vec3<size_t>{1, 1, 0}, Vec3<size_t>{0, 0, 0}, 64, 1));

    uint32_t flagsArgNum = 6;
    uint32_t val = CL_ME_SKIP_BLOCK_TYPE_8x8_INTEL;
    avmeBuilder.setExplicitArg(flagsArgNum, sizeof(val), &val, err);

    uint32_t searchCostPenaltyArgNum = 7;
    val = CL_ME_COST_PENALTY_NONE_INTEL;
    avmeBuilder.setExplicitArg(searchCostPenaltyArgNum, sizeof(uint32_t), &val, err);

    uint32_t searchCostPrecisionArgNum = 8;
    val = CL_ME_COST_PRECISION_QPEL_INTEL;
    avmeBuilder.setExplicitArg(searchCostPrecisionArgNum, sizeof(uint32_t), &val, err);

    uint32_t bidirWeightArgNum = 10;
    val = 255;
    avmeBuilder.setExplicitArg(bidirWeightArgNum, sizeof(uint8_t), &val, err);
    EXPECT_EQ(CL_INVALID_KERNEL_ARGS, avmeBuilder.validateVmeDispatch(Vec3<size_t>{1, 1, 0}, Vec3<size_t>{0, 0, 0}, 64, 1));
    val = CL_ME_BIDIR_WEIGHT_QUARTER_INTEL;
    avmeBuilder.setExplicitArg(bidirWeightArgNum, sizeof(uint8_t), &val, err);

    EXPECT_EQ(CL_SUCCESS, avmeBuilder.validateVmeDispatch(Vec3<size_t>{1, 1, 0}, Vec3<size_t>{0, 0, 0}, 64, 1));

    // test bufferSize checking
    uint32_t countMotionVectorBufferArgNum = 11;
    MockBuffer mb;
    cl_mem mem = &mb;
    avmeBuilder.setExplicitArg(countMotionVectorBufferArgNum, sizeof(cl_mem), &mem, err);
    EXPECT_EQ(CL_SUCCESS, avmeBuilder.validateVmeDispatch(Vec3<size_t>{1, 1, 0}, Vec3<size_t>{0, 0, 0}, 1, 1));
    EXPECT_EQ(CL_INVALID_BUFFER_SIZE, avmeBuilder.validateVmeDispatch(Vec3<size_t>{1, 1, 0}, Vec3<size_t>{0, 0, 0}, mb.getSize() * 2, 1));
}

TEST_F(VmeBuiltInTests, GivenAdvancedVmeWhenGettingSkipResidualsBuffExpSizeThenDefaultSizeIsReturned) {
    overwriteBuiltInBinaryName("media_kernels_backend");
    BuiltInOp<EBuiltInOps::vmeBlockAdvancedMotionEstimateCheckIntel> vmeBuilder(*this->pBuiltIns, *pClDevice);
    restoreBuiltInBinaryName();

    auto size16x16 = vmeBuilder.getSkipResidualsBuffExpSize(CL_ME_MB_TYPE_16x16_INTEL, 4);
    auto sizeDefault = vmeBuilder.getSkipResidualsBuffExpSize(8192, 4);
    EXPECT_EQ(size16x16, sizeDefault);
}

TEST_F(BuiltInTests, GivenInvalidBuiltinKernelNameWhenCreatingBuiltInProgramThenInvalidValueErrorIsReturned) {
    const char *kernelNames = "invalid_kernel";
    cl_int retVal = CL_SUCCESS;

    cl_program program = Vme::createBuiltInProgram(
        *pContext,
        pContext->getDevices(),
        kernelNames,
        retVal);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, program);
}

TEST_F(BuiltInTests, WhenGettingSipKernelThenReturnProgramCreatedFromIsaAcquiredThroughCompilerInterface) {
    auto mockCompilerInterface = new MockCompilerInterface();
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex]->compilerInterface.reset(mockCompilerInterface);
    auto builtins = new BuiltIns;
    MockRootDeviceEnvironment::resetBuiltins(pDevice->getExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex].get(), builtins);
    mockCompilerInterface->sipKernelBinaryOverride = mockCompilerInterface->getDummyGenBinary();

    const SipKernel &sipKernel = builtins->getSipKernel(SipKernelType::csr, *pDevice);

    auto expectedMem = mockCompilerInterface->sipKernelBinaryOverride.data();
    EXPECT_EQ(0, memcmp(expectedMem, sipKernel.getSipAllocation()->getUnderlyingBuffer(), mockCompilerInterface->sipKernelBinaryOverride.size()));
    EXPECT_EQ(SipKernelType::csr, mockCompilerInterface->requestedSipKernel);

    mockCompilerInterface->releaseDummyGenBinary();
}

TEST_F(BuiltInTests, givenSipKernelWhenItIsCreatedThenItHasGraphicsAllocationForKernel) {
    const SipKernel &sipKern = pDevice->getBuiltIns()->getSipKernel(SipKernelType::csr, pContext->getDevice(0)->getDevice());
    auto sipAllocation = sipKern.getSipAllocation();
    EXPECT_NE(nullptr, sipAllocation);
}

TEST_F(BuiltInTests, givenSipKernelWhenAllocationFailsThenItHasNullptrGraphicsAllocation) {
    auto executionEnvironment = new MockExecutionEnvironment;
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto memoryManager = new MockMemoryManager(*executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    auto device = std::unique_ptr<RootDevice>(Device::create<RootDevice>(executionEnvironment, 0u));
    EXPECT_NE(nullptr, device);

    memoryManager->failAllocate32Bit = true;

    auto builtins = std::make_unique<BuiltIns>();
    const SipKernel &sipKern = builtins->getSipKernel(SipKernelType::csr, *device);
    auto sipAllocation = sipKern.getSipAllocation();
    EXPECT_EQ(nullptr, sipAllocation);
}

TEST_F(BuiltInTests, givenDebugFlagForceUseSourceWhenArgIsBinaryThenReturnBuiltinCodeBinary) {
    debugManager.flags.RebuildPrecompiledKernels.set(true);
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    BuiltinCode code = builtinsLib->getBuiltinCode(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::binary, *pDevice);
    EXPECT_EQ(BuiltinCode::ECodeType::binary, code.type);
    EXPECT_NE(0u, code.resource.size());
    EXPECT_EQ(pDevice, code.targetDevice);
}

TEST_F(BuiltInTests, givenDebugFlagForceUseSourceWhenArgIsAnyThenReturnBuiltinCodeSource) {
    debugManager.flags.RebuildPrecompiledKernels.set(true);
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    BuiltinCode code = builtinsLib->getBuiltinCode(EBuiltInOps::copyBufferToBuffer, BuiltinCode::ECodeType::any, *pDevice);
    EXPECT_EQ(BuiltinCode::ECodeType::source, code.type);
    EXPECT_NE(0u, code.resource.size());
    EXPECT_EQ(pDevice, code.targetDevice);
}

using BuiltInOwnershipWrapperTests = BuiltInTests;

TEST_F(BuiltInOwnershipWrapperTests, givenBuiltinWhenConstructedThenLockAndUnlockOnDestruction) {
    MockAuxBuilInOp mockAuxBuiltInOp(*pBuiltIns, *pClDevice);
    MockContext context(pClDevice);
    {
        EXPECT_EQ(nullptr, mockAuxBuiltInOp.baseKernel->getProgram()->getContextPtr());
        BuiltInOwnershipWrapper lock(mockAuxBuiltInOp, &context);
        EXPECT_TRUE(mockAuxBuiltInOp.baseKernel->getMultiDeviceKernel()->hasOwnership());
        EXPECT_TRUE(mockAuxBuiltInOp.baseKernel->getProgram()->hasOwnership());
        EXPECT_EQ(&context, mockAuxBuiltInOp.baseKernel->getProgram()->getContextPtr());
    }
    EXPECT_FALSE(mockAuxBuiltInOp.baseKernel->getMultiDeviceKernel()->hasOwnership());
    EXPECT_FALSE(mockAuxBuiltInOp.baseKernel->getProgram()->hasOwnership());
    EXPECT_EQ(nullptr, mockAuxBuiltInOp.baseKernel->getProgram()->getContextPtr());
}

TEST_F(BuiltInOwnershipWrapperTests, givenLockWithoutParametersWhenConstructingThenLockOnlyWhenRequested) {
    MockAuxBuilInOp mockAuxBuiltInOp(*pBuiltIns, *pClDevice);
    MockContext context(pClDevice);
    {
        BuiltInOwnershipWrapper lock;
        EXPECT_EQ(nullptr, mockAuxBuiltInOp.baseKernel->getProgram()->getContextPtr());
        lock.takeOwnership(mockAuxBuiltInOp, &context);
        EXPECT_TRUE(mockAuxBuiltInOp.baseKernel->getMultiDeviceKernel()->hasOwnership());
        EXPECT_TRUE(mockAuxBuiltInOp.baseKernel->getProgram()->hasOwnership());
        EXPECT_EQ(&context, mockAuxBuiltInOp.baseKernel->getProgram()->getContextPtr());
    }
    EXPECT_FALSE(mockAuxBuiltInOp.baseKernel->getMultiDeviceKernel()->hasOwnership());
    EXPECT_FALSE(mockAuxBuiltInOp.baseKernel->getProgram()->hasOwnership());
    EXPECT_EQ(nullptr, mockAuxBuiltInOp.baseKernel->getProgram()->getContextPtr());
}

TEST_F(BuiltInOwnershipWrapperTests, givenLockWithAcquiredOwnershipWhenTakeOwnershipCalledThenAbort) {
    MockAuxBuilInOp mockAuxBuiltInOp1(*pBuiltIns, *pClDevice);
    MockAuxBuilInOp mockAuxBuiltInOp2(*pBuiltIns, *pClDevice);
    MockContext context(pClDevice);

    BuiltInOwnershipWrapper lock(mockAuxBuiltInOp1, &context);
    EXPECT_THROW(lock.takeOwnership(mockAuxBuiltInOp1, &context), std::exception);
    EXPECT_THROW(lock.takeOwnership(mockAuxBuiltInOp2, &context), std::exception);
}

HWTEST_F(BuiltInOwnershipWrapperTests, givenBuiltInOwnershipWrapperWhenAskedForTypeTraitsThenDisableCopyConstructorAndOperator) {
    EXPECT_FALSE(std::is_copy_constructible<BuiltInOwnershipWrapper>::value);
    EXPECT_FALSE(std::is_copy_assignable<BuiltInOwnershipWrapper>::value);
}

HWTEST2_F(BuiltInTests, whenBuilderCopyBufferToBufferStatelessHeaplessIsUsedThenParamsAreCorrect, HeaplessSupportedMatcher) {

    if (is32bit) {
        GTEST_SKIP();
    }

    BuiltinDispatchInfoBuilder &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferToBufferStatelessHeapless, *pClDevice);

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t bigOffset = 4ull * MemoryConstants::gigaByte;
    uint64_t size = 4ull * MemoryConstants::gigaByte;

    MockBuffer srcBuffer;
    srcBuffer.size = static_cast<size_t>(bigSize);
    MockBuffer dstBuffer;
    dstBuffer.size = static_cast<size_t>(bigSize);

    BuiltinOpParams builtinOpsParams;

    builtinOpsParams.srcMemObj = &srcBuffer;
    builtinOpsParams.srcOffset = {static_cast<size_t>(bigOffset), 0, 0};
    builtinOpsParams.dstMemObj = &dstBuffer;
    builtinOpsParams.dstOffset = {0, 0, 0};
    builtinOpsParams.size = {static_cast<size_t>(size), 0, 0};

    MultiDispatchInfo multiDispatchInfo(builtinOpsParams);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), builtinOpsParams));
}

HWTEST2_F(BuiltInTests, whenBuilderCopyBufferToSystemBufferRectStatelessHeaplessIsUsedThenParamsAreCorrect, HeaplessSupportedMatcher) {
    if (is32bit) {
        GTEST_SKIP();
    }

    BuiltinDispatchInfoBuilder &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferRectStatelessHeapless, *pClDevice);

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t bigOffset = 4ull * MemoryConstants::gigaByte;
    uint64_t size = 4ull * MemoryConstants::gigaByte;

    MockBuffer srcBuffer;
    srcBuffer.size = static_cast<size_t>(bigSize);
    MockBuffer dstBuffer;
    dstBuffer.size = static_cast<size_t>(bigSize);

    srcBuffer.mockGfxAllocation.setAllocationType(AllocationType::buffer);
    dstBuffer.mockGfxAllocation.setAllocationType(AllocationType::bufferHostMemory);

    BuiltinOpParams dc;
    dc.srcMemObj = &srcBuffer;
    dc.dstMemObj = &dstBuffer;
    dc.srcOffset = {static_cast<size_t>(bigOffset), 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {static_cast<size_t>(size), 1, 1};
    dc.srcRowPitch = static_cast<size_t>(size);
    dc.srcSlicePitch = 0;
    dc.dstRowPitch = static_cast<size_t>(size);
    dc.dstSlicePitch = 0;

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), dc));

    for (auto &dispatchInfo : multiDispatchInfo) {
        EXPECT_TRUE(dispatchInfo.getKernel()->getDestinationAllocationInSystemMemory());
    }
}

HWTEST2_F(BuiltInTests, whenBuilderCopyBufferToLocalBufferRectStatelessHeaplessIsUsedThenParamsAreCorrect, HeaplessSupportedMatcher) {
    if (is32bit) {
        GTEST_SKIP();
    }

    BuiltinDispatchInfoBuilder &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferRectStatelessHeapless, *pClDevice);

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t bigOffset = 4ull * MemoryConstants::gigaByte;
    uint64_t size = 4ull * MemoryConstants::gigaByte;

    MockBuffer srcBuffer;
    srcBuffer.size = static_cast<size_t>(bigSize);
    MockBuffer dstBuffer;
    dstBuffer.size = static_cast<size_t>(bigSize);

    srcBuffer.mockGfxAllocation.setAllocationType(AllocationType::bufferHostMemory);
    dstBuffer.mockGfxAllocation.setAllocationType(AllocationType::buffer);

    BuiltinOpParams dc;
    dc.srcMemObj = &srcBuffer;
    dc.dstMemObj = &dstBuffer;
    dc.srcOffset = {static_cast<size_t>(bigOffset), 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {static_cast<size_t>(size), 1, 1};
    dc.srcRowPitch = static_cast<size_t>(size);
    dc.srcSlicePitch = 0;
    dc.dstRowPitch = static_cast<size_t>(size);
    dc.dstSlicePitch = 0;

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), dc));

    for (auto &dispatchInfo : multiDispatchInfo) {
        EXPECT_FALSE(dispatchInfo.getKernel()->getDestinationAllocationInSystemMemory());
    }
}

HWTEST2_F(BuiltInTests, whenBuilderFillSystemBufferStatelessHeaplessIsUsedThenParamsAreCorrect, HeaplessSupportedMatcher) {
    if (is32bit) {
        GTEST_SKIP();
    }

    BuiltinDispatchInfoBuilder &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::fillBufferStatelessHeapless, *pClDevice);

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t bigOffset = 4ull * MemoryConstants::gigaByte;
    uint64_t size = 4ull * MemoryConstants::gigaByte;

    MockBuffer srcBuffer;
    srcBuffer.size = static_cast<size_t>(bigSize);
    MockBuffer dstBuffer;
    dstBuffer.size = static_cast<size_t>(bigSize);

    srcBuffer.mockGfxAllocation.setAllocationType(AllocationType::buffer);
    dstBuffer.mockGfxAllocation.setAllocationType(AllocationType::bufferHostMemory);

    BuiltinOpParams dc;
    dc.srcMemObj = &srcBuffer;
    dc.dstMemObj = &dstBuffer;
    dc.dstOffset = {static_cast<size_t>(bigOffset), 0, 0};
    dc.size = {static_cast<size_t>(size), 0, 0};

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), dc));

    for (auto &dispatchInfo : multiDispatchInfo) {
        EXPECT_TRUE(dispatchInfo.getKernel()->getDestinationAllocationInSystemMemory());
    }
}

HWTEST2_F(BuiltInTests, whenBuilderFillLocalBufferStatelessHeaplessIsUsedThenParamsAreCorrect, HeaplessSupportedMatcher) {
    if (is32bit) {
        GTEST_SKIP();
    }

    BuiltinDispatchInfoBuilder &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::fillBufferStatelessHeapless, *pClDevice);

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t bigOffset = 4ull * MemoryConstants::gigaByte;
    uint64_t size = 4ull * MemoryConstants::gigaByte;

    MockBuffer srcBuffer;
    srcBuffer.size = static_cast<size_t>(bigSize);
    MockBuffer dstBuffer;
    dstBuffer.size = static_cast<size_t>(bigSize);

    srcBuffer.mockGfxAllocation.setAllocationType(AllocationType::bufferHostMemory);
    dstBuffer.mockGfxAllocation.setAllocationType(AllocationType::buffer);

    BuiltinOpParams dc;
    dc.srcMemObj = &srcBuffer;
    dc.dstMemObj = &dstBuffer;
    dc.dstOffset = {static_cast<size_t>(bigOffset), 0, 0};
    dc.size = {static_cast<size_t>(size), 0, 0};

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), dc));

    for (auto &dispatchInfo : multiDispatchInfo) {
        EXPECT_FALSE(dispatchInfo.getKernel()->getDestinationAllocationInSystemMemory());
    }
}
