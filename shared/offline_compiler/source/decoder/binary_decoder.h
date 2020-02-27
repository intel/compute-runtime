/*
 * Copyright (C) 2018-2020 Intel Corporation
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

using PTMap = std::unordered_map<uint8_t, std::unique_ptr<PatchToken>>;

class BinaryDecoder {
  public:
    BinaryDecoder() : iga(new IgaWrapper) {
        iga->setMessagePrinter(messagePrinter);
        if (nullptr == argHelper) {
            argHelper = std::make_unique<OclocArgHelper>();
        }
    }
    BinaryDecoder(const std::string &file, const std::string &patch, const std::string &dump)
        : binaryFile(file), pathToPatch(patch), pathToDump(dump){};
    BinaryDecoder(std::unique_ptr<OclocArgHelper> helper) : argHelper(std::move(helper)), iga(new IgaWrapper) {
        iga->setMessagePrinter(messagePrinter);
    };
    int decode();
    int validateInput(const std::vector<std::string> &args);
    void setMessagePrinter(const MessagePrinter &messagePrinter);

  protected:
    std::unique_ptr<OclocArgHelper> argHelper = nullptr;
    bool ignoreIsaPadding = false;
    BinaryHeader programHeader, kernelHeader;
    std::vector<char> binary;
    std::unique_ptr<IgaWrapper> iga;
    PTMap patchTokens;
    std::string binaryFile, pathToPatch, pathToDump;
    MessagePrinter messagePrinter;

    void dumpField(const void *&binaryPtr, const PTField &field, std::ostream &ptmFile);
    uint8_t getSize(const std::string &typeStr);
    const void *getDevBinary();
    std::vector<std::string> loadPatchList();
    void parseTokens();
    void printHelp();
    int processBinary(const void *&ptr, std::ostream &ptmFile);
    void processKernel(const void *&ptr, std::ostream &ptmFile);
    void readPatchTokens(const void *&patchListPtr, uint32_t patchListSize, std::ostream &ptmFile);
    uint32_t readStructFields(const std::vector<std::string> &patchList,
                              const size_t &structPos, std::vector<PTField> &fields);
};
