/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "helper.h"
#include "iga_wrapper.h"

#include <sstream>
#include <string>
#include <vector>

class BinaryEncoder {
  public:
    BinaryEncoder() : iga(new IgaWrapper) {
        iga->setMessagePrinter(messagePrinter);
    }
    BinaryEncoder(const std::string &dump, const std::string &elf)
        : pathToDump(dump), elfName(elf){};
    int encode();
    int validateInput(uint32_t argc, const char **argv);

    void setMessagePrinter(const MessagePrinter &messagePrinter);

  protected:
    std::string pathToDump, elfName;
    MessagePrinter messagePrinter;
    std::unique_ptr<IgaWrapper> iga;
    void calculatePatchListSizes(std::vector<std::string> &ptmFile);
    MOCKABLE_VIRTUAL bool copyBinaryToBinary(const std::string &srcFileName, std::ostream &outBinary, uint32_t *binaryLength);
    bool copyBinaryToBinary(const std::string &srcFileName, std::ostream &outBinary) {
        return copyBinaryToBinary(srcFileName, outBinary, nullptr);
    }
    int createElf();
    void printHelp();
    int processBinary(const std::vector<std::string> &ptmFile, std::ostream &deviceBinary);
    int processKernel(size_t &i, const std::vector<std::string> &ptmFileLines, std::ostream &deviceBinary);
    template <typename T>
    void write(std::stringstream &in, std::ostream &deviceBinary);
    int writeDeviceBinary(const std::string &line, std::ostream &deviceBinary);
    void addPadding(std::ostream &out, size_t numBytes);
    MOCKABLE_VIRTUAL bool fileExists(const std::string &path) const;
    MOCKABLE_VIRTUAL std::vector<char> readBinaryFile(const std::string &path) const;
};
