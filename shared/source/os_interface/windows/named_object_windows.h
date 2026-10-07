/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/windows/windows_wrapper.h"

#include <cstdint>
#include <functional>
#include <limits>
#include <string>

namespace NEO {
namespace NamedObjectWindows {

// RtlInitUnicodeString stores Length and MaximumLength as USHORT byte counts, with
// MaximumLength including a terminator, so this is the longest name it can represent.
inline constexpr size_t maxNameLengthInWideChars = (static_cast<size_t>(std::numeric_limits<USHORT>::max()) - sizeof(wchar_t)) / sizeof(wchar_t);

// Resolves the NT object-manager directory for a Win32-style named object, honoring an
// optional "Global\\" / "Local\\" prefix the same way kernelbase does. relativeName is set
// to the portion of name to open under the returned directory. sessionId is the caller's
// session; session 0 has no \Sessions\0 directory, so it collapses to \BaseNamedObjects.
std::wstring getNamedObjectDirectoryPath(uint32_t sessionId, const wchar_t *name, const wchar_t **relativeName);

using OpenByAttributesFn = std::function<HANDLE(OBJECT_ATTRIBUTES *objectAttributes)>;

HANDLE openNamedObject(const wchar_t *name, bool forceGlobal, const OpenByAttributesFn &openFn);

} // namespace NamedObjectWindows
} // namespace NEO
