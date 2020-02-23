/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_library.h"
#include "offline_compiler/offline_compiler.h"
#include "offline_compiler/utilities/get_current_dir.h"
#include "offline_compiler/utilities/safety_caller.h"

#include "decoder/binary_decoder.h"
#include "decoder/binary_encoder.h"
#include <CL/cl.h>

#include <fstream>
#include <iostream>

namespace NEO {

class MultiCommand {
  public:
    static MultiCommand *create(const std::vector<std::string> &argv, int &retVal);
    void deleteBuildsWithWarnigs();

    std::vector<OfflineCompiler *> singleBuilds;

    MultiCommand &operator=(const MultiCommand &) = delete;
    MultiCommand(const MultiCommand &) = delete;
    ~MultiCommand();

    std::string outDirForBuilds;
    std::string outputFileList = "";

  protected:
    int splitLineInSeparateArgs(std::vector<std::string> &qargs, const std::string &command, int numberOfBuild);
    void openFileWithBuildsArguments();
    void addAdditionalOptionsToSingleCommandLine(std::vector<std::string> &, int);
    void printHelp();
    int initialize(const std::vector<std::string> &allArgs);
    int showResults();
    int singleBuild(size_t numArgs, const std::vector<std::string> &allArgs);
    std::string eraseExtensionFromPath(std::string &filePath);
    std::string OutFileName;

    std::vector<int> retValues;
    std::string pathToCMD;
    std::vector<std::string> lines;
    bool quiet = false;

    MultiCommand();
};
} // namespace NEO
