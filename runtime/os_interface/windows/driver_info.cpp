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

#include "runtime/os_interface/windows/wddm/wddm.h"
#include "runtime/device/driver_info.h"
#include "runtime/os_interface/windows/driver_info.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "runtime/os_interface/windows/registry_reader.h"

namespace OCLRT {

DriverInfo *DriverInfo::create(OSInterface *osInterface) {
    if (osInterface) {
        auto wddm = osInterface->get()->getWddm();
        DEBUG_BREAK_IF(wddm == nullptr);

        std::string path(wddm->getDeviceRegistryPath());

        auto result = new DriverInfoWindows();
        path = result->trimRegistryKey(path);

        result->setRegistryReader(new RegistryReader(path));
        return result;
    }

    return nullptr;
};

void DriverInfoWindows::setRegistryReader(SettingsReader *reader) {
    registryReader.reset(reader);
}

std::string DriverInfoWindows::trimRegistryKey(std::string path) {
    std::string prefix("\\REGISTRY\\MACHINE\\");
    auto pos = prefix.find(prefix);
    if (pos != std::string::npos)
        path.erase(pos, prefix.length());

    return path;
}

std::string DriverInfoWindows::getDeviceName(std::string defaultName) {
    return registryReader.get()->getSetting("HardwareInformation.AdapterString", defaultName);
}

std::string DriverInfoWindows::getVersion(std::string defaultVersion) {
    return registryReader.get()->getSetting("DriverVersion", defaultVersion);
};
} // namespace OCLRT
