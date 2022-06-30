/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_concat.h"

#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/offline_compiler/source/ocloc_error_code.h"
#include "shared/source/device_binary_format/ar/ar_decoder.h"
#include "shared/source/device_binary_format/ar/ar_encoder.h"

namespace NEO {
ErrorCode OclocConcat::initialize(const std::vector<std::string> &args) {
    auto error = parseArguments(args);
    if (error) {
        return error;
    }
    error = checkIfFatBinariesExist();
    return error;
}

Ar::Ar OclocConcat::decodeAr(const std::vector<char> &arFile, std::string &outErrors, std::string &outWarnings) {
    return NEO::Ar::decodeAr({reinterpret_cast<const uint8_t *>(arFile.data()), arFile.size()}, outErrors, outWarnings);
}

ErrorCode OclocConcat::parseArguments(const std::vector<std::string> &args) {
    for (size_t i = 2; i < args.size(); i++) {
        if (NEO::ConstStringRef("-out") == args[i]) {
            if (i + 1 >= args.size()) {
                argHelper->printf("Missing out file name after \"-out\" argument\n");
                return OclocErrorCode::INVALID_COMMAND_LINE;
            }
            fatBinaryName = args[++i];
        } else {
            fileNamesToConcat.push_back(args[i]);
        }
    }

    if (fileNamesToConcat.empty()) {
        argHelper->printf("No files to concatenate were provided.\n");
        return OclocErrorCode::INVALID_COMMAND_LINE;
    }

    return OclocErrorCode::SUCCESS;
}

ErrorCode OclocConcat::checkIfFatBinariesExist() {
    bool filesExist = true;
    for (auto &fileName : fileNamesToConcat) {
        if (false == argHelper->fileExists(fileName)) {
            filesExist = false;
            auto errorMsg = fileName + " doesn't exist!\n";
            argHelper->printf(errorMsg.c_str());
        }
    }
    return filesExist ? OclocErrorCode::SUCCESS : OclocErrorCode::INVALID_COMMAND_LINE;
}

ErrorCode OclocConcat::concatenate() {
    NEO::Ar::ArEncoder arEncoder(true);
    for (auto &fileName : fileNamesToConcat) {
        auto arFile = argHelper->readBinaryFile(fileName);

        std::string warnings;
        std::string errors;
        auto ar = decodeAr(arFile, errors, warnings);
        if (false == errors.empty()) {
            argHelper->printf(errors.c_str());
            return OclocErrorCode::INVALID_FILE;
        }
        argHelper->printf(warnings.c_str());

        for (auto &fileEntry : ar.files) {
            if (NEO::ConstStringRef(fileEntry.fileName).startsWith("pad_")) {
                continue;
            }

            arEncoder.appendFileEntry(fileEntry.fileName, fileEntry.fileData);
        }
    }

    auto arFile = arEncoder.encode();
    argHelper->saveOutput(fatBinaryName, arFile.data(), arFile.size());
    return OclocErrorCode::SUCCESS;
}

void OclocConcat::printHelp() {
    argHelper->printf(helpMessage.data());
}

} // namespace NEO
