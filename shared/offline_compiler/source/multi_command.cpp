/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/multi_command.h"

#include "shared/offline_compiler/source/ocloc_error_code.h"
#include "shared/offline_compiler/source/ocloc_fatbinary.h"
#include "shared/source/utilities/const_stringref.h"

#include <memory>

namespace NEO {
int MultiCommand::singleBuild(const std::vector<std::string> &args) {
    int retVal = OclocErrorCode::SUCCESS;

    if (requestedFatBinary(args, argHelper)) {
        retVal = buildFatBinary(args, argHelper);
    } else {
        std::unique_ptr<OfflineCompiler> pCompiler{OfflineCompiler::create(args.size(), args, true, retVal, argHelper)};
        if (retVal == OclocErrorCode::SUCCESS) {
            retVal = buildWithSafetyGuard(pCompiler.get());

            std::string &buildLog = pCompiler->getBuildLog();
            if (buildLog.empty() == false) {
                argHelper->printf("%s\n", buildLog.c_str());
            }
        }
        outFileName += ".bin";
    }
    if (retVal == OclocErrorCode::SUCCESS) {
        if (!quiet)
            argHelper->printf("Build succeeded.\n");
    } else {
        argHelper->printf("Build failed with error code: %d\n", retVal);
    }

    if (retVal == OclocErrorCode::SUCCESS) {
        outputFile << getCurrentDirectoryOwn(outDirForBuilds) + outFileName;
    } else {
        outputFile << "Unsuccesful build";
    }
    outputFile << '\n';

    return retVal;
}

MultiCommand *MultiCommand::create(const std::vector<std::string> &args, int &retVal, OclocArgHelper *helper) {
    retVal = OclocErrorCode::SUCCESS;
    auto pMultiCommand = new MultiCommand();

    if (pMultiCommand) {
        pMultiCommand->argHelper = helper;
        retVal = pMultiCommand->initialize(args);
    }

    if (retVal != OclocErrorCode::SUCCESS) {
        delete pMultiCommand;
        pMultiCommand = nullptr;
    }

    return pMultiCommand;
}

void MultiCommand::addAdditionalOptionsToSingleCommandLine(std::vector<std::string> &singleLineWithArguments, size_t buildId) {
    bool hasOutDir = false;
    bool hasOutName = false;
    for (const auto &arg : singleLineWithArguments) {
        if (ConstStringRef("-out_dir") == arg) {
            hasOutDir = true;
        } else if (ConstStringRef("-output") == arg) {
            hasOutName = true;
        }
    }

    if (!hasOutDir) {
        singleLineWithArguments.push_back("-out_dir");
        outDirForBuilds = OfflineCompiler::getFileNameTrunk(pathToCommandFile);
        singleLineWithArguments.push_back(outDirForBuilds);
    }
    if (!hasOutName) {
        singleLineWithArguments.push_back("-output");
        outFileName = "build_no_" + std::to_string(buildId + 1);
        singleLineWithArguments.push_back(outFileName);
    }
    if (quiet)
        singleLineWithArguments.push_back("-q");
}

int MultiCommand::initialize(const std::vector<std::string> &args) {
    if (args[args.size() - 1] == "--help") {
        printHelp();
        return -1;
    }

    for (size_t argIndex = 1; argIndex < args.size(); argIndex++) {
        const auto &currArg = args[argIndex];
        const bool hasMoreArgs = (argIndex + 1 < args.size());
        if (hasMoreArgs && ConstStringRef("multi") == currArg) {
            pathToCommandFile = args[++argIndex];
        } else if (hasMoreArgs && ConstStringRef("-output_file_list") == currArg) {
            outputFileList = args[++argIndex];
        } else if (ConstStringRef("-q") == currArg) {
            quiet = true;
        } else {
            argHelper->printf("Invalid option (arg %zu): %s\n", argIndex, currArg.c_str());
            printHelp();
            return OclocErrorCode::INVALID_COMMAND_LINE;
        }
    }

    //save file with builds arguments to vector of strings, line by line
    if (argHelper->fileExists(pathToCommandFile)) {
        argHelper->readFileToVectorOfStrings(pathToCommandFile, lines);
        if (lines.empty()) {
            argHelper->printf("Command file was empty.\n");
            return OclocErrorCode::INVALID_FILE;
        }
    } else {
        argHelper->printf("Could not find/open file with builds argument.s\n");
        return OclocErrorCode::INVALID_FILE;
    }

    runBuilds(args[0]);

    if (outputFileList != "") {
        argHelper->saveOutput(outputFileList, outputFile);
    }
    return showResults();
}

void MultiCommand::runBuilds(const std::string &argZero) {
    for (size_t i = 0; i < lines.size(); ++i) {
        std::vector<std::string> args = {argZero};

        int retVal = splitLineInSeparateArgs(args, lines[i], i);
        if (retVal != OclocErrorCode::SUCCESS) {
            retValues.push_back(retVal);
            continue;
        }

        if (!quiet) {
            argHelper->printf("Command number %zu: \n", i + 1);
        }

        addAdditionalOptionsToSingleCommandLine(args, i);
        retVal = singleBuild(args);
        retValues.push_back(retVal);
    }
}

void MultiCommand::printHelp() {
    argHelper->printf(R"===(Compiles multiple files using a config file.

Usage: ocloc multi <file_name>
  <file_name>   Input file containing a list of arguments for subsequent
                ocloc invocations.
                Expected format of each line inside such file is:
                '-file <filename> -device <device_type> [compile_options]'.
                See 'ocloc compile --help' for available compile_options.
                Results of subsequent compilations will be dumped into 
                a directory with name indentical file_name's base name.

  -output_file_list             Name of optional file containing 
                                paths to outputs .bin files

)===");
}

int MultiCommand::splitLineInSeparateArgs(std::vector<std::string> &qargs, const std::string &commandsLine, size_t numberOfBuild) {
    size_t start, end, argLen;
    for (size_t i = 0; i < commandsLine.length(); ++i) {
        const char &currChar = commandsLine[i];
        if ('\"' == currChar) {
            start = i + 1;
            end = commandsLine.find('\"', start);
        } else if ('\'' == currChar) {
            start = i + 1;
            end = commandsLine.find('\'', start);
        } else if (' ' == currChar) {
            continue;
        } else {
            start = i;
            end = commandsLine.find(" ", start);
            end = (end == std::string::npos) ? commandsLine.length() : end;
        }
        if (end == std::string::npos) {
            argHelper->printf("One of the quotes is open in build number %zu\n", numberOfBuild + 1);
            return OclocErrorCode::INVALID_FILE;
        }
        argLen = end - start;
        i = end;
        qargs.push_back(commandsLine.substr(start, argLen));
    }
    return OclocErrorCode::SUCCESS;
}

int MultiCommand::showResults() {
    int retValue = OclocErrorCode::SUCCESS;
    int indexRetVal = 0;
    for (int retVal : retValues) {
        retValue |= retVal;
        if (!quiet) {
            if (retVal != OclocErrorCode::SUCCESS) {
                argHelper->printf("Build command %d: failed. Error code: %d\n", indexRetVal, retVal);
            } else {
                argHelper->printf("Build command %d: successful\n", indexRetVal);
            }
        }
        indexRetVal++;
    }
    return retValue;
}
} // namespace NEO
