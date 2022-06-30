/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <string>

class KernelBinaryHelper {
  public:
    KernelBinaryHelper(const std::string &name = "copybuffer", bool appendOptionsToFileName = true);
    ~KernelBinaryHelper();

    static const std::string BUILT_INS;             // NOLINT(readability-identifier-naming)
    static const std::string BUILT_INS_WITH_IMAGES; // NOLINT(readability-identifier-naming)
};
