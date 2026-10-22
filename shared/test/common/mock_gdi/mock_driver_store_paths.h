/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

template <size_t... lengths>
constexpr auto joinWideStrings(const wchar_t (&...parts)[lengths]) {
    std::array<wchar_t, (lengths + ...) - sizeof...(lengths) + 1> joined{};
    size_t offset = 0;
    ((std::copy_n(parts, lengths - 1, joined.begin() + offset), offset += lengths - 1), ...);
    return joined;
}

inline constexpr wchar_t mockSystemRootPrefix[] = L"\\SystemRoot";
inline constexpr wchar_t mockWindowsDirectoryPrefix[] = L"C:\\Windows";
inline constexpr wchar_t mockDriverStoreRepository[] = L"\\System32\\DriverStore\\FileRepository\\";
inline constexpr wchar_t mockAdapterDriverPackage[] = L"iigd_dch.inf_amd64_0x8086";
inline constexpr wchar_t mockOtherDriverPackage[] = L"iigd_dch.inf_amd64_0x1234";
inline constexpr wchar_t mockOpenClLibraryName[] = L"\\igdrcl64.dll";

inline constexpr auto mockAdapterDriverStorePathStorage = joinWideStrings(mockSystemRootPrefix, mockDriverStoreRepository, mockAdapterDriverPackage);
inline constexpr auto mockOtherAdapterDriverStorePathStorage = joinWideStrings(mockSystemRootPrefix, mockDriverStoreRepository, mockOtherDriverPackage);
inline constexpr auto mockComputeRuntimePathStorage = joinWideStrings(mockWindowsDirectoryPrefix, mockDriverStoreRepository, mockAdapterDriverPackage);
inline constexpr auto mockLibraryFromAdapterDriverStoreStorage = joinWideStrings(mockWindowsDirectoryPrefix, mockDriverStoreRepository, mockAdapterDriverPackage, mockOpenClLibraryName);
inline constexpr auto mockLibraryFromOtherDriverStoreStorage = joinWideStrings(mockWindowsDirectoryPrefix, mockDriverStoreRepository, mockOtherDriverPackage, mockOpenClLibraryName);

inline constexpr std::wstring_view mockAdapterDriverStorePath{mockAdapterDriverStorePathStorage.data()};
inline constexpr std::wstring_view mockOtherAdapterDriverStorePath{mockOtherAdapterDriverStorePathStorage.data()};
inline constexpr std::wstring_view mockComputeRuntimePath{mockComputeRuntimePathStorage.data()};
inline constexpr std::wstring_view mockLibraryFromAdapterDriverStore{mockLibraryFromAdapterDriverStoreStorage.data()};
inline constexpr std::wstring_view mockLibraryFromOtherDriverStore{mockLibraryFromOtherDriverStoreStorage.data()};
