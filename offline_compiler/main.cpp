/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "ocloc_wrapper.h"

int main(int argc, const char *argv[]) {
    OclocWrapper oclocWrapper;
    return oclocWrapper.invokeOcloc(argc, argv);
}
