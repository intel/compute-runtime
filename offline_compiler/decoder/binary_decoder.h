/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/elf/types.h"

#include "helper.h"
#include "iga_wrapper.h"

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
    }
    BinaryDecoder(const std::string &file, const std::string &patch, const std::string &dump)
        : binaryFile(file), pathToPatch(patch), pathToDump(dump){};
    int decode();
    int validateInput(uint32_t argc, const char **argv);

    void setMessagePrinter(const MessagePrinter &messagePrinter);

  protected:
    bool ignoreIsaPadding = false;
    BinaryHeader programHeader, kernelHeader;
    CLElfLib::ElfBinaryStorage binary;
    std::unique_ptr<IgaWrapper> iga;
    PTMap patchTokens;
    std::string binaryFile, pathToPatch, pathToDump;
    MessagePrinter messagePrinter;

    void dumpField(void *&binaryPtr, const PTField &field, std::ostream &ptmFile);
    uint8_t getSize(const std::string &typeStr);
    void *getDevBinary();
    void parseTokens();
    void printHelp();
    int processBinary(void *&ptr, std::ostream &ptmFile);
    void processKernel(void *&ptr, std::ostream &ptmFile);
    void readPatchTokens(void *&patchListPtr, uint32_t patchListSize, std::ostream &ptmFile);
    uint32_t readStructFields(const std::vector<std::string> &patchList,
                              const size_t &structPos, std::vector<PTField> &fields);
};
