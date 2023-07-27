/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <sstream>
#include <string>
#include <vector>

class OclocArgHelper;

namespace NEO {

class MultiCommand {
  public:
    MultiCommand &operator=(const MultiCommand &) = delete;
    MultiCommand(const MultiCommand &) = delete;
    MOCKABLE_VIRTUAL ~MultiCommand() = default;

    static MultiCommand *create(const std::vector<std::string> &args, int &retVal, OclocArgHelper *helper);

    std::string outDirForBuilds;
    std::string outputFileList;

  protected:
    MultiCommand() = default;

    int initialize(const std::vector<std::string> &args);
    int splitLineInSeparateArgs(std::vector<std::string> &qargs, const std::string &command, size_t numberOfBuild);
    int showResults();
    MOCKABLE_VIRTUAL int singleBuild(const std::vector<std::string> &args);
    void addAdditionalOptionsToSingleCommandLine(std::vector<std::string> &, size_t buildId);
    void printHelp();
    void runBuilds(const std::string &argZero);

    OclocArgHelper *argHelper = nullptr;
    std::vector<int> retValues;
    std::vector<std::string> lines;
    std::string outFileName;
    std::string pathToCommandFile;
    std::stringstream outputFile;
    bool quiet = false;
};
} // namespace NEO
