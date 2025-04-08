/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/multi_command.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_argument_helper.h"

#include <optional>
#include <string>

namespace NEO {

class MockMultiCommand : public MultiCommand {
  public:
    using MultiCommand::argHelper;
    using MultiCommand::lines;
    using MultiCommand::quiet;
    using MultiCommand::retValues;

    using MultiCommand::addAdditionalOptionsToSingleCommandLine;
    using MultiCommand::initialize;
    using MultiCommand::printHelp;
    using MultiCommand::runBuilds;
    using MultiCommand::showResults;
    using MultiCommand::singleBuild;
    using MultiCommand::splitLineInSeparateArgs;

    MockMultiCommand() : MultiCommand{} {
        filesMap[clCopybufferFilename] = kernelSources;
        uniqueHelper = std::make_unique<MockOclocArgHelper>(filesMap);
        uniqueHelper->setAllCallBase(false);
        argHelper = uniqueHelper.get();
    }

    ~MockMultiCommand() override = default;

    int singleBuild(const std::vector<std::string> &args) override {
        ++singleBuildCalledCount;

        if (callBaseSingleBuild) {
            return MultiCommand::singleBuild(args);
        }

        return OCLOC_SUCCESS;
    }

    std::map<std::string, std::string> filesMap{};
    std::unique_ptr<MockOclocArgHelper> uniqueHelper{};
    int singleBuildCalledCount{0};
    bool callBaseSingleBuild{true};
    const std::string clCopybufferFilename = "some_kernel.cl";
    std::string kernelSources = "example_kernel(){}";
};

} // namespace NEO