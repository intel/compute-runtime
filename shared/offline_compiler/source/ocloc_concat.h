/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/const_stringref.h"

#include <cstdint>
#include <string>
#include <vector>

namespace AOT {
enum PRODUCT_CONFIG : uint32_t; // NOLINT(readability-identifier-naming)
}
class OclocArgHelper;
namespace NEO {
namespace Ar {
struct Ar;
}

class OclocConcat : NEO::NonCopyableAndNonMovableClass {
  public:
    using ErrorCode = uint32_t;

    OclocConcat() = delete;
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
    ErrorCode checkIfFatBinariesExist();
    MOCKABLE_VIRTUAL Ar::Ar decodeAr(ArrayRef<const uint8_t> arFile, std::string &outErrors, std::string &outWarnings);
    AOT::PRODUCT_CONFIG getAOTProductConfigFromBinary(ArrayRef<const uint8_t> binary, std::string &outErrors);
    ErrorCode parseArguments(const std::vector<std::string> &args);
    void printMsg(ConstStringRef fileName, const std::string &message);

    OclocArgHelper *argHelper;
    std::vector<std::string> fileNamesToConcat;
    std::string fatBinaryName = "concat.ar";
};

static_assert(NEO::NonCopyableAndNonMovable<OclocConcat>);

} // namespace NEO
