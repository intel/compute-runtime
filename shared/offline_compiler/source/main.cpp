/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "ocloc_api.h"

int main(int argc, const char *argv[]) {
    return oclocInvoke(argc, argv,
                       0, nullptr, nullptr, nullptr,
                       0, nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr, nullptr);
}
