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

    MockOfflineLinker(OclocArgHelper *argHelper, std::unique_ptr<OclocIgcFacade> igcFacade)
        : OfflineLinker{argHelper, std::move(igcFacade)} {}

    ArrayRef<const HardwareInfo *> getHardwareInfoTable() const override {
        if (shouldReturnEmptyHardwareInfoTable) {
            return {};
        } else {
            return OfflineLinker::getHardwareInfoTable();
        }
    }
};

} // namespace NEO