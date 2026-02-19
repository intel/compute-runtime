/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_kernel_info_helper.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/zebin/debug_zebin.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/program/program_with_zebin.h"

#include <memory>

using namespace NEO;

TEST_F(ProgramWithZebinFixture, givenNoZebinThenSegmentsAreEmpty) {
    auto segments = program->getZebinSegments(pClDevice->getRootDeviceIndex());

    EXPECT_EQ(std::numeric_limits<uintptr_t>::max(), segments.constData.address);
    EXPECT_EQ(0ULL, segments.constData.size);

    EXPECT_EQ(std::numeric_limits<uintptr_t>::max(), segments.varData.address);
    EXPECT_EQ(0ULL, segments.varData.size);

    EXPECT_EQ(std::numeric_limits<uintptr_t>::max(), segments.stringData.address);
    EXPECT_EQ(0ULL, segments.stringData.size);

    EXPECT_TRUE(segments.nameToSegMap.empty());
}

TEST_F(ProgramWithZebinFixture, givenZebinSegmentsThenSegmentsArePopulated) {
    populateProgramWithSegments(program.get());
    auto segments = program->getZebinSegments(rootDeviceIndex);

    auto checkGPUSeg = [](NEO::GraphicsAllocation *alloc, NEO::Zebin::Debug::Segments::Segment segment) {
        EXPECT_EQ(static_cast<uintptr_t>(alloc->getGpuAddress()), segment.address);
        EXPECT_EQ(static_cast<size_t>(alloc->getUnderlyingBufferSize()), segment.size);
    };
    checkGPUSeg(program->buildInfos[rootDeviceIndex].constantSurface->getGraphicsAllocation(), segments.constData);
    checkGPUSeg(program->buildInfos[rootDeviceIndex].globalSurface->getGraphicsAllocation(), segments.varData);
    checkGPUSeg(program->getKernelInfoArray(rootDeviceIndex)[0]->getIsaGraphicsAllocation(), segments.nameToSegMap[ZebinTestData::ValidEmptyProgram<>::kernelName]);

    EXPECT_EQ(reinterpret_cast<uintptr_t>(program->buildInfos[rootDeviceIndex].constStringSectionData.initData), segments.stringData.address);
    EXPECT_EQ(reinterpret_cast<const char *>(program->buildInfos[rootDeviceIndex].constStringSectionData.initData), strings);
    EXPECT_EQ(program->buildInfos[rootDeviceIndex].constStringSectionData.size, sizeof(strings));
}

TEST_F(ProgramWithZebinFixture, givenZebinSegmentsWithSharedGlobalAndConstSurfacesThenSegmentsArePopulated) {
    const bool createWithSharedGlobalConstSurfaces = true;
    populateProgramWithSegments(program.get(), createWithSharedGlobalConstSurfaces);
    auto segments = program->getZebinSegments(rootDeviceIndex);

    auto checkGPUSeg = [](NEO::SharedPoolAllocation *surface, NEO::Zebin::Debug::Segments::Segment segment) {
        EXPECT_EQ(static_cast<uintptr_t>(surface->getGpuAddress()), segment.address);
        EXPECT_EQ(static_cast<size_t>(surface->getSize()), segment.size);

        EXPECT_NE(surface->getGraphicsAllocation()->getGpuAddress(), surface->getGpuAddress());
        EXPECT_NE(surface->getGraphicsAllocation()->getUnderlyingBufferSize(), surface->getSize());
    };
    checkGPUSeg(program->buildInfos[rootDeviceIndex].constantSurface.get(), segments.constData);
    checkGPUSeg(program->buildInfos[rootDeviceIndex].globalSurface.get(), segments.varData);
}

TEST_F(ProgramWithZebinFixture, givenSharedIsaAllocationWhenGetZebinSegmentsThenSegmentsAreCorrectlyPopulated) {
    isUsingSharedIsaAllocation = true;
    populateProgramWithSegments(program.get());
    auto segments = program->getZebinSegments(rootDeviceIndex);

    auto checkGPUSeg = [](NEO::GraphicsAllocation *alloc, NEO::Zebin::Debug::Segments::Segment segment) {
        EXPECT_EQ(static_cast<uintptr_t>(alloc->getGpuAddress()), segment.address);
        EXPECT_EQ(static_cast<size_t>(alloc->getUnderlyingBufferSize()), segment.size);
    };
    checkGPUSeg(program->buildInfos[rootDeviceIndex].constantSurface->getGraphicsAllocation(), segments.constData);
    checkGPUSeg(program->buildInfos[rootDeviceIndex].globalSurface->getGraphicsAllocation(), segments.varData);

    {
        auto kernelInfo = program->getKernelInfoArray(rootDeviceIndex)[0];
        auto segment = segments.nameToSegMap[ZebinTestData::ValidEmptyProgram<>::kernelName];
        auto isaAlloc = kernelInfo->getIsaGraphicsAllocation();
        auto offset = kernelInfo->getIsaOffsetInParentAllocation();

        EXPECT_EQ(static_cast<uintptr_t>(isaAlloc->getGpuAddress() + offset), segment.address);
        EXPECT_EQ(static_cast<size_t>(kernelInfo->getIsaSubAllocationSize()), segment.size);
    }

    EXPECT_EQ(reinterpret_cast<uintptr_t>(program->buildInfos[rootDeviceIndex].constStringSectionData.initData), segments.stringData.address);
    EXPECT_EQ(reinterpret_cast<const char *>(program->buildInfos[rootDeviceIndex].constStringSectionData.initData), strings);
    EXPECT_EQ(program->buildInfos[rootDeviceIndex].constStringSectionData.size, sizeof(strings));
}

TEST_F(ProgramWithZebinFixture, givenNonEmptyDebugDataThenDebugZebinIsNotCreated) {
    addEmptyZebin(program.get());
    populateProgramWithSegments(program.get());
    program->buildInfos[rootDeviceIndex].debugDataSize = 8u;
    program->buildInfos[rootDeviceIndex].debugData.reset(nullptr);
    program->createDebugZebin(rootDeviceIndex);
    EXPECT_EQ(nullptr, program->buildInfos[rootDeviceIndex].debugData.get());
}

TEST_F(ProgramWithZebinFixture, givenEmptyDebugDataThenDebugZebinIsCreatedAndStoredInDebugData) {
    addEmptyZebin(program.get());
    populateProgramWithSegments(program.get());

    program->buildInfos[rootDeviceIndex].debugDataSize = 0u;
    program->buildInfos[rootDeviceIndex].debugData.reset(nullptr);
    program->createDebugZebin(rootDeviceIndex);
    EXPECT_NE(nullptr, program->buildInfos[rootDeviceIndex].debugData.get());
}

TEST_F(ProgramWithZebinFixture, givenEmptyDebugDataAndZebinBinaryFormatThenCreateDebugZebinAndReturnOnGetInfo) {
    addEmptyZebin(program.get());
    populateProgramWithSegments(program.get());
    program->buildInfos[rootDeviceIndex].debugDataSize = 0u;
    program->buildInfos[rootDeviceIndex].debugData.reset(nullptr);

    EXPECT_FALSE(program->wasCreateDebugZebinCalled);
    auto retVal = CL_INVALID_VALUE;
    size_t debugDataSize = 0;
    retVal = program->getInfo(CL_PROGRAM_DEBUG_INFO_SIZES_INTEL, sizeof(debugDataSize), &debugDataSize, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, program->buildInfos[rootDeviceIndex].debugData);
    EXPECT_TRUE(program->wasCreateDebugZebinCalled);

    program->wasCreateDebugZebinCalled = false;

    retVal = program->getInfo(CL_PROGRAM_DEBUG_INFO_SIZES_INTEL, sizeof(debugDataSize), &debugDataSize, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(program->wasCreateDebugZebinCalled);

    std::unique_ptr<char[]> debugData = std::make_unique<char[]>(debugDataSize);
    for (size_t n = 0; n < sizeof(debugData); n++) {
        debugData[n] = 0;
    }
    char *pDebugData = &debugData[0];
    size_t retData = 0;

    retVal = program->getInfo(CL_PROGRAM_DEBUG_INFO_INTEL, debugDataSize, &pDebugData, &retData);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(program->wasCreateDebugZebinCalled);

    program->buildInfos[rootDeviceIndex].debugDataSize = 0u;
    program->buildInfos[rootDeviceIndex].debugData.reset(nullptr);

    std::unique_ptr<char[]> debugData2 = std::make_unique<char[]>(debugDataSize);
    for (size_t n = 0; n < sizeof(debugData2); n++) {
        debugData2[n] = 0;
    }
    char *pDebugData2 = &debugData2[0];
    size_t retData2 = 0;
    retVal = program->getInfo(CL_PROGRAM_DEBUG_INFO_INTEL, debugDataSize, &pDebugData2, &retData2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(program->wasCreateDebugZebinCalled);

    cl_uint numDevices;
    retVal = clGetProgramInfo(program.get(), CL_PROGRAM_NUM_DEVICES, sizeof(numDevices), &numDevices, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(numDevices * sizeof(debugData), retData);
}

TEST_F(ProgramWithZebinFixture, givenZebinFormatAndDebuggerNotAvailableWhenCreatingDebugDataThenCreateDebugZebinIsCalled) {
    pClDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->debugger.reset(nullptr);

    addEmptyZebin(program.get());
    populateProgramWithSegments(program.get());
    auto &buildInfo = program->buildInfos[rootDeviceIndex];
    buildInfo.debugDataSize = 0u;
    buildInfo.debugData.reset(nullptr);
    for (auto &device : program->getDevices()) {
        program->createDebugData(device);
    }
    EXPECT_TRUE(program->wasCreateDebugZebinCalled);
    EXPECT_FALSE(program->wasProcessDebugDataCalled);
    EXPECT_NE(nullptr, program->buildInfos[rootDeviceIndex].debugData);
    EXPECT_GT(program->buildInfos[rootDeviceIndex].debugDataSize, 0u);
}

HWTEST_F(ProgramWithZebinFixture, givenProgramWhenDumpKernelInfoToAubCommentsIsCalledThenExpectedTokensAreVisible) {
    DebugManagerStateRestore restorer{};
    NEO::debugManager.flags.PrintZeInfoInAub.set(true);

    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    ultCsr.commandStreamReceiverType = NEO::CommandStreamReceiverType::aub;
    pDevice->getRootDeviceEnvironmentRef().initAubCenter(false, "", NEO::CommandStreamReceiverType::aub);
    auto aubManager = static_cast<MockAubManager *>(pDevice->getRootDeviceEnvironmentRef().aubCenter->getAubManager());

    ZebinTestData::ZebinWithExternalFunctionsInfo zebin;
    auto copyHwInfo = pDevice->getHardwareInfo();
    auto &compilerProductHelper = pDevice->getRootDeviceEnvironment().getHelper<NEO::CompilerProductHelper>();
    compilerProductHelper.adjustHwInfoForIgc(copyHwInfo);
    zebin.setProductFamily(static_cast<uint16_t>(copyHwInfo.platform.eProductFamily));

    auto &buildInfo = program->buildInfos[rootDeviceIndex];
    buildInfo.unpackedDeviceBinary = makeCopy<char>(reinterpret_cast<const char *>(zebin.storage.data()), zebin.storage.size());
    buildInfo.unpackedDeviceBinarySize = zebin.storage.size();

    auto blob = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(buildInfo.unpackedDeviceBinary.get()), buildInfo.unpackedDeviceBinarySize);
    NEO::SingleDeviceBinary binary = {};
    binary.deviceBinary = blob;
    binary.targetDevice = NEO::getTargetDevice(pDevice->getRootDeviceEnvironment());
    std::string decodeErrors;
    std::string decodeWarnings;
    auto &gfxCoreHelper = pClDevice->getGfxCoreHelper();
    NEO::ProgramInfo programInfo;
    NEO::decodeSingleDeviceBinary(programInfo, binary, decodeErrors, decodeWarnings, gfxCoreHelper);

    for (auto *kernelInfo : programInfo.kernelInfos) {
        buildInfo.kernelInfoArray.push_back(kernelInfo);
    }
    programInfo.kernelInfos.clear();

    for (auto &kernelInfo : buildInfo.kernelInfoArray) {
        kernelInfo->createKernelAllocation(pClDevice->getDevice(), false);
    }

    NEO::Linker::RelocatedSymbol<NEO::SymbolInfo> fun0Symbol;
    fun0Symbol.gpuAddress = 0x1234;
    fun0Symbol.symbol.size = 16;
    fun0Symbol.symbol.segment = NEO::SegmentType::instructions;
    buildInfo.symbols["fun0"] = fun0Symbol;

    NEO::Linker::RelocatedSymbol<NEO::SymbolInfo> fun1Symbol;
    fun1Symbol.gpuAddress = 0x5678;
    fun1Symbol.symbol.size = 16;
    fun1Symbol.symbol.segment = NEO::SegmentType::instructions;
    buildInfo.symbols["fun1"] = fun1Symbol;

    program->dumpKernelInfoToAubComments();

    auto &comments = aubManager->receivedComments;

    // <KernelInfo> tag exists
    auto kernelInfoPos = comments.find("<KernelInfo>");
    EXPECT_NE(std::string::npos, kernelInfoPos);

    // <ExportedSymbols> tag exists
    auto exportedSymbolsPos = comments.find("<ExportedSymbols>");
    EXPECT_NE(std::string::npos, exportedSymbolsPos);

    // <BuildOptions> tag doesn't exist (options are empty)
    auto buildOptionsPos = comments.find("<BuildOptions>");
    EXPECT_EQ(std::string::npos, buildOptionsPos);

    // <Zeinfo> tag exists
    auto zeInfoPos = comments.find("<Zeinfo>");
    EXPECT_NE(std::string::npos, zeInfoPos);

    // correct ordering: <KernelInfo> before <ExportedSymbols> before <Zeinfo>
    EXPECT_LT(kernelInfoPos, exportedSymbolsPos);
    EXPECT_LT(exportedSymbolsPos, zeInfoPos);

    // kernel names exist between <KernelInfo> and <ExportedSymbols>
    auto kernelInfoSection = comments.substr(kernelInfoPos, exportedSymbolsPos - kernelInfoPos);
    EXPECT_NE(std::string::npos, kernelInfoSection.find("name : kernel"));
    EXPECT_NE(std::string::npos, kernelInfoSection.find("name : Intel_Symbol_Table_Void_Program"));

    // exported symbol names exist between <ExportedSymbols> and <Zeinfo>
    auto exportedSymbolsSection = comments.substr(exportedSymbolsPos, zeInfoPos - exportedSymbolsPos);
    EXPECT_NE(std::string::npos, exportedSymbolsSection.find("name : fun0"));
    EXPECT_NE(std::string::npos, exportedSymbolsSection.find("name : fun1"));

    // zeinfo content exists after <Zeinfo>
    auto zeInfoSection = comments.substr(zeInfoPos);
    EXPECT_NE(std::string::npos, zeInfoSection.find("name: kernel"));
    EXPECT_NE(std::string::npos, zeInfoSection.find("name: Intel_Symbol_Table_Void_Program"));
    EXPECT_NE(std::string::npos, zeInfoSection.find("name: fun0"));
    EXPECT_NE(std::string::npos, zeInfoSection.find("name: fun1"));

    {
        // no exported symbols, add build options
        aubManager->receivedComments.clear();

        std::string testBuildOptions = "-ze-opt-disable -ze-opt-greater-than-4GB-buffer-required";
        program->options = testBuildOptions;
        buildInfo.symbols.clear();

        program->dumpKernelInfoToAubComments();

        // <KernelInfo> tag exists
        auto kernelInfoPos = comments.find("<KernelInfo>");
        EXPECT_NE(std::string::npos, kernelInfoPos);

        // <ExportedSymbols> tag doesn't exist
        auto exportedSymbolsPos = comments.find("<ExportedSymbols>");
        EXPECT_EQ(std::string::npos, exportedSymbolsPos);

        // <BuildOptions> tag exists
        auto buildOptionsPos = comments.find("<BuildOptions>");
        EXPECT_NE(std::string::npos, buildOptionsPos);

        // build options content present
        EXPECT_NE(std::string::npos, comments.find(testBuildOptions));

        // <Zeinfo> tag exists
        auto zeInfoPos = comments.find("<Zeinfo>");
        EXPECT_NE(std::string::npos, zeInfoPos);

        // correct ordering: <KernelInfo> before <BuildOptions> before <Zeinfo>
        EXPECT_LT(kernelInfoPos, buildOptionsPos);
        EXPECT_LT(buildOptionsPos, zeInfoPos);
    }

    auto memoryManager = pDevice->getMemoryManager();
    for (auto &kernelInfo : buildInfo.kernelInfoArray) {
        if (kernelInfo->getIsaGraphicsAllocation()) {
            memoryManager->freeGraphicsMemory(kernelInfo->getIsaGraphicsAllocation());
        }
        delete kernelInfo;
    }
}
