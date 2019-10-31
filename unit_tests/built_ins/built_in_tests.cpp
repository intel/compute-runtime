/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/file_io.h"
#include "core/helpers/hash.h"
#include "core/helpers/string.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "core/unit_tests/utilities/base_object_utils.h"
#include "runtime/built_ins/aux_translation_builtin.h"
#include "runtime/built_ins/built_ins.h"
#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "runtime/built_ins/vme_dispatch_builder.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/helpers/dispatch_info_builder.h"
#include "runtime/kernel/kernel.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "test.h"
#include "unit_tests/fixtures/built_in_fixture.h"
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/fixtures/run_kernel_fixture.h"
#include "unit_tests/global_environment.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_builtins.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_compilers.h"
#include "unit_tests/mocks/mock_kernel.h"

#include "gtest/gtest.h"
#include "os_inc.h"

#include <string>

using namespace NEO;

class BuiltInTests
    : public BuiltInFixture,
      public DeviceFixture,
      public ContextFixture,
      public ::testing::Test {

    using BuiltInFixture::SetUp;
    using ContextFixture::SetUp;

  public:
    BuiltInTests() {
        // reserving space here to avoid the appearance of a memory management
        // leak being reported
        allBuiltIns.reserve(5000);
    }

    void SetUp() override {
        DeviceFixture::SetUp();
        cl_device_id device = pDevice;
        ContextFixture::SetUp(1, &device);
        BuiltInFixture::SetUp(pDevice);
    }

    void TearDown() override {
        allBuiltIns.clear();
        BuiltInFixture::TearDown();
        ContextFixture::TearDown();
        DeviceFixture::TearDown();
    }

    void AppendBuiltInStringFromFile(std::string builtInFile, size_t &size) {
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

TEST_F(BuiltInTests, SourceConsistency) {
    size_t size = 0;

    AppendBuiltInStringFromFile(
        "test_files/aux_translation.igdrcl_built_in", size);
    ASSERT_NE(0u, size);

    AppendBuiltInStringFromFile(
        "test_files/copy_buffer_to_buffer.igdrcl_built_in", size);
    ASSERT_NE(0u, size);

    AppendBuiltInStringFromFile(
        "test_files/fill_buffer.igdrcl_built_in", size);
    ASSERT_NE(0u, size);

    AppendBuiltInStringFromFile(
        "test_files/fill_image1d.igdrcl_built_in", size);
    ASSERT_NE(0u, size);

    AppendBuiltInStringFromFile(
        "test_files/fill_image2d.igdrcl_built_in", size);
    ASSERT_NE(0u, size);

    AppendBuiltInStringFromFile(
        "test_files/fill_image3d.igdrcl_built_in", size);
    ASSERT_NE(0u, size);

    AppendBuiltInStringFromFile(
        "test_files/copy_image_to_image1d.igdrcl_built_in", size);
    ASSERT_NE(0u, size);

    AppendBuiltInStringFromFile(
        "test_files/copy_image_to_image2d.igdrcl_built_in", size);
    ASSERT_NE(0u, size);

    AppendBuiltInStringFromFile(
        "test_files/copy_image_to_image3d.igdrcl_built_in", size);
    ASSERT_NE(0u, size);

    AppendBuiltInStringFromFile(
        "test_files/copy_buffer_rect.igdrcl_built_in", size);
    ASSERT_NE(0u, size);

    AppendBuiltInStringFromFile(
        "test_files/copy_buffer_to_image3d.igdrcl_built_in", size);
    ASSERT_NE(0u, size);

    AppendBuiltInStringFromFile(
        "test_files/copy_image3d_to_buffer.igdrcl_built_in", size);
    ASSERT_NE(0u, size);

    // convert /r/n to /n
    size_t start_pos = 0;
    while ((start_pos = allBuiltIns.find("\r\n", start_pos)) != std::string::npos) {
        allBuiltIns.replace(start_pos, 2, "\n");
    }

    // convert /r to /n
    start_pos = 0;
    while ((start_pos = allBuiltIns.find("\r", start_pos)) != std::string::npos) {
        allBuiltIns.replace(start_pos, 1, "\n");
    }

    uint64_t hash = Hash::hash(allBuiltIns.c_str(), allBuiltIns.length());
    std::string hashName = "test_files/" + std::to_string(hash);
    hashName.append(".cl");

    //Fisrt fail, if we are inconsistent
    EXPECT_EQ(true, fileExists(hashName)) << "**********\nBuilt in kernels need to be regenerated for the mock compilers!\n**********";

//then write to file if needed
#define GENERATE_NEW_HASH_FOR_BUILT_INS 0
#if GENERATE_NEW_HASH_FOR_BUILT_INS
    std::cout << "writing builtins to file: " << hashName << std::endl;
    const char *pData = allBuiltIns.c_str();
    writeDataToFile(hashName.c_str(), pData, allBuiltIns.length());
#endif
}

TEST_F(BuiltInTests, BuiltinDispatchInfoBuilderCopyBufferToBuffer) {
    BuiltinDispatchInfoBuilder &builder = pBuiltIns->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer, *pContext, *pDevice);

    MockBuffer *srcPtr = new MockBuffer();
    MockBuffer *dstPtr = new MockBuffer();

    MockBuffer &src = *srcPtr;
    MockBuffer &dst = *dstPtr;

    MultiDispatchInfo multiDispatchInfo;
    BuiltinOpParams builtinOpsParams;

    builtinOpsParams.srcMemObj = &src;
    builtinOpsParams.dstMemObj = &dst;
    builtinOpsParams.srcPtr = src.getCpuAddress();
    builtinOpsParams.dstPtr = dst.getCpuAddress();
    builtinOpsParams.size = {dst.getSize(), 0, 0};

    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo, builtinOpsParams));

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
    }
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), builtinOpsParams));
    delete srcPtr;
    delete dstPtr;
}

HWTEST_F(BuiltInTests, givenInputBufferWhenBuildingNonAuxDispatchInfoForAuxTranslationThenPickAndSetupCorrectKernels) {
    BuiltinDispatchInfoBuilder &baseBuilder = pBuiltIns->getBuiltinDispatchInfoBuilder(EBuiltInOps::AuxTranslation, *pContext, *pDevice);
    auto &builder = static_cast<BuiltInOp<EBuiltInOps::AuxTranslation> &>(baseBuilder);

    MemObjsForAuxTranslation memObjsForAuxTranslation;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.setMemObjsForAuxTranslation(memObjsForAuxTranslation);
    std::vector<Kernel *> builtinKernels;
    MockBuffer mockBuffer[3];
    mockBuffer[0].getGraphicsAllocation()->setSize(0x1000);
    mockBuffer[1].getGraphicsAllocation()->setSize(0x20000);
    mockBuffer[2].getGraphicsAllocation()->setSize(0x30000);

    BuiltinOpParams builtinOpsParams;
    builtinOpsParams.auxTranslationDirection = AuxTranslationDirection::AuxToNonAux;

    for (auto &buffer : mockBuffer) {
        memObjsForAuxTranslation.insert(&buffer);
    }

    EXPECT_TRUE(builder.buildDispatchInfosForAuxTranslation<FamilyType>(multiDispatchInfo, builtinOpsParams));
    EXPECT_EQ(3u, multiDispatchInfo.size());

    for (auto &dispatchInfo : multiDispatchInfo) {
        auto kernel = dispatchInfo.getKernel();
        builtinKernels.push_back(kernel);
        MemObj *buffer = *memObjsForAuxTranslation.find(castToObject<Buffer>(kernel->getKernelArguments().at(0).object));
        EXPECT_NE(nullptr, buffer);
        memObjsForAuxTranslation.erase(buffer);

        cl_mem clMem = buffer;
        EXPECT_EQ(clMem, kernel->getKernelArguments().at(0).object);
        EXPECT_EQ(clMem, kernel->getKernelArguments().at(1).object);

        EXPECT_EQ(1u, dispatchInfo.getDim());
        size_t xGws = alignUp(buffer->getSize(), 512) / 16;
        Vec3<size_t> gws = {xGws, 1, 1};
        EXPECT_EQ(gws, dispatchInfo.getGWS());
    }
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), builtinOpsParams));
    // always pick different kernel
    EXPECT_EQ(3u, builtinKernels.size());
    EXPECT_NE(builtinKernels[0], builtinKernels[1]);
    EXPECT_NE(builtinKernels[0], builtinKernels[2]);
    EXPECT_NE(builtinKernels[1], builtinKernels[2]);
}

HWTEST_F(BuiltInTests, givenInputBufferWhenBuildingAuxDispatchInfoForAuxTranslationThenPickAndSetupCorrectKernels) {
    BuiltinDispatchInfoBuilder &baseBuilder = pBuiltIns->getBuiltinDispatchInfoBuilder(EBuiltInOps::AuxTranslation, *pContext, *pDevice);
    auto &builder = static_cast<BuiltInOp<EBuiltInOps::AuxTranslation> &>(baseBuilder);

    MemObjsForAuxTranslation memObjsForAuxTranslation;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.setMemObjsForAuxTranslation(memObjsForAuxTranslation);
    std::vector<Kernel *> builtinKernels;
    MockBuffer mockBuffer[3];
    mockBuffer[0].getGraphicsAllocation()->setSize(0x1000);
    mockBuffer[1].getGraphicsAllocation()->setSize(0x20000);
    mockBuffer[2].getGraphicsAllocation()->setSize(0x30000);

    BuiltinOpParams builtinOpsParams;
    builtinOpsParams.auxTranslationDirection = AuxTranslationDirection::NonAuxToAux;

    for (auto &buffer : mockBuffer) {
        memObjsForAuxTranslation.insert(&buffer);
    }

    EXPECT_TRUE(builder.buildDispatchInfosForAuxTranslation<FamilyType>(multiDispatchInfo, builtinOpsParams));
    EXPECT_EQ(3u, multiDispatchInfo.size());

    for (auto &dispatchInfo : multiDispatchInfo) {
        auto kernel = dispatchInfo.getKernel();
        builtinKernels.push_back(kernel);
        MemObj *buffer = *memObjsForAuxTranslation.find(castToObject<Buffer>(kernel->getKernelArguments().at(1).object));
        EXPECT_NE(nullptr, buffer);
        memObjsForAuxTranslation.erase(buffer);

        cl_mem clMem = buffer;
        EXPECT_EQ(clMem, kernel->getKernelArguments().at(0).object);
        EXPECT_EQ(clMem, kernel->getKernelArguments().at(1).object);

        EXPECT_EQ(1u, dispatchInfo.getDim());
        size_t xGws = alignUp(buffer->getSize(), 4) / 4;
        Vec3<size_t> gws = {xGws, 1, 1};
        EXPECT_EQ(gws, dispatchInfo.getGWS());
    }
    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), builtinOpsParams));
    // always pick different kernel
    EXPECT_EQ(3u, builtinKernels.size());
    EXPECT_NE(builtinKernels[0], builtinKernels[1]);
    EXPECT_NE(builtinKernels[0], builtinKernels[2]);
    EXPECT_NE(builtinKernels[1], builtinKernels[2]);
}

HWTEST_F(BuiltInTests, givenInputBufferWhenBuildingAuxTranslationDispatchThenPickDifferentKernelsDependingOnRequest) {
    BuiltinDispatchInfoBuilder &baseBuilder = pBuiltIns->getBuiltinDispatchInfoBuilder(EBuiltInOps::AuxTranslation, *pContext, *pDevice);
    auto &builder = static_cast<BuiltInOp<EBuiltInOps::AuxTranslation> &>(baseBuilder);

    MemObjsForAuxTranslation memObjsForAuxTranslation;
    MockBuffer mockBuffer[3];
    std::vector<Kernel *> builtinKernels;

    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.setMemObjsForAuxTranslation(memObjsForAuxTranslation);
    BuiltinOpParams builtinOpsParams;

    for (auto &buffer : mockBuffer) {
        memObjsForAuxTranslation.insert(&buffer);
    }

    builtinOpsParams.auxTranslationDirection = AuxTranslationDirection::AuxToNonAux;
    EXPECT_TRUE(builder.buildDispatchInfosForAuxTranslation<FamilyType>(multiDispatchInfo, builtinOpsParams));

    builtinOpsParams.auxTranslationDirection = AuxTranslationDirection::NonAuxToAux;
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

HWTEST_F(BuiltInTests, givenInvalidAuxTranslationDirectionWhenBuildingDispatchInfosThenAbort) {
    BuiltinDispatchInfoBuilder &baseBuilder = pBuiltIns->getBuiltinDispatchInfoBuilder(EBuiltInOps::AuxTranslation, *pContext, *pDevice);
    auto &builder = static_cast<BuiltInOp<EBuiltInOps::AuxTranslation> &>(baseBuilder);

    MemObjsForAuxTranslation memObjsForAuxTranslation;
    MockBuffer mockBuffer;

    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.setMemObjsForAuxTranslation(memObjsForAuxTranslation);
    BuiltinOpParams builtinOpsParams;

    memObjsForAuxTranslation.insert(&mockBuffer);

    builtinOpsParams.auxTranslationDirection = AuxTranslationDirection::None;
    EXPECT_THROW(builder.buildDispatchInfosForAuxTranslation<FamilyType>(multiDispatchInfo, builtinOpsParams), std::exception);
}

class MockAuxBuilInOp : public BuiltInOp<EBuiltInOps::AuxTranslation> {
  public:
    using BuiltinDispatchInfoBuilder::populate;
    using BaseClass = BuiltInOp<EBuiltInOps::AuxTranslation>;
    using BaseClass::baseKernel;
    using BaseClass::convertToAuxKernel;
    using BaseClass::convertToNonAuxKernel;
    using BaseClass::resizeKernelInstances;
    using BaseClass::usedKernels;

    MockAuxBuilInOp(BuiltIns &kernelsLib, Context &context, Device &device) : BaseClass(kernelsLib, context, device) {}
};

TEST_F(BuiltInTests, whenAuxBuiltInIsConstructedThenResizeKernelInstancedTo5) {
    MockAuxBuilInOp mockAuxBuiltInOp(*pBuiltIns, *pContext, *pDevice);
    EXPECT_EQ(5u, mockAuxBuiltInOp.convertToAuxKernel.size());
    EXPECT_EQ(5u, mockAuxBuiltInOp.convertToNonAuxKernel.size());
}

HWTEST_F(BuiltInTests, givenMoreBuffersForAuxTranslationThanKernelInstancesWhenDispatchingThenResize) {
    MockAuxBuilInOp mockAuxBuiltInOp(*pBuiltIns, *pContext, *pDevice);
    EXPECT_EQ(5u, mockAuxBuiltInOp.convertToAuxKernel.size());
    EXPECT_EQ(5u, mockAuxBuiltInOp.convertToNonAuxKernel.size());

    MemObjsForAuxTranslation memObjsForAuxTranslation;
    BuiltinOpParams builtinOpsParams;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.setMemObjsForAuxTranslation(memObjsForAuxTranslation);
    MockBuffer mockBuffer[7];

    builtinOpsParams.auxTranslationDirection = AuxTranslationDirection::AuxToNonAux;

    for (auto &buffer : mockBuffer) {
        memObjsForAuxTranslation.insert(&buffer);
    }

    EXPECT_TRUE(mockAuxBuiltInOp.buildDispatchInfosForAuxTranslation<FamilyType>(multiDispatchInfo, builtinOpsParams));
    EXPECT_EQ(7u, mockAuxBuiltInOp.convertToAuxKernel.size());
    EXPECT_EQ(7u, mockAuxBuiltInOp.convertToNonAuxKernel.size());
}

TEST_F(BuiltInTests, givenkAuxBuiltInWhenResizeIsCalledThenCloneAllNewInstancesFromBaseKernel) {
    MockAuxBuilInOp mockAuxBuiltInOp(*pBuiltIns, *pContext, *pDevice);
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

HWTEST_F(BuiltInTests, givenKernelWithAuxTranslationRequiredWhenEnqueueCalledThenLockOnBuiltin) {
    pBuiltIns->getBuiltinDispatchInfoBuilder(EBuiltInOps::AuxTranslation, *pContext, *pDevice);
    auto mockAuxBuiltInOp = new MockAuxBuilInOp(*pBuiltIns, *pContext, *pDevice);
    pBuiltIns->BuiltinOpsBuilders[static_cast<uint32_t>(EBuiltInOps::AuxTranslation)].first.reset(mockAuxBuiltInOp);

    auto mockProgram = clUniquePtr(new MockProgram(*pDevice->getExecutionEnvironment()));
    auto mockBuiltinKernel = MockKernel::create(*pDevice, mockProgram.get());
    mockAuxBuiltInOp->usedKernels.at(0).reset(mockBuiltinKernel);

    MockKernelWithInternals mockKernel(*pDevice, pContext);
    MockCommandQueueHw<FamilyType> cmdQ(pContext, pDevice, nullptr);
    size_t gws[3] = {1, 0, 0};
    MockBuffer buffer;
    cl_mem clMem = &buffer;

    buffer.getGraphicsAllocation()->setAllocationType(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
    mockKernel.kernelInfo.kernelArgInfo.resize(1);
    mockKernel.kernelInfo.kernelArgInfo.at(0).kernelArgPatchInfoVector.resize(1);
    mockKernel.kernelInfo.kernelArgInfo.at(0).pureStatefulBufferAccess = false;
    mockKernel.mockKernel->initialize();
    mockKernel.mockKernel->setArgBuffer(0, sizeof(cl_mem *), &clMem);

    mockKernel.mockKernel->auxTranslationRequired = false;
    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(0u, mockBuiltinKernel->takeOwnershipCalls);
    EXPECT_EQ(0u, mockBuiltinKernel->releaseOwnershipCalls);

    mockKernel.mockKernel->auxTranslationRequired = true;
    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(1u, mockBuiltinKernel->takeOwnershipCalls);
    EXPECT_EQ(1u, mockBuiltinKernel->releaseOwnershipCalls);
}

HWTEST_F(BuiltInTests, givenAuxTranslationKernelWhenSettingKernelArgsThenSetValidMocs) {
    if (this->pDevice->areSharedSystemAllocationsAllowed()) {
        GTEST_SKIP();
    }

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    MockAuxBuilInOp mockAuxBuiltInOp(*pBuiltIns, *pContext, *pDevice);
    MultiDispatchInfo multiDispatchInfo;
    MemObjsForAuxTranslation memObjsForAuxTranslation;
    multiDispatchInfo.setMemObjsForAuxTranslation(memObjsForAuxTranslation);

    BuiltinOpParams builtinOpParamsToAux;
    builtinOpParamsToAux.auxTranslationDirection = AuxTranslationDirection::NonAuxToAux;

    BuiltinOpParams builtinOpParamsToNonAux;
    builtinOpParamsToNonAux.auxTranslationDirection = AuxTranslationDirection::AuxToNonAux;

    cl_int retVal = CL_SUCCESS;
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(pContext, 0, MemoryConstants::pageSize, nullptr, retVal));
    memObjsForAuxTranslation.insert(buffer.get());

    mockAuxBuiltInOp.buildDispatchInfosForAuxTranslation<FamilyType>(multiDispatchInfo, builtinOpParamsToAux);
    mockAuxBuiltInOp.buildDispatchInfosForAuxTranslation<FamilyType>(multiDispatchInfo, builtinOpParamsToNonAux);

    {
        // read args
        auto argNum = 0;
        auto expectedMocs = pDevice->getExecutionEnvironment()->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED);
        auto sshBase = mockAuxBuiltInOp.convertToAuxKernel[0]->getSurfaceStateHeap();
        auto sshOffset = mockAuxBuiltInOp.convertToAuxKernel[0]->getKernelInfo().kernelArgInfo[argNum].offsetHeap;
        auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(sshBase, sshOffset));
        EXPECT_EQ(expectedMocs, surfaceState->getMemoryObjectControlState());

        sshBase = mockAuxBuiltInOp.convertToNonAuxKernel[0]->getSurfaceStateHeap();
        sshOffset = mockAuxBuiltInOp.convertToNonAuxKernel[0]->getKernelInfo().kernelArgInfo[argNum].offsetHeap;
        surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(sshBase, sshOffset));
        EXPECT_EQ(expectedMocs, surfaceState->getMemoryObjectControlState());
    }

    {
        // write args
        auto argNum = 1;
        auto expectedMocs = pDevice->getExecutionEnvironment()->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
        auto sshBase = mockAuxBuiltInOp.convertToAuxKernel[0]->getSurfaceStateHeap();
        auto sshOffset = mockAuxBuiltInOp.convertToAuxKernel[0]->getKernelInfo().kernelArgInfo[argNum].offsetHeap;
        auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(sshBase, sshOffset));
        EXPECT_EQ(expectedMocs, surfaceState->getMemoryObjectControlState());

        sshBase = mockAuxBuiltInOp.convertToNonAuxKernel[0]->getSurfaceStateHeap();
        sshOffset = mockAuxBuiltInOp.convertToNonAuxKernel[0]->getKernelInfo().kernelArgInfo[argNum].offsetHeap;
        surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(sshBase, sshOffset));
        EXPECT_EQ(expectedMocs, surfaceState->getMemoryObjectControlState());
    }
}

HWTEST_F(BuiltInTests, givenAuxToNonAuxTranslationWhenSettingSurfaceStateThenSetValidAuxMode) {
    if (this->pDevice->areSharedSystemAllocationsAllowed()) {
        GTEST_SKIP();
    }

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    MockAuxBuilInOp mockAuxBuiltInOp(*pBuiltIns, *pContext, *pDevice);
    MultiDispatchInfo multiDispatchInfo;
    MemObjsForAuxTranslation memObjsForAuxTranslation;
    multiDispatchInfo.setMemObjsForAuxTranslation(memObjsForAuxTranslation);

    BuiltinOpParams builtinOpParams;
    builtinOpParams.auxTranslationDirection = AuxTranslationDirection::AuxToNonAux;

    cl_int retVal = CL_SUCCESS;
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(pContext, 0, MemoryConstants::pageSize, nullptr, retVal));
    buffer->getGraphicsAllocation()->setAllocationType(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
    auto gmm = new Gmm(nullptr, 1, false);
    gmm->isRenderCompressed = true;
    buffer->getGraphicsAllocation()->setDefaultGmm(gmm);

    memObjsForAuxTranslation.insert(buffer.get());

    mockAuxBuiltInOp.buildDispatchInfosForAuxTranslation<FamilyType>(multiDispatchInfo, builtinOpParams);

    {
        // read arg
        auto argNum = 0;
        auto sshBase = mockAuxBuiltInOp.convertToNonAuxKernel[0]->getSurfaceStateHeap();
        auto sshOffset = mockAuxBuiltInOp.convertToNonAuxKernel[0]->getKernelInfo().kernelArgInfo[argNum].offsetHeap;
        auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(sshBase, sshOffset));
        EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E, surfaceState->getAuxiliarySurfaceMode());
    }

    {
        // write arg
        auto argNum = 1;
        auto sshBase = mockAuxBuiltInOp.convertToNonAuxKernel[0]->getSurfaceStateHeap();
        auto sshOffset = mockAuxBuiltInOp.convertToNonAuxKernel[0]->getKernelInfo().kernelArgInfo[argNum].offsetHeap;
        auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(sshBase, sshOffset));
        EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE, surfaceState->getAuxiliarySurfaceMode());
    }
}

HWTEST_F(BuiltInTests, givenNonAuxToAuxTranslationWhenSettingSurfaceStateThenSetValidAuxMode) {
    if (this->pDevice->areSharedSystemAllocationsAllowed()) {
        GTEST_SKIP();
    }

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    MockAuxBuilInOp mockAuxBuiltInOp(*pBuiltIns, *pContext, *pDevice);
    MultiDispatchInfo multiDispatchInfo;
    MemObjsForAuxTranslation memObjsForAuxTranslation;
    multiDispatchInfo.setMemObjsForAuxTranslation(memObjsForAuxTranslation);

    BuiltinOpParams builtinOpParams;
    builtinOpParams.auxTranslationDirection = AuxTranslationDirection::NonAuxToAux;

    cl_int retVal = CL_SUCCESS;
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(pContext, 0, MemoryConstants::pageSize, nullptr, retVal));
    buffer->getGraphicsAllocation()->setAllocationType(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
    auto gmm = new Gmm(nullptr, 1, false);
    gmm->isRenderCompressed = true;
    buffer->getGraphicsAllocation()->setDefaultGmm(gmm);
    memObjsForAuxTranslation.insert(buffer.get());

    mockAuxBuiltInOp.buildDispatchInfosForAuxTranslation<FamilyType>(multiDispatchInfo, builtinOpParams);

    {
        // read arg
        auto argNum = 0;
        auto sshBase = mockAuxBuiltInOp.convertToAuxKernel[0]->getSurfaceStateHeap();
        auto sshOffset = mockAuxBuiltInOp.convertToAuxKernel[0]->getKernelInfo().kernelArgInfo[argNum].offsetHeap;
        auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(sshBase, sshOffset));
        EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE, surfaceState->getAuxiliarySurfaceMode());
    }

    {
        // write arg
        auto argNum = 1;
        auto sshBase = mockAuxBuiltInOp.convertToAuxKernel[0]->getSurfaceStateHeap();
        auto sshOffset = mockAuxBuiltInOp.convertToAuxKernel[0]->getKernelInfo().kernelArgInfo[argNum].offsetHeap;
        auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(sshBase, sshOffset));
        EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E, surfaceState->getAuxiliarySurfaceMode());
    }
}

TEST_F(BuiltInTests, BuiltinDispatchInfoBuilderCopyBufferToBufferAligned) {
    BuiltinDispatchInfoBuilder &builder = pBuiltIns->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer, *pContext, *pDevice);

    AlignedBuffer src;
    AlignedBuffer dst;

    MultiDispatchInfo multiDispatchInfo;
    BuiltinOpParams builtinOpsParams;

    builtinOpsParams.srcMemObj = &src;
    builtinOpsParams.dstMemObj = &dst;
    builtinOpsParams.size = {src.getSize(), 0, 0};

    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo, builtinOpsParams));

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

TEST_F(BuiltInTests, BuiltinDispatchInfoBuilderCopyBufferToBufferWithSourceOffsetUnalignedToFour) {
    BuiltinDispatchInfoBuilder &builder = pBuiltIns->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer, *pContext, *pDevice);

    AlignedBuffer src;
    AlignedBuffer dst;

    MultiDispatchInfo multiDispatchInfo;
    BuiltinOpParams builtinOpsParams;

    builtinOpsParams.srcMemObj = &src;
    builtinOpsParams.srcOffset.x = 1;
    builtinOpsParams.dstMemObj = &dst;
    builtinOpsParams.size = {src.getSize(), 0, 0};

    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo, builtinOpsParams));

    EXPECT_EQ(1u, multiDispatchInfo.size());

    const DispatchInfo *dispatchInfo = multiDispatchInfo.begin();

    EXPECT_EQ(dispatchInfo->getKernel()->getKernelInfo().name, "CopyBufferToBufferLeftLeftover");

    EXPECT_TRUE(compareBuiltinOpParams(multiDispatchInfo.peekBuiltinOpParams(), builtinOpsParams));
}

TEST_F(BuiltInTests, BuiltinDispatchInfoBuilderReadBufferAligned) {
    BuiltinDispatchInfoBuilder &builder = pBuiltIns->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer, *pContext, *pDevice);

    AlignedBuffer srcMemObj;
    auto size = 10 * MemoryConstants::cacheLineSize;
    auto dstPtr = alignedMalloc(size, MemoryConstants::cacheLineSize);

    MultiDispatchInfo multiDispatchInfo;
    BuiltinOpParams builtinOpsParams;

    builtinOpsParams.srcMemObj = &srcMemObj;
    builtinOpsParams.dstPtr = dstPtr;
    builtinOpsParams.size = {size, 0, 0};

    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo, builtinOpsParams));

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
    alignedFree(dstPtr);
}

TEST_F(BuiltInTests, BuiltinDispatchInfoBuilderWriteBufferAligned) {
    BuiltinDispatchInfoBuilder &builder = pBuiltIns->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer, *pContext, *pDevice);

    auto size = 10 * MemoryConstants::cacheLineSize;
    auto srcPtr = alignedMalloc(size, MemoryConstants::cacheLineSize);
    AlignedBuffer dstMemObj;

    MultiDispatchInfo multiDispatchInfo;
    BuiltinOpParams builtinOpsParams;

    builtinOpsParams.srcPtr = srcPtr;
    builtinOpsParams.dstMemObj = &dstMemObj;
    builtinOpsParams.size = {size, 0, 0};

    ASSERT_TRUE(builder.buildDispatchInfos(multiDispatchInfo, builtinOpsParams));

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

TEST_F(BuiltInTests, BuiltinDispatchInfoBuilderGetBuilderTwice) {
    BuiltinDispatchInfoBuilder &builder1 = pBuiltIns->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer, *pContext, *pDevice);
    BuiltinDispatchInfoBuilder &builder2 = pBuiltIns->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToBuffer, *pContext, *pDevice);

    EXPECT_EQ(&builder1, &builder2);
}

TEST_F(BuiltInTests, BuiltinDispatchInfoBuilderGetBuilderForUnknownBuiltInOp) {
    bool caughtException = false;
    try {
        pBuiltIns->getBuiltinDispatchInfoBuilder(EBuiltInOps::COUNT, *pContext, *pDevice);
    } catch (const std::runtime_error &) {
        caughtException = true;
    }
    EXPECT_TRUE(caughtException);
}

HWCMDTEST_F(IGFX_GEN8_CORE, BuiltInTests, getSchedulerKernel) {
    if (pDevice->getSupportedClVersion() >= 20) {
        Context &context = *pContext;
        SchedulerKernel &schedulerKernel = pBuiltIns->getSchedulerKernel(context);
        std::string name = SchedulerKernel::schedulerName;
        EXPECT_EQ(name, schedulerKernel.getKernelInfo().name);
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, BuiltInTests, getSchedulerKernelForSecondTimeDoesNotCreateNewKernel) {
    if (pDevice->getSupportedClVersion() >= 20) {
        Context &context = *pContext;
        SchedulerKernel &schedulerKernel = pBuiltIns->getSchedulerKernel(context);

        Program *program = schedulerKernel.getProgram();

        SchedulerKernel &schedulerKernelSecond = pBuiltIns->getSchedulerKernel(context);

        Program *program2 = schedulerKernelSecond.getProgram();

        EXPECT_EQ(&schedulerKernel, &schedulerKernelSecond);
        EXPECT_EQ(program, program2);
    }
}

TEST_F(BuiltInTests, BuiltinDispatchInfoBuilderReturnFalseIfUnsupportedBuildType) {
    auto &bs = *pDevice->getExecutionEnvironment()->getBuiltIns();
    BuiltinDispatchInfoBuilder bdib{bs};
    MultiDispatchInfo multiDispatchInfo;
    BuiltinOpParams params;

    auto ret = bdib.buildDispatchInfos(multiDispatchInfo, params);
    EXPECT_FALSE(ret);
    ASSERT_EQ(0U, multiDispatchInfo.size());

    ret = bdib.buildDispatchInfos(multiDispatchInfo, nullptr, 0, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
    EXPECT_FALSE(ret);
    EXPECT_EQ(0U, multiDispatchInfo.size());
}

TEST_F(BuiltInTests, GeivenDefaultBuiltinDispatchInfoBuilderWhendValidateDispatchIsCalledThenClSuccessIsReturned) {
    auto &bs = *pDevice->getExecutionEnvironment()->getBuiltIns();
    BuiltinDispatchInfoBuilder bdib{bs};
    auto ret = bdib.validateDispatch(nullptr, 1, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0}, Vec3<size_t>{0, 0, 0});
    EXPECT_EQ(CL_SUCCESS, ret);
}

TEST_F(BuiltInTests, BuiltinDispatchInfoBuilderReturnTrueIfExplicitKernelArgNotTakenCareOfInBuiltinDispatchBInfoBuilder) {
    auto &bs = *pDevice->getExecutionEnvironment()->getBuiltIns();
    BuiltinDispatchInfoBuilder bdib{bs};
    MultiDispatchInfo multiDispatchInfo;
    BuiltinOpParams params;

    cl_int err;
    auto ret = bdib.setExplicitArg(1, 5, nullptr, err);
    EXPECT_TRUE(ret);
}

TEST_F(VmeBuiltInTests, BuiltinDispatchInfoBuilderGetVMEBuilderReturnNonNull) {
    overwriteBuiltInBinaryName(pDevice, "media_kernels_backend");

    EBuiltInOps::Type vmeOps[] = {EBuiltInOps::VmeBlockMotionEstimateIntel, EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel, EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel};
    for (auto op : vmeOps) {
        BuiltinDispatchInfoBuilder &builder = pBuiltIns->getBuiltinDispatchInfoBuilder(op, *pContext, *pDevice);
        EXPECT_NE(nullptr, &builder);
    }

    restoreBuiltInBinaryName(pDevice);
}

TEST_F(VmeBuiltInTests, BuiltinDispatchInfoBuilderVMEBuilderNullKernel) {
    overwriteBuiltInBinaryName(pDevice, "media_kernels_backend");
    EBuiltInOps::Type vmeOps[] = {EBuiltInOps::VmeBlockMotionEstimateIntel, EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel, EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel};
    for (auto op : vmeOps) {
        BuiltinDispatchInfoBuilder &builder = pBuiltIns->getBuiltinDispatchInfoBuilder(op, *pContext, *pDevice);

        MultiDispatchInfo outMdi;
        Vec3<size_t> gws{352, 288, 0};
        Vec3<size_t> elws{0, 0, 0};
        Vec3<size_t> offset{0, 0, 0};
        auto ret = builder.buildDispatchInfos(outMdi, nullptr, 0, gws, elws, offset);
        EXPECT_FALSE(ret);
        EXPECT_EQ(0U, outMdi.size());
    }
    restoreBuiltInBinaryName(pDevice);
}

TEST_F(VmeBuiltInTests, BuiltinDispatchInfoBuilderVMEBuilder) {
    MockKernelWithInternals mockKernel{*pDevice};
    ((SPatchExecutionEnvironment *)mockKernel.kernelInfo.patchInfo.executionEnvironment)->CompiledSIMD32 = 0;
    ((SPatchExecutionEnvironment *)mockKernel.kernelInfo.patchInfo.executionEnvironment)->CompiledSIMD16 = 1;
    mockKernel.kernelInfo.reqdWorkGroupSize[0] = 16;
    mockKernel.kernelInfo.reqdWorkGroupSize[1] = 0;
    mockKernel.kernelInfo.reqdWorkGroupSize[2] = 0;

    overwriteBuiltInBinaryName(pDevice, "media_kernels_backend");
    BuiltinDispatchInfoBuilder &builder = pBuiltIns->getBuiltinDispatchInfoBuilder(EBuiltInOps::VmeBlockMotionEstimateIntel, *pContext, *pDevice);
    restoreBuiltInBinaryName(pDevice);

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
    ASSERT_EQ(vmeImplicitArgsBase + vmeImplicitArgs, outDi->getKernel()->getKernelInfo().kernelArgInfo.size());
    uint32_t vmeExtraArgsExpectedVals[] = {18, 22, 18}; // height, width, stride
    for (uint32_t i = 0; i < vmeImplicitArgs; ++i) {
        auto &argInfo = outDi->getKernel()->getKernelInfo().kernelArgInfo[vmeImplicitArgsBase + i];
        ASSERT_EQ(1U, argInfo.kernelArgPatchInfoVector.size());
        auto off = argInfo.kernelArgPatchInfoVector[0].crossthreadOffset;
        EXPECT_EQ(vmeExtraArgsExpectedVals[i], *((uint32_t *)(outDi->getKernel()->getCrossThreadData() + off)));
    }
}

TEST_F(VmeBuiltInTests, BuiltinDispatchInfoBuilderAdvancedVMEBuilder) {
    MockKernelWithInternals mockKernel{*pDevice};
    ((SPatchExecutionEnvironment *)mockKernel.kernelInfo.patchInfo.executionEnvironment)->CompiledSIMD32 = 0;
    ((SPatchExecutionEnvironment *)mockKernel.kernelInfo.patchInfo.executionEnvironment)->CompiledSIMD16 = 1;
    mockKernel.kernelInfo.reqdWorkGroupSize[0] = 16;
    mockKernel.kernelInfo.reqdWorkGroupSize[1] = 0;
    mockKernel.kernelInfo.reqdWorkGroupSize[2] = 0;

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

    EBuiltInOps::Type vmeOps[] = {EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel, EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel};
    for (auto op : vmeOps) {
        MultiDispatchInfo outMdi;
        overwriteBuiltInBinaryName(pDevice, "media_kernels_backend");
        BuiltinDispatchInfoBuilder &builder = pBuiltIns->getBuiltinDispatchInfoBuilder(op, *pContext, *pDevice);
        restoreBuiltInBinaryName(pDevice);

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
        ASSERT_EQ(vmeImplicitArgsBase + vmeImplicitArgs, outDi->getKernel()->getKernelInfo().kernelArgInfo.size());
        EXPECT_EQ(srcImageArg, outDi->getKernel()->getKernelArg(vmeImplicitArgsBase));
        ++vmeImplicitArgsBase;
        --vmeImplicitArgs;
        uint32_t vmeExtraArgsExpectedVals[] = {18, 22, 18}; // height, width, stride
        for (uint32_t i = 0; i < vmeImplicitArgs; ++i) {
            auto &argInfo = outDi->getKernel()->getKernelInfo().kernelArgInfo[vmeImplicitArgsBase + i];
            ASSERT_EQ(1U, argInfo.kernelArgPatchInfoVector.size());
            auto off = argInfo.kernelArgPatchInfoVector[0].crossthreadOffset;
            EXPECT_EQ(vmeExtraArgsExpectedVals[i], *((uint32_t *)(outDi->getKernel()->getCrossThreadData() + off)));
        }
    }
}

TEST_F(VmeBuiltInTests, getBuiltinAsString) {
    EXPECT_EQ(0, strcmp("aux_translation.igdrcl_built_in", getBuiltinAsString(EBuiltInOps::AuxTranslation)));
    EXPECT_EQ(0, strcmp("copy_buffer_to_buffer.igdrcl_built_in", getBuiltinAsString(EBuiltInOps::CopyBufferToBuffer)));
    EXPECT_EQ(0, strcmp("copy_buffer_rect.igdrcl_built_in", getBuiltinAsString(EBuiltInOps::CopyBufferRect)));
    EXPECT_EQ(0, strcmp("fill_buffer.igdrcl_built_in", getBuiltinAsString(EBuiltInOps::FillBuffer)));
    EXPECT_EQ(0, strcmp("copy_buffer_to_image3d.igdrcl_built_in", getBuiltinAsString(EBuiltInOps::CopyBufferToImage3d)));
    EXPECT_EQ(0, strcmp("copy_image3d_to_buffer.igdrcl_built_in", getBuiltinAsString(EBuiltInOps::CopyImage3dToBuffer)));
    EXPECT_EQ(0, strcmp("copy_image_to_image1d.igdrcl_built_in", getBuiltinAsString(EBuiltInOps::CopyImageToImage1d)));
    EXPECT_EQ(0, strcmp("copy_image_to_image2d.igdrcl_built_in", getBuiltinAsString(EBuiltInOps::CopyImageToImage2d)));
    EXPECT_EQ(0, strcmp("copy_image_to_image3d.igdrcl_built_in", getBuiltinAsString(EBuiltInOps::CopyImageToImage3d)));
    EXPECT_EQ(0, strcmp("fill_image1d.igdrcl_built_in", getBuiltinAsString(EBuiltInOps::FillImage1d)));
    EXPECT_EQ(0, strcmp("fill_image2d.igdrcl_built_in", getBuiltinAsString(EBuiltInOps::FillImage2d)));
    EXPECT_EQ(0, strcmp("fill_image3d.igdrcl_built_in", getBuiltinAsString(EBuiltInOps::FillImage3d)));
    EXPECT_EQ(0, strcmp("vme_block_motion_estimate_intel.igdrcl_built_in", getBuiltinAsString(EBuiltInOps::VmeBlockMotionEstimateIntel)));
    EXPECT_EQ(0, strcmp("vme_block_advanced_motion_estimate_check_intel.igdrcl_built_in", getBuiltinAsString(EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel)));
    EXPECT_EQ(0, strcmp("vme_block_advanced_motion_estimate_bidirectional_check_intel", getBuiltinAsString(EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel)));
    EXPECT_EQ(0, strcmp("scheduler.igdrcl_built_in", getBuiltinAsString(EBuiltInOps::Scheduler)));
    EXPECT_EQ(0, strcmp("unknown", getBuiltinAsString(EBuiltInOps::COUNT)));
}

TEST_F(BuiltInTests, WhenUnknownOperationIsSpecifiedThenUnknownNameIsReturned) {
    EXPECT_EQ(0, strcmp("unknown", getUnknownBuiltinAsString(EBuiltInOps::CopyImage3dToBuffer)));
    EXPECT_EQ(0, strcmp("unknown", getUnknownBuiltinAsString(EBuiltInOps::COUNT)));
}

TEST_F(BuiltInTests, getExtension) {
    EXPECT_EQ(0, strcmp("", BuiltinCode::getExtension(BuiltinCode::ECodeType::Any)));
    EXPECT_EQ(0, strcmp(".bin", BuiltinCode::getExtension(BuiltinCode::ECodeType::Binary)));
    EXPECT_EQ(0, strcmp(".bc", BuiltinCode::getExtension(BuiltinCode::ECodeType::Intermediate)));
    EXPECT_EQ(0, strcmp(".cl", BuiltinCode::getExtension(BuiltinCode::ECodeType::Source)));
    EXPECT_EQ(0, strcmp("", BuiltinCode::getExtension(BuiltinCode::ECodeType::COUNT)));
    EXPECT_EQ(0, strcmp("", BuiltinCode::getExtension(BuiltinCode::ECodeType::INVALID)));
}

TEST_F(BuiltInTests, createBuiltinResource) {
    std::string resource = "__kernel";

    auto br1 = createBuiltinResource(resource.data(), resource.size());
    EXPECT_NE(0u, br1.size());
    auto br2 = createBuiltinResource(br1);
    EXPECT_NE(0u, br2.size());

    EXPECT_EQ(br1, br2);
}

TEST_F(BuiltInTests, createBuiltinResourceName) {
    EBuiltInOps::Type builtin = EBuiltInOps::CopyBufferToBuffer;
    const std::string extension = ".cl";
    const std::string platformName = "skl";
    const uint32_t deviceRevId = 9;

    std::string resourceNameGeneric = createBuiltinResourceName(builtin, extension);
    std::string resourceNameForPlatform = createBuiltinResourceName(builtin, extension, platformName);
    std::string resourceNameForPlatformAndStepping = createBuiltinResourceName(builtin, extension, platformName, deviceRevId);

    EXPECT_EQ(0, strcmp("copy_buffer_to_buffer.igdrcl_built_in.cl", resourceNameGeneric.c_str()));
    EXPECT_EQ(0, strcmp("skl_0_copy_buffer_to_buffer.igdrcl_built_in.cl", resourceNameForPlatform.c_str()));
    EXPECT_EQ(0, strcmp("skl_9_copy_buffer_to_buffer.igdrcl_built_in.cl", resourceNameForPlatformAndStepping.c_str()));
}

TEST_F(BuiltInTests, joinPath) {
    std::string resourceName = "copy_buffer_to_buffer.igdrcl_built_in.cl";
    std::string resourcePath = "path";

    EXPECT_EQ(0, strcmp(resourceName.c_str(), joinPath("", resourceName).c_str()));
    EXPECT_EQ(0, strcmp(resourcePath.c_str(), joinPath(resourcePath, "").c_str()));
    EXPECT_EQ(0, strcmp((resourcePath + PATH_SEPARATOR + resourceName).c_str(), joinPath(resourcePath + PATH_SEPARATOR, resourceName).c_str()));
    EXPECT_EQ(0, strcmp((resourcePath + PATH_SEPARATOR + resourceName).c_str(), joinPath(resourcePath, resourceName).c_str()));
}

TEST_F(BuiltInTests, EmbeddedStorageRegistry) {
    EmbeddedStorageRegistry storageRegistry;

    std::string resource = "__kernel";
    storageRegistry.store("kernel.cl", createBuiltinResource(resource.data(), resource.size() + 1));

    const BuiltinResourceT *br = storageRegistry.get("kernel.cl");
    EXPECT_NE(nullptr, br);
    EXPECT_EQ(0, strcmp(resource.data(), br->data()));

    const BuiltinResourceT *bnr = storageRegistry.get("unknown.cl");
    EXPECT_EQ(nullptr, bnr);
}

TEST_F(BuiltInTests, StorageRootPath) {
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

TEST_F(BuiltInTests, EmbeddedStorageLoadImpl) {
    class MockEmbeddedStorage : EmbeddedStorage {
      public:
        MockEmbeddedStorage(const std::string &rootPath) : EmbeddedStorage(rootPath){};
        BuiltinResourceT loadImpl(const std::string &fullResourceName) override {
            return EmbeddedStorage::loadImpl(fullResourceName);
        }
    };
    MockEmbeddedStorage mockEmbeddedStorage("root");

    BuiltinResourceT br = mockEmbeddedStorage.loadImpl("copy_buffer_to_buffer.igdrcl_built_in.cl");
    EXPECT_NE(0u, br.size());

    BuiltinResourceT bnr = mockEmbeddedStorage.loadImpl("unknown.cl");
    EXPECT_EQ(0u, bnr.size());
}

TEST_F(BuiltInTests, FileStorageLoadImpl) {
    class MockFileStorage : FileStorage {
      public:
        MockFileStorage(const std::string &rootPath) : FileStorage(rootPath){};
        BuiltinResourceT loadImpl(const std::string &fullResourceName) override {
            return FileStorage::loadImpl(fullResourceName);
        }
    };
    MockFileStorage mockEmbeddedStorage("root");

    BuiltinResourceT br = mockEmbeddedStorage.loadImpl("test_files/copybuffer.cl");
    EXPECT_NE(0u, br.size());

    BuiltinResourceT bnr = mockEmbeddedStorage.loadImpl("unknown.cl");
    EXPECT_EQ(0u, bnr.size());
}

TEST_F(BuiltInTests, builtinsLib) {
    class MockBuiltinsLib : BuiltinsLib {
      public:
        StoragesContainerT &getAllStorages() {
            return BuiltinsLib::allStorages;
        }
    };
    auto mockBuiltinsLib = std::unique_ptr<MockBuiltinsLib>(new MockBuiltinsLib());
    EXPECT_EQ(2u, mockBuiltinsLib->getAllStorages().size());
}

TEST_F(BuiltInTests, getBuiltinCodeForTypeAny) {
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    BuiltinCode code = builtinsLib->getBuiltinCode(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::ECodeType::Any, *pDevice);
    EXPECT_EQ(BuiltinCode::ECodeType::Binary, code.type);
    EXPECT_NE(0u, code.resource.size());
    EXPECT_EQ(pDevice, code.targetDevice);
}

TEST_F(BuiltInTests, getBuiltinCodeForTypeBinary) {
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    BuiltinCode code = builtinsLib->getBuiltinCode(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::ECodeType::Binary, *pDevice);
    EXPECT_EQ(BuiltinCode::ECodeType::Binary, code.type);
    EXPECT_NE(0u, code.resource.size());
    EXPECT_EQ(pDevice, code.targetDevice);
}

TEST_F(BuiltInTests, getBuiltinCodeForTypeIntermediate) {
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    BuiltinCode code = builtinsLib->getBuiltinCode(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::ECodeType::Intermediate, *pDevice);
    EXPECT_EQ(BuiltinCode::ECodeType::Intermediate, code.type);
    EXPECT_EQ(0u, code.resource.size());
    EXPECT_EQ(pDevice, code.targetDevice);
}

TEST_F(BuiltInTests, getBuiltinCodeForTypeSource) {
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    BuiltinCode code = builtinsLib->getBuiltinCode(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::ECodeType::Source, *pDevice);
    EXPECT_EQ(BuiltinCode::ECodeType::Source, code.type);
    EXPECT_NE(0u, code.resource.size());
    EXPECT_EQ(pDevice, code.targetDevice);
}

TEST_F(BuiltInTests, getBuiltinCodeForTypeInvalid) {
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    BuiltinCode code = builtinsLib->getBuiltinCode(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::ECodeType::INVALID, *pDevice);
    EXPECT_EQ(BuiltinCode::ECodeType::INVALID, code.type);
    EXPECT_EQ(0u, code.resource.size());
    EXPECT_EQ(pDevice, code.targetDevice);
}

TEST_F(BuiltInTests, getBuiltinResourcesForTypeSource) {
    class MockBuiltinsLib : BuiltinsLib {
      public:
        BuiltinResourceT getBuiltinResource(EBuiltInOps::Type builtin, BuiltinCode::ECodeType requestedCodeType, Device &device) {
            return BuiltinsLib::getBuiltinResource(builtin, requestedCodeType, device);
        }
    };
    auto mockBuiltinsLib = std::unique_ptr<MockBuiltinsLib>(new MockBuiltinsLib());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::AuxTranslation, BuiltinCode::ECodeType::Source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::ECodeType::Source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyBufferRect, BuiltinCode::ECodeType::Source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::FillBuffer, BuiltinCode::ECodeType::Source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyBufferToImage3d, BuiltinCode::ECodeType::Source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyImage3dToBuffer, BuiltinCode::ECodeType::Source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyImageToImage1d, BuiltinCode::ECodeType::Source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyImageToImage2d, BuiltinCode::ECodeType::Source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyImageToImage3d, BuiltinCode::ECodeType::Source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::FillImage1d, BuiltinCode::ECodeType::Source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::FillImage2d, BuiltinCode::ECodeType::Source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::FillImage3d, BuiltinCode::ECodeType::Source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::VmeBlockMotionEstimateIntel, BuiltinCode::ECodeType::Source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel, BuiltinCode::ECodeType::Source, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel, BuiltinCode::ECodeType::Source, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::COUNT, BuiltinCode::ECodeType::Source, *pDevice).size());

    if (pDevice->getHardwareInfo().capabilityTable.supportsDeviceEnqueue) {
        EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::Scheduler, BuiltinCode::ECodeType::Source, *pDevice).size());
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, BuiltInTests, getBuiltinResourcesForTypeBinary) {
    class MockBuiltinsLib : BuiltinsLib {
      public:
        BuiltinResourceT getBuiltinResource(EBuiltInOps::Type builtin, BuiltinCode::ECodeType requestedCodeType, Device &device) {
            return BuiltinsLib::getBuiltinResource(builtin, requestedCodeType, device);
        }
    };
    auto mockBuiltinsLib = std::unique_ptr<MockBuiltinsLib>(new MockBuiltinsLib());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::AuxTranslation, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyBufferRect, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::FillBuffer, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyBufferToImage3d, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyImage3dToBuffer, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyImageToImage1d, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyImageToImage2d, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::CopyImageToImage3d, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::FillImage1d, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::FillImage2d, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::FillImage3d, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::VmeBlockMotionEstimateIntel, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel, BuiltinCode::ECodeType::Binary, *pDevice).size());
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel, BuiltinCode::ECodeType::Binary, *pDevice).size());
    if (this->pDevice->getEnabledClVersion() >= 20) {
        EXPECT_NE(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::Scheduler, BuiltinCode::ECodeType::Binary, *pDevice).size());
    }
    EXPECT_EQ(0u, mockBuiltinsLib->getBuiltinResource(EBuiltInOps::COUNT, BuiltinCode::ECodeType::Binary, *pDevice).size());
}

TEST_F(BuiltInTests, createProgramFromCodeForTypeAny) {
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    const BuiltinCode bc = builtinsLib->getBuiltinCode(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::ECodeType::Any, *pDevice);
    EXPECT_NE(0u, bc.resource.size());
    auto program = std::unique_ptr<Program>(BuiltinsLib::createProgramFromCode(bc, *pContext, *pDevice));
    EXPECT_NE(nullptr, program.get());
}

TEST_F(BuiltInTests, createProgramFromCodeForTypeSource) {
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    const BuiltinCode bc = builtinsLib->getBuiltinCode(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::ECodeType::Source, *pDevice);
    EXPECT_NE(0u, bc.resource.size());
    auto program = std::unique_ptr<Program>(BuiltinsLib::createProgramFromCode(bc, *pContext, *pDevice));
    EXPECT_NE(nullptr, program.get());
}

TEST_F(BuiltInTests, givenCreateProgramFromSourceWhenDeviceSupportSharedSystemAllocationThenInternalOptionsDisableStosoFlag) {
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    pDevice->deviceInfo.sharedSystemMemCapabilities = CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS_INTEL;

    const BuiltinCode bc = builtinsLib->getBuiltinCode(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::ECodeType::Source, *pDevice);
    EXPECT_NE(0u, bc.resource.size());
    auto program = std::unique_ptr<Program>(BuiltinsLib::createProgramFromCode(bc, *pContext, *pDevice));
    EXPECT_NE(nullptr, program.get());
    EXPECT_THAT(program->getInternalOptions(), testing::HasSubstr(std::string("-cl-intel-greater-than-4GB-buffer-required")));
}

TEST_F(BuiltInTests, createProgramFromCodeForTypeIntermediate) {
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    const BuiltinCode bc = builtinsLib->getBuiltinCode(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::ECodeType::Intermediate, *pDevice);
    EXPECT_EQ(0u, bc.resource.size());
    auto program = std::unique_ptr<Program>(BuiltinsLib::createProgramFromCode(bc, *pContext, *pDevice));
    EXPECT_EQ(nullptr, program.get());
}

TEST_F(BuiltInTests, createProgramFromCodeForTypeBinary) {
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    const BuiltinCode bc = builtinsLib->getBuiltinCode(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::ECodeType::Binary, *pDevice);
    EXPECT_NE(0u, bc.resource.size());
    auto program = std::unique_ptr<Program>(BuiltinsLib::createProgramFromCode(bc, *pContext, *pDevice));
    EXPECT_NE(nullptr, program.get());
}

TEST_F(BuiltInTests, createProgramFromCodeForTypeInvalid) {
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    const BuiltinCode bc = builtinsLib->getBuiltinCode(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::ECodeType::INVALID, *pDevice);
    EXPECT_EQ(0u, bc.resource.size());
    auto program = std::unique_ptr<Program>(BuiltinsLib::createProgramFromCode(bc, *pContext, *pDevice));
    EXPECT_EQ(nullptr, program.get());
}

TEST_F(BuiltInTests, createProgramFromCodeForTypeAnyButBuiltinInvalid) {
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    const BuiltinCode bc = builtinsLib->getBuiltinCode(EBuiltInOps::COUNT, BuiltinCode::ECodeType::Any, *pDevice);
    EXPECT_EQ(0u, bc.resource.size());
    auto program = std::unique_ptr<Program>(BuiltinsLib::createProgramFromCode(bc, *pContext, *pDevice));
    EXPECT_EQ(nullptr, program.get());
}

TEST_F(BuiltInTests, createProgramFromCodeInternalOptionsFor32Bit) {
    bool force32BitAddressess = pDevice->getDeviceInfo().force32BitAddressess;
    const_cast<DeviceInfo *>(&pDevice->getDeviceInfo())->force32BitAddressess = true;

    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    const BuiltinCode bc = builtinsLib->getBuiltinCode(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::ECodeType::Source, *pDevice);
    ASSERT_NE(0u, bc.resource.size());
    auto program = std::unique_ptr<Program>(BuiltinsLib::createProgramFromCode(bc, *pContext, *pDevice));
    ASSERT_NE(nullptr, program.get());

    auto builtinInternalOptions = program->getInternalOptions();
    auto it = builtinInternalOptions.find("-m32");
    EXPECT_EQ(std::string::npos, it);

    it = builtinInternalOptions.find("-cl-intel-greater-than-4GB-buffer-required");
    if (is32bit || pDevice->areSharedSystemAllocationsAllowed()) {
        EXPECT_NE(std::string::npos, it);
    } else {
        EXPECT_EQ(std::string::npos, it);
    }

    const_cast<DeviceInfo *>(&pDevice->getDeviceInfo())->force32BitAddressess = force32BitAddressess;
}

TEST_F(BuiltInTests, whenQueriedProperVmeVersionIsReturned) {
    if (!pDevice->getHardwareInfo().capabilityTable.supportsVme) {
        GTEST_SKIP();
    }
    cl_uint param;
    auto ret = pDevice->getDeviceInfo(CL_DEVICE_ME_VERSION_INTEL, sizeof(param), &param, nullptr);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_EQ(static_cast<cl_uint>(CL_ME_VERSION_ADVANCED_VER_2_INTEL), param);
}

TEST_F(VmeBuiltInTests, vmeDispatchValidationHelpers) {
    this->pBuiltIns->setCacheingEnableState(false);
    overwriteBuiltInBinaryName(pDevice, "media_kernels_backend");
    BuiltInOp<EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel> vmeBuilder(*this->pBuiltIns, *pContext, *this->pDevice);
    restoreBuiltInBinaryName(pDevice);

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

TEST_F(VmeBuiltInTests, vmeDispatchIsBidir) {
    this->pBuiltIns->setCacheingEnableState(false);
    overwriteBuiltInBinaryName(pDevice, "media_kernels_backend");
    BuiltInOp<EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel> avmeBuilder(*this->pBuiltIns, *pContext, *this->pDevice);
    BuiltInOp<EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel> avmeBidirBuilder(*this->pBuiltIns, *pContext, *this->pDevice);
    restoreBuiltInBinaryName(pDevice);

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

TEST_F(VmeBuiltInTests, vmeValidateImages) {
    this->pBuiltIns->setCacheingEnableState(false);
    overwriteBuiltInBinaryName(pDevice, "media_kernels_backend");
    BuiltInOp<EBuiltInOps::VmeBlockMotionEstimateIntel> vmeBuilder(*this->pBuiltIns, *pContext, *this->pDevice);
    restoreBuiltInBinaryName(pDevice);

    uint32_t srcImgArgNum = 1;
    uint32_t refImgArgNum = 2;

    cl_int err;
    { // validate images are not null
        std::unique_ptr<Image> image1(ImageHelper<ImageVmeValidFormat>::create(pContext));

        cl_mem srcImgMem = 0;
        cl_mem refImgMem = 0;
        EXPECT_EQ(CL_INVALID_KERNEL_ARGS, vmeBuilder.validateImages(Vec3<size_t>{3, 3, 0}, Vec3<size_t>{0, 0, 0}));

        srcImgMem = image1.get();
        refImgMem = 0;
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
        pContext->isSharedContext = true;
        std::unique_ptr<Image> imageLinear(ImageHelper<ImageVmeValidFormat>::create(pContext));
        pContext->isSharedContext = false;
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

TEST_F(VmeBuiltInTests, vmeValidateFlags) {
    this->pBuiltIns->setCacheingEnableState(false);
    overwriteBuiltInBinaryName(pDevice, "media_kernels_backend");
    BuiltInOp<EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel> vmeBuilder(*this->pBuiltIns, *pContext, *this->pDevice);
    restoreBuiltInBinaryName(pDevice);

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

TEST_F(VmeBuiltInTests, vmeValidateSkipBlockType) {
    this->pBuiltIns->setCacheingEnableState(false);
    overwriteBuiltInBinaryName(pDevice, "media_kernels_backend");
    BuiltInOp<EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel> avmeBidirectionalBuilder(*this->pBuiltIns, *pContext, *this->pDevice);
    BuiltInOp<EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel> avmeBuilder(*this->pBuiltIns, *pContext, *this->pDevice);
    restoreBuiltInBinaryName(pDevice);

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

TEST_F(VmeBuiltInTests, setExplicitArgAccelerator) {
    this->pBuiltIns->setCacheingEnableState(false);
    overwriteBuiltInBinaryName(pDevice, "media_kernels_backend");

    BuiltInOp<EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel> vmeBuilder(*this->pBuiltIns, *pContext, *this->pDevice);
    restoreBuiltInBinaryName(pDevice);

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

TEST_F(VmeBuiltInTests, vmeValidateDispatch) {
    this->pBuiltIns->setCacheingEnableState(false);
    overwriteBuiltInBinaryName(pDevice, "media_kernels_backend");
    struct MockVmeBuilder : BuiltInOp<EBuiltInOps::VmeBlockMotionEstimateIntel> {
        MockVmeBuilder(BuiltIns &kernelsLib, Context &context, Device &device)
            : BuiltInOp<EBuiltInOps::VmeBlockMotionEstimateIntel>(kernelsLib, context, device) {
        }

        cl_int validateVmeDispatch(Vec3<size_t> inputRegion, Vec3<size_t> offset, size_t blkNum, size_t blkMul) const override {
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
    MockVmeBuilder vmeBuilder(*this->pBuiltIns, *pContext, *this->pDevice);
    restoreBuiltInBinaryName(pDevice);

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

TEST_F(VmeBuiltInTests, vmeValidateVmeDispatch) {
    this->pBuiltIns->setCacheingEnableState(false);
    overwriteBuiltInBinaryName(pDevice, "media_kernels_backend");
    BuiltInOp<EBuiltInOps::VmeBlockMotionEstimateIntel> vmeBuilder(*this->pBuiltIns, *pContext, *this->pDevice);
    restoreBuiltInBinaryName(pDevice);

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

TEST_F(VmeBuiltInTests, advancedVmeValidateVmeDispatch) {
    this->pBuiltIns->setCacheingEnableState(false);
    overwriteBuiltInBinaryName(pDevice, "media_kernels_backend");
    BuiltInOp<EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel> avmeBuilder(*this->pBuiltIns, *pContext, *this->pDevice);
    restoreBuiltInBinaryName(pDevice);

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

TEST_F(VmeBuiltInTests, advancedBidirectionalVmeValidateVmeDispatch) {
    this->pBuiltIns->setCacheingEnableState(false);
    overwriteBuiltInBinaryName(pDevice, "media_kernels_backend");
    BuiltInOp<EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel> avmeBuilder(*this->pBuiltIns, *pContext, *this->pDevice);
    restoreBuiltInBinaryName(pDevice);

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

TEST_F(VmeBuiltInTests, advancedVmeGetSkipResidualsBuffExpSizeDefaultValue) {
    this->pBuiltIns->setCacheingEnableState(false);
    overwriteBuiltInBinaryName(pDevice, "media_kernels_backend");
    BuiltInOp<EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel> vmeBuilder(*this->pBuiltIns, *pContext, *this->pDevice);
    restoreBuiltInBinaryName(pDevice);

    auto size16x16 = vmeBuilder.getSkipResidualsBuffExpSize(CL_ME_MB_TYPE_16x16_INTEL, 4);
    auto sizeDefault = vmeBuilder.getSkipResidualsBuffExpSize(8192, 4);
    EXPECT_EQ(size16x16, sizeDefault);
}

TEST_F(BuiltInTests, createBuiltInProgramForInvalidBuiltinKernelName) {
    const char *kernelNames = "invalid_kernel";
    cl_int retVal = CL_SUCCESS;

    cl_program program = pDevice->getExecutionEnvironment()->getBuiltIns()->createBuiltInProgram(
        *pContext,
        *pDevice,
        kernelNames,
        retVal);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, program);
}

TEST_F(BuiltInTests, getSipKernelReturnsProgramCreatedOutOfIsaAcquiredFromCompilerInterface) {
    MockBuiltins mockBuiltins;
    auto mockCompilerInterface = new MockCompilerInterface();
    pDevice->getExecutionEnvironment()->compilerInterface.reset(mockCompilerInterface);
    mockCompilerInterface->sipKernelBinaryOverride = mockCompilerInterface->getDummyGenBinary();
    cl_int errCode = CL_BUILD_PROGRAM_FAILURE;
    auto p = Program::createFromGenBinary(*pDevice->getExecutionEnvironment(), pContext, mockCompilerInterface->sipKernelBinaryOverride.data(), mockCompilerInterface->sipKernelBinaryOverride.size(),
                                          false, &errCode);
    ASSERT_EQ(CL_SUCCESS, errCode);
    errCode = p->processGenBinary();
    ASSERT_EQ(CL_SUCCESS, errCode);

    const SipKernel &sipKern = mockBuiltins.getSipKernel(SipKernelType::Csr, *pContext->getDevice(0));

    const auto &sipKernelInfo = p->getKernelInfo(static_cast<size_t>(0));

    auto compbinedKernelHeapSize = sipKernelInfo->heapInfo.pKernelHeader->KernelHeapSize;
    auto sipOffset = sipKernelInfo->systemKernelOffset;
    ASSERT_GT(compbinedKernelHeapSize, sipOffset);
    auto expectedMem = reinterpret_cast<const char *>(sipKernelInfo->heapInfo.pKernelHeap) + sipOffset;
    ASSERT_EQ(compbinedKernelHeapSize - sipOffset, sipKern.getBinarySize());
    EXPECT_EQ(0, memcmp(expectedMem, sipKern.getBinary(), sipKern.getBinarySize()));
    EXPECT_EQ(SipKernelType::Csr, mockCompilerInterface->requestedSipKernel);
    p->release();
}

TEST_F(BuiltInTests, givenSipKernelWhenItIsCreatedThenItHasGraphicsAllocationForKernel) {
    const SipKernel &sipKern = pDevice->getExecutionEnvironment()->getBuiltIns()->getSipKernel(SipKernelType::Csr, *pContext->getDevice(0));
    auto sipAllocation = sipKern.getSipAllocation();
    EXPECT_NE(nullptr, sipAllocation);
}

TEST_F(BuiltInTests, givenDebugFlagForceUseSourceWhenArgIsBinaryThenReturnBuiltinCodeBinary) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.RebuildPrecompiledKernels.set(true);
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    BuiltinCode code = builtinsLib->getBuiltinCode(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::ECodeType::Binary, *pDevice);
    EXPECT_EQ(BuiltinCode::ECodeType::Binary, code.type);
    EXPECT_NE(0u, code.resource.size());
    EXPECT_EQ(pDevice, code.targetDevice);
}

TEST_F(BuiltInTests, givenDebugFlagForceUseSourceWhenArgIsAnyThenReturnBuiltinCodeSource) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.RebuildPrecompiledKernels.set(true);
    auto builtinsLib = std::unique_ptr<BuiltinsLib>(new BuiltinsLib());
    BuiltinCode code = builtinsLib->getBuiltinCode(EBuiltInOps::CopyBufferToBuffer, BuiltinCode::ECodeType::Any, *pDevice);
    EXPECT_EQ(BuiltinCode::ECodeType::Source, code.type);
    EXPECT_NE(0u, code.resource.size());
    EXPECT_EQ(pDevice, code.targetDevice);
}

using BuiltInOwnershipWrapperTests = BuiltInTests;

TEST_F(BuiltInOwnershipWrapperTests, givenBuiltinWhenConstructedThenLockAndUnlockOnDestruction) {
    MockAuxBuilInOp mockAuxBuiltInOp(*pBuiltIns, *pContext, *pDevice);
    MockContext mockContext;

    {
        BuiltInOwnershipWrapper lock(mockAuxBuiltInOp, &mockContext);
        EXPECT_TRUE(mockAuxBuiltInOp.baseKernel->hasOwnership());
        EXPECT_EQ(&mockContext, &mockAuxBuiltInOp.baseKernel->getContext());
    }
    EXPECT_FALSE(mockAuxBuiltInOp.baseKernel->hasOwnership());
    EXPECT_EQ(pContext, &mockAuxBuiltInOp.baseKernel->getContext());
}

TEST_F(BuiltInOwnershipWrapperTests, givenLockWithoutParametersWhenConstructingThenLockOnlyWhenRequested) {
    MockAuxBuilInOp mockAuxBuiltInOp(*pBuiltIns, *pContext, *pDevice);
    MockContext mockContext;

    {
        BuiltInOwnershipWrapper lock;
        lock.takeOwnership(mockAuxBuiltInOp, &mockContext);
        EXPECT_TRUE(mockAuxBuiltInOp.baseKernel->hasOwnership());
        EXPECT_EQ(&mockContext, &mockAuxBuiltInOp.baseKernel->getContext());
    }
    EXPECT_FALSE(mockAuxBuiltInOp.baseKernel->hasOwnership());
    EXPECT_EQ(pContext, &mockAuxBuiltInOp.baseKernel->getContext());
}

TEST_F(BuiltInOwnershipWrapperTests, givenLockWithAcquiredOwnershipWhenTakeOwnershipCalledThenAbort) {
    MockAuxBuilInOp mockAuxBuiltInOp1(*pBuiltIns, *pContext, *pDevice);
    MockAuxBuilInOp mockAuxBuiltInOp2(*pBuiltIns, *pContext, *pDevice);

    BuiltInOwnershipWrapper lock(mockAuxBuiltInOp1, pContext);
    EXPECT_THROW(lock.takeOwnership(mockAuxBuiltInOp1, pContext), std::exception);
    EXPECT_THROW(lock.takeOwnership(mockAuxBuiltInOp2, pContext), std::exception);
}

HWTEST_F(BuiltInOwnershipWrapperTests, givenBuiltInOwnershipWrapperWhenAskedForTypeTraitsThenDisableCopyConstructorAndOperator) {
    EXPECT_FALSE(std::is_copy_constructible<BuiltInOwnershipWrapper>::value);
    EXPECT_FALSE(std::is_copy_assignable<BuiltInOwnershipWrapper>::value);
}
