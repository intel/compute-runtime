/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "offline_compiler/offline_compiler.h"
#include "offline_compiler/utilities/safety_caller.h"
#include "runtime/os_interface/os_library.h"

#include "decoder/binary_decoder.h"
#include "decoder/binary_encoder.h"
#include <CL/cl.h>

#include <fstream>
#include <iostream>

namespace NEO {

class MultiCommand {
  public:
    static MultiCommand *create(int numArgs, const char *argv[], int &retVal);
    void deleteBuildsWithWarnigs();

    std::vector<OfflineCompiler *> singleBuilds;

    MultiCommand &operator=(const MultiCommand &) = delete;
    MultiCommand(const MultiCommand &) = delete;
    ~MultiCommand();

    std::string OutDirForBuilds;

  protected:
    int splitLineInSeparateArgs(std::vector<std::string> &qargs, const std::string &command, int numberOfBuild);
    void openFileWithBuildsArguments();
    void addAdditionalOptionsToSingleCommandLine(std::vector<std::string> &, int);
    void printHelp();
    int initialize(int numArgs, const char *argv[]);
    int showResults();
    int singleBuild(size_t numArgs, const std::vector<std::string> &allArgs);
    std::string eraseExtensionFromPath(std::string &filePath);

    std::vector<int> retValues;
    std::string pathToCMD;
    std::vector<std::string> lines;
    bool quiet = false;

    MultiCommand();
};
} // namespace NEO
