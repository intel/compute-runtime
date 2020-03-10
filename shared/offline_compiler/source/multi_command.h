/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/decoder/binary_decoder.h"
#include "shared/offline_compiler/source/decoder/binary_encoder.h"
#include "shared/offline_compiler/source/offline_compiler.h"
#include "shared/offline_compiler/source/utilities/get_current_dir.h"
#include "shared/offline_compiler/source/utilities/safety_caller.h"
#include "shared/source/os_interface/os_library.h"

#include <CL/cl.h>

#include <iostream>
#include <sstream>

namespace NEO {

class MultiCommand {
  public:
    MultiCommand &operator=(const MultiCommand &) = delete;
    MultiCommand(const MultiCommand &) = delete;
    ~MultiCommand() = default;

    static MultiCommand *create(const std::vector<std::string> &args, int &retVal, OclocArgHelper *helper);

    std::string outDirForBuilds;
    std::string outputFileList;

  protected:
    MultiCommand() = default;

    int initialize(const std::vector<std::string> &args);
    int splitLineInSeparateArgs(std::vector<std::string> &qargs, const std::string &command, size_t numberOfBuild);
    int showResults();
    int singleBuild(const std::vector<std::string> &args);
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
