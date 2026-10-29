/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace NEO {
class OsLibrary;
class Thread;

class TbxInProcessSimulator {
  public:
    using MainFunction = int (*)(int, const char **);
    static constexpr char argumentSeparator = ';';

    static std::unique_ptr<TbxInProcessSimulator> start(const std::string &libraryPath, const std::vector<std::string> &arguments);
    static std::vector<std::string> splitArguments(const std::string &arguments);
    static void replaceRootDeviceArguments(std::vector<std::string> &arguments, uint16_t port, const std::vector<std::string> &sharedMemoryFiles);

    ~TbxInProcessSimulator();

  protected:
    static const char *const mainFunctionName;
    static int *getLibraryLoadFlags();
    static void *run(void *simulator);

    TbxInProcessSimulator(std::unique_ptr<OsLibrary> library, MainFunction mainFunction, const std::string &libraryPath, const std::vector<std::string> &arguments);

    std::unique_ptr<OsLibrary> library;
    MainFunction mainFunction = nullptr;
    std::vector<std::string> arguments;
    std::vector<const char *> argumentPointers;
    std::unique_ptr<Thread> thread;
};

} // namespace NEO
