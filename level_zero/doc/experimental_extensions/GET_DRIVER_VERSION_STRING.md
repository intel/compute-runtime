<!---

Copyright (C) 2024 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Get Driver Versiopn String

* [Overview](#Overview)
* [Definitions](#Definitions)

# Overview

To support reporting the Level Zero gpu driver version as a set string value, a new extension API `zeIntelGetDriverVersionString` to retrieve a specific version string from the driver has been created.

## Outline

`zeIntelGetDriverVersionString` reports the driver version in the form: `Major.Minor.Patch+Optional`.

Example Versions:

| Level Zero  Package Version | Level Zero Driver Version String
|---|---|
| 1.3.27642.40 | 1.3.27642+40 |
| 1.3.27191 | 1.3.27191


# Definitions

```cpp
#define ZE_INTEL_GET_DRIVER_VERSION_STRING_EXP_NAME "ZE_intel_get_driver_version_string"
```

## Interfaces

```cpp
/// @brief Query to read the Intel Level Zero Driver Version String
///
/// @details
///     - The application may call this function from simultaneous threads.
///     - The implementation of this function should be lock-free.
///     - The Driver Version String will be in the format:
///     - Major.Minor.Patch+Optional per semver guidelines https://semver.org/#spec-item-10
/// @returns
///     - ::ZE_RESULT_SUCCESS
ze_result_t ZE_APICALL
zeIntelGetDriverVersionString(
    ze_driver_handle_t hDriver, ///< [in] Driver handle whose version is being read.
    char *pDriverVersion,       ///< [in,out] pointer to driver version string.
    size_t *pVersionSize);      ///< [in,out] pointer to the size of the driver version string.
                                ///< if size is zero, then the size of the version string is returned.
```

## Programming example

```cpp
//Check for extension existence
uint32_t count = 0;
bool supported = false;
zeDriverGetExtensionProperties(driverHandle, &count, nullptr);

std::vector<ze_driver_extension_properties_t> properties(count);
memset(properties.data(), 0,
        sizeof(ze_driver_extension_properties_t) * count);
zeDriverGetExtensionProperties(driverHandle, &count, properties.data());
for (auto &extension : properties) {
    if (strcmp(extension.name, ZE_intel_get_driver_version_string) == 0) {
        supported = true;
    }
}
//Call the driver extension if it exists.
if (supported) {
    zeDriverGetExtensionFunctionAddress(driverHandle, "zeIntelGetDriverVersionString", pfnGetDriverVersionFn);
    size_t sizeOfDriverString = 0;
    pfnGetDriverVersionFn(driverHandle, nullptr, &sizeOfDriverString);
    char *driverVersionString = reinterpret_cast<char *>(malloc(sizeOfDriverString * sizeof(char)));
    pfnGetDriverVersionFn(driverHandle, driverVersionString, &sizeOfDriverString);
    free(driverVersionString);
}
```
