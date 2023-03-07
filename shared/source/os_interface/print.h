/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstddef>

void printToStdout(const char *str);
void printToStderr(const char *str);

template <class T>
size_t simpleSprintf(char *output, size_t outputSize, const char *format, T value);
size_t simpleSprintf(char *output, size_t outputSize, const char *format, const char *value);
