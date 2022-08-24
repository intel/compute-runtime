/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/const_stringref.h"

#include <string>
#include <vector>

class OclocArgHelper;
namespace NEO {
namespace Ar {
struct Ar;
}

class OclocConcat {
  public:
    using ErrorCode = uint32_t;

    OclocConcat() = delete;
    OclocConcat(const OclocConcat &) = delete;
    OclocConcat &operator=(const OclocConcat &) = delete;

    OclocConcat(OclocArgHelper *argHelper) : argHelper(argHelper){};
    ErrorCode initialize(const std::vector<std::string> &args);
    ErrorCode concatenate();
    void printHelp();

    static constexpr ConstStringRef commandStr = "concat";
    static constexpr ConstStringRef helpMessage = R"===(
ocloc concat - concatenates fat binary files
Usage: ocloc concat <fat binary> <fat binary> ... [-out <concatenated fat binary file name>]
)===";

  protected:
    MOCKABLE_VIRTUAL Ar::Ar decodeAr(const std::vector<char> &arFile, std::string &outErrors, std::string &outWarnings);
    ErrorCode parseArguments(const std::vector<std::string> &args);
    ErrorCode checkIfFatBinariesExist();

    OclocArgHelper *argHelper;
    std::vector<std::string> fileNamesToConcat;
    std::string fatBinaryName = "concat.ar";
};
} // namespace NEO