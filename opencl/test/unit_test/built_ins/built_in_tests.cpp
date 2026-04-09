/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/built_ins/sip.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/gmm_resource_usage_ocl_buffer.h"
#include "shared/source/helpers/append_operations.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/path.h"
#include "shared/source/indirect_heap/indirect_heap_type.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/helpers/test_traits.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_builtinslib.h"
#include "shared/test/common/mocks/mock_compiler_interface.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/heapless_matchers.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/test_macros/test_checks_shared.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/built_ins/aux_translation_builtin.h"
#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/fixtures/built_in_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue_hw.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "gtest/gtest.h"
#include "os_inc.h"

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
    void SetUp() override {
        debugManager.flags.ForceAuxTranslationMode.set(static_cast<int32_t>(AuxTranslationMode::builtin));
        ClDeviceFixture::setUp();
        cl_device_id device = pClDevice;
        ContextFixture::setUp(1, &device);
        BuiltInFixture::setUp(pDevice);
        auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
        bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());
        bool isForceStateless = compilerProductHelper.isForceToStatelessRequired();
        copyBufferToBufferBuiltin = BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyBufferToBuffer>(isForceStateless, heaplessAllowed);
        copyBufferToImage3dStatelessBuiltIn = BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyBufferToImage3d>(true, heaplessAllowed);
        copyImage3dToBufferStatelessBuiltIn = BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyImage3dToBuffer>(true, heaplessAllowed);
        fillBufferStatelessBuiltIn = BuiltIn::adjustBuiltinGroup<BuiltIn::Group::fillBuffer>(true, heaplessAllowed);
        copyBufferRectStatelessBuiltIn = BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyBufferRect>(true, heaplessAllowed);
        copyBufferToBufferStatelessBuiltIn = BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyBufferToBuffer>(true, heaplessAllowed);
    }

    void TearDown() override {
        auto builders = pClDevice->peekBuilders();
        if (builders) {
            for (uint32_t i = 0; i < static_cast<uint32_t>(BuiltIn::Group::count); ++i) {
                builders[i].first.reset();
            }
        }
        BuiltInFixture::tearDown();
        ContextFixture::tearDown();
        ClDeviceFixture::tearDown();
    }

    bool compareBuiltinOpParams(const BuiltIn::OpParams &left,
                                const BuiltIn::OpParams &right) {
        return left.srcPtr == right.srcPtr &&
               left.dstPtr == right.dstPtr &&
               left.size == right.size &&
               left.srcOffset == right.srcOffset &&
               left.dstOffset == right.dstOffset &&
               left.dstMemObj == right.dstMemObj &&
               left.srcMemObj == right.srcMemObj;
    }

    BuiltIn::Group copyBufferToBufferBuiltin;
    BuiltIn::Group copyBufferToImage3dStatelessBuiltIn;
    BuiltIn::Group copyImage3dToBufferStatelessBuiltIn;
    BuiltIn::Group fillBufferStatelessBuiltIn;
    BuiltIn::Group copyBufferRectStatelessBuiltIn;
    BuiltIn::Group copyBufferToBufferStatelessBuiltIn;

    DebugManagerStateRestore restore;
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

HWTEST2_F(BuiltInTests, GivenBuiltinTypeBinaryWhenGettingAuxTranslationBuiltinThenResourceSizeIsNonZero, MatchAny) {
    auto mockBuiltInLibrary = std::unique_ptr<MockBuiltInResourceLoader>(new MockBuiltInResourceLoader());

    EXPECT_EQ(TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::auxBuiltinsSupported,
              mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::auxTranslation, BuiltIn::CodeType::binary, *pDevice).size() != 0);
}

class MockAuxBuilInOp : public BuiltIn::Op<BuiltIn::Group::auxTranslation> {
  public:
    using BuiltIn::DispatchInfoBuilder::populate;
    using BaseClass = BuiltIn::Op<BuiltIn::Group::auxTranslation>;
    using BaseClass::baseKernel;
    using BaseClass::convertToAuxKernel;
    using BaseClass::convertToNonAuxKernel;
    using BaseClass::resizeKernelInstances;
    using BaseClass::usedKernels;

    using BaseClass::Op;
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

    BuiltIn::OpParams builtinOpParamsToAux;
    builtinOpParamsToAux.auxTranslationDirection = AuxTranslationDirection::nonAuxToAux;

    BuiltIn::OpParams builtinOpParamsToNonAux;
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
        auto expectedMocs = pDevice->getGmmHelper()->getUncachedMOCS();
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
        auto expectedMocs = pDevice->getGmmHelper()->getL1EnabledMOCS();
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

TEST_F(BuiltInTests, GivenCopyBufferToSystemMemoryBufferWhenDispatchInfoIsCreatedThenParamsAreCorrect) {
    BuiltIn::DispatchInfoBuilder &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(copyBufferToBufferBuiltin, *pClDevice);

    MockBuffer *srcPtr = new MockBuffer();
    MockBuffer *dstPtr = new MockBuffer();

    MockBuffer &src = *srcPtr;
    MockBuffer &dst = *dstPtr;

    srcPtr->mockGfxAllocation.setAllocationType(AllocationType::buffer);
    dstPtr->mockGfxAllocation.setAllocationType(AllocationType::bufferHostMemory);

    BuiltIn::OpParams builtinOpsParams;

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
    BuiltIn::DispatchInfoBuilder &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(copyBufferToBufferBuiltin, *pClDevice);

    MockBuffer *srcPtr = new MockBuffer();
    MockBuffer *dstPtr = new MockBuffer();

    MockBuffer &src = *srcPtr;
    MockBuffer &dst = *dstPtr;

    srcPtr->mockGfxAllocation.setAllocationType(AllocationType::bufferHostMemory);
    dstPtr->mockGfxAllocation.setAllocationType(AllocationType::buffer);

    BuiltIn::OpParams builtinOpsParams;

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
    BuiltIn::DispatchInfoBuilder &baseBuilder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(BuiltIn::Group::auxTranslation, *pClDevice);
    auto &builder = static_cast<BuiltIn::Op<BuiltIn::Group::auxTranslation> &>(baseBuilder);

    std::vector<Kernel *> builtinKernels;
    std::vector<MockKernelObjForAuxTranslation> mockKernelObjForAuxTranslation;
    mockKernelObjForAuxTranslation.push_back(MockKernelObjForAuxTranslation(kernelObjType, 0x1000));
    mockKernelObjForAuxTranslation.push_back(MockKernelObjForAuxTranslation(kernelObjType, 0x20000));
    mockKernelObjForAuxTranslation.push_back(MockKernelObjForAuxTranslation(kernelObjType, 0x30000));

    BuiltIn::OpParams builtinOpsParams;
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
    BuiltIn::DispatchInfoBuilder &baseBuilder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(BuiltIn::Group::auxTranslation, *pClDevice);
    auto &builder = static_cast<BuiltIn::Op<BuiltIn::Group::auxTranslation> &>(baseBuilder);

    std::vector<Kernel *> builtinKernels;
    std::vector<MockKernelObjForAuxTranslation> mockKernelObjForAuxTranslation;
    mockKernelObjForAuxTranslation.push_back(MockKernelObjForAuxTranslation(kernelObjType, 0x1000));
    mockKernelObjForAuxTranslation.push_back(MockKernelObjForAuxTranslation(kernelObjType, 0x20000));
    mockKernelObjForAuxTranslation.push_back(MockKernelObjForAuxTranslation(kernelObjType, 0x30000));

    BuiltIn::OpParams builtinOpsParams;
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
    BuiltIn::DispatchInfoBuilder &baseBuilder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(BuiltIn::Group::auxTranslation, *pClDevice);
    auto &builder = static_cast<BuiltIn::Op<BuiltIn::Group::auxTranslation> &>(baseBuilder);

    std::vector<MockKernelObjForAuxTranslation> mockKernelObjForAuxTranslation;
    for (int i = 0; i < 3; i++) {
        mockKernelObjForAuxTranslation.push_back(MockKernelObjForAuxTranslation(kernelObjType));
    }
    std::vector<Kernel *> builtinKernels;

    BuiltIn::OpParams builtinOpsParams;

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
    BuiltIn::DispatchInfoBuilder &baseBuilder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(BuiltIn::Group::auxTranslation, *pClDevice);
    auto &builder = static_cast<BuiltIn::Op<BuiltIn::Group::auxTranslation> &>(baseBuilder);

    auto kernelObjsForAuxTranslation = std::make_unique<KernelObjsForAuxTranslation>();
    auto kernelObjsForAuxTranslationPtr = kernelObjsForAuxTranslation.get();
    MockKernelObjForAuxTranslation mockKernelObjForAuxTranslation(kernelObjType);

    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.setKernelObjsForAuxTranslation(std::move(kernelObjsForAuxTranslation));
    BuiltIn::OpParams builtinOpsParams;

    kernelObjsForAuxTranslationPtr->insert(mockKernelObjForAuxTranslation);

    builtinOpsParams.auxTranslationDirection = AuxTranslationDirection::none;
    EXPECT_THROW(builder.buildDispatchInfosForAuxTranslation<FamilyType>(multiDispatchInfo, builtinOpsParams), std::exception);
}

HWTEST2_F(BuiltInTests, whenAuxBuiltInIsConstructedThenResizeKernelInstancedTo5, AuxBuiltinsMatcher) {
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

    BuiltIn::OpParams builtinOpsParams;
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

HWTEST2_F(BuiltInTests, givenAuxBuiltInWhenResizeIsCalledThenCloneAllNewInstancesFromBaseKernel, AuxBuiltinsMatcher) {
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
    BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(BuiltIn::Group::auxTranslation, *pClDevice);
    auto mockAuxBuiltInOp = new MockAuxBuilInOp(*pBuiltIns, *pClDevice);
    pClDevice->setBuiltinDispatchInfoBuilder(BuiltIn::Group::auxTranslation, std::unique_ptr<MockAuxBuilInOp>(mockAuxBuiltInOp));

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

HWCMDTEST_P(IGFX_GEN12LP_CORE, AuxBuiltInTests, givenAuxTranslationKernelWhenSettingKernelArgsThenSetValidMocs) {
    if (this->pDevice->areSharedSystemAllocationsAllowed()) {
        GTEST_SKIP();
    }

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    MockAuxBuilInOp mockAuxBuiltInOp(*pBuiltIns, *pClDevice);

    BuiltIn::OpParams builtinOpParamsToAux;
    builtinOpParamsToAux.auxTranslationDirection = AuxTranslationDirection::nonAuxToAux;

    BuiltIn::OpParams builtinOpParamsToNonAux;
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
        auto expectedMocs = pDevice->getGmmHelper()->getUncachedMOCS();
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
        auto expectedMocs = pDevice->getGmmHelper()->getL3EnabledMOCS();
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

    BuiltIn::OpParams builtinOpParams;
    builtinOpParams.auxTranslationDirection = AuxTranslationDirection::auxToNonAux;

    std::unique_ptr<Buffer> buffer = nullptr;
    std::unique_ptr<GraphicsAllocation> gfxAllocation = nullptr;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::unique_ptr<Gmm>(new Gmm(pDevice->getGmmHelper(), nullptr, 1, 0, gmmResourceUsageOclBuffer, {}, gmmRequirements));
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

    BuiltIn::OpParams builtinOpParams;
    builtinOpParams.auxTranslationDirection = AuxTranslationDirection::nonAuxToAux;

    MockKernelObjForAuxTranslation mockKernelObjForAuxTranslation(kernelObjType);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(pDevice->getGmmHelper(), nullptr, 1, 0, gmmResourceUsageOclBuffer, StorageInfo{}, gmmRequirements);
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
    BuiltIn::DispatchInfoBuilder &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(copyBufferToBufferBuiltin, *pClDevice);

    AlignedBuffer src;
    AlignedBuffer dst;

    BuiltIn::OpParams builtinOpsParams;

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

    BuiltIn::DispatchInfoBuilder &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(copyBufferToBufferStatelessBuiltIn, *pClDevice);

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t bigOffset = 4ull * MemoryConstants::gigaByte;
    uint64_t size = 4ull * MemoryConstants::gigaByte;

    MockBuffer srcBuffer;
    srcBuffer.size = static_cast<size_t>(bigSize);
    MockBuffer dstBuffer;
    dstBuffer.size = static_cast<size_t>(bigSize);

    BuiltIn::OpParams builtinOpsParams;

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

    BuiltIn::DispatchInfoBuilder &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(copyBufferRectStatelessBuiltIn, *pClDevice);

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t bigOffset = 4ull * MemoryConstants::gigaByte;
    uint64_t size = 4ull * MemoryConstants::gigaByte;

    MockBuffer srcBuffer;
    srcBuffer.size = static_cast<size_t>(bigSize);
    MockBuffer dstBuffer;
    dstBuffer.size = static_cast<size_t>(bigSize);

    srcBuffer.mockGfxAllocation.setAllocationType(AllocationType::buffer);
    dstBuffer.mockGfxAllocation.setAllocationType(AllocationType::bufferHostMemory);

    BuiltIn::OpParams dc;
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

    BuiltIn::DispatchInfoBuilder &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(copyBufferRectStatelessBuiltIn, *pClDevice);

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t bigOffset = 4ull * MemoryConstants::gigaByte;
    uint64_t size = 4ull * MemoryConstants::gigaByte;

    MockBuffer srcBuffer;
    srcBuffer.size = static_cast<size_t>(bigSize);
    MockBuffer dstBuffer;
    dstBuffer.size = static_cast<size_t>(bigSize);

    srcBuffer.mockGfxAllocation.setAllocationType(AllocationType::bufferHostMemory);
    dstBuffer.mockGfxAllocation.setAllocationType(AllocationType::buffer);

    BuiltIn::OpParams dc;
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

    BuiltIn::DispatchInfoBuilder &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(copyBufferRectStatelessBuiltIn, *pClDevice);

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t size = 4ull * MemoryConstants::gigaByte;

    MockBuffer srcBuffer;
    srcBuffer.size = static_cast<size_t>(bigSize);
    MockBuffer dstBuffer;
    dstBuffer.size = static_cast<size_t>(bigSize);

    srcBuffer.mockGfxAllocation.setAllocationType(AllocationType::buffer);
    dstBuffer.mockGfxAllocation.setAllocationType(AllocationType::buffer);

    BuiltIn::OpParams dc;
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

    BuiltIn::DispatchInfoBuilder &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(fillBufferStatelessBuiltIn, *pClDevice);

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t bigOffset = 4ull * MemoryConstants::gigaByte;
    uint64_t size = 4ull * MemoryConstants::gigaByte;

    MockBuffer srcBuffer;
    srcBuffer.size = static_cast<size_t>(bigSize);
    MockBuffer dstBuffer;
    dstBuffer.size = static_cast<size_t>(bigSize);

    srcBuffer.mockGfxAllocation.setAllocationType(AllocationType::buffer);
    dstBuffer.mockGfxAllocation.setAllocationType(AllocationType::bufferHostMemory);

    BuiltIn::OpParams dc;
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

    BuiltIn::DispatchInfoBuilder &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(fillBufferStatelessBuiltIn, *pClDevice);

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t bigOffset = 4ull * MemoryConstants::gigaByte;
    uint64_t size = 4ull * MemoryConstants::gigaByte;

    MockBuffer srcBuffer;
    srcBuffer.size = static_cast<size_t>(bigSize);
    MockBuffer dstBuffer;
    dstBuffer.size = static_cast<size_t>(bigSize);

    srcBuffer.mockGfxAllocation.setAllocationType(AllocationType::bufferHostMemory);
    dstBuffer.mockGfxAllocation.setAllocationType(AllocationType::buffer);

    BuiltIn::OpParams dc;
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

TEST_F(BuiltInTests, givenSystemPtrWhenBuilderFillBufferStatelessIsUsedThenParamsAreCorrect) {
    BuiltIn::DispatchInfoBuilder &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(fillBufferStatelessBuiltIn, *pClDevice);

    size_t size = 1024 * 1024 * 1024;
    void *systemPtr = &size;

    MockBuffer srcBuffer;
    srcBuffer.size = 4; // pattern size

    BuiltIn::OpParams dc;
    dc.srcMemObj = &srcBuffer;
    dc.dstPtr = systemPtr;
    dc.dstOffset = {0, 0, 0};
    dc.size = {size, 0, 0};

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), dc));

    for (auto &dispatchInfo : multiDispatchInfo) {
        EXPECT_TRUE(dispatchInfo.getKernel()->getDestinationAllocationInSystemMemory());
    }
}

HWTEST2_F(BuiltInTests, givenBigOffsetAndSizeWhenBuilderCopyBufferToImageStatelessIsUsedThenParamsAreCorrect, IsHeapfulRequired) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t bigOffset = 4ull * MemoryConstants::gigaByte;

    MockBuffer srcBuffer;
    srcBuffer.size = static_cast<size_t>(bigSize);
    std ::unique_ptr<Image> pDstImage(Image2dHelperUlt<>::create(pContext));
    ASSERT_NE(nullptr, pDstImage.get());

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(copyBufferToImage3dStatelessBuiltIn, *pClDevice);

    BuiltIn::OpParams dc;
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

HWTEST2_F(BuiltInTests, givenHeaplessWhenBuilderCopyBufferToImageHeaplessIsUsedThenParamsAreCorrect, HeaplessSupport) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    MockBuffer buffer;
    std ::unique_ptr<Image> image(Image2dHelperUlt<>::create(pContext));
    ASSERT_NE(nullptr, image.get());

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(BuiltIn::Group::copyBufferToImage3dStatelessHeapless, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcPtr = &buffer;
    dc.dstMemObj = image.get();
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};
    dc.srcRowPitch = 0;
    dc.srcSlicePitch = 0;

    MultiDispatchInfo multiDispatchInfo(dc);
    EXPECT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), dc));
}

HWTEST2_F(BuiltInTests, givenHeaplessWhenBuilderCopyImageToBufferHeaplessIsUsedThenParamsAreCorrect, HeaplessSupport) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    MockBuffer buffer;
    std ::unique_ptr<Image> image(Image2dHelperUlt<>::create(pContext));
    ASSERT_NE(nullptr, image.get());

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(BuiltIn::Group::copyImage3dToBufferStatelessHeapless, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcMemObj = image.get();
    dc.dstPtr = &buffer;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};
    dc.dstRowPitch = 0;
    dc.dstSlicePitch = 0;

    MultiDispatchInfo multiDispatchInfo(dc);
    EXPECT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), dc));
}

HWTEST_F(BuiltInTests, WhenBuilderCopyImageToImageIsUsedThenParamsAreCorrect) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    std ::unique_ptr<Image> srcImage(Image2dHelperUlt<>::create(pContext));
    std ::unique_ptr<Image> dstImage(Image2dHelperUlt<>::create(pContext));

    ASSERT_NE(nullptr, srcImage.get());
    ASSERT_NE(nullptr, dstImage.get());

    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());
    auto builtin = BuiltIn::adjustImageBuiltinGroup<BuiltIn::Group::copyImageToImage3d>(heaplessAllowed);
    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtin, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcMemObj = srcImage.get();
    dc.dstMemObj = dstImage.get();

    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};

    MultiDispatchInfo multiDispatchInfo(dc);
    EXPECT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), dc));
}

HWTEST_F(BuiltInTests, WhenBuilderFillImageIsUsedThenParamsAreCorrect) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);
    MockBuffer fillColor;
    std ::unique_ptr<Image> image(Image2dHelperUlt<>::create(pContext));
    ASSERT_NE(nullptr, image.get());

    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());
    auto builtin = BuiltIn::adjustImageBuiltinGroup<BuiltIn::Group::fillImage3d>(heaplessAllowed);
    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtin, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcPtr = &fillColor;
    dc.dstMemObj = image.get();
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};

    MultiDispatchInfo multiDispatchInfo(dc);
    EXPECT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), dc));
}

HWTEST_F(BuiltInTests, WhenBuilderFillImage1dBufferIsUsedThenParamsAreCorrect) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);
    MockBuffer fillColor;
    std ::unique_ptr<Image> image(Image1dBufferHelperUlt<>::create(pContext));
    ASSERT_NE(nullptr, image.get());

    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());
    auto builtin = BuiltIn::adjustImageBuiltinGroup<BuiltIn::Group::fillImage1dBuffer>(heaplessAllowed);
    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtin, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcPtr = &fillColor;
    dc.dstMemObj = image.get();
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};

    MultiDispatchInfo multiDispatchInfo(dc);
    EXPECT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), dc));
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
    std ::unique_ptr<Image> pSrcImage(Image2dHelperUlt<>::create(pContext));
    ASSERT_NE(nullptr, pSrcImage.get());

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(copyImage3dToBufferStatelessBuiltIn, *pClDevice);

    BuiltIn::OpParams dc;
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
    std ::unique_ptr<Image> pSrcImage(Image2dHelperUlt<>::create(pContext));
    ASSERT_NE(nullptr, pSrcImage.get());

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(copyImage3dToBufferStatelessBuiltIn, *pClDevice);

    BuiltIn::OpParams dc;
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
    BuiltIn::DispatchInfoBuilder &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(copyBufferToBufferBuiltin, *pClDevice);

    AlignedBuffer src;
    AlignedBuffer dst;

    BuiltIn::OpParams builtinOpsParams;

    builtinOpsParams.srcMemObj = &src;
    builtinOpsParams.srcOffset.x = 5; // causes misalignment from 4-byte boundary by 1 byte (8 bits)
    builtinOpsParams.dstMemObj = &dst;
    builtinOpsParams.size = {src.getSize(), 0, 0};

    MultiDispatchInfo multiDispatchInfo(builtinOpsParams);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));

    EXPECT_EQ(1u, multiDispatchInfo.size());

    const Kernel *kernel = multiDispatchInfo.begin()->getKernel();

    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.starts_with("CopyBufferToBufferMiddleMisaligned"));

    const auto crossThreadData = kernel->getCrossThreadData();
    const auto crossThreadOffset = kernel->getKernelInfo().getArgDescriptorAt(4).as<ArgDescValue>().elements[0].offset;
    EXPECT_EQ(8u, *reinterpret_cast<uint32_t *>(ptrOffset(crossThreadData, crossThreadOffset)));

    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), builtinOpsParams));
}

TEST_F(BuiltInTests, GivenReadBufferAlignedWhenDispatchInfoIsCreatedThenParamsAreCorrect) {
    BuiltIn::DispatchInfoBuilder &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(copyBufferToBufferBuiltin, *pClDevice);

    AlignedBuffer srcMemObj;
    auto size = 10 * MemoryConstants::cacheLineSize;
    auto dstPtr = alignedMalloc(size, MemoryConstants::cacheLineSize);

    BuiltIn::OpParams builtinOpsParams;

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
    BuiltIn::DispatchInfoBuilder &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(copyBufferToBufferBuiltin, *pClDevice);

    auto size = 10 * MemoryConstants::cacheLineSize;
    auto srcPtr = alignedMalloc(size, MemoryConstants::cacheLineSize);
    AlignedBuffer dstMemObj;

    BuiltIn::OpParams builtinOpsParams;

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
    BuiltIn::DispatchInfoBuilder &builder1 = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(copyBufferToBufferBuiltin, *pClDevice);
    BuiltIn::DispatchInfoBuilder &builder2 = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(copyBufferToBufferBuiltin, *pClDevice);

    EXPECT_EQ(&builder1, &builder2);
}

HWTEST_F(BuiltInTests, GivenBuiltInOperationWhenGettingBuilderThenCorrectBuiltInBuilderIsReturned) {

    auto verifyBuilder = [&](auto operation) {
        auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(operation, *pClDevice);
        auto *expectedBuilder = pClDevice->peekBuilders()[static_cast<uint32_t>(operation)].first.get();

        EXPECT_EQ(expectedBuilder, &builder);
    };

    // heapless platforms
    if (FamilyType::isHeaplessRequired()) {

        BuiltIn::Group operationsBuffers[] = {
            BuiltIn::Group::copyBufferToBufferStatelessHeapless,
            BuiltIn::Group::copyBufferRectStatelessHeapless,
            BuiltIn::Group::fillBufferStatelessHeapless};

        BuiltIn::Group operationsImages[] = {
            BuiltIn::Group::copyBufferToImage3dStatelessHeapless,
            BuiltIn::Group::copyImage3dToBufferStatelessHeapless,
            BuiltIn::Group::copyImageToImage3dHeapless,
            BuiltIn::Group::fillImage3dHeapless,
            BuiltIn::Group::fillImage1dBufferHeapless};

        for (auto operation : operationsBuffers) {
            verifyBuilder(operation);
        }

        if (pClDevice->getHardwareInfo().capabilityTable.supportsImages) {
            for (auto operation : operationsImages) {
                verifyBuilder(operation);
            }
        }

        return;
    }

    // stateless preferred platforms has stateless and stateful builtIns
    if (pClDevice->getCompilerProductHelper().isForceToStatelessRequired()) {
        BuiltIn::Group operationsBuffers[] = {BuiltIn::Group::copyBufferToBufferStateless,
                                              BuiltIn::Group::copyBufferRectStateless,
                                              BuiltIn::Group::fillBufferStateless};

        BuiltIn::Group operationsImages[] = {
            BuiltIn::Group::copyBufferToImage3dStateless,
            BuiltIn::Group::copyImage3dToBufferStateless};

        for (auto operation : operationsBuffers) {
            verifyBuilder(operation);
        }

        if (pClDevice->getHardwareInfo().capabilityTable.supportsImages) {
            for (auto operation : operationsImages) {
                verifyBuilder(operation);
            }
        }
    }

    {
        BuiltIn::Group operationsBuffers[] = {BuiltIn::Group::copyBufferToBuffer,
                                              BuiltIn::Group::copyBufferRect,
                                              BuiltIn::Group::fillBuffer};

        BuiltIn::Group operationsImages[] = {
            BuiltIn::Group::copyBufferToImage3d,
            BuiltIn::Group::copyImage3dToBuffer,
            BuiltIn::Group::copyImageToImage3d,
            BuiltIn::Group::fillImage3d,
            BuiltIn::Group::fillImage1dBuffer};

        for (auto operation : operationsBuffers) {
            verifyBuilder(operation);
        }

        if (pClDevice->getHardwareInfo().capabilityTable.supportsImages) {
            for (auto operation : operationsImages) {
                verifyBuilder(operation);
            }
        }
    }
}

TEST_F(BuiltInTests, GivenUnknownBuiltInOpWhenGettingBuilderInfoThenExceptionThrown) {
    EXPECT_THROW(
        BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(BuiltIn::Group::count, *pClDevice),
        std::runtime_error);
}

TEST_F(BuiltInTests, GivenUnsupportedBuildTypeWhenBuildingDispatchInfoThenFalseIsReturned) {
    auto &builtIns = *pDevice->getBuiltIns();
    BuiltIn::DispatchInfoBuilder dispatchInfoBuilder{builtIns, *pClDevice};
    BuiltIn::OpParams params;

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
    BuiltIn::DispatchInfoBuilder dispatchInfoBuilder{builtIns, *pClDevice};
    auto ret = dispatchInfoBuilder.validateDispatch(nullptr, 1, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
    EXPECT_EQ(CL_SUCCESS, ret);
}

TEST_F(BuiltInTests, WhenSettingExplictArgThenTrueIsReturned) {
    auto &builtIns = *pDevice->getBuiltIns();
    BuiltIn::DispatchInfoBuilder dispatchInfoBuilder{builtIns, *pClDevice};
    MultiDispatchInfo multiDispatchInfo;
    BuiltIn::OpParams params;

    cl_int err;
    auto ret = dispatchInfoBuilder.setExplicitArg(1, 5, nullptr, err);
    EXPECT_TRUE(ret);
}

TEST_F(BuiltInTests, GivenEncodeTypeWhenGettingExtensionThenCorrectStringIsReturned) {
    EXPECT_EQ(0, strcmp("", BuiltIn::Code::getExtension(BuiltIn::CodeType::any)));
    EXPECT_EQ(0, strcmp(".bin", BuiltIn::Code::getExtension(BuiltIn::CodeType::binary)));
    EXPECT_EQ(0, strcmp(".spv", BuiltIn::Code::getExtension(BuiltIn::CodeType::intermediate)));
    EXPECT_EQ(0, strcmp(".cl", BuiltIn::Code::getExtension(BuiltIn::CodeType::source)));
    EXPECT_EQ(0, strcmp("", BuiltIn::Code::getExtension(BuiltIn::CodeType::count)));
    EXPECT_EQ(0, strcmp("", BuiltIn::Code::getExtension(BuiltIn::CodeType::invalid)));
}

TEST_F(BuiltInTests, GivenBuiltinResourceWhenCreatingBuiltinResourceThenSizesAreEqual) {
    std::string resource = "__kernel";

    auto br1 = BuiltIn::createResource(resource.data(), resource.size());
    EXPECT_NE(0u, br1.size());
    auto br2 = BuiltIn::createResource(br1);
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
    class MockEmbeddedStorageRegistry : public BuiltIn::EmbeddedStorageRegistry {
        using BuiltIn::EmbeddedStorageRegistry::EmbeddedStorageRegistry;
    };
    MockEmbeddedStorageRegistry storageRegistry;

    std::string resource = "__kernel";
    storageRegistry.store("kernel.cl", BuiltIn::createResource(resource.data(), resource.size() + 1));

    const BuiltIn::Resource *br = storageRegistry.get("kernel.cl");
    EXPECT_NE(nullptr, br);
    EXPECT_EQ(0, strcmp(resource.data(), br->data()));

    const BuiltIn::Resource *bnr = storageRegistry.get("unknown.cl");
    EXPECT_EQ(nullptr, bnr);
}

TEST_F(BuiltInTests, WhenStoringRootPathThenPathIsSavedCorrectly) {
    class MockStorage : BuiltIn::Storage {
      public:
        MockStorage(const std::string &rootPath) : BuiltIn::Storage(rootPath) {};
        std::string &getRootPath() {
            return BuiltIn::Storage::rootPath;
        }

      protected:
        BuiltIn::Resource loadImpl(const std::string &fullResourceName) override {
            BuiltIn::Resource ret;
            return ret;
        }
    };
    const std::string rootPath("root");
    MockStorage mockStorage(rootPath);
    EXPECT_EQ(0, strcmp(rootPath.data(), mockStorage.getRootPath().data()));
}

TEST_F(BuiltInTests, GivenFiledNameWhenLoadingImplKernelFromEmbeddedStorageRegistryThenValidPtrIsReturnedForExisitngKernels) {
    class MockEmbeddedStorage : BuiltIn::EmbeddedStorage {
      public:
        MockEmbeddedStorage(const std::string &rootPath) : BuiltIn::EmbeddedStorage(rootPath) {};
        BuiltIn::Resource loadImpl(const std::string &fullResourceName) override {
            return BuiltIn::EmbeddedStorage::loadImpl(fullResourceName);
        }
    };
    MockEmbeddedStorage mockEmbeddedStorage("root");

    BuiltIn::Resource br = mockEmbeddedStorage.loadImpl("copy_buffer_to_buffer.builtin_kernel.cl");
    EXPECT_NE(0u, br.size());

    BuiltIn::Resource bnr = mockEmbeddedStorage.loadImpl("unknown.cl");
    EXPECT_EQ(0u, bnr.size());
}

TEST_F(BuiltInTests, WhenBuiltInLibraryIsCreatedThenAllStoragesSizeIsTwo) {
    auto mockBuiltInLibrary = std::unique_ptr<MockBuiltInResourceLoader>(new MockBuiltInResourceLoader());
    EXPECT_EQ(2u, mockBuiltInLibrary->allStorages.size());
}

TEST_F(BuiltInTests, GivenTypeAnyWhenGettingBuiltinCodeThenCorrectBuiltinReturned) {
    auto builtinsLib = std::unique_ptr<BuiltIn::ResourceLoader>(new BuiltIn::ResourceLoader());
    BuiltIn::Code code = builtinsLib->getBuiltinCode(BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyBufferToBuffer>(false, pDevice->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)), BuiltIn::CodeType::any, *pDevice);
    EXPECT_EQ(BuiltIn::CodeType::binary, code.type);
    EXPECT_NE(0u, code.resource.size());
    EXPECT_EQ(pDevice, code.targetDevice);
}

TEST_F(BuiltInTests, GivenTypeBinaryWhenGettingBuiltinCodeThenCorrectBuiltinReturned) {
    auto builtinsLib = std::unique_ptr<BuiltIn::ResourceLoader>(new BuiltIn::ResourceLoader());
    BuiltIn::Code code = builtinsLib->getBuiltinCode(BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyBufferToBuffer>(false, pDevice->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)), BuiltIn::CodeType::binary, *pDevice);
    EXPECT_EQ(BuiltIn::CodeType::binary, code.type);
    EXPECT_NE(0u, code.resource.size());
    EXPECT_EQ(pDevice, code.targetDevice);
}

TEST_F(BuiltInTests, GivenTypeIntermediateWhenGettingBuiltinCodeThenCorrectBuiltinReturned) {
    auto builtinsLib = std::unique_ptr<BuiltIn::ResourceLoader>(new BuiltIn::ResourceLoader());
    BuiltIn::Code code = builtinsLib->getBuiltinCode(BuiltIn::Group::copyBufferToBuffer, BuiltIn::CodeType::intermediate, *pDevice);
    EXPECT_EQ(BuiltIn::CodeType::intermediate, code.type);
    EXPECT_EQ(0u, code.resource.size());
    EXPECT_EQ(pDevice, code.targetDevice);
}

TEST_F(BuiltInTests, GivenTypeSourceWhenGettingBuiltinCodeThenCorrectBuiltinReturned) {
    auto builtinsLib = std::unique_ptr<BuiltIn::ResourceLoader>(new BuiltIn::ResourceLoader());
    BuiltIn::Code code = builtinsLib->getBuiltinCode(BuiltIn::Group::copyBufferToBuffer, BuiltIn::CodeType::source, *pDevice);
    EXPECT_EQ(BuiltIn::CodeType::source, code.type);
    EXPECT_NE(0u, code.resource.size());
    EXPECT_EQ(pDevice, code.targetDevice);
}

TEST_F(BuiltInTests, GivenTypeInvalidWhenGettingBuiltinCodeThenKernelIsEmpty) {
    auto builtinsLib = std::unique_ptr<BuiltIn::ResourceLoader>(new BuiltIn::ResourceLoader());
    BuiltIn::Code code = builtinsLib->getBuiltinCode(BuiltIn::Group::copyBufferToBuffer, BuiltIn::CodeType::invalid, *pDevice);
    EXPECT_EQ(BuiltIn::CodeType::invalid, code.type);
    EXPECT_EQ(0u, code.resource.size());
    EXPECT_EQ(pDevice, code.targetDevice);
}

HWTEST2_F(BuiltInTests, GivenImagesAndHeaplessBuiltinTypeSourceWhenGettingBuiltinResourceThenResourceSizeIsNonZero, HeaplessSupport) {

    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);
    auto mockBuiltInLibrary = std::unique_ptr<MockBuiltInResourceLoader>(new MockBuiltInResourceLoader());

    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyBufferToImage3dStatelessHeapless, BuiltIn::CodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyImage3dToBufferStatelessHeapless, BuiltIn::CodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyImageToImage1dHeapless, BuiltIn::CodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyImageToImage2dHeapless, BuiltIn::CodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyImageToImage3dHeapless, BuiltIn::CodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::fillImage1dHeapless, BuiltIn::CodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::fillImage2dHeapless, BuiltIn::CodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::fillImage3dHeapless, BuiltIn::CodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::fillImage1dBufferHeapless, BuiltIn::CodeType::binary, *pDevice).size());
}

HWTEST2_F(BuiltInTests, GivenHeaplessBuiltinTypeSourceWhenGettingBuiltinResourceThenResourceSizeIsNonZero, HeaplessSupport) {
    auto mockBuiltInLibrary = std::unique_ptr<MockBuiltInResourceLoader>(new MockBuiltInResourceLoader());

    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyBufferToBufferStatelessHeapless, BuiltIn::CodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyBufferRectStatelessHeapless, BuiltIn::CodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::fillBufferStatelessHeapless, BuiltIn::CodeType::binary, *pDevice).size());
}

TEST_F(BuiltInTests, GivenBuiltinTypeSourceWhenGettingBuiltinResourceThenResourceSizeIsNonZero) {
    auto mockBuiltInLibrary = std::unique_ptr<MockBuiltInResourceLoader>(new MockBuiltInResourceLoader());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::auxTranslation, BuiltIn::CodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyBufferToBuffer, BuiltIn::CodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyBufferRect, BuiltIn::CodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::fillBuffer, BuiltIn::CodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyBufferToImage3d, BuiltIn::CodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyImage3dToBuffer, BuiltIn::CodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyImageToImage1d, BuiltIn::CodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyImageToImage2d, BuiltIn::CodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyImageToImage3d, BuiltIn::CodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::fillImage1d, BuiltIn::CodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::fillImage2d, BuiltIn::CodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::fillImage3d, BuiltIn::CodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::fillImage1dBuffer, BuiltIn::CodeType::source, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::count, BuiltIn::CodeType::source, *pDevice).size());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, BuiltInTests, GivenBuiltinTypeBinaryWhenGettingBuiltinResourceThenResourceSizeIsNonZero) {
    auto mockBuiltInLibrary = std::unique_ptr<MockBuiltInResourceLoader>(new MockBuiltInResourceLoader());

    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyBufferToBuffer, BuiltIn::CodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyBufferRect, BuiltIn::CodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::fillBuffer, BuiltIn::CodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyBufferToImage3d, BuiltIn::CodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyImage3dToBuffer, BuiltIn::CodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyImageToImage1d, BuiltIn::CodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyImageToImage2d, BuiltIn::CodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyImageToImage3d, BuiltIn::CodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::fillImage1d, BuiltIn::CodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::fillImage2d, BuiltIn::CodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::fillImage3d, BuiltIn::CodeType::binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::fillImage1dBuffer, BuiltIn::CodeType::binary, *pDevice).size());

    EXPECT_EQ(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::count, BuiltIn::CodeType::binary, *pDevice).size());
}

TEST_F(BuiltInTests, GivenBuiltinTypeSourceWhenGettingBuiltinResourceForNotRegisteredRevisionThenResourceSizeIsNonZero) {
    pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->platform.usRevId += 0xdead;
    auto mockBuiltInLibrary = std::unique_ptr<MockBuiltInResourceLoader>(new MockBuiltInResourceLoader());

    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyBufferToBuffer, BuiltIn::CodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyBufferRect, BuiltIn::CodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::fillBuffer, BuiltIn::CodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyBufferToImage3d, BuiltIn::CodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyImage3dToBuffer, BuiltIn::CodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyImageToImage1d, BuiltIn::CodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyImageToImage2d, BuiltIn::CodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::copyImageToImage3d, BuiltIn::CodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::fillImage1d, BuiltIn::CodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::fillImage2d, BuiltIn::CodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::fillImage3d, BuiltIn::CodeType::source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::fillImage1dBuffer, BuiltIn::CodeType::source, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltInLibrary->getBuiltinResource(BuiltIn::Group::count, BuiltIn::CodeType::source, *pDevice).size());
}

TEST_F(BuiltInTests, GivenTypeAnyWhenCreatingProgramFromCodeThenValidPointerIsReturned) {
    auto builtinsLib = std::unique_ptr<BuiltIn::ResourceLoader>(new BuiltIn::ResourceLoader());
    const BuiltIn::Code bc = builtinsLib->getBuiltinCode(BuiltIn::Group::copyBufferToBuffer, BuiltIn::CodeType::any, *pDevice);
    EXPECT_NE(0u, bc.resource.size());
    auto program = std::unique_ptr<Program>(BuiltIn::DispatchInfoBuilder::createProgramFromCode(bc, toClDeviceVector(*pClDevice)));
    EXPECT_NE(nullptr, program.get());
}

TEST_F(BuiltInTests, GivenTypeSourceWhenCreatingProgramFromCodeThenValidPointerIsReturned) {
    auto builtinsLib = std::unique_ptr<BuiltIn::ResourceLoader>(new BuiltIn::ResourceLoader());
    const BuiltIn::Code bc = builtinsLib->getBuiltinCode(BuiltIn::Group::copyBufferToBuffer, BuiltIn::CodeType::source, *pDevice);
    EXPECT_NE(0u, bc.resource.size());
    auto program = std::unique_ptr<Program>(BuiltIn::DispatchInfoBuilder::createProgramFromCode(bc, toClDeviceVector(*pClDevice)));
    EXPECT_NE(nullptr, program.get());
}

TEST_F(BuiltInTests, givenCreateProgramFromSourceWhenForceToStatelessRequiredOr32BitThenInternalOptionsHasGreaterThan4gbBuffersRequired) {
    auto builtinsLib = std::unique_ptr<BuiltIn::ResourceLoader>(new BuiltIn::ResourceLoader());

    const BuiltIn::Code bc = builtinsLib->getBuiltinCode(BuiltIn::Group::copyBufferToBuffer, BuiltIn::CodeType::source, *pDevice);
    EXPECT_NE(0u, bc.resource.size());
    auto program = std::unique_ptr<Program>(BuiltIn::DispatchInfoBuilder::createProgramFromCode(bc, toClDeviceVector(*pClDevice)));
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
    auto builtinsLib = std::unique_ptr<BuiltIn::ResourceLoader>(new BuiltIn::ResourceLoader());
    const BuiltIn::Code bc = builtinsLib->getBuiltinCode(BuiltIn::Group::copyBufferToBuffer, BuiltIn::CodeType::intermediate, *pDevice);
    EXPECT_EQ(0u, bc.resource.size());
    auto program = std::unique_ptr<Program>(BuiltIn::DispatchInfoBuilder::createProgramFromCode(bc, toClDeviceVector(*pClDevice)));
    EXPECT_EQ(nullptr, program.get());
}

TEST_F(BuiltInTests, GivenTypeBinaryWhenCreatingProgramFromCodeThenValidPointerIsReturned) {
    auto builtinsLib = std::unique_ptr<BuiltIn::ResourceLoader>(new BuiltIn::ResourceLoader());
    const BuiltIn::Code bc = builtinsLib->getBuiltinCode(BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyBufferToBuffer>(false, pDevice->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)), BuiltIn::CodeType::binary, *pDevice);
    EXPECT_NE(0u, bc.resource.size());
    auto program = std::unique_ptr<Program>(BuiltIn::DispatchInfoBuilder::createProgramFromCode(bc, toClDeviceVector(*pClDevice)));
    EXPECT_NE(nullptr, program.get());
}

TEST_F(BuiltInTests, GivenTypeInvalidWhenCreatingProgramFromCodeThenNullPointerIsReturned) {
    auto builtinsLib = std::unique_ptr<BuiltIn::ResourceLoader>(new BuiltIn::ResourceLoader());
    const BuiltIn::Code bc = builtinsLib->getBuiltinCode(BuiltIn::Group::copyBufferToBuffer, BuiltIn::CodeType::invalid, *pDevice);
    EXPECT_EQ(0u, bc.resource.size());
    auto program = std::unique_ptr<Program>(BuiltIn::DispatchInfoBuilder::createProgramFromCode(bc, toClDeviceVector(*pClDevice)));
    EXPECT_EQ(nullptr, program.get());
}

TEST_F(BuiltInTests, GivenInvalidBuiltinWhenCreatingProgramFromCodeThenNullPointerIsReturned) {
    auto builtinsLib = std::unique_ptr<BuiltIn::ResourceLoader>(new BuiltIn::ResourceLoader());
    const BuiltIn::Code bc = builtinsLib->getBuiltinCode(BuiltIn::Group::count, BuiltIn::CodeType::any, *pDevice);
    EXPECT_EQ(0u, bc.resource.size());
    auto program = std::unique_ptr<Program>(BuiltIn::DispatchInfoBuilder::createProgramFromCode(bc, toClDeviceVector(*pClDevice)));
    EXPECT_EQ(nullptr, program.get());
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
    auto builtinsLib = std::unique_ptr<BuiltIn::ResourceLoader>(new BuiltIn::ResourceLoader());
    BuiltIn::Code code = builtinsLib->getBuiltinCode(BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyBufferToBuffer>(false, pDevice->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)), BuiltIn::CodeType::binary, *pDevice);
    EXPECT_EQ(BuiltIn::CodeType::binary, code.type);
    EXPECT_NE(0u, code.resource.size());
    EXPECT_EQ(pDevice, code.targetDevice);
}

TEST_F(BuiltInTests, givenDebugFlagForceUseSourceWhenArgIsAnyThenReturnBuiltinCodeSource) {
    debugManager.flags.RebuildPrecompiledKernels.set(true);
    auto builtinsLib = std::unique_ptr<BuiltIn::ResourceLoader>(new BuiltIn::ResourceLoader());
    BuiltIn::Code code = builtinsLib->getBuiltinCode(BuiltIn::Group::copyBufferToBuffer, BuiltIn::CodeType::any, *pDevice);
    EXPECT_EQ(BuiltIn::CodeType::source, code.type);
    EXPECT_NE(0u, code.resource.size());
    EXPECT_EQ(pDevice, code.targetDevice);
}

TEST_F(BuiltInTests, givenOneApiPvcSendWarWaEnvFalseWhenGettingBuiltinCodeThenSourceCodeTypeIsUsed) {
    pDevice->getExecutionEnvironment()->setOneApiPvcWaEnv(false);
    auto builtinsLib = std::unique_ptr<BuiltIn::ResourceLoader>(new BuiltIn::ResourceLoader());
    BuiltIn::Code code = builtinsLib->getBuiltinCode(BuiltIn::Group::copyBufferToBuffer, BuiltIn::CodeType::any, *pDevice);
    EXPECT_EQ(BuiltIn::CodeType::source, code.type);
    EXPECT_NE(0u, code.resource.size());
    EXPECT_EQ(pDevice, code.targetDevice);
}

using BuiltInOwnershipWrapperTests = BuiltInTests;

HWTEST2_F(BuiltInOwnershipWrapperTests, givenBuiltinWhenConstructedThenLockAndUnlockOnDestruction, AuxBuiltinsMatcher) {
    MockAuxBuilInOp mockAuxBuiltInOp(*pBuiltIns, *pClDevice);
    MockContext context(pClDevice);
    {
        EXPECT_EQ(nullptr, mockAuxBuiltInOp.baseKernel->getProgram()->getContextPtr());
        BuiltIn::OwnershipWrapper lock(mockAuxBuiltInOp, &context);
        EXPECT_TRUE(mockAuxBuiltInOp.baseKernel->getMultiDeviceKernel()->hasOwnership());
        EXPECT_TRUE(mockAuxBuiltInOp.baseKernel->getProgram()->hasOwnership());
        EXPECT_EQ(&context, mockAuxBuiltInOp.baseKernel->getProgram()->getContextPtr());
    }
    EXPECT_FALSE(mockAuxBuiltInOp.baseKernel->getMultiDeviceKernel()->hasOwnership());
    EXPECT_FALSE(mockAuxBuiltInOp.baseKernel->getProgram()->hasOwnership());
    EXPECT_EQ(nullptr, mockAuxBuiltInOp.baseKernel->getProgram()->getContextPtr());
}

HWTEST2_F(BuiltInOwnershipWrapperTests, givenLockWithoutParametersWhenConstructingThenLockOnlyWhenRequested, AuxBuiltinsMatcher) {
    MockAuxBuilInOp mockAuxBuiltInOp(*pBuiltIns, *pClDevice);
    MockContext context(pClDevice);
    {
        BuiltIn::OwnershipWrapper lock;
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

HWTEST2_F(BuiltInOwnershipWrapperTests, givenLockWithAcquiredOwnershipWhenTakeOwnershipCalledThenAbort, AuxBuiltinsMatcher) {
    MockAuxBuilInOp mockAuxBuiltInOp1(*pBuiltIns, *pClDevice);
    MockAuxBuilInOp mockAuxBuiltInOp2(*pBuiltIns, *pClDevice);
    MockContext context(pClDevice);

    BuiltIn::OwnershipWrapper lock(mockAuxBuiltInOp1, &context);
    EXPECT_THROW(lock.takeOwnership(mockAuxBuiltInOp1, &context), std::exception);
    EXPECT_THROW(lock.takeOwnership(mockAuxBuiltInOp2, &context), std::exception);
}

HWTEST_F(BuiltInOwnershipWrapperTests, givenBuiltInOwnershipWrapperWhenAskedForTypeTraitsThenDisableCopyConstructorAndOperator) {
    EXPECT_FALSE(std::is_copy_constructible<BuiltIn::OwnershipWrapper>::value);
    EXPECT_FALSE(std::is_copy_assignable<BuiltIn::OwnershipWrapper>::value);
}

HWTEST2_F(BuiltInTests, whenBuilderCopyBufferToBufferStatelessHeaplessIsUsedThenParamsAreCorrect, HeaplessSupport) {

    if (is32bit) {
        GTEST_SKIP();
    }

    BuiltIn::DispatchInfoBuilder &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(BuiltIn::Group::copyBufferToBufferStatelessHeapless, *pClDevice);

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t bigOffset = 4ull * MemoryConstants::gigaByte;
    uint64_t size = 4ull * MemoryConstants::gigaByte;

    MockBuffer srcBuffer;
    srcBuffer.size = static_cast<size_t>(bigSize);
    MockBuffer dstBuffer;
    dstBuffer.size = static_cast<size_t>(bigSize);

    BuiltIn::OpParams builtinOpsParams;

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

HWTEST2_F(BuiltInTests, whenBuilderCopyBufferToSystemBufferRectStatelessHeaplessIsUsedThenParamsAreCorrect, HeaplessSupport) {
    if (is32bit) {
        GTEST_SKIP();
    }

    BuiltIn::DispatchInfoBuilder &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(BuiltIn::Group::copyBufferRectStatelessHeapless, *pClDevice);

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t bigOffset = 4ull * MemoryConstants::gigaByte;
    uint64_t size = 4ull * MemoryConstants::gigaByte;

    MockBuffer srcBuffer;
    srcBuffer.size = static_cast<size_t>(bigSize);
    MockBuffer dstBuffer;
    dstBuffer.size = static_cast<size_t>(bigSize);

    srcBuffer.mockGfxAllocation.setAllocationType(AllocationType::buffer);
    dstBuffer.mockGfxAllocation.setAllocationType(AllocationType::bufferHostMemory);

    BuiltIn::OpParams dc;
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

HWTEST2_F(BuiltInTests, whenBuilderCopyBufferToLocalBufferRectStatelessHeaplessIsUsedThenParamsAreCorrect, HeaplessSupport) {
    if (is32bit) {
        GTEST_SKIP();
    }

    BuiltIn::DispatchInfoBuilder &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(BuiltIn::Group::copyBufferRectStatelessHeapless, *pClDevice);

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t bigOffset = 4ull * MemoryConstants::gigaByte;
    uint64_t size = 4ull * MemoryConstants::gigaByte;

    MockBuffer srcBuffer;
    srcBuffer.size = static_cast<size_t>(bigSize);
    MockBuffer dstBuffer;
    dstBuffer.size = static_cast<size_t>(bigSize);

    srcBuffer.mockGfxAllocation.setAllocationType(AllocationType::bufferHostMemory);
    dstBuffer.mockGfxAllocation.setAllocationType(AllocationType::buffer);

    BuiltIn::OpParams dc;
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

HWTEST2_F(BuiltInTests, whenBuilderFillSystemBufferStatelessHeaplessIsUsedThenParamsAreCorrect, HeaplessSupport) {
    if (is32bit) {
        GTEST_SKIP();
    }

    BuiltIn::DispatchInfoBuilder &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(BuiltIn::Group::fillBufferStatelessHeapless, *pClDevice);

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t bigOffset = 4ull * MemoryConstants::gigaByte;
    uint64_t size = 4ull * MemoryConstants::gigaByte;

    MockBuffer srcBuffer;
    srcBuffer.size = static_cast<size_t>(bigSize);
    MockBuffer dstBuffer;
    dstBuffer.size = static_cast<size_t>(bigSize);

    srcBuffer.mockGfxAllocation.setAllocationType(AllocationType::buffer);
    dstBuffer.mockGfxAllocation.setAllocationType(AllocationType::bufferHostMemory);

    BuiltIn::OpParams dc;
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

HWTEST2_F(BuiltInTests, whenBuilderFillLocalBufferStatelessHeaplessIsUsedThenParamsAreCorrect, HeaplessSupport) {
    if (is32bit) {
        GTEST_SKIP();
    }

    BuiltIn::DispatchInfoBuilder &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(BuiltIn::Group::fillBufferStatelessHeapless, *pClDevice);

    uint64_t bigSize = 10ull * MemoryConstants::gigaByte;
    uint64_t bigOffset = 4ull * MemoryConstants::gigaByte;
    uint64_t size = 4ull * MemoryConstants::gigaByte;

    MockBuffer srcBuffer;
    srcBuffer.size = static_cast<size_t>(bigSize);
    MockBuffer dstBuffer;
    dstBuffer.size = static_cast<size_t>(bigSize);

    srcBuffer.mockGfxAllocation.setAllocationType(AllocationType::bufferHostMemory);
    dstBuffer.mockGfxAllocation.setAllocationType(AllocationType::buffer);

    BuiltIn::OpParams dc;
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

HWTEST_F(BuiltInTests, givenCopyBufferToImage16BytesAlignedSrcPtrWhenBuilderIsUsedThenAlignedKernelIsSelected) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    bool isStateless = compilerProductHelper.isForceToStatelessRequired();
    bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());
    auto builtInGroup = BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyBufferToImage3d>(isStateless, heaplessAllowed);

    cl_image_format imageFormat = {CL_RGBA, CL_FLOAT};
    cl_image_desc imageDesc = {CL_MEM_OBJECT_IMAGE3D, 4, 4, 4, 1, 0, 0, 0, 0, {nullptr}};

    auto image = ImageHelperUlt<Image3dDefaults>::create(pContext, &imageDesc, &imageFormat);
    ASSERT_NE(nullptr, image);

    constexpr size_t bufferSize = 1024;
    alignas(16) uint8_t alignedBuffer[bufferSize];
    void *srcPtr = alignedBuffer;
    ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(srcPtr) & 0x0000000F);

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInGroup, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcPtr = srcPtr;
    dc.dstMemObj = image;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};
    dc.srcRowPitch = 0;
    dc.srcSlicePitch = 0;

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("16Bytes") != std::string::npos);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("Aligned") != std::string::npos);

    delete image;
}

HWTEST_F(BuiltInTests, givenCopyBufferToImage16BytesAlignedSrcMemObjWhenBuilderIsUsedThenAlignedKernelIsSelected) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    bool isStateless = compilerProductHelper.isForceToStatelessRequired();
    bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());
    auto builtInGroup = BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyBufferToImage3d>(isStateless, heaplessAllowed);

    cl_image_format imageFormat = {CL_RGBA, CL_FLOAT};
    cl_image_desc imageDesc = {CL_MEM_OBJECT_IMAGE3D, 4, 4, 4, 1, 0, 0, 0, 0, {nullptr}};

    auto image = ImageHelperUlt<Image3dDefaults>::create(pContext, &imageDesc, &imageFormat);
    ASSERT_NE(nullptr, image);

    MockBuffer srcBuffer;
    srcBuffer.size = 1024;
    srcBuffer.mockGfxAllocation.gpuAddress = 0x1000;
    ASSERT_EQ(0u, srcBuffer.mockGfxAllocation.gpuAddress & 0x0000000F);

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInGroup, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcMemObj = &srcBuffer;
    dc.dstMemObj = image;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};
    dc.srcRowPitch = 0;
    dc.srcSlicePitch = 0;

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("16Bytes") != std::string::npos);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("Aligned") != std::string::npos);

    delete image;
}

HWTEST_F(BuiltInTests, givenCopyBufferToImage16BytesWithAddressMisalignedSrcPtrWhenBuilderIsUsedThenRegularKernelIsSelected) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    bool isStateless = compilerProductHelper.isForceToStatelessRequired();
    bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());
    auto builtInGroup = BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyBufferToImage3d>(isStateless, heaplessAllowed);

    cl_image_format imageFormat = {CL_RGBA, CL_FLOAT};
    cl_image_desc imageDesc = {CL_MEM_OBJECT_IMAGE3D, 4, 4, 4, 1, 0, 0, 0, 0, {nullptr}};

    auto image = ImageHelperUlt<Image3dDefaults>::create(pContext, &imageDesc, &imageFormat);
    ASSERT_NE(nullptr, image);

    constexpr size_t bufferSize = 1024;
    uint8_t buffer[bufferSize + 16];
    void *srcPtr = buffer + 1;
    ASSERT_NE(0u, reinterpret_cast<uintptr_t>(srcPtr) & 0x0000000F);

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInGroup, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcPtr = srcPtr;
    dc.dstMemObj = image;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};
    dc.srcRowPitch = 0;
    dc.srcSlicePitch = 0;

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("16Bytes") != std::string::npos);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("Aligned") == std::string::npos);

    delete image;
}

HWTEST_F(BuiltInTests, givenCopyImage3dToBuffer16BytesAlignedDstPtrWhenBuilderIsUsedThenAlignedKernelIsSelected) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    bool isStateless = compilerProductHelper.isForceToStatelessRequired();
    bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());
    auto builtInGroup = BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyImage3dToBuffer>(isStateless, heaplessAllowed);

    cl_image_format imageFormat = {CL_RGBA, CL_FLOAT};
    cl_image_desc imageDesc = {CL_MEM_OBJECT_IMAGE3D, 4, 4, 4, 1, 0, 0, 0, 0, {nullptr}};

    auto image = ImageHelperUlt<Image3dDefaults>::create(pContext, &imageDesc, &imageFormat);
    ASSERT_NE(nullptr, image);

    constexpr size_t bufferSize = 1024;
    alignas(16) uint8_t alignedBuffer[bufferSize];
    void *dstPtr = alignedBuffer;
    ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(dstPtr) & 0x0000000F);

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInGroup, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcMemObj = image;
    dc.dstPtr = dstPtr;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};
    dc.dstRowPitch = 0;
    dc.dstSlicePitch = 0;

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("16Bytes") != std::string::npos);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("Aligned") != std::string::npos);

    delete image;
}

HWTEST_F(BuiltInTests, givenCopyImage3dToBuffer16BytesAlignedDstMemObjWhenBuilderIsUsedThenAlignedKernelIsSelected) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    bool isStateless = compilerProductHelper.isForceToStatelessRequired();
    bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());
    auto builtInGroup = BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyImage3dToBuffer>(isStateless, heaplessAllowed);

    cl_image_format imageFormat = {CL_RGBA, CL_FLOAT};
    cl_image_desc imageDesc = {CL_MEM_OBJECT_IMAGE3D, 4, 4, 4, 1, 0, 0, 0, 0, {nullptr}};

    auto image = ImageHelperUlt<Image3dDefaults>::create(pContext, &imageDesc, &imageFormat);
    ASSERT_NE(nullptr, image);

    MockBuffer dstBuffer;
    dstBuffer.size = 1024;
    dstBuffer.mockGfxAllocation.gpuAddress = 0x2000;
    ASSERT_EQ(0u, dstBuffer.mockGfxAllocation.gpuAddress & 0x0000000F);

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInGroup, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcMemObj = image;
    dc.dstMemObj = &dstBuffer;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};
    dc.dstRowPitch = 0;
    dc.dstSlicePitch = 0;

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("16Bytes") != std::string::npos);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("Aligned") != std::string::npos);

    delete image;
}

HWTEST_F(BuiltInTests, givenCopyImage3dToBuffer16BytesWithAddressMisalignedDstPtrWhenBuilderIsUsedThenRegularKernelIsSelected) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    bool isStateless = compilerProductHelper.isForceToStatelessRequired();
    bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());
    auto builtInGroup = BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyImage3dToBuffer>(isStateless, heaplessAllowed);

    cl_image_format imageFormat = {CL_RGBA, CL_FLOAT};
    cl_image_desc imageDesc = {CL_MEM_OBJECT_IMAGE3D, 4, 4, 4, 1, 0, 0, 0, 0, {nullptr}};

    auto image = ImageHelperUlt<Image3dDefaults>::create(pContext, &imageDesc, &imageFormat);
    ASSERT_NE(nullptr, image);

    constexpr size_t bufferSize = 1024;
    uint8_t buffer[bufferSize + 16];
    void *dstPtr = buffer + 1;
    ASSERT_NE(0u, reinterpret_cast<uintptr_t>(dstPtr) & 0x0000000F);

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInGroup, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcMemObj = image;
    dc.dstPtr = dstPtr;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};
    dc.dstRowPitch = 0;
    dc.dstSlicePitch = 0;

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("16Bytes") != std::string::npos);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("Aligned") == std::string::npos);

    delete image;
}

HWTEST_F(BuiltInTests, givenCopyImage3dToBufferNon16BytesFormatWhenBuilderIsUsedThenAlignmentCheckIsSkipped) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    bool isStateless = compilerProductHelper.isForceToStatelessRequired();
    bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());
    auto builtInGroup = BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyImage3dToBuffer>(isStateless, heaplessAllowed);

    cl_image_format imageFormat = {CL_R, CL_FLOAT};
    cl_image_desc imageDesc = {CL_MEM_OBJECT_IMAGE3D, 4, 4, 4, 1, 0, 0, 0, 0, {nullptr}};

    auto image = ImageHelperUlt<Image3dDefaults>::create(pContext, &imageDesc, &imageFormat);
    ASSERT_NE(nullptr, image);

    constexpr size_t bufferSize = 1024;
    alignas(16) uint8_t alignedBuffer[bufferSize];
    void *dstPtr = alignedBuffer;

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInGroup, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcMemObj = image;
    dc.dstPtr = dstPtr;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};
    dc.dstRowPitch = 0;
    dc.dstSlicePitch = 0;

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("4Bytes") != std::string::npos);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("16Bytes") == std::string::npos);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("Aligned") == std::string::npos);

    delete image;
}

HWTEST_F(BuiltInTests, givenCopyBufferToImageNon16BytesFormatWhenBuilderIsUsedThenAlignmentCheckIsSkipped) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    bool isStateless = compilerProductHelper.isForceToStatelessRequired();
    bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());
    auto builtInGroup = BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyBufferToImage3d>(isStateless, heaplessAllowed);

    cl_image_format imageFormat = {CL_R, CL_FLOAT};
    cl_image_desc imageDesc = {CL_MEM_OBJECT_IMAGE3D, 4, 4, 4, 1, 0, 0, 0, 0, {nullptr}};

    auto image = ImageHelperUlt<Image3dDefaults>::create(pContext, &imageDesc, &imageFormat);
    ASSERT_NE(nullptr, image);

    constexpr size_t bufferSize = 1024;
    alignas(16) uint8_t alignedBuffer[bufferSize];
    void *srcPtr = alignedBuffer;

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInGroup, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcPtr = srcPtr;
    dc.dstMemObj = image;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};
    dc.srcRowPitch = 0;
    dc.srcSlicePitch = 0;

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("4Bytes") != std::string::npos);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("16Bytes") == std::string::npos);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("Aligned") == std::string::npos);

    delete image;
}

HWTEST_F(BuiltInTests, givenCopyBufferToImage16BytesWithImageAsSrcMemObjWhenBuilderIsUsedThenRegularKernelIsSelected) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    bool isStateless = compilerProductHelper.isForceToStatelessRequired();
    bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());
    auto builtInGroup = BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyBufferToImage3d>(isStateless, heaplessAllowed);

    cl_image_format imageFormat = {CL_RGBA, CL_FLOAT};
    cl_image_desc imageDesc = {CL_MEM_OBJECT_IMAGE3D, 4, 4, 4, 1, 0, 0, 0, 0, {nullptr}};

    auto dstImage = ImageHelperUlt<Image3dDefaults>::create(pContext, &imageDesc, &imageFormat);
    ASSERT_NE(nullptr, dstImage);

    auto srcImage = ImageHelperUlt<Image3dDefaults>::create(pContext, &imageDesc, &imageFormat);
    ASSERT_NE(nullptr, srcImage);

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInGroup, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcMemObj = srcImage;
    dc.dstMemObj = dstImage;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};
    dc.srcRowPitch = 0;
    dc.srcSlicePitch = 0;

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("16Bytes") != std::string::npos);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("Aligned") == std::string::npos);

    delete dstImage;
    delete srcImage;
}

HWTEST_F(BuiltInTests, givenCopyBufferToImage16BytesWithAlignedBufferWhenBuilderIsUsedThenAlignedKernelIsSelected) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    bool isStateless = compilerProductHelper.isForceToStatelessRequired();
    bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());
    auto builtInGroup = BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyBufferToImage3d>(isStateless, heaplessAllowed);

    cl_image_format imageFormat = {CL_RGBA, CL_FLOAT};
    cl_image_desc imageDesc = {CL_MEM_OBJECT_IMAGE3D, 4, 4, 4, 1, 0, 0, 0, 0, {nullptr}};

    auto image = ImageHelperUlt<Image3dDefaults>::create(pContext, &imageDesc, &imageFormat);
    ASSERT_NE(nullptr, image);

    constexpr size_t bufferSize = 1024;
    cl_int retVal = CL_SUCCESS;
    auto srcBuffer = Buffer::create(pContext, 0, bufferSize, nullptr, retVal);
    ASSERT_NE(nullptr, srcBuffer);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInGroup, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcMemObj = srcBuffer;
    dc.dstMemObj = image;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};
    dc.srcRowPitch = 0;
    dc.srcSlicePitch = 0;

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("16Bytes") != std::string::npos);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("Aligned") != std::string::npos);

    delete image;
    delete srcBuffer;
}

HWTEST_F(BuiltInTests, givenCopyBufferToImage16BytesWithOffsetMisalignedBufferWhenBuilderIsUsedThenRegularKernelIsSelected) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    bool isStateless = compilerProductHelper.isForceToStatelessRequired();
    bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());
    auto builtInGroup = BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyBufferToImage3d>(isStateless, heaplessAllowed);

    cl_image_format imageFormat = {CL_RGBA, CL_FLOAT};
    cl_image_desc imageDesc = {CL_MEM_OBJECT_IMAGE3D, 4, 4, 4, 1, 0, 0, 0, 0, {nullptr}};

    auto image = ImageHelperUlt<Image3dDefaults>::create(pContext, &imageDesc, &imageFormat);
    ASSERT_NE(nullptr, image);

    constexpr size_t bufferSize = 1024;
    cl_int retVal = CL_SUCCESS;
    auto srcBuffer = Buffer::create(pContext, 0, bufferSize, nullptr, retVal);
    ASSERT_NE(nullptr, srcBuffer);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInGroup, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcMemObj = srcBuffer;
    dc.dstMemObj = image;
    dc.srcOffset = {1, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};
    dc.srcRowPitch = 0;
    dc.srcSlicePitch = 0;

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("16Bytes") != std::string::npos);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("Aligned") == std::string::npos);

    delete image;
    delete srcBuffer;
}

HWTEST_F(BuiltInTests, givenCopyImageToBuffer16BytesWithImageAsDstMemObjWhenBuilderIsUsedThenRegularKernelIsSelected) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    bool isStateless = compilerProductHelper.isForceToStatelessRequired();
    bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());
    auto builtInGroup = BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyImage3dToBuffer>(isStateless, heaplessAllowed);

    cl_image_format imageFormat = {CL_RGBA, CL_FLOAT};
    cl_image_desc imageDesc = {CL_MEM_OBJECT_IMAGE3D, 4, 4, 4, 1, 0, 0, 0, 0, {nullptr}};

    auto srcImage = ImageHelperUlt<Image3dDefaults>::create(pContext, &imageDesc, &imageFormat);
    ASSERT_NE(nullptr, srcImage);

    auto dstImage = ImageHelperUlt<Image3dDefaults>::create(pContext, &imageDesc, &imageFormat);
    ASSERT_NE(nullptr, dstImage);

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInGroup, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcMemObj = srcImage;
    dc.dstMemObj = dstImage;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};
    dc.dstRowPitch = 0;
    dc.dstSlicePitch = 0;

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("16Bytes") != std::string::npos);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("Aligned") == std::string::npos);

    delete srcImage;
    delete dstImage;
}

HWTEST_F(BuiltInTests, givenCopyImageToBuffer16BytesWithAlignedBufferWhenBuilderIsUsedThenAlignedKernelIsSelected) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    bool isStateless = compilerProductHelper.isForceToStatelessRequired();
    bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());
    auto builtInGroup = BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyImage3dToBuffer>(isStateless, heaplessAllowed);

    cl_image_format imageFormat = {CL_RGBA, CL_FLOAT};
    cl_image_desc imageDesc = {CL_MEM_OBJECT_IMAGE3D, 4, 4, 4, 1, 0, 0, 0, 0, {nullptr}};

    auto srcImage = ImageHelperUlt<Image3dDefaults>::create(pContext, &imageDesc, &imageFormat);
    ASSERT_NE(nullptr, srcImage);

    constexpr size_t bufferSize = 1024;
    cl_int retVal = CL_SUCCESS;
    auto dstBuffer = Buffer::create(pContext, 0, bufferSize, nullptr, retVal);
    ASSERT_NE(nullptr, dstBuffer);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInGroup, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcMemObj = srcImage;
    dc.dstMemObj = dstBuffer;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};
    dc.dstRowPitch = 0;
    dc.dstSlicePitch = 0;

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("16Bytes") != std::string::npos);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("Aligned") != std::string::npos);

    delete srcImage;
    delete dstBuffer;
}

HWTEST_F(BuiltInTests, givenCopyImageToBuffer16BytesWithOffsetMisalignedBufferWhenBuilderIsUsedThenRegularKernelIsSelected) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    bool isStateless = compilerProductHelper.isForceToStatelessRequired();
    bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());
    auto builtInGroup = BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyImage3dToBuffer>(isStateless, heaplessAllowed);

    cl_image_format imageFormat = {CL_RGBA, CL_FLOAT};
    cl_image_desc imageDesc = {CL_MEM_OBJECT_IMAGE3D, 4, 4, 4, 1, 0, 0, 0, 0, {nullptr}};

    auto srcImage = ImageHelperUlt<Image3dDefaults>::create(pContext, &imageDesc, &imageFormat);
    ASSERT_NE(nullptr, srcImage);

    constexpr size_t bufferSize = 1024;
    cl_int retVal = CL_SUCCESS;
    auto dstBuffer = Buffer::create(pContext, 0, bufferSize, nullptr, retVal);
    ASSERT_NE(nullptr, dstBuffer);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInGroup, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcMemObj = srcImage;
    dc.dstMemObj = dstBuffer;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {1, 0, 0};
    dc.size = {1, 1, 1};
    dc.dstRowPitch = 0;
    dc.dstSlicePitch = 0;

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("16Bytes") != std::string::npos);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("Aligned") == std::string::npos);

    delete srcImage;
    delete dstBuffer;
}

HWTEST_F(BuiltInTests, givenCopyBufferToImage16BytesWithRowPitchMisalignedWhenBuilderIsUsedThenRegularKernelIsSelected) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    bool isStateless = compilerProductHelper.isForceToStatelessRequired();
    bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());
    auto builtInGroup = BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyBufferToImage3d>(isStateless, heaplessAllowed);

    cl_image_format imageFormat = {CL_RGBA, CL_FLOAT};
    cl_image_desc imageDesc = {CL_MEM_OBJECT_IMAGE3D, 4, 4, 4, 1, 0, 0, 0, 0, {nullptr}};

    auto image = ImageHelperUlt<Image3dDefaults>::create(pContext, &imageDesc, &imageFormat);
    ASSERT_NE(nullptr, image);

    constexpr size_t bufferSize = 1024;
    alignas(16) uint8_t alignedBuffer[bufferSize];
    void *srcPtr = alignedBuffer;
    ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(srcPtr) & 0x0000000F);

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInGroup, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcPtr = srcPtr;
    dc.dstMemObj = image;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};
    dc.srcRowPitch = 68; // 16-byte misaligned row pitch
    dc.srcSlicePitch = 0;

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("16Bytes") != std::string::npos);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("Aligned") == std::string::npos);

    delete image;
}

HWTEST_F(BuiltInTests, givenCopyImage3dToBuffer16BytesWithSlicePitchMisalignedWhenBuilderIsUsedThenRegularKernelIsSelected) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    bool isStateless = compilerProductHelper.isForceToStatelessRequired();
    bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());
    auto builtInGroup = BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyImage3dToBuffer>(isStateless, heaplessAllowed);

    cl_image_format imageFormat = {CL_RGBA, CL_FLOAT};
    cl_image_desc imageDesc = {CL_MEM_OBJECT_IMAGE3D, 4, 4, 4, 1, 0, 0, 0, 0, {nullptr}};

    auto image = ImageHelperUlt<Image3dDefaults>::create(pContext, &imageDesc, &imageFormat);
    ASSERT_NE(nullptr, image);

    constexpr size_t bufferSize = 1024;
    alignas(16) uint8_t alignedBuffer[bufferSize];
    void *dstPtr = alignedBuffer;
    ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(dstPtr) & 0x0000000F);

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInGroup, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcMemObj = image;
    dc.dstPtr = dstPtr;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};
    dc.dstRowPitch = 0;
    dc.dstSlicePitch = 132; // 16-byte misaligned slice pitch

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("16Bytes") != std::string::npos);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("Aligned") == std::string::npos);

    delete image;
}

HWTEST_F(BuiltInTests, givenCopyBufferToImage16BytesWithOffsetMisalignedWhenBuilderIsUsedThenRegularKernelIsSelected) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    bool isStateless = compilerProductHelper.isForceToStatelessRequired();
    bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());
    auto builtInGroup = BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyBufferToImage3d>(isStateless, heaplessAllowed);

    cl_image_format imageFormat = {CL_RGBA, CL_FLOAT};
    cl_image_desc imageDesc = {CL_MEM_OBJECT_IMAGE3D, 4, 4, 4, 1, 0, 0, 0, 0, {nullptr}};

    auto image = ImageHelperUlt<Image3dDefaults>::create(pContext, &imageDesc, &imageFormat);
    ASSERT_NE(nullptr, image);

    constexpr size_t bufferSize = 1024;
    alignas(16) uint8_t alignedBuffer[bufferSize];
    void *srcPtr = alignedBuffer;
    ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(srcPtr) & 0x0000000F);

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInGroup, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcPtr = srcPtr;
    dc.dstMemObj = image;
    dc.srcOffset = {8, 0, 0}; // 16-byte misaligned offset
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};
    dc.srcRowPitch = 0;
    dc.srcSlicePitch = 0;

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("16Bytes") != std::string::npos);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("Aligned") == std::string::npos);

    delete image;
}

HWTEST_F(BuiltInTests, givenCopyImage3dToBuffer16BytesWithNullAddressWhenBuilderIsUsedThenRegularKernelIsSelected) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    bool isStateless = compilerProductHelper.isForceToStatelessRequired();
    bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());
    auto builtInGroup = BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyImage3dToBuffer>(isStateless, heaplessAllowed);

    cl_image_format imageFormat = {CL_RGBA, CL_FLOAT};
    cl_image_desc imageDesc = {CL_MEM_OBJECT_IMAGE3D, 4, 4, 4, 1, 0, 0, 0, 0, {nullptr}};

    auto image = ImageHelperUlt<Image3dDefaults>::create(pContext, &imageDesc, &imageFormat);
    ASSERT_NE(nullptr, image);

    MockBuffer dstBuffer;
    dstBuffer.size = 1024;
    dstBuffer.mockGfxAllocation.gpuAddress = 0;

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInGroup, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcMemObj = image;
    dc.dstMemObj = &dstBuffer;
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};
    dc.dstRowPitch = 0;
    dc.dstSlicePitch = 0;

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("16Bytes") != std::string::npos);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("Aligned") == std::string::npos);

    delete image;
}

HWTEST_F(BuiltInTests, givenCopyBufferToImage16BytesWithNullPtrAndNullMemObjWhenBuilderIsUsedThenRegularKernelIsSelected) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    bool isStateless = compilerProductHelper.isForceToStatelessRequired();
    bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());
    auto builtInGroup = BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyBufferToImage3d>(isStateless, heaplessAllowed);

    cl_image_format imageFormat = {CL_RGBA, CL_FLOAT};
    cl_image_desc imageDesc = {CL_MEM_OBJECT_IMAGE3D, 4, 4, 4, 1, 0, 0, 0, 0, {nullptr}};

    auto image = ImageHelperUlt<Image3dDefaults>::create(pContext, &imageDesc, &imageFormat);
    ASSERT_NE(nullptr, image);

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInGroup, *pClDevice);

    BuiltIn::OpParams dc;
    dc.dstMemObj = image;
    dc.size = {1, 1, 1};

    MultiDispatchInfo multiDispatchInfo(dc);
    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo));
    EXPECT_EQ(1u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("16Bytes") != std::string::npos);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.find("Aligned") == std::string::npos);

    delete image;
}

HWTEST_F(BuiltInTests, givenAdjustedCopyBufferToBufferWhenWidenessEnabledAndStatelessRequiredThenBuiltinOpParamsAndKernelArgsAreCorrect) {
    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    const bool isStateless = compilerProductHelper.isForceToStatelessRequired();
    if (!isStateless) {
        GTEST_SKIP();
    }
    const bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());

    const auto builtInGroup =
        BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyBufferToBuffer>(isStateless, heaplessAllowed, true);

    MockBuffer srcBuffer;
    MockBuffer dstBuffer;

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInGroup, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcPtr = &srcBuffer;
    dc.dstPtr = &dstBuffer;
    dc.size = {1, 1, 1};

    MultiDispatchInfo mdi(dc);
    EXPECT_TRUE(builder.buildDispatchInfos(mdi));
    ASSERT_EQ(1u, mdi.size());
    EXPECT_TRUE(compareBuiltinOpParams(mdi.peekBuiltinOpParams(), dc));

    auto kernel = mdi.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelAttributes.supportsBuffersBiggerThan4Gb());

    EXPECT_EQ(sizeof(uint64_t), kernel->getKernelInfo().getArgDescriptorAt(2).as<ArgDescValue>().elements[0].size);
    EXPECT_EQ(sizeof(uint64_t), kernel->getKernelInfo().getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size);
}

HWTEST_F(BuiltInTests, givenAdjustedCopyBufferRect2dWhenWidenessEnabledAndStatelessRequiredThenBuiltinOpParamsAndKernelArgsAreCorrect) {
    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    const bool isStateless = compilerProductHelper.isForceToStatelessRequired();
    if (!isStateless) {
        GTEST_SKIP();
    }
    const bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());

    const auto builtInGroup =
        BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyBufferRect>(isStateless, heaplessAllowed, true);

    MockBuffer srcBuffer;
    MockBuffer dstBuffer;

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInGroup, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcPtr = &srcBuffer;
    dc.dstPtr = &dstBuffer;

    dc.srcOffset = {1, 2, 0};
    dc.dstOffset = {3, 4, 0};
    dc.size = {5, 6, 1};

    dc.srcRowPitch = 64;
    dc.srcSlicePitch = 0;
    dc.dstRowPitch = 128;
    dc.dstSlicePitch = 0;

    MultiDispatchInfo mdi(dc);
    EXPECT_TRUE(builder.buildDispatchInfos(mdi));
    ASSERT_EQ(1u, mdi.size());
    EXPECT_TRUE(compareBuiltinOpParams(mdi.peekBuiltinOpParams(), dc));

    auto kernel = mdi.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelAttributes.supportsBuffersBiggerThan4Gb());

    EXPECT_EQ(2 * sizeof(uint64_t), kernel->getKernelInfo().getArgDescriptorAt(2).as<ArgDescValue>().elements[0].size);
    EXPECT_EQ(2 * sizeof(uint64_t), kernel->getKernelInfo().getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size);
    EXPECT_EQ(sizeof(uint64_t), kernel->getKernelInfo().getArgDescriptorAt(4).as<ArgDescValue>().elements[0].size);
    EXPECT_EQ(sizeof(uint64_t), kernel->getKernelInfo().getArgDescriptorAt(5).as<ArgDescValue>().elements[0].size);
}

HWTEST_F(BuiltInTests, givenAdjustedFillBufferWhenWidenessEnabledAndStatelessRequiredThenBuiltinOpParamsAndKernelArgsAreCorrect) {
    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    const bool isStateless = compilerProductHelper.isForceToStatelessRequired();
    if (!isStateless) {
        GTEST_SKIP();
    }
    const bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());

    const auto builtInGroup =
        BuiltIn::adjustBuiltinGroup<BuiltIn::Group::fillBuffer>(isStateless, heaplessAllowed, true);

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInGroup, *pClDevice);

    uint64_t size = MemoryConstants::gigaByte;

    MockBuffer srcBuffer;
    srcBuffer.size = static_cast<size_t>(size);
    MockBuffer dstBuffer;
    dstBuffer.size = static_cast<size_t>(size);

    BuiltIn::OpParams dc;
    dc.srcMemObj = &srcBuffer;
    dc.dstMemObj = &dstBuffer;
    dc.dstOffset = {static_cast<size_t>(size), 0, 0};
    dc.size = {static_cast<size_t>(size), 0, 0};

    MultiDispatchInfo mdi(dc);
    EXPECT_TRUE(builder.buildDispatchInfos(mdi));
    ASSERT_EQ(1u, mdi.size());
    EXPECT_TRUE(compareBuiltinOpParams(mdi.peekBuiltinOpParams(), dc));

    auto kernel = mdi.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelAttributes.supportsBuffersBiggerThan4Gb());

    EXPECT_EQ(sizeof(uint64_t), kernel->getKernelInfo().getArgDescriptorAt(1).as<ArgDescValue>().elements[0].size);
}

HWTEST_F(BuiltInTests, givenAdjustedCopyBufferToImage3dWhenWidenessEnabledAndStatelessRequiredThenBuiltinOpParamsAndKernelArgsAreCorrect) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    const bool isStateless = compilerProductHelper.isForceToStatelessRequired();
    if (!isStateless) {
        GTEST_SKIP();
    }
    const bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());

    const auto builtInGroup =
        BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyBufferToImage3d>(isStateless, heaplessAllowed, true);

    MockBuffer buffer;
    std::unique_ptr<Image> image(Image2dHelperUlt<>::create(pContext));
    ASSERT_NE(nullptr, image.get());

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInGroup, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcPtr = &buffer;
    dc.dstMemObj = image.get();
    dc.size = {1, 1, 1};

    MultiDispatchInfo mdi(dc);
    EXPECT_TRUE(builder.buildDispatchInfos(mdi));
    ASSERT_EQ(1u, mdi.size());
    EXPECT_TRUE(compareBuiltinOpParams(mdi.peekBuiltinOpParams(), dc));

    auto kernel = mdi.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelAttributes.supportsBuffersBiggerThan4Gb());

    EXPECT_EQ(sizeof(uint64_t), kernel->getKernelInfo().getArgDescriptorAt(2).as<ArgDescValue>().elements[0].size);
    EXPECT_EQ(2 * sizeof(uint64_t), kernel->getKernelInfo().getArgDescriptorAt(4).as<ArgDescValue>().elements[0].size);
}

HWTEST_F(BuiltInTests, givenAdjustedCopyImage3dToBufferWhenWidenessEnabledAndStatelessRequiredThenBuiltinOpParamsAndKernelArgsAreCorrect) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    const bool isStateless = compilerProductHelper.isForceToStatelessRequired();
    if (!isStateless) {
        GTEST_SKIP();
    }
    const bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());

    const auto builtInGroup =
        BuiltIn::adjustBuiltinGroup<BuiltIn::Group::copyImage3dToBuffer>(isStateless, heaplessAllowed, true);

    MockBuffer buffer;
    std::unique_ptr<Image> image(Image2dHelperUlt<>::create(pContext));
    ASSERT_NE(nullptr, image.get());

    auto &builder = BuiltIn::DispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInGroup, *pClDevice);

    BuiltIn::OpParams dc;
    dc.srcMemObj = image.get();
    dc.dstPtr = &buffer;
    dc.size = {1, 1, 1};

    MultiDispatchInfo mdi(dc);
    EXPECT_TRUE(builder.buildDispatchInfos(mdi));
    ASSERT_EQ(1u, mdi.size());
    EXPECT_TRUE(compareBuiltinOpParams(mdi.peekBuiltinOpParams(), dc));

    auto kernel = mdi.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);
    EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelAttributes.supportsBuffersBiggerThan4Gb());

    EXPECT_EQ(sizeof(uint64_t), kernel->getKernelInfo().getArgDescriptorAt(3).as<ArgDescValue>().elements[0].size);
    EXPECT_EQ(2 * sizeof(uint64_t), kernel->getKernelInfo().getArgDescriptorAt(4).as<ArgDescValue>().elements[0].size);
}
