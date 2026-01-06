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

#define USE_REAL_FILE_SYSTEM()                                                                                                                                                                                       \
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopenSetter(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {                                                         \
        FILE *pFile = nullptr;                                                                                                                                                                                       \
        fopen_s(&pFile, filename, mode);                                                                                                                                                                             \
        return pFile;                                                                                                                                                                                                \
    });                                                                                                                                                                                                              \
    VariableBackup<decltype(NEO::IoFunctions::fseekPtr)> mockFseekSetter(&NEO::IoFunctions::fseekPtr, [](FILE *stream, long int offset, int origin) -> int { return fseek(stream, offset, origin); });               \
    VariableBackup<decltype(NEO::IoFunctions::ftellPtr)> mockFtellSetter(&NEO::IoFunctions::ftellPtr, [](FILE *stream) -> long int { return ftell(stream); });                                                       \
    VariableBackup<decltype(NEO::IoFunctions::freadPtr)> mockFreadSetter(&NEO::IoFunctions::freadPtr, [](void *ptr, size_t size, size_t count, FILE *stream) -> size_t { return fread(ptr, size, count, stream); }); \
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFcloseSetter(&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int { return fclose(stream); });

#define FORBID_REAL_FILE_SYSTEM_CALLS()                                                                                                                                     \
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopenAbort{&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * { EXPECT_FALSE("The filesystem function 'fopen' in use:") << "(Note: Direct access to the real disk file system is forbidden in this code scope here.)"; return nullptr; }};             \
    VariableBackup<decltype(NEO::IoFunctions::fseekPtr)> mockFseekAbort{&NEO::IoFunctions::fseekPtr, [](FILE *stream, long int offset, int origin) -> int { EXPECT_FALSE("The filesystem function 'fseek' in use:") << "(Note: Direct access to the real disk file system is forbidden in this code scope here.)"; return 0; }};             \
    VariableBackup<decltype(NEO::IoFunctions::ftellPtr)> mockFtellAbort{&NEO::IoFunctions::ftellPtr, [](FILE *stream) -> long int { EXPECT_FALSE("The filesystem function 'ftell' in use:") << "(Note: Direct access to the real disk file system is forbidden in this code scope here.)"; return 0; }};                                     \
    VariableBackup<decltype(NEO::IoFunctions::freadPtr)> mockFreadAbort{&NEO::IoFunctions::freadPtr, [](void *ptr, size_t size, size_t count, FILE *stream) -> size_t { EXPECT_FALSE("The filesystem function 'fread' in use:") << "(Note: Direct access to the real disk file system is forbidden in this code scope here.)"; return 0; }}; \
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFcloseAbort{&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int { EXPECT_FALSE("The filesystem function 'fclose' in use:") << "(Note: Direct access to the real disk file system is forbidden in this code scope here.)"; return 0; }};
