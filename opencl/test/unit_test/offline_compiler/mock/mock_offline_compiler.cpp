/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/offline_compiler/mock/mock_offline_compiler.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_argument_helper.h"
#include "opencl/test/unit_test/offline_compiler/mock/mock_ocloc_fcl_facade.h"
#include "opencl/test/unit_test/offline_compiler/mock/mock_ocloc_igc_facade.h"

namespace NEO {

MockOfflineCompiler::MockOfflineCompiler() : OfflineCompiler() {
    uniqueHelper = std::make_unique<MockOclocArgHelper>(filesMap);
    uniqueHelper->setAllCallBase(false);
    argHelper = uniqueHelper.get();

    auto uniqueFclFacadeMock = std::make_unique<MockOclocFclFacade>(argHelper);
    mockFclFacade = uniqueFclFacadeMock.get();
    fclFacade = std::move(uniqueFclFacadeMock);

    auto uniqueIgcFacadeMock = std::make_unique<MockOclocIgcFacade>(argHelper);
    mockIgcFacade = uniqueIgcFacadeMock.get();
    igcFacade = std::move(uniqueIgcFacadeMock);
}

MockOfflineCompiler::~MockOfflineCompiler() = default;

int MockOfflineCompiler::initialize(size_t numArgs, const std::vector<std::string> &argv) {
    auto ret = OfflineCompiler::initialize(numArgs, argv, true);
    igcFacade->initialize(hwInfo);
    fclFacade->initialize(hwInfo);
    return ret;
}

void MockOfflineCompiler::storeGenBinary(const void *pSrc, const size_t srcSize) {
    OfflineCompiler::storeBinary(genBinary, genBinarySize, pSrc, srcSize);
}

int MockOfflineCompiler::build() {
    ++buildCalledCount;

    if (buildReturnValue.has_value()) {
        return *buildReturnValue;
    }

    return OfflineCompiler::build();
}

int MockOfflineCompiler::buildToIrBinary() {
    if (overrideBuildToIrBinaryStatus) {
        return buildToIrBinaryStatus;
    }
    return OfflineCompiler::buildToIrBinary();
}

int MockOfflineCompiler::buildSourceCode() {
    if (overrideBuildSourceCodeStatus) {
        return buildSourceCodeStatus;
    }
    return OfflineCompiler::buildSourceCode();
}

bool MockOfflineCompiler::generateElfBinary() {
    generateElfBinaryCalled++;
    return OfflineCompiler::generateElfBinary();
}

void MockOfflineCompiler::writeOutAllFiles() {
    writeOutAllFilesCalled++;
    OfflineCompiler::writeOutAllFiles();
}

void MockOfflineCompiler::clearLog() {
    uniqueHelper = std::make_unique<MockOclocArgHelper>(filesMap);
    uniqueHelper->setAllCallBase(true);
    argHelper = uniqueHelper.get();
}

int MockOfflineCompiler::createDir(const std::string &path) {
    if (interceptCreatedDirs) {
        createdDirs.push_back(path);
        return OCLOC_SUCCESS;
    } else {
        return OfflineCompiler::createDir(path);
    }
}

} // namespace NEO