/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include <stdint.h>
#include <string>
#include <fstream>

namespace OCLRT {

class SettingsReader {
  public:
    virtual ~SettingsReader() {}
    static SettingsReader *create() {
        SettingsReader *readerImpl = createFileReader();
        if (readerImpl != nullptr)
            return readerImpl;

        return createOsReader(false);
    }
    static SettingsReader *createOsReader(bool userScope);
    static SettingsReader *createFileReader();
    virtual int32_t getSetting(const char *settingName, int32_t defaultValue) = 0;
    virtual bool getSetting(const char *settingName, bool defaultValue) = 0;
    virtual std::string getSetting(const char *settingName, const std::string &value) = 0;

    static const char *settingsFileName;
};
}; // namespace OCLRT
