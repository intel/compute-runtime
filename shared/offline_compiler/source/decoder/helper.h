/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "neo_igfxfmid.h"

#include <memory>
#include <sstream>
#include <string>
#include <vector>

class OclocArgHelper;
struct IgaWrapper;

extern void (*abortOclocExecution)(int);

void abortOclocExecutionDefaultHandler(int errorCode);

void addSlash(std::string &path);

std::vector<char> readBinaryFile(const std::string &fileName);

void istreamToVectorOfStrings(std::istream &input, std::vector<std::string> &lines, bool replaceTabs = false);
void readFileToVectorOfStrings(std::vector<std::string> &lines, const std::string &fileName, bool replaceTabs = false);
void setProductFamilyForIga(const std::string &device, IgaWrapper *iga, OclocArgHelper *argHelper);

size_t findPos(const std::vector<std::string> &lines, const std::string &whatToFind);

PRODUCT_FAMILY getProductFamilyFromDeviceName(const std::string &deviceName);

class MessagePrinter : NEO::NonCopyableAndNonMovableClass {
  public:
    explicit MessagePrinter() = default;
    explicit MessagePrinter(bool suppressMessages) : suppressMessages(suppressMessages) {}

    void printf(const char *message) {
        if (!suppressMessages) {
            ::printf("%s", message);
        }
        ss << std::string(message);
    }

    template <typename... Args>
    void printf(const char *format, Args... args) {
        if (!suppressMessages) {
            ::printf(format, args...);
        }
        ss << stringFormat(format, args...);
    }

    const std::stringstream &getLog() {
        return ss;
    }

    bool isSuppressed() const { return suppressMessages; }
    void setSuppressMessages(bool suppress) {
        suppressMessages = suppress;
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
        return outputString.c_str();
    }

    std::stringstream ss;
    bool suppressMessages = false;
};
