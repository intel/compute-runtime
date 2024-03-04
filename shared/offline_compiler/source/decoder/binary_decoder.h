/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/offline_compiler/source/decoder/helper.h"
#include "shared/offline_compiler/source/decoder/iga_wrapper.h"
#include "shared/offline_compiler/source/ocloc_arg_helper.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct PTField {
    uint8_t size = 0U;
    std::string name;
};

struct BinaryHeader {
    std::vector<PTField> fields;
    uint32_t size = 0U;
};

struct PatchToken : BinaryHeader {
    std::string name;
};

struct KernelSizeData {
    NEO::ConstStringRef name;
    uint32_t size = 0u;
};

using PTMap = std::unordered_map<uint8_t, std::unique_ptr<PatchToken>>;

class BinaryDecoder {
  public:
    BinaryDecoder(const std::string &file, const std::string &patch, const std::string &dump)
        : binaryFile(file), pathToPatch(patch), pathToDump(dump){};

    BinaryDecoder(OclocArgHelper *helper) : argHelper(helper), iga(new IgaWrapper) {
        iga->setMessagePrinter(argHelper->getPrinterRef());
    }

    MOCKABLE_VIRTUAL ~BinaryDecoder() = default;

    int decode();
    int validateInput(const std::vector<std::string> &args);

    bool showHelp = false;
    void printHelp();

  protected:
    OclocArgHelper *argHelper = nullptr;
    bool ignoreIsaPadding = false;
    BinaryHeader programHeader, kernelHeader;
    std::vector<char> binary;
    std::unique_ptr<IgaWrapper> iga;
    PTMap patchTokens;
    std::string binaryFile, pathToPatch, pathToDump;

    void dumpField(const void *&binaryPtr, const PTField &field, std::stringstream &ptmFile);
    uint8_t getSize(const std::string &typeStr);
    MOCKABLE_VIRTUAL std::pair<const void *, size_t> getDevBinary();
    std::vector<std::string> loadPatchList();
    MOCKABLE_VIRTUAL void parseTokens();
    int processBinary(const void *&ptr, size_t sectionSize, std::stringstream &ptmFile);
    void processKernel(const void *&ptr, size_t sectionSize, std::stringstream &ptmFile);
    void readPatchTokens(const void *&patchListPtr, uint32_t patchListSize, std::stringstream &ptmFile);
    uint32_t readStructFields(const std::vector<std::string> &patchList,
                              const size_t &structPos, std::vector<PTField> &fields);
    void validateLoadedKernelData(KernelSizeData kernelDataSize, size_t sectionSize);
};
