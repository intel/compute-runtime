/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/print.h"

#include "shared/source/helpers/debug_helpers.h"

#include <cstdint>
#include <cstdio>
#include <iostream>

void printToStdout(const char *str) {
    fprintf(stdout, "%s", str);
    fflush(stdout);
}

void printToStderr(const char *str) {
    fprintf(stderr, "%s", str);
    fflush(stderr);
}

template <class T>
size_t simpleSprintf(char *output, size_t outputSize, const char *format, T value) {
    auto retVal = snprintf(output, outputSize, format, value);
    UNRECOVERABLE_IF(retVal < 0);
    return static_cast<size_t>(retVal);
}

size_t simpleSprintf(char *output, size_t outputSize, const char *format, const char *value) {
    auto retVal = snprintf(output, outputSize, format, value);
    UNRECOVERABLE_IF(retVal < 0);
    return static_cast<size_t>(retVal);
}

template size_t simpleSprintf<float>(char *output, size_t output_size, const char *format, float value);
template size_t simpleSprintf<double>(char *output, size_t output_size, const char *format, double value);
template size_t simpleSprintf<char>(char *output, size_t output_size, const char *format, char value);
template size_t simpleSprintf<int8_t>(char *output, size_t output_size, const char *format, int8_t value);
template size_t simpleSprintf<int16_t>(char *output, size_t output_size, const char *format, int16_t value);
template size_t simpleSprintf<int32_t>(char *output, size_t output_size, const char *format, int32_t value);
template size_t simpleSprintf<int64_t>(char *output, size_t output_size, const char *format, int64_t value);
template size_t simpleSprintf<uint8_t>(char *output, size_t output_size, const char *format, uint8_t value);
template size_t simpleSprintf<uint16_t>(char *output, size_t output_size, const char *format, uint16_t value);
template size_t simpleSprintf<uint32_t>(char *output, size_t output_size, const char *format, uint32_t value);
template size_t simpleSprintf<uint64_t>(char *output, size_t output_size, const char *format, uint64_t value);
