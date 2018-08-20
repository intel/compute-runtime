/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <sstream>
#include <vector>
#include <string>

class BinaryEncoder {
  public:
    BinaryEncoder() = default;
    BinaryEncoder(const std::string &dump, const std::string &elf)
        : pathToDump(dump), elfName(elf){};
    int encode();
    int validateInput(uint32_t argc, const char **argv);

  protected:
    std::string pathToDump, elfName;
    void calculatePatchListSizes(std::vector<std::string> &ptmFile);
    int copyBinaryToBinary(const std::string &srcFileName, std::ostream &outBinary);
    int createElf();
    void printHelp();
    int processBinary(const std::vector<std::string> &ptmFile, std::ostream &deviceBinary);
    int processKernel(size_t &i, const std::vector<std::string> &ptmFile, std::ostream &deviceBinary);
    template <typename T>
    void write(std::stringstream &in, std::ostream &deviceBinary);
    int writeDeviceBinary(const std::string &line, std::ostream &deviceBinary);
};
