/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/source/os_interface/windows/shared_section.h"
#include "shared/source/os_interface/windows/sys_calls.h"

#include <algorithm>
#include <cstring>
#include <cwchar>

namespace NEO {

bool SharedSection::isSection(HANDLE handle) {
    constexpr wchar_t sectionTypeName[] = L"Section";
    constexpr size_t sectionTypeNameLength = (sizeof(sectionTypeName) / sizeof(wchar_t)) - 1;
    struct {
        PUBLIC_OBJECT_TYPE_INFORMATION information;
        wchar_t typeName[sectionTypeNameLength + 1];
    } typeInformation = {};
    ULONG returnLength = 0;
    if (SysCalls::ntQueryObject(handle, ObjectTypeInformation, &typeInformation, sizeof(typeInformation), &returnLength) != STATUS_SUCCESS) {
        return false;
    }
    auto &typeName = typeInformation.information.TypeName;
    return typeName.Buffer != nullptr &&
           typeName.Length == sectionTypeNameLength * sizeof(wchar_t) &&
           wcsncmp(typeName.Buffer, sectionTypeName, sectionTypeNameLength) == 0;
}

bool SharedSection::initialize() {
    MEMORY_BASIC_INFORMATION memoryInfo = {};
    if (SysCalls::virtualQuery(view, &memoryInfo, sizeof(memoryInfo)) != sizeof(memoryInfo)) {
        return false;
    }

    size_t viewSize = memoryInfo.RegionSize;
    if (viewSize < sizeof(SharedMemoryContentTail)) {
        return false;
    }

    SharedMemoryContentTail tail = {};
    memcpy(&tail, ptrOffset(view, viewSize - sizeof(SharedMemoryContentTail)), sizeof(tail));
    if (tail.serializedSecurityDescriptorStringSize > viewSize - sizeof(SharedMemoryContentTail)) {
        return false;
    }

    memcpy(&gmmParams, &tail.gmmParams, sizeof(gmmParams));
    if (gmmParams.ExistingSysMemSize == 0u || gmmParams.ExistingSysMemSize > viewSize) {
        return false;
    }

    gmmParams.pExistingSysMem = castToUint64(view);
    gmmParams.Flags.Info.Shared = 0;
    gmmParams.pPreallocatedResInfo = nullptr;
    gmmParams.pPrivateData = 0u;
    gmmParams.PrivateDataSize = 0u;

    auto sizeBeforeMetadata = viewSize - sizeof(SharedMemoryContentTail) - tail.serializedSecurityDescriptorStringSize;
    usableSize = std::min(static_cast<size_t>(gmmParams.ExistingSysMemSize), sizeBeforeMetadata);
    return true;
}

} // namespace NEO
