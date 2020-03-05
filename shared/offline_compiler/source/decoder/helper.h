/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_library.h"

#include "igfxfmid.h"

#include <exception>
#include <memory>
#include <sstream>
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
        ss << std::string(message);
    }

    template <typename... Args>
    void printf(const char *format, Args... args) {
        if (!suppressMessages) {
            ::printf(format, std::forward<Args>(args)...);
        }
        ss << stringFormat(format, std::forward<Args>(args)...);
    }

    const std::ostream &getLog() {
        return ss;
    }

  private:
    template <typename... Args>
    std::string stringFormat(const std::string &format, Args... args) {
        std::string outputString;
        size_t size = static_cast<size_t>(snprintf(nullptr, 0, format.c_str(), args...) + 1);
        if (size <= 0) {
            return outputString;
        }
        outputString.resize(size);
        snprintf(&*outputString.begin(), size, format.c_str(), args...);
        return outputString;
    }

    std::stringstream ss;
    bool suppressMessages = false;
};
