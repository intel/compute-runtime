/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/stdio.h"
#include "shared/source/utilities/io_functions.h"
#include "shared/test/common/helpers/variable_backup.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

std::unique_ptr<char[]> loadDataFromFile(
    const char *filename,
    size_t &retSize);

size_t writeDataToFile(
    const char *filename,
    std::string_view data);

bool fileExists(const std::string &fileName);
bool fileExistsHasSize(const std::string &fileName);
void dumpFileIncrement(const char *data, size_t dataSize, const std::string &filename, const std::string &extension);

#define USE_REAL_FILE_SYSTEM()                                                                                                                                                                                 \
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {                                                         \
        FILE *pFile = nullptr;                                                                                                                                                                                 \
        fopen_s(&pFile, filename, mode);                                                                                                                                                                       \
        return pFile;                                                                                                                                                                                          \
    });                                                                                                                                                                                                        \
    VariableBackup<decltype(NEO::IoFunctions::fseekPtr)> mockFseek(&NEO::IoFunctions::fseekPtr, [](FILE *stream, long int offset, int origin) -> int { return fseek(stream, offset, origin); });               \
    VariableBackup<decltype(NEO::IoFunctions::ftellPtr)> mockFtell(&NEO::IoFunctions::ftellPtr, [](FILE *stream) -> long int { return ftell(stream); });                                                       \
    VariableBackup<decltype(NEO::IoFunctions::freadPtr)> mockFread(&NEO::IoFunctions::freadPtr, [](void *ptr, size_t size, size_t count, FILE *stream) -> size_t { return fread(ptr, size, count, stream); }); \
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFclose(&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int { return fclose(stream); });
