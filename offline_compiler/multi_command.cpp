/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "offline_compiler/multi_command.h"

namespace NEO {
int MultiCommand::singleBuild(size_t numArgs, const std::vector<std::string> &allArgs) {
    int retVal = CL_SUCCESS;
    std::string buildLog;
    OfflineCompiler *pCompiler = OfflineCompiler::create(numArgs, allArgs, retVal);
    if (retVal == CL_SUCCESS) {
        retVal = buildWithSafetyGuard(pCompiler);

        buildLog = pCompiler->getBuildLog();
        if (buildLog.empty() == false) {
            printf("%s\n", buildLog.c_str());
        }

        if (retVal == CL_SUCCESS) {
            if (!pCompiler->isQuiet())
                printf("Build succeeded.\n");
        } else {
            printf("Build failed with error code: %d\n", retVal);
        }
    }
    if (buildLog.empty() == false) {
        singleBuilds.push_back(pCompiler);
    } else {
        delete pCompiler;
    }
    return retVal;
}

MultiCommand::MultiCommand() = default;

MultiCommand::~MultiCommand() {
    deleteBuildsWithWarnigs();
}

void MultiCommand::deleteBuildsWithWarnigs() {
    for (OfflineCompiler *pSingle : singleBuilds)
        delete pSingle;
    singleBuilds.clear();
}

MultiCommand *MultiCommand::create(int numArgs, const char *argv[], int &retVal) {
    retVal = CL_SUCCESS;
    auto pMultiCommand = new MultiCommand();

    if (pMultiCommand) {
        retVal = pMultiCommand->initialize(numArgs, argv);
    }

    if (retVal != CL_SUCCESS) {
        delete pMultiCommand;
        pMultiCommand = nullptr;
    }

    return pMultiCommand;
}

std::string MultiCommand::eraseExtensionFromPath(std::string &filePath) {
    size_t extPos = filePath.find_last_of(".", filePath.size());
    if (extPos == std::string::npos) {
        extPos = filePath.size();
    }
    std::string fileName;
    std::string fileTrunk = filePath.substr(0, extPos);

    return fileTrunk;
}

void MultiCommand::addAdditionalOptionsToSingleCommandLine(std::vector<std::string> &singleLineWithArguments, int buildId) {
    std::string OutFileName;
    bool hasOutDir = false;
    bool hasSpecificName = false;
    for (auto arg : singleLineWithArguments) {
        if (arg == "-out_dir") {
            hasOutDir = true;
        }
        if (arg == "-output") {
            hasSpecificName = true;
        }
    }
    if (!hasOutDir) {
        singleLineWithArguments.push_back("-out_dir");
        OutDirForBuilds = eraseExtensionFromPath(pathToCMD);
        singleLineWithArguments.push_back(OutDirForBuilds);
    }
    if (!hasSpecificName) {
        singleLineWithArguments.push_back("-output");
        OutFileName = "build_no_" + std::to_string(buildId + 1);
        singleLineWithArguments.push_back(OutFileName);
    }
    if (quiet)
        singleLineWithArguments.push_back("-q");
}

int MultiCommand::initialize(int numArgs, const char *argv[]) {
    int retVal = CL_SUCCESS;

    if (!strcmp(argv[numArgs - 1], "--help")) {
        printHelp();
        return -1;
    }

    if (numArgs > 2)
        pathToCMD = argv[2];
    else {
        printHelp();
        return INVALID_COMMAND_LINE;
    }
    if (numArgs > 3 && strcmp(argv[3], "-q") == 0)
        quiet = true;

    //save file with builds arguments to vector of strings, line by line
    openFileWithBuildsArguments();
    if (!lines.empty()) {
        for (unsigned int i = 0; i < lines.size(); i++) {
            std::vector<std::string> singleLineWithArguments;
            unsigned int numberOfArg;

            singleLineWithArguments.push_back(argv[0]);
            retVal = splitLineInSeparateArgs(singleLineWithArguments, lines[i], i);
            if (retVal != CL_SUCCESS) {
                retValues.push_back(retVal);
                continue;
            }

            addAdditionalOptionsToSingleCommandLine(singleLineWithArguments, i);

            numberOfArg = static_cast<unsigned int>(singleLineWithArguments.size());

            if (!quiet)
                printf("\nCommand number %d: ", i + 1);
            retVal = singleBuild(numberOfArg, singleLineWithArguments);
            retValues.push_back(retVal);
        }

        return showResults();
    } else {
        printHelp();
        return INVALID_COMMAND_LINE;
    }
}

void MultiCommand::printHelp() {
    printf(R"===(Compiles multiple files using a config file.

Usage: ocloc multi <file_name>
  <file_name>   Input file containing a list of arguments for subsequent
                ocloc invocations.
                Expected format of each line inside such file is:
                '-file <filename> -device <device_type> [compile_options].
                See 'ocloc compile --help' for available compile_options.
                Results of subsequent compilations will be dumped into 
                a directory with name indentical file_name's base name.
)===");
}

int MultiCommand::splitLineInSeparateArgs(std::vector<std::string> &qargs, const std::string &command, int numberOfBuild) {
    unsigned int len = static_cast<unsigned int>(command.length());

    bool qot = false, sqot = false;
    int arglen;

    for (unsigned int i = 0; i < len; i++) {
        int start = i;
        if (command[i] == '\"') {
            qot = true;
        } else if (command[i] == '\'')
            sqot = true;

        if (qot) {
            i++;
            start++;
            while (i < len && command[i] != '\"')
                i++;
            if (i < len)
                qot = false;
            arglen = i - start;
            i++;
        } else if (sqot) {
            i++;
            while (i < len && command[i] != '\'')
                i++;
            if (i < len)
                sqot = false;
            arglen = i - start;
            i++;
        } else {
            while (i < len && command[i] != ' ')
                i++;
            arglen = i - start;
        }
        qargs.push_back(command.substr(start, arglen));
    }
    if (qot || sqot) {
        printf("One of the quotes is open in build number %d\n", numberOfBuild + 1);
        return INVALID_COMMAND_LINE;
    }
    return CL_SUCCESS;
}

void MultiCommand::openFileWithBuildsArguments() {
    std::fstream multiCmdFile;
    std::stringstream fileContent;
    multiCmdFile.open(pathToCMD, std::fstream::in);

    if (multiCmdFile.is_open()) {
        std::string param;
        fileContent << multiCmdFile.rdbuf();
        multiCmdFile.close();
        while (std::getline(fileContent, param, '\n')) {
            param.erase(param.find_last_not_of(" \r\t") + 1);
            param.erase(0, param.find_first_not_of(" \r\t"));
            if (!param.empty()) {
                lines.push_back(param);
            }
        }
    } else {
        printf("Can not open file with builds arguments\n");
    }
}

int MultiCommand::showResults() {
    int retValue = CL_SUCCESS;
    int indexRetVal = 0;
    for (int retVal : retValues) {
        if (retVal != CL_SUCCESS) {
            if (retValue == CL_SUCCESS)
                retValue = retVal;
            if (!quiet)
                printf("Build %d: failed. Error code: %d\n", indexRetVal, retVal);
        } else {
            if (!quiet)
                printf("Build %d: successful\n", indexRetVal);
        }
        indexRetVal++;
    }
    return retValue;
}
} // namespace NEO
