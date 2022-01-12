/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/offline_linker.h"

namespace NEO {

class MockOfflineLinker : public OfflineLinker {
  public:
    using OfflineLinker::InputFileContent;
    using OfflineLinker::OperationMode;

    using OfflineLinker::initHardwareInfo;
    using OfflineLinker::initialize;
    using OfflineLinker::loadInputFilesContent;
    using OfflineLinker::parseCommand;
    using OfflineLinker::prepareIgc;
    using OfflineLinker::tryToStoreBuildLog;
    using OfflineLinker::verifyLinkerCommand;

    using OfflineLinker::hwInfo;
    using OfflineLinker::inputFilenames;
    using OfflineLinker::inputFilesContent;
    using OfflineLinker::internalOptions;
    using OfflineLinker::operationMode;
    using OfflineLinker::options;
    using OfflineLinker::outputFilename;
    using OfflineLinker::outputFormat;

    bool shouldReturnEmptyHardwareInfoTable{false};
    bool shouldFailLoadingOfIgcLib{false};
    bool shouldFailLoadingOfIgcCreateMainFunction{false};
    bool shouldFailCreationOfIgcMain{false};
    bool shouldFailCreationOfIgcDeviceContext{false};
    bool shouldReturnInvalidIgcPlatformHandle{false};
    bool shouldReturnInvalidGTSystemInfoHandle{false};

    MockOfflineLinker(OclocArgHelper *argHelper) : OfflineLinker{argHelper} {}

    ArrayRef<const HardwareInfo *> getHardwareInfoTable() const override {
        if (shouldReturnEmptyHardwareInfoTable) {
            return {};
        } else {
            return OfflineLinker::getHardwareInfoTable();
        }
    }

    std::unique_ptr<OsLibrary> loadIgcLibrary() const override {
        if (shouldFailLoadingOfIgcLib) {
            return nullptr;
        } else {
            return OfflineLinker::loadIgcLibrary();
        }
    }

    CIF::CreateCIFMainFunc_t loadCreateIgcMainFunction() const override {
        if (shouldFailLoadingOfIgcCreateMainFunction) {
            return nullptr;
        } else {
            return OfflineLinker::loadCreateIgcMainFunction();
        }
    }

    CIF::RAII::UPtr_t<CIF::CIFMain> createIgcMain(CIF::CreateCIFMainFunc_t createMainFunction) const override {
        if (shouldFailCreationOfIgcMain) {
            return nullptr;
        } else {
            return OfflineLinker::createIgcMain(createMainFunction);
        }
    }

    CIF::RAII::UPtr_t<IGC::IgcOclDeviceCtxTagOCL> createIgcDeviceContext() const override {
        if (shouldFailCreationOfIgcDeviceContext) {
            return nullptr;
        } else {
            return OfflineLinker::createIgcDeviceContext();
        }
    }

    CIF::RAII::UPtr_t<IGC::PlatformTagOCL> getIgcPlatformHandle() const override {
        if (shouldReturnInvalidIgcPlatformHandle) {
            return nullptr;
        } else {
            return OfflineLinker::getIgcPlatformHandle();
        }
    }

    CIF::RAII::UPtr_t<IGC::GTSystemInfoTagOCL> getGTSystemInfoHandle() const override {
        if (shouldReturnInvalidGTSystemInfoHandle) {
            return nullptr;
        } else {
            return OfflineLinker::getGTSystemInfoHandle();
        }
    }
};

} // namespace NEO