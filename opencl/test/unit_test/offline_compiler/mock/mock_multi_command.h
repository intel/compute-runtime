/*
 * Copyright (C) 2022 Intel Corporation
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
        uniqueHelper = std::make_unique<MockOclocArgHelper>(filesMap);
        uniqueHelper->setAllCallBase(true);
        argHelper = uniqueHelper.get();
    }

    ~MockMultiCommand() override = default;

    int singleBuild(const std::vector<std::string> &args) override {
        ++singleBuildCalledCount;

        if (callBaseSingleBuild) {
            return MultiCommand::singleBuild(args);
        }

        return OclocErrorCode::SUCCESS;
    }

    std::map<std::string, std::string> filesMap{};
    std::unique_ptr<MockOclocArgHelper> uniqueHelper{};
    int singleBuildCalledCount{0};
    bool callBaseSingleBuild{true};
};

} // namespace NEO
