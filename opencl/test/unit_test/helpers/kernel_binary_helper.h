/*
 * Copyright (C) 2017-2020 Intel Corporation
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

    static const std::string BUILT_INS;
};
