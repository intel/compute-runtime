/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_concat.h"

#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/source/device_binary_format/ar/ar_decoder.h"
#include "shared/source/device_binary_format/ar/ar_encoder.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/zebin/zebin_decoder.h"
#include "shared/source/helpers/product_config_helper.h"

namespace NEO {
OclocConcat::ErrorCode OclocConcat::initialize(const std::vector<std::string> &args) {
    auto error = parseArguments(args);
    if (error) {
        return error;
    }
    error = checkIfFatBinariesExist();
    return error;
}

Ar::Ar OclocConcat::decodeAr(ArrayRef<const uint8_t> arFile, std::string &outErrors, std::string &outWarnings) {
    return NEO::Ar::decodeAr(arFile, outErrors, outWarnings);
}

OclocConcat::ErrorCode OclocConcat::parseArguments(const std::vector<std::string> &args) {
    for (size_t i = 2; i < args.size(); i++) {
        if (NEO::ConstStringRef("-out") == args[i]) {
            if (i + 1 >= args.size()) {
                argHelper->printf("Missing out file name after \"-out\" argument\n");
                return OCLOC_INVALID_COMMAND_LINE;
            }
            fatBinaryName = args[++i];
        } else {
            fileNamesToConcat.push_back(args[i]);
        }
    }

    if (fileNamesToConcat.empty()) {
        argHelper->printf("No files to concatenate were provided.\n");
        return OCLOC_INVALID_COMMAND_LINE;
    }

    return OCLOC_SUCCESS;
}

OclocConcat::ErrorCode OclocConcat::checkIfFatBinariesExist() {
    bool filesExist = true;
    for (auto &fileName : fileNamesToConcat) {
        if (false == argHelper->fileExists(fileName)) {
            filesExist = false;
            auto errorMsg = fileName + " doesn't exist!\n";
            argHelper->printf(errorMsg.c_str());
        }
    }
    return filesExist ? OCLOC_SUCCESS : OCLOC_INVALID_COMMAND_LINE;
}

void OclocConcat::printMsg(ConstStringRef fileName, const std::string &message) {
    if (false == message.empty()) {
        argHelper->printf(fileName.data());
        argHelper->printf(" : ");
        argHelper->printf(message.c_str());
    }
}

AOT::PRODUCT_CONFIG OclocConcat::getAOTProductConfigFromBinary(ArrayRef<const uint8_t> binary, std::string &outErrors) {
    std::vector<Zebin::Elf::IntelGTNote> intelGTNotes;
    if (Zebin::isZebin<Elf::EI_CLASS_64>(binary)) {
        std::string warnings;
        auto elf = NEO::Elf::decodeElf(binary, outErrors, warnings);
        Zebin::getIntelGTNotes<Elf::EI_CLASS_64>(elf, intelGTNotes, outErrors, warnings);
    } else if (Zebin::isZebin<Elf::EI_CLASS_32>(binary)) {
        std::string warnings;
        auto elf = NEO::Elf::decodeElf<Elf::EI_CLASS_32>(binary, outErrors, warnings);
        Zebin::getIntelGTNotes<Elf::EI_CLASS_32>(elf, intelGTNotes, outErrors, warnings);
    } else {
        outErrors.append("Not a zebin file\n");
        return {};
    }

    AOT::PRODUCT_CONFIG productConfig{};
    bool productConfigFound = false;
    for (auto &note : intelGTNotes) {
        if (note.type == Zebin::Elf::IntelGTSectionType::productConfig) {
            productConfig = *reinterpret_cast<const AOT::PRODUCT_CONFIG *>(note.data.begin());
            productConfigFound = true;
            break;
        }
    }
    if (false == productConfigFound) {
        outErrors.append("Couldn't find AOT product configuration in intelGTNotes section.\n");
    }
    return productConfig;
}

OclocConcat::ErrorCode OclocConcat::concatenate() {
    NEO::Ar::ArEncoder arEncoder(true);
    for (auto &fileName : fileNamesToConcat) {
        auto file = argHelper->readBinaryFile(fileName);
        auto fileRef = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(file.data()), file.size());

        if (NEO::Ar::isAr(fileRef)) {
            std::string warnings;
            std::string errors;
            auto ar = decodeAr(fileRef, errors, warnings);

            if (false == errors.empty()) {
                printMsg(fileName, errors);
                return OCLOC_INVALID_FILE;
            }
            printMsg(fileName, warnings);

            for (auto &fileEntry : ar.files) {
                if (NEO::ConstStringRef(fileEntry.fileName).startsWith("pad_")) {
                    continue;
                }
                arEncoder.appendFileEntry(fileEntry.fileName, fileEntry.fileData);
            }
        } else {
            std::string errors;
            auto productConfig = getAOTProductConfigFromBinary(fileRef, errors);
            if (false == errors.empty()) {
                printMsg(fileName, errors);
                return OCLOC_INVALID_FILE;
            }
            auto entryName = ProductConfigHelper::parseMajorMinorRevisionValue(productConfig);
            arEncoder.appendFileEntry(entryName, fileRef);
        }
    }

    auto arFile = arEncoder.encode();
    argHelper->saveOutput(fatBinaryName, arFile.data(), arFile.size());
    return OCLOC_SUCCESS;
}

void OclocConcat::printHelp() {
    argHelper->printf(helpMessage.data());
}

} // namespace NEO
