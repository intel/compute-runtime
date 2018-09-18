/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/print.h"

#include <iostream>
#include <cstdio>

void printToSTDOUT(const char *str) {
    fprintf(stdout, "%s", str);
    fflush(stdout);
}

template <class T>
size_t simple_sprintf(char *output, size_t outputSize, const char *format, T value) {
    return snprintf(output, outputSize, format, value);
}

size_t simple_sprintf(char *output, size_t outputSize, const char *format, const char *value) {
    return snprintf(output, outputSize, format, value);
}

template size_t simple_sprintf<float>(char *output, size_t output_size, const char *format, float value);
template size_t simple_sprintf<double>(char *output, size_t output_size, const char *format, double value);
template size_t simple_sprintf<char>(char *output, size_t output_size, const char *format, char value);
template size_t simple_sprintf<int8_t>(char *output, size_t output_size, const char *format, int8_t value);
template size_t simple_sprintf<int16_t>(char *output, size_t output_size, const char *format, int16_t value);
template size_t simple_sprintf<int32_t>(char *output, size_t output_size, const char *format, int32_t value);
template size_t simple_sprintf<int64_t>(char *output, size_t output_size, const char *format, int64_t value);
template size_t simple_sprintf<uint8_t>(char *output, size_t output_size, const char *format, uint8_t value);
template size_t simple_sprintf<uint16_t>(char *output, size_t output_size, const char *format, uint16_t value);
template size_t simple_sprintf<uint32_t>(char *output, size_t output_size, const char *format, uint32_t value);
template size_t simple_sprintf<uint64_t>(char *output, size_t output_size, const char *format, uint64_t value);
