/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstddef>

void printToSTDOUT(const char *str);

template <class T>
size_t simple_sprintf(char *output, size_t outputSize, const char *format, T value);
size_t simple_sprintf(char *output, size_t outputSize, const char *format, const char *value);
size_t simple_sprintf(char *output, size_t outputSize, const char *format, void *value);
