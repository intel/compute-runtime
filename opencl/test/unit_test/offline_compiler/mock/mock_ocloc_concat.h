/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/ocloc_concat.h"
#include "shared/source/device_binary_format/ar/ar_decoder.h"

namespace NEO {
class MockOclocConcat : public OclocConcat {
  public:
    MockOclocConcat(OclocArgHelper *argHelper) : OclocConcat(argHelper){};

    using OclocConcat::checkIfFatBinariesExist;
    using OclocConcat::fatBinaryName;
    using OclocConcat::fileNamesToConcat;
    using OclocConcat::parseArguments;

    Ar::Ar decodeAr(const std::vector<char> &arFile, std::string &outErrors, std::string &outWarnings) override {
        outErrors.append(decodeArErrorMessage.str());
        return {};
    }

    bool shouldFailDecodingAr = false;
    static constexpr ConstStringRef decodeArErrorMessage = "Error while decoding AR file\n";
};

} // namespace NEO