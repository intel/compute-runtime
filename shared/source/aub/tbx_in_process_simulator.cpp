/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/tbx_in_process_simulator.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/os_library.h"
#include "shared/source/os_interface/os_thread.h"

#include <sstream>

namespace NEO {

std::unique_ptr<TbxInProcessSimulator> TbxInProcessSimulator::start(const std::string &libraryPath, const std::vector<std::string> &arguments) {
    OsLibraryCreateProperties libraryProperties(libraryPath);
    libraryProperties.customLoadFlags = getLibraryLoadFlags();
    std::unique_ptr<OsLibrary> library(OsLibrary::loadFunc(libraryProperties));
    if (!library || !library->isLoaded()) {
        return nullptr;
    }

    MainFunction mainFunction = (*library)[mainFunctionName];
    if (mainFunction == nullptr) {
        return nullptr;
    }

    auto simulator = std::unique_ptr<TbxInProcessSimulator>(new TbxInProcessSimulator(std::move(library), mainFunction, libraryPath, arguments));
    simulator->thread = Thread::createFunc(TbxInProcessSimulator::run, simulator.get());
    if (!simulator->thread) {
        return nullptr;
    }
    return simulator;
}

std::vector<std::string> TbxInProcessSimulator::splitArguments(const std::string &arguments) {
    std::vector<std::string> splitArguments;
    std::istringstream argumentStream(arguments);
    std::string argument;
    if (arguments.find(argumentSeparator) != std::string::npos) {
        while (std::getline(argumentStream, argument, argumentSeparator)) {
            if (!argument.empty()) {
                splitArguments.push_back(argument);
            }
        }
    } else {
        while (argumentStream >> argument) {
            splitArguments.push_back(argument);
        }
    }
    return splitArguments;
}

void TbxInProcessSimulator::replaceRootDeviceArguments(std::vector<std::string> &arguments, uint16_t port, const std::vector<std::string> &sharedMemoryFiles) {
    for (size_t i = 0; i < arguments.size(); i++) {
        if (arguments[i] == "-socket" && i + 1 < arguments.size()) {
            auto &address = arguments[i + 1];
            address = address.substr(0, address.rfind(':') + 1) + std::to_string(port);
        } else if (arguments[i] == "-shmname") {
            auto filesEnd = arguments.begin() + i + 1;
            while (filesEnd != arguments.end() && filesEnd->front() != '-') {
                filesEnd++;
            }
            auto filesPosition = arguments.erase(arguments.begin() + i + 1, filesEnd);
            arguments.insert(filesPosition, sharedMemoryFiles.begin(), sharedMemoryFiles.end());
            i += sharedMemoryFiles.size();
        }
    }
}

TbxInProcessSimulator::TbxInProcessSimulator(std::unique_ptr<OsLibrary> library, MainFunction mainFunction, const std::string &libraryPath, const std::vector<std::string> &arguments)
    : library(std::move(library)), mainFunction(mainFunction) {
    this->arguments.push_back(libraryPath);
    this->arguments.insert(this->arguments.end(), arguments.begin(), arguments.end());
    for (const auto &storedArgument : this->arguments) {
        this->argumentPointers.push_back(storedArgument.c_str());
    }
}

TbxInProcessSimulator::~TbxInProcessSimulator() {
    if (this->thread) {
        this->thread->join();
    }
}

void *TbxInProcessSimulator::run(void *simulator) {
    auto inProcessSimulator = static_cast<TbxInProcessSimulator *>(simulator);
    const int exitCode = inProcessSimulator->mainFunction(static_cast<int>(inProcessSimulator->argumentPointers.size()), inProcessSimulator->argumentPointers.data());
    PRINT_STRING(exitCode != 0 && debugManager.flags.PrintDebugMessages.get(), stderr, "TBX in-process simulator exited with code %d\n", exitCode);
    return nullptr;
}

} // namespace NEO
