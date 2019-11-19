/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/os_interface/os_library.h"

#include "igfxfmid.h"

#include <exception>
#include <memory>
#include <string>
#include <vector>

void addSlash(std::string &path);

std::vector<char> readBinaryFile(const std::string &fileName);

void readFileToVectorOfStrings(std::vector<std::string> &lines, const std::string &fileName, bool replaceTabs = false);

size_t findPos(const std::vector<std::string> &lines, const std::string &whatToFind);

PRODUCT_FAMILY getProductFamilyFromDeviceName(const std::string &deviceName);

class MessagePrinter {
  public:
    MessagePrinter() = default;
    MessagePrinter(bool suppressMessages) : suppressMessages(suppressMessages) {}

    void printf(const char *message) {
        if (!suppressMessages) {
            ::printf("%s", message);
        }
    }

    template <typename... Args>
    void printf(const char *format, Args... args) {
        if (!suppressMessages) {
            ::printf(format, std::forward<Args>(args)...);
        }
    }

  private:
    bool suppressMessages = false;
};
