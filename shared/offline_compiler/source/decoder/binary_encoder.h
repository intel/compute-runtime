/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/offline_compiler/source/ocloc_arg_helper.h"

#include "helper.h"
#include "iga_wrapper.h"

#include <sstream>
#include <string>
#include <vector>

class BinaryEncoder {
  public:
    BinaryEncoder(const std::string &dump, const std::string &elf)
        : pathToDump(dump), elfName(elf){};
    BinaryEncoder(OclocArgHelper *helper) : argHelper(helper), iga(new IgaWrapper) {
        iga->setMessagePrinter(argHelper->getPrinterRef());
    }
    int encode();
    int validateInput(const std::vector<std::string> &args);

    bool showHelp = false;
    void printHelp();

  protected:
    OclocArgHelper *argHelper = nullptr;
    bool ignoreIsaPadding = false;
    std::string pathToDump, elfName;
    std::unique_ptr<IgaWrapper> iga;

    void calculatePatchListSizes(std::vector<std::string> &ptmFile);
    MOCKABLE_VIRTUAL bool copyBinaryToBinary(const std::string &srcFileName, std::ostream &outBinary, uint32_t *binaryLength);
    bool copyBinaryToBinary(const std::string &srcFileName, std::ostream &outBinary) {
        return copyBinaryToBinary(srcFileName, outBinary, nullptr);
    }
    int createElf(std::stringstream &deviceBinary);
    int processBinary(const std::vector<std::string> &ptmFile, std::ostream &deviceBinary);
    int processKernel(size_t &i, const std::vector<std::string> &ptmFileLines, std::ostream &deviceBinary);
    template <typename T>
    void write(std::stringstream &in, std::ostream &deviceBinary);
    int writeDeviceBinary(const std::string &line, std::ostream &deviceBinary);
    void addPadding(std::ostream &out, size_t numBytes);
};
