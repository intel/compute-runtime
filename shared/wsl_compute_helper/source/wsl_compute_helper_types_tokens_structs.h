/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

// This is a generated file - please don't modify directly
#pragma once
#include "wsl_compute_helper_types_tokens.h"

// Token layout
struct TOKSTR_GT_SUBSLICE_INFO {
    TokenVariableLength base;

    TOKSTR_GT_SUBSLICE_INFO(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR_GT_SUBSLICE_INFO, EuEnabledMask) + sizeof(EuEnabledMask) - offsetof(TOKSTR_GT_SUBSLICE_INFO, Enabled), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR_GT_SUBSLICE_INFO()
        : base(TOK_S_GT_SUBSLICE_INFO, 0, sizeof(*this) - sizeof(base)) {}

    TokenBool Enabled = {TOK_FBB_GT_SUBSLICE_INFO__ENABLED};
    TokenDword EuEnabledCount = {TOK_FBD_GT_SUBSLICE_INFO__EU_ENABLED_COUNT};
    TokenDword EuEnabledMask = {TOK_FBD_GT_SUBSLICE_INFO__EU_ENABLED_MASK};
};
static_assert(std::is_standard_layout_v<TOKSTR_GT_SUBSLICE_INFO>, "");
static_assert(sizeof(TOKSTR_GT_SUBSLICE_INFO) % sizeof(uint32_t) == 0, "");

struct TOKSTR_GT_DUALSUBSLICE_INFO {
    TokenVariableLength base;

    TOKSTR_GT_DUALSUBSLICE_INFO(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR_GT_DUALSUBSLICE_INFO, SubSlice) + sizeof(SubSlice) - offsetof(TOKSTR_GT_DUALSUBSLICE_INFO, Enabled), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR_GT_DUALSUBSLICE_INFO()
        : base(TOK_S_GT_DUALSUBSLICE_INFO, 0, sizeof(*this) - sizeof(base)) {}

    TokenBool Enabled = {TOK_FBB_GT_DUALSUBSLICE_INFO__ENABLED};
    TOKSTR_GT_SUBSLICE_INFO SubSlice[2] = {{TOK_FS_GT_DUALSUBSLICE_INFO__SUB_SLICE, 0}, {TOK_FS_GT_DUALSUBSLICE_INFO__SUB_SLICE, 1}};
};
static_assert(std::is_standard_layout_v<TOKSTR_GT_DUALSUBSLICE_INFO>, "");
static_assert(sizeof(TOKSTR_GT_DUALSUBSLICE_INFO) % sizeof(uint32_t) == 0, "");

struct TOKSTR_GT_SLICE_INFO {
    TokenVariableLength base;

    TOKSTR_GT_SLICE_INFO(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR_GT_SLICE_INFO, DualSubSliceEnabledCount) + sizeof(DualSubSliceEnabledCount) - offsetof(TOKSTR_GT_SLICE_INFO, Enabled), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR_GT_SLICE_INFO()
        : base(TOK_S_GT_SLICE_INFO, 0, sizeof(*this) - sizeof(base)) {}

    TokenBool Enabled = {TOK_FBB_GT_SLICE_INFO__ENABLED};
    TOKSTR_GT_SUBSLICE_INFO SubSliceInfo[8] = {{TOK_FS_GT_SLICE_INFO__SUB_SLICE_INFO, 0}, {TOK_FS_GT_SLICE_INFO__SUB_SLICE_INFO, 1}, {TOK_FS_GT_SLICE_INFO__SUB_SLICE_INFO, 2}, {TOK_FS_GT_SLICE_INFO__SUB_SLICE_INFO, 3}, {TOK_FS_GT_SLICE_INFO__SUB_SLICE_INFO, 4}, {TOK_FS_GT_SLICE_INFO__SUB_SLICE_INFO, 5}, {TOK_FS_GT_SLICE_INFO__SUB_SLICE_INFO, 6}, {TOK_FS_GT_SLICE_INFO__SUB_SLICE_INFO, 7}};
    TOKSTR_GT_DUALSUBSLICE_INFO DSSInfo[6] = {{TOK_FS_GT_SLICE_INFO__DSSINFO, 0}, {TOK_FS_GT_SLICE_INFO__DSSINFO, 1}, {TOK_FS_GT_SLICE_INFO__DSSINFO, 2}, {TOK_FS_GT_SLICE_INFO__DSSINFO, 3}, {TOK_FS_GT_SLICE_INFO__DSSINFO, 4}, {TOK_FS_GT_SLICE_INFO__DSSINFO, 5}};
    TokenDword SubSliceEnabledCount = {TOK_FBD_GT_SLICE_INFO__SUB_SLICE_ENABLED_COUNT};
    TokenDword DualSubSliceEnabledCount = {TOK_FBD_GT_SLICE_INFO__DUAL_SUB_SLICE_ENABLED_COUNT};
};
static_assert(std::is_standard_layout_v<TOKSTR_GT_SLICE_INFO>, "");
static_assert(sizeof(TOKSTR_GT_SLICE_INFO) % sizeof(uint32_t) == 0, "");

struct TOKSTR_GT_VEBOX_INFO {
    TokenVariableLength base;

    TOKSTR_GT_VEBOX_INFO(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR_GT_VEBOX_INFO, IsValid) + sizeof(IsValid) - offsetof(TOKSTR_GT_VEBOX_INFO, Instances), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR_GT_VEBOX_INFO()
        : base(TOK_S_GT_VEBOX_INFO, 0, sizeof(*this) - sizeof(base)) {}

    struct TOKSTR_VEBoxInstances {
        TokenVariableLength base;

        TOKSTR_VEBoxInstances(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_VEBoxInstances, VEBoxEnableMask) + sizeof(VEBoxEnableMask) - offsetof(TOKSTR_VEBoxInstances, Bits), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_VEBoxInstances()
            : base(TOK_S_GT_VEBOX_INFO__VEBOX_INSTANCES, 0, sizeof(*this) - sizeof(base)) {}

        struct TOKSTR_VEBitStruct {
            TokenVariableLength base;

            TOKSTR_VEBitStruct(uint16_t tokenId, uint32_t elementId = 0)
                : base(tokenId, elementId, offsetof(TOKSTR_VEBitStruct, VEBox3Enabled) + sizeof(VEBox3Enabled) - offsetof(TOKSTR_VEBitStruct, VEBox0Enabled), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

            TOKSTR_VEBitStruct()
                : base(TOK_S_GT_VEBOX_INFO__VEBOX_INSTANCES__VEBIT_STRUCT, 0, sizeof(*this) - sizeof(base)) {}

            TokenDword VEBox0Enabled = {TOK_FBD_GT_VEBOX_INFO__VEBOX_INSTANCES__VEBIT_STRUCT__VEBOX0ENABLED};
            TokenDword VEBox1Enabled = {TOK_FBD_GT_VEBOX_INFO__VEBOX_INSTANCES__VEBIT_STRUCT__VEBOX1ENABLED};
            TokenDword VEBox2Enabled = {TOK_FBD_GT_VEBOX_INFO__VEBOX_INSTANCES__VEBIT_STRUCT__VEBOX2ENABLED};
            TokenDword VEBox3Enabled = {TOK_FBD_GT_VEBOX_INFO__VEBOX_INSTANCES__VEBIT_STRUCT__VEBOX3ENABLED};
        };
        static_assert(std::is_standard_layout_v<TOKSTR_VEBitStruct>, "");
        static_assert(sizeof(TOKSTR_VEBitStruct) % sizeof(uint32_t) == 0, "");

        TOKSTR_VEBitStruct Bits = {TOK_FS_GT_VEBOX_INFO__VEBOX_INSTANCES__BITS};
        TokenDword VEBoxEnableMask = {TOK_FBD_GT_VEBOX_INFO__VEBOX_INSTANCES__VEBOX_ENABLE_MASK};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_VEBoxInstances>, "");
    static_assert(sizeof(TOKSTR_VEBoxInstances) % sizeof(uint32_t) == 0, "");

    struct TOKSTR_ANONYMOUS3862 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS3862(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS3862, Value) + sizeof(Value) - offsetof(TOKSTR_ANONYMOUS3862, SfcSupportedBits), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS3862()
            : base(TOK_S_GT_VEBOX_INFO__ANONYMOUS3862, 0, sizeof(*this) - sizeof(base)) {}

        struct TOKSTR_ANONYMOUS3882 {
            TokenVariableLength base;

            TOKSTR_ANONYMOUS3882(uint16_t tokenId, uint32_t elementId = 0)
                : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS3882, VEBox3) + sizeof(VEBox3) - offsetof(TOKSTR_ANONYMOUS3882, VEBox0), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

            TOKSTR_ANONYMOUS3882()
                : base(TOK_S_GT_VEBOX_INFO__ANONYMOUS3862__ANONYMOUS3882, 0, sizeof(*this) - sizeof(base)) {}

            TokenDword VEBox0 = {TOK_FBD_GT_VEBOX_INFO__ANONYMOUS3862__ANONYMOUS3882__VEBOX0};
            TokenDword VEBox1 = {TOK_FBD_GT_VEBOX_INFO__ANONYMOUS3862__ANONYMOUS3882__VEBOX1};
            TokenDword VEBox2 = {TOK_FBD_GT_VEBOX_INFO__ANONYMOUS3862__ANONYMOUS3882__VEBOX2};
            TokenDword VEBox3 = {TOK_FBD_GT_VEBOX_INFO__ANONYMOUS3862__ANONYMOUS3882__VEBOX3};
        };
        static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS3882>, "");
        static_assert(sizeof(TOKSTR_ANONYMOUS3882) % sizeof(uint32_t) == 0, "");

        TOKSTR_ANONYMOUS3882 SfcSupportedBits = {TOK_FS_GT_VEBOX_INFO__ANONYMOUS3862__SFC_SUPPORTED_BITS};
        TokenDword Value = {TOK_FBD_GT_VEBOX_INFO__ANONYMOUS3862__VALUE};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS3862>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS3862) % sizeof(uint32_t) == 0, "");

    TOKSTR_VEBoxInstances Instances = {TOK_FS_GT_VEBOX_INFO__INSTANCES};
    TOKSTR_ANONYMOUS3862 SFCSupport = {TOK_FS_GT_VEBOX_INFO__SFCSUPPORT};
    TokenDword NumberOfVEBoxEnabled = {TOK_FBD_GT_VEBOX_INFO__NUMBER_OF_VEBOX_ENABLED};
    TokenBool IsValid = {TOK_FBB_GT_VEBOX_INFO__IS_VALID};
};
static_assert(std::is_standard_layout_v<TOKSTR_GT_VEBOX_INFO>, "");
static_assert(sizeof(TOKSTR_GT_VEBOX_INFO) % sizeof(uint32_t) == 0, "");

struct TOKSTR_GT_VDBOX_INFO {
    TokenVariableLength base;

    TOKSTR_GT_VDBOX_INFO(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR_GT_VDBOX_INFO, IsValid) + sizeof(IsValid) - offsetof(TOKSTR_GT_VDBOX_INFO, Instances), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR_GT_VDBOX_INFO()
        : base(TOK_S_GT_VDBOX_INFO, 0, sizeof(*this) - sizeof(base)) {}

    struct TOKSTR_VDBoxInstances {
        TokenVariableLength base;

        TOKSTR_VDBoxInstances(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_VDBoxInstances, VDBoxEnableMask) + sizeof(VDBoxEnableMask) - offsetof(TOKSTR_VDBoxInstances, Bits), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_VDBoxInstances()
            : base(TOK_S_GT_VDBOX_INFO__VDBOX_INSTANCES, 0, sizeof(*this) - sizeof(base)) {}

        struct TOKSTR_VDBitStruct {
            TokenVariableLength base;

            TOKSTR_VDBitStruct(uint16_t tokenId, uint32_t elementId = 0)
                : base(tokenId, elementId, offsetof(TOKSTR_VDBitStruct, VDBox7Enabled) + sizeof(VDBox7Enabled) - offsetof(TOKSTR_VDBitStruct, VDBox0Enabled), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

            TOKSTR_VDBitStruct()
                : base(TOK_S_GT_VDBOX_INFO__VDBOX_INSTANCES__VDBIT_STRUCT, 0, sizeof(*this) - sizeof(base)) {}

            TokenDword VDBox0Enabled = {TOK_FBD_GT_VDBOX_INFO__VDBOX_INSTANCES__VDBIT_STRUCT__VDBOX0ENABLED};
            TokenDword VDBox1Enabled = {TOK_FBD_GT_VDBOX_INFO__VDBOX_INSTANCES__VDBIT_STRUCT__VDBOX1ENABLED};
            TokenDword VDBox2Enabled = {TOK_FBD_GT_VDBOX_INFO__VDBOX_INSTANCES__VDBIT_STRUCT__VDBOX2ENABLED};
            TokenDword VDBox3Enabled = {TOK_FBD_GT_VDBOX_INFO__VDBOX_INSTANCES__VDBIT_STRUCT__VDBOX3ENABLED};
            TokenDword VDBox4Enabled = {TOK_FBD_GT_VDBOX_INFO__VDBOX_INSTANCES__VDBIT_STRUCT__VDBOX4ENABLED};
            TokenDword VDBox5Enabled = {TOK_FBD_GT_VDBOX_INFO__VDBOX_INSTANCES__VDBIT_STRUCT__VDBOX5ENABLED};
            TokenDword VDBox6Enabled = {TOK_FBD_GT_VDBOX_INFO__VDBOX_INSTANCES__VDBIT_STRUCT__VDBOX6ENABLED};
            TokenDword VDBox7Enabled = {TOK_FBD_GT_VDBOX_INFO__VDBOX_INSTANCES__VDBIT_STRUCT__VDBOX7ENABLED};
        };
        static_assert(std::is_standard_layout_v<TOKSTR_VDBitStruct>, "");
        static_assert(sizeof(TOKSTR_VDBitStruct) % sizeof(uint32_t) == 0, "");

        TOKSTR_VDBitStruct Bits = {TOK_FS_GT_VDBOX_INFO__VDBOX_INSTANCES__BITS};
        TokenDword VDBoxEnableMask = {TOK_FBD_GT_VDBOX_INFO__VDBOX_INSTANCES__VDBOX_ENABLE_MASK};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_VDBoxInstances>, "");
    static_assert(sizeof(TOKSTR_VDBoxInstances) % sizeof(uint32_t) == 0, "");

    struct TOKSTR_ANONYMOUS5662 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS5662(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS5662, Value) + sizeof(Value) - offsetof(TOKSTR_ANONYMOUS5662, SfcSupportedBits), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS5662()
            : base(TOK_S_GT_VDBOX_INFO__ANONYMOUS5662, 0, sizeof(*this) - sizeof(base)) {}

        struct TOKSTR_ANONYMOUS5682 {
            TokenVariableLength base;

            TOKSTR_ANONYMOUS5682(uint16_t tokenId, uint32_t elementId = 0)
                : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS5682, VDBox7) + sizeof(VDBox7) - offsetof(TOKSTR_ANONYMOUS5682, VDBox0), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

            TOKSTR_ANONYMOUS5682()
                : base(TOK_S_GT_VDBOX_INFO__ANONYMOUS5662__ANONYMOUS5682, 0, sizeof(*this) - sizeof(base)) {}

            TokenDword VDBox0 = {TOK_FBD_GT_VDBOX_INFO__ANONYMOUS5662__ANONYMOUS5682__VDBOX0};
            TokenDword VDBox1 = {TOK_FBD_GT_VDBOX_INFO__ANONYMOUS5662__ANONYMOUS5682__VDBOX1};
            TokenDword VDBox2 = {TOK_FBD_GT_VDBOX_INFO__ANONYMOUS5662__ANONYMOUS5682__VDBOX2};
            TokenDword VDBox3 = {TOK_FBD_GT_VDBOX_INFO__ANONYMOUS5662__ANONYMOUS5682__VDBOX3};
            TokenDword VDBox4 = {TOK_FBD_GT_VDBOX_INFO__ANONYMOUS5662__ANONYMOUS5682__VDBOX4};
            TokenDword VDBox5 = {TOK_FBD_GT_VDBOX_INFO__ANONYMOUS5662__ANONYMOUS5682__VDBOX5};
            TokenDword VDBox6 = {TOK_FBD_GT_VDBOX_INFO__ANONYMOUS5662__ANONYMOUS5682__VDBOX6};
            TokenDword VDBox7 = {TOK_FBD_GT_VDBOX_INFO__ANONYMOUS5662__ANONYMOUS5682__VDBOX7};
        };
        static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS5682>, "");
        static_assert(sizeof(TOKSTR_ANONYMOUS5682) % sizeof(uint32_t) == 0, "");

        TOKSTR_ANONYMOUS5682 SfcSupportedBits = {TOK_FS_GT_VDBOX_INFO__ANONYMOUS5662__SFC_SUPPORTED_BITS};
        TokenDword Value = {TOK_FBD_GT_VDBOX_INFO__ANONYMOUS5662__VALUE};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS5662>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS5662) % sizeof(uint32_t) == 0, "");

    TOKSTR_VDBoxInstances Instances = {TOK_FS_GT_VDBOX_INFO__INSTANCES};
    TOKSTR_ANONYMOUS5662 SFCSupport = {TOK_FS_GT_VDBOX_INFO__SFCSUPPORT};
    TokenDword NumberOfVDBoxEnabled = {TOK_FBD_GT_VDBOX_INFO__NUMBER_OF_VDBOX_ENABLED};
    TokenBool IsValid = {TOK_FBB_GT_VDBOX_INFO__IS_VALID};
};
static_assert(std::is_standard_layout_v<TOKSTR_GT_VDBOX_INFO>, "");
static_assert(sizeof(TOKSTR_GT_VDBOX_INFO) % sizeof(uint32_t) == 0, "");

struct TOKSTR_GT_CCS_INFO {
    TokenVariableLength base;

    TOKSTR_GT_CCS_INFO(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR_GT_CCS_INFO, IsValid) + sizeof(IsValid) - offsetof(TOKSTR_GT_CCS_INFO, Instances), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR_GT_CCS_INFO()
        : base(TOK_S_GT_CCS_INFO, 0, sizeof(*this) - sizeof(base)) {}

    struct TOKSTR_CCSInstances {
        TokenVariableLength base;

        TOKSTR_CCSInstances(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_CCSInstances, CCSEnableMask) + sizeof(CCSEnableMask) - offsetof(TOKSTR_CCSInstances, Bits), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_CCSInstances()
            : base(TOK_S_GT_CCS_INFO__CCSINSTANCES, 0, sizeof(*this) - sizeof(base)) {}

        struct TOKSTR_CCSBitStruct {
            TokenVariableLength base;

            TOKSTR_CCSBitStruct(uint16_t tokenId, uint32_t elementId = 0)
                : base(tokenId, elementId, offsetof(TOKSTR_CCSBitStruct, CCS3Enabled) + sizeof(CCS3Enabled) - offsetof(TOKSTR_CCSBitStruct, CCS0Enabled), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

            TOKSTR_CCSBitStruct()
                : base(TOK_S_GT_CCS_INFO__CCSINSTANCES__CCSBIT_STRUCT, 0, sizeof(*this) - sizeof(base)) {}

            TokenDword CCS0Enabled = {TOK_FBD_GT_CCS_INFO__CCSINSTANCES__CCSBIT_STRUCT__CCS0ENABLED};
            TokenDword CCS1Enabled = {TOK_FBD_GT_CCS_INFO__CCSINSTANCES__CCSBIT_STRUCT__CCS1ENABLED};
            TokenDword CCS2Enabled = {TOK_FBD_GT_CCS_INFO__CCSINSTANCES__CCSBIT_STRUCT__CCS2ENABLED};
            TokenDword CCS3Enabled = {TOK_FBD_GT_CCS_INFO__CCSINSTANCES__CCSBIT_STRUCT__CCS3ENABLED};
        };
        static_assert(std::is_standard_layout_v<TOKSTR_CCSBitStruct>, "");
        static_assert(sizeof(TOKSTR_CCSBitStruct) % sizeof(uint32_t) == 0, "");

        TOKSTR_CCSBitStruct Bits = {TOK_FS_GT_CCS_INFO__CCSINSTANCES__BITS};
        TokenDword CCSEnableMask = {TOK_FBD_GT_CCS_INFO__CCSINSTANCES__CCSENABLE_MASK};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_CCSInstances>, "");
    static_assert(sizeof(TOKSTR_CCSInstances) % sizeof(uint32_t) == 0, "");

    TOKSTR_CCSInstances Instances = {TOK_FS_GT_CCS_INFO__INSTANCES};
    TokenDword NumberOfCCSEnabled = {TOK_FBD_GT_CCS_INFO__NUMBER_OF_CCSENABLED};
    TokenBool IsValid = {TOK_FBB_GT_CCS_INFO__IS_VALID};
};
static_assert(std::is_standard_layout_v<TOKSTR_GT_CCS_INFO>, "");
static_assert(sizeof(TOKSTR_GT_CCS_INFO) % sizeof(uint32_t) == 0, "");

struct TOKSTR_GT_MULTI_TILE_ARCH_INFO {
    TokenVariableLength base;

    TOKSTR_GT_MULTI_TILE_ARCH_INFO(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR_GT_MULTI_TILE_ARCH_INFO, IsValid) + sizeof(IsValid) - offsetof(TOKSTR_GT_MULTI_TILE_ARCH_INFO, TileCount), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR_GT_MULTI_TILE_ARCH_INFO()
        : base(TOK_S_GT_MULTI_TILE_ARCH_INFO, 0, sizeof(*this) - sizeof(base)) {}

    struct TOKSTR_ANONYMOUS8856 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS8856(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS8856, TileMask) + sizeof(TileMask) - offsetof(TOKSTR_ANONYMOUS8856, Tile0), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS8856()
            : base(TOK_S_GT_MULTI_TILE_ARCH_INFO__ANONYMOUS8856, 0, sizeof(*this) - sizeof(base)) {}

        struct TOKSTR_ANONYMOUS8876 {
            TokenVariableLength base;

            TOKSTR_ANONYMOUS8876(uint16_t tokenId, uint32_t elementId = 0)
                : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS8876, Tile3) + sizeof(Tile3) - offsetof(TOKSTR_ANONYMOUS8876, Tile0), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

            TOKSTR_ANONYMOUS8876()
                : base(TOK_S_GT_MULTI_TILE_ARCH_INFO__ANONYMOUS8856__ANONYMOUS8876, 0, sizeof(*this) - sizeof(base)) {}

            TokenDword Tile0 = {TOK_FBC_GT_MULTI_TILE_ARCH_INFO__ANONYMOUS8856__ANONYMOUS8876__TILE0};
            TokenDword Tile1 = {TOK_FBC_GT_MULTI_TILE_ARCH_INFO__ANONYMOUS8856__ANONYMOUS8876__TILE1};
            TokenDword Tile2 = {TOK_FBC_GT_MULTI_TILE_ARCH_INFO__ANONYMOUS8856__ANONYMOUS8876__TILE2};
            TokenDword Tile3 = {TOK_FBC_GT_MULTI_TILE_ARCH_INFO__ANONYMOUS8856__ANONYMOUS8876__TILE3};
        };
        static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS8876>, "");
        static_assert(sizeof(TOKSTR_ANONYMOUS8876) % sizeof(uint32_t) == 0, "");

        TokenDword Tile0 = {TOK_FBC_GT_MULTI_TILE_ARCH_INFO__ANONYMOUS8856__ANONYMOUS8876__TILE0}; // Indirect field from anonymous struct
        TokenDword Tile1 = {TOK_FBC_GT_MULTI_TILE_ARCH_INFO__ANONYMOUS8856__ANONYMOUS8876__TILE1}; // Indirect field from anonymous struct
        TokenDword Tile2 = {TOK_FBC_GT_MULTI_TILE_ARCH_INFO__ANONYMOUS8856__ANONYMOUS8876__TILE2}; // Indirect field from anonymous struct
        TokenDword Tile3 = {TOK_FBC_GT_MULTI_TILE_ARCH_INFO__ANONYMOUS8856__ANONYMOUS8876__TILE3}; // Indirect field from anonymous struct
        TokenDword TileMask = {TOK_FBC_GT_MULTI_TILE_ARCH_INFO__ANONYMOUS8856__TILE_MASK};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS8856>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS8856) % sizeof(uint32_t) == 0, "");

    TokenDword TileCount = {TOK_FBC_GT_MULTI_TILE_ARCH_INFO__TILE_COUNT};
    TokenDword Tile0 = {TOK_FBC_GT_MULTI_TILE_ARCH_INFO__ANONYMOUS8856__ANONYMOUS8876__TILE0}; // Indirect field from anonymous struct
    TokenDword Tile1 = {TOK_FBC_GT_MULTI_TILE_ARCH_INFO__ANONYMOUS8856__ANONYMOUS8876__TILE1}; // Indirect field from anonymous struct
    TokenDword Tile2 = {TOK_FBC_GT_MULTI_TILE_ARCH_INFO__ANONYMOUS8856__ANONYMOUS8876__TILE2}; // Indirect field from anonymous struct
    TokenDword Tile3 = {TOK_FBC_GT_MULTI_TILE_ARCH_INFO__ANONYMOUS8856__ANONYMOUS8876__TILE3}; // Indirect field from anonymous struct
    TokenDword TileMask = {TOK_FBC_GT_MULTI_TILE_ARCH_INFO__ANONYMOUS8856__TILE_MASK};         // Indirect field from anonymous struct
    TokenBool IsValid = {TOK_FBB_GT_MULTI_TILE_ARCH_INFO__IS_VALID};
};
static_assert(std::is_standard_layout_v<TOKSTR_GT_MULTI_TILE_ARCH_INFO>, "");
static_assert(sizeof(TOKSTR_GT_MULTI_TILE_ARCH_INFO) % sizeof(uint32_t) == 0, "");

struct TOKSTR_GT_SQIDI_INFO {
    TokenVariableLength base;

    TOKSTR_GT_SQIDI_INFO(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR_GT_SQIDI_INFO, NumberofDoorbellPerSQIDI) + sizeof(NumberofDoorbellPerSQIDI) - offsetof(TOKSTR_GT_SQIDI_INFO, NumberofSQIDI), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR_GT_SQIDI_INFO()
        : base(TOK_S_GT_SQIDI_INFO, 0, sizeof(*this) - sizeof(base)) {}

    TokenDword NumberofSQIDI = {TOK_FBD_GT_SQIDI_INFO__NUMBEROF_SQIDI};
    TokenDword NumberofDoorbellPerSQIDI = {TOK_FBD_GT_SQIDI_INFO__NUMBEROF_DOORBELL_PER_SQIDI};
};
static_assert(std::is_standard_layout_v<TOKSTR_GT_SQIDI_INFO>, "");
static_assert(sizeof(TOKSTR_GT_SQIDI_INFO) % sizeof(uint32_t) == 0, "");

struct TOKSTR__GT_CACHE_TYPES {
    TokenVariableLength base;

    TOKSTR__GT_CACHE_TYPES(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR__GT_CACHE_TYPES, CacheTypeMask) + sizeof(CacheTypeMask) - offsetof(TOKSTR__GT_CACHE_TYPES, L3), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR__GT_CACHE_TYPES()
        : base(TOK_S_GT_CACHE_TYPES, 0, sizeof(*this) - sizeof(base)) {}

    struct TOKSTR_ANONYMOUS9544 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS9544(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS9544, eDRAM) + sizeof(eDRAM) - offsetof(TOKSTR_ANONYMOUS9544, L3), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS9544()
            : base(TOK_S_GT_CACHE_TYPES__ANONYMOUS9544, 0, sizeof(*this) - sizeof(base)) {}

        TokenDword L3 = {TOK_FBD_GT_CACHE_TYPES__ANONYMOUS9544__L3};
        TokenDword LLC = {TOK_FBD_GT_CACHE_TYPES__ANONYMOUS9544__LLC};
        TokenDword eDRAM = {TOK_FBD_GT_CACHE_TYPES__ANONYMOUS9544__E_DRAM};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS9544>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS9544) % sizeof(uint32_t) == 0, "");

    TokenDword L3 = {TOK_FBD_GT_CACHE_TYPES__ANONYMOUS9544__L3};        // Indirect field from anonymous struct
    TokenDword LLC = {TOK_FBD_GT_CACHE_TYPES__ANONYMOUS9544__LLC};      // Indirect field from anonymous struct
    TokenDword eDRAM = {TOK_FBD_GT_CACHE_TYPES__ANONYMOUS9544__E_DRAM}; // Indirect field from anonymous struct
    TokenDword CacheTypeMask = {TOK_FBD_GT_CACHE_TYPES__CACHE_TYPE_MASK};
};
static_assert(std::is_standard_layout_v<TOKSTR__GT_CACHE_TYPES>, "");
static_assert(sizeof(TOKSTR__GT_CACHE_TYPES) % sizeof(uint32_t) == 0, "");

struct TOKSTR_GT_SYSTEM_INFO {
    TokenVariableLength base;

    TOKSTR_GT_SYSTEM_INFO(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR_GT_SYSTEM_INFO, MaxVECS) + sizeof(MaxVECS) - offsetof(TOKSTR_GT_SYSTEM_INFO, EUCount), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR_GT_SYSTEM_INFO()
        : base(TOK_S_GT_SYSTEM_INFO, 0, sizeof(*this) - sizeof(base)) {}

    TokenDword EUCount = {TOK_FBD_GT_SYSTEM_INFO__EUCOUNT};
    TokenDword ThreadCount = {TOK_FBD_GT_SYSTEM_INFO__THREAD_COUNT};
    TokenDword SliceCount = {TOK_FBD_GT_SYSTEM_INFO__SLICE_COUNT};
    TokenDword SubSliceCount = {TOK_FBD_GT_SYSTEM_INFO__SUB_SLICE_COUNT};
    TokenDword DualSubSliceCount = {TOK_FBD_GT_SYSTEM_INFO__DUAL_SUB_SLICE_COUNT};
    TokenQword L3CacheSizeInKb = {TOK_FBQ_GT_SYSTEM_INFO__L3CACHE_SIZE_IN_KB};
    TokenQword LLCCacheSizeInKb = {TOK_FBQ_GT_SYSTEM_INFO__LLCCACHE_SIZE_IN_KB};
    TokenQword EdramSizeInKb = {TOK_FBQ_GT_SYSTEM_INFO__EDRAM_SIZE_IN_KB};
    TokenDword L3BankCount = {TOK_FBD_GT_SYSTEM_INFO__L3BANK_COUNT};
    TokenDword MaxFillRate = {TOK_FBD_GT_SYSTEM_INFO__MAX_FILL_RATE};
    TokenDword EuCountPerPoolMax = {TOK_FBD_GT_SYSTEM_INFO__EU_COUNT_PER_POOL_MAX};
    TokenDword EuCountPerPoolMin = {TOK_FBD_GT_SYSTEM_INFO__EU_COUNT_PER_POOL_MIN};
    TokenDword TotalVsThreads = {TOK_FBD_GT_SYSTEM_INFO__TOTAL_VS_THREADS};
    TokenDword TotalHsThreads = {TOK_FBD_GT_SYSTEM_INFO__TOTAL_HS_THREADS};
    TokenDword TotalDsThreads = {TOK_FBD_GT_SYSTEM_INFO__TOTAL_DS_THREADS};
    TokenDword TotalGsThreads = {TOK_FBD_GT_SYSTEM_INFO__TOTAL_GS_THREADS};
    TokenDword TotalPsThreadsWindowerRange = {TOK_FBD_GT_SYSTEM_INFO__TOTAL_PS_THREADS_WINDOWER_RANGE};
    TokenDword TotalVsThreads_Pocs = {TOK_FBD_GT_SYSTEM_INFO__TOTAL_VS_THREADS_POCS};
    TokenDword CsrSizeInMb = {TOK_FBD_GT_SYSTEM_INFO__CSR_SIZE_IN_MB};
    TokenDword MaxEuPerSubSlice = {TOK_FBD_GT_SYSTEM_INFO__MAX_EU_PER_SUB_SLICE};
    TokenDword MaxSlicesSupported = {TOK_FBD_GT_SYSTEM_INFO__MAX_SLICES_SUPPORTED};
    TokenDword MaxSubSlicesSupported = {TOK_FBD_GT_SYSTEM_INFO__MAX_SUB_SLICES_SUPPORTED};
    TokenDword MaxDualSubSlicesSupported = {TOK_FBD_GT_SYSTEM_INFO__MAX_DUAL_SUB_SLICES_SUPPORTED};
    TokenBool IsL3HashModeEnabled = {TOK_FBB_GT_SYSTEM_INFO__IS_L3HASH_MODE_ENABLED};
    TOKSTR_GT_SLICE_INFO SliceInfo[10] = {{TOK_FS_GT_SYSTEM_INFO__SLICE_INFO, 0}, {TOK_FS_GT_SYSTEM_INFO__SLICE_INFO, 1}, {TOK_FS_GT_SYSTEM_INFO__SLICE_INFO, 2}, {TOK_FS_GT_SYSTEM_INFO__SLICE_INFO, 3}, {TOK_FS_GT_SYSTEM_INFO__SLICE_INFO, 4}, {TOK_FS_GT_SYSTEM_INFO__SLICE_INFO, 5}, {TOK_FS_GT_SYSTEM_INFO__SLICE_INFO, 6}, {TOK_FS_GT_SYSTEM_INFO__SLICE_INFO, 7}, {TOK_FS_GT_SYSTEM_INFO__SLICE_INFO, 8}, {TOK_FS_GT_SYSTEM_INFO__SLICE_INFO, 9}};
    TokenBool IsDynamicallyPopulated = {TOK_FBB_GT_SYSTEM_INFO__IS_DYNAMICALLY_POPULATED};
    TOKSTR_GT_SQIDI_INFO SqidiInfo = {TOK_FS_GT_SYSTEM_INFO__SQIDI_INFO};
    TokenDword ReservedCCSWays = {TOK_FBD_GT_SYSTEM_INFO__RESERVED_CCSWAYS};
    TOKSTR_GT_CCS_INFO CCSInfo = {TOK_FS_GT_SYSTEM_INFO__CCSINFO};
    TOKSTR_GT_MULTI_TILE_ARCH_INFO MultiTileArchInfo = {TOK_FS_GT_SYSTEM_INFO__MULTI_TILE_ARCH_INFO};
    TOKSTR_GT_VDBOX_INFO VDBoxInfo = {TOK_FS_GT_SYSTEM_INFO__VDBOX_INFO};
    TOKSTR_GT_VEBOX_INFO VEBoxInfo = {TOK_FS_GT_SYSTEM_INFO__VEBOX_INFO};
    TokenDword NumThreadsPerEu = {TOK_FBD_GT_SYSTEM_INFO__NUM_THREADS_PER_EU};
    TOKSTR__GT_CACHE_TYPES CacheTypes = {TOK_FS_GT_SYSTEM_INFO__CACHE_TYPES};
    TokenDword MaxVECS = {TOK_FBD_GT_SYSTEM_INFO__MAX_VECS};
};
static_assert(std::is_standard_layout_v<TOKSTR_GT_SYSTEM_INFO>, "");
static_assert(sizeof(TOKSTR_GT_SYSTEM_INFO) % sizeof(uint32_t) == 0, "");

struct TOKSTR__SKU_FEATURE_TABLE {
    TokenVariableLength base;

    TOKSTR__SKU_FEATURE_TABLE(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR__SKU_FEATURE_TABLE, FtrAssignedGpuTile) + sizeof(FtrAssignedGpuTile) - offsetof(TOKSTR__SKU_FEATURE_TABLE, FtrULT), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR__SKU_FEATURE_TABLE()
        : base(TOK_S_SKU_FEATURE_TABLE, 0, sizeof(*this) - sizeof(base)) {}

    struct TOKSTR_ANONYMOUS3245 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS3245(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS3245, FtrCCSMultiInstance) + sizeof(FtrCCSMultiInstance) - offsetof(TOKSTR_ANONYMOUS3245, FtrULT), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS3245()
            : base(TOK_S_SKU_FEATURE_TABLE__ANONYMOUS3245, 0, sizeof(*this) - sizeof(base)) {}

        TokenDword FtrULT = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS3245__FTR_ULT};
        TokenDword FtrLCIA = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS3245__FTR_LCIA};
        TokenDword FtrCCSRing = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS3245__FTR_CCSRING};
        TokenDword FtrCCSNode = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS3245__FTR_CCSNODE};
        TokenDword FtrCCSMultiInstance = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS3245__FTR_CCSMULTI_INSTANCE};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS3245>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS3245) % sizeof(uint32_t) == 0, "");

    struct TOKSTR_ANONYMOUS11155 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS11155(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS11155, FtrDisplayDisabled) + sizeof(FtrDisplayDisabled) - offsetof(TOKSTR_ANONYMOUS11155, FtrDisplayDisabled), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS11155()
            : base(TOK_S_SKU_FEATURE_TABLE__ANONYMOUS11155, 0, sizeof(*this) - sizeof(base)) {}

        TokenDword FtrDisplayDisabled = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS11155__FTR_DISPLAY_DISABLED};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS11155>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS11155) % sizeof(uint32_t) == 0, "");

    struct TOKSTR_ANONYMOUS21584 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS21584(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS21584, FtrPooledEuEnabled) + sizeof(FtrPooledEuEnabled) - offsetof(TOKSTR_ANONYMOUS21584, FtrPooledEuEnabled), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS21584()
            : base(TOK_S_SKU_FEATURE_TABLE__ANONYMOUS21584, 0, sizeof(*this) - sizeof(base)) {}

        TokenDword FtrPooledEuEnabled = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21584__FTR_POOLED_EU_ENABLED};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS21584>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS21584) % sizeof(uint32_t) == 0, "");

    struct TOKSTR_ANONYMOUS21990 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS21990(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS21990, FtrDriverFLR) + sizeof(FtrDriverFLR) - offsetof(TOKSTR_ANONYMOUS21990, FtrGpGpuMidBatchPreempt), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS21990()
            : base(TOK_S_SKU_FEATURE_TABLE__ANONYMOUS21990, 0, sizeof(*this) - sizeof(base)) {}

        TokenDword FtrGpGpuMidBatchPreempt = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_GP_GPU_MID_BATCH_PREEMPT};
        TokenDword FtrGpGpuThreadGroupLevelPreempt = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_GP_GPU_THREAD_GROUP_LEVEL_PREEMPT};
        TokenDword FtrGpGpuMidThreadLevelPreempt = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_GP_GPU_MID_THREAD_LEVEL_PREEMPT};
        TokenDword FtrPPGTT = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_PPGTT};
        TokenDword FtrIA32eGfxPTEs = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_IA32E_GFX_PTES};
        TokenDword FtrMemTypeMocsDeferPAT = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_MEM_TYPE_MOCS_DEFER_PAT};
        TokenDword FtrPml4Support = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_PML4SUPPORT};
        TokenDword FtrSVM = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_SVM};
        TokenDword FtrTileMappedResource = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_TILE_MAPPED_RESOURCE};
        TokenDword FtrTranslationTable = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_TRANSLATION_TABLE};
        TokenDword FtrUserModeTranslationTable = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_USER_MODE_TRANSLATION_TABLE};
        TokenDword FtrNullPages = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_NULL_PAGES};
        TokenDword FtrL3IACoherency = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_L3IACOHERENCY};
        TokenDword FtrEDram = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_EDRAM};
        TokenDword FtrLLCBypass = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_LLCBYPASS};
        TokenDword FtrCentralCachePolicy = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_CENTRAL_CACHE_POLICY};
        TokenDword FtrWddm2GpuMmu = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_WDDM2GPU_MMU};
        TokenDword FtrWddm2Svm = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_WDDM2SVM};
        TokenDword FtrStandardMipTailFormat = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_STANDARD_MIP_TAIL_FORMAT};
        TokenDword FtrWddm2_1_64kbPages = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_WDDM2_1_64KB_PAGES};
        TokenDword FtrE2ECompression = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_E2ECOMPRESSION};
        TokenDword FtrLinearCCS = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_LINEAR_CCS};
        TokenDword FtrLocalMemory = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_LOCAL_MEMORY};
        TokenDword FtrPpgtt64KBWalkOptimization = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_PPGTT64KBWALK_OPTIMIZATION};
        TokenDword FtrTileY = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_TILE_Y};
        TokenDword FtrFlatPhysCCS = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_FLAT_PHYS_CCS};
        TokenDword FtrMultiTileArch = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_MULTI_TILE_ARCH};
        TokenDword FtrLocalMemoryAllows4KB = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_LOCAL_MEMORY_ALLOWS4KB};
        TokenDword FtrDisplayXTiling = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_DISPLAY_XTILING};
        TokenDword FtrCameraCaptureCaching = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_CAMERA_CAPTURE_CACHING};
        TokenDword FtrKmdDaf = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_KMD_DAF};
        TokenDword FtrFrameBufferLLC = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_FRAME_BUFFER_LLC};
        TokenDword FtrDriverFLR = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_DRIVER_FLR};
        TokenDword FtrHwScheduling = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_HW_SCHEDULING};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS21990>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS21990) % sizeof(uint32_t) == 0, "");

    struct TOKSTR_ANONYMOUS37751 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS37751(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS37751, FtrAstc3D) + sizeof(FtrAstc3D) - offsetof(TOKSTR_ANONYMOUS37751, FtrAstcLdr2D), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS37751()
            : base(TOK_S_SKU_FEATURE_TABLE__ANONYMOUS37751, 0, sizeof(*this) - sizeof(base)) {}

        TokenDword FtrAstcLdr2D = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS37751__FTR_ASTC_LDR2D};
        TokenDword FtrAstcHdr2D = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS37751__FTR_ASTC_HDR2D};
        TokenDword FtrAstc3D = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS37751__FTR_ASTC3D};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS37751>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS37751) % sizeof(uint32_t) == 0, "");

    struct TOKSTR_ANONYMOUS54736 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS54736(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS54736, FtrDisplayYTiling) + sizeof(FtrDisplayYTiling) - offsetof(TOKSTR_ANONYMOUS54736, FtrRendComp), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS54736()
            : base(TOK_S_SKU_FEATURE_TABLE__ANONYMOUS54736, 0, sizeof(*this) - sizeof(base)) {}

        TokenDword FtrRendComp = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS54736__FTR_REND_COMP};
        TokenDword FtrDisplayYTiling = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS54736__FTR_DISPLAY_YTILING};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS54736>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS54736) % sizeof(uint32_t) == 0, "");

    struct TOKSTR_ANONYMOUS66219 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS66219(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS66219, FtrDisplayEngineS3d) + sizeof(FtrDisplayEngineS3d) - offsetof(TOKSTR_ANONYMOUS66219, FtrS3D), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS66219()
            : base(TOK_S_SKU_FEATURE_TABLE__ANONYMOUS66219, 0, sizeof(*this) - sizeof(base)) {}

        TokenDword FtrS3D = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS66219__FTR_S3D};
        TokenDword FtrDisplayEngineS3d = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS66219__FTR_DISPLAY_ENGINE_S3D};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS66219>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS66219) % sizeof(uint32_t) == 0, "");

    struct TOKSTR_ANONYMOUS89755 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS89755(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS89755, FtrVgt) + sizeof(FtrVgt) - offsetof(TOKSTR_ANONYMOUS89755, FtrVgt), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS89755()
            : base(TOK_S_SKU_FEATURE_TABLE__ANONYMOUS89755, 0, sizeof(*this) - sizeof(base)) {}

        TokenDword FtrVgt = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS89755__FTR_VGT};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS89755>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS89755) % sizeof(uint32_t) == 0, "");

    struct TOKSTR_ANONYMOUS91822 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS91822(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS91822, FtrAssignedGpuTile) + sizeof(FtrAssignedGpuTile) - offsetof(TOKSTR_ANONYMOUS91822, FtrAssignedGpuTile), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS91822()
            : base(TOK_S_SKU_FEATURE_TABLE__ANONYMOUS91822, 0, sizeof(*this) - sizeof(base)) {}

        TokenDword FtrAssignedGpuTile = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS91822__FTR_ASSIGNED_GPU_TILE};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS91822>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS91822) % sizeof(uint32_t) == 0, "");

    TokenDword FtrULT = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS3245__FTR_ULT};                                                         // Indirect field from anonymous struct
    TokenDword FtrLCIA = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS3245__FTR_LCIA};                                                       // Indirect field from anonymous struct
    TokenDword FtrCCSRing = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS3245__FTR_CCSRING};                                                 // Indirect field from anonymous struct
    TokenDword FtrCCSNode = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS3245__FTR_CCSNODE};                                                 // Indirect field from anonymous struct
    TokenDword FtrCCSMultiInstance = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS3245__FTR_CCSMULTI_INSTANCE};                              // Indirect field from anonymous struct
    TokenDword FtrDisplayDisabled = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS11155__FTR_DISPLAY_DISABLED};                               // Indirect field from anonymous struct
    TokenDword FtrPooledEuEnabled = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21584__FTR_POOLED_EU_ENABLED};                              // Indirect field from anonymous struct
    TokenDword FtrGpGpuMidBatchPreempt = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_GP_GPU_MID_BATCH_PREEMPT};                  // Indirect field from anonymous struct
    TokenDword FtrGpGpuThreadGroupLevelPreempt = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_GP_GPU_THREAD_GROUP_LEVEL_PREEMPT}; // Indirect field from anonymous struct
    TokenDword FtrGpGpuMidThreadLevelPreempt = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_GP_GPU_MID_THREAD_LEVEL_PREEMPT};     // Indirect field from anonymous struct
    TokenDword FtrPPGTT = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_PPGTT};                                                    // Indirect field from anonymous struct
    TokenDword FtrIA32eGfxPTEs = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_IA32E_GFX_PTES};                                    // Indirect field from anonymous struct
    TokenDword FtrMemTypeMocsDeferPAT = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_MEM_TYPE_MOCS_DEFER_PAT};                    // Indirect field from anonymous struct
    TokenDword FtrPml4Support = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_PML4SUPPORT};                                        // Indirect field from anonymous struct
    TokenDword FtrSVM = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_SVM};                                                        // Indirect field from anonymous struct
    TokenDword FtrTileMappedResource = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_TILE_MAPPED_RESOURCE};                        // Indirect field from anonymous struct
    TokenDword FtrTranslationTable = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_TRANSLATION_TABLE};                             // Indirect field from anonymous struct
    TokenDword FtrUserModeTranslationTable = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_USER_MODE_TRANSLATION_TABLE};           // Indirect field from anonymous struct
    TokenDword FtrNullPages = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_NULL_PAGES};                                           // Indirect field from anonymous struct
    TokenDword FtrL3IACoherency = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_L3IACOHERENCY};                                    // Indirect field from anonymous struct
    TokenDword FtrEDram = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_EDRAM};                                                    // Indirect field from anonymous struct
    TokenDword FtrLLCBypass = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_LLCBYPASS};                                            // Indirect field from anonymous struct
    TokenDword FtrCentralCachePolicy = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_CENTRAL_CACHE_POLICY};                        // Indirect field from anonymous struct
    TokenDword FtrWddm2GpuMmu = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_WDDM2GPU_MMU};                                       // Indirect field from anonymous struct
    TokenDword FtrWddm2Svm = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_WDDM2SVM};                                              // Indirect field from anonymous struct
    TokenDword FtrStandardMipTailFormat = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_STANDARD_MIP_TAIL_FORMAT};                 // Indirect field from anonymous struct
    TokenDword FtrWddm2_1_64kbPages = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_WDDM2_1_64KB_PAGES};                           // Indirect field from anonymous struct
    TokenDword FtrE2ECompression = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_E2ECOMPRESSION};                                  // Indirect field from anonymous struct
    TokenDword FtrLinearCCS = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_LINEAR_CCS};                                           // Indirect field from anonymous struct
    TokenDword FtrLocalMemory = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_LOCAL_MEMORY};                                       // Indirect field from anonymous struct
    TokenDword FtrPpgtt64KBWalkOptimization = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_PPGTT64KBWALK_OPTIMIZATION};           // Indirect field from anonymous struct
    TokenDword FtrTileY = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_TILE_Y};                                                   // Indirect field from anonymous struct
    TokenDword FtrFlatPhysCCS = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_FLAT_PHYS_CCS};                                      // Indirect field from anonymous struct
    TokenDword FtrMultiTileArch = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_MULTI_TILE_ARCH};                                  // Indirect field from anonymous struct
    TokenDword FtrLocalMemoryAllows4KB = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_LOCAL_MEMORY_ALLOWS4KB};                    // Indirect field from anonymous struct
    TokenDword FtrDisplayXTiling = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_DISPLAY_XTILING};                                 // Indirect field from anonymous struct
    TokenDword FtrCameraCaptureCaching = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_CAMERA_CAPTURE_CACHING};                    // Indirect field from anonymous struct
    TokenDword FtrKmdDaf = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_KMD_DAF};                                                 // Indirect field from anonymous struct
    TokenDword FtrFrameBufferLLC = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_FRAME_BUFFER_LLC};                                // Indirect field from anonymous struct
    TokenDword FtrDriverFLR = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_DRIVER_FLR};                                           // Indirect field from anonymous struct
    TokenDword FtrHwScheduling = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_HW_SCHEDULING};                                     // Indirect field from anonymous struct
    TokenDword FtrAstcLdr2D = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS37751__FTR_ASTC_LDR2D};                                           // Indirect field from anonymous struct
    TokenDword FtrAstcHdr2D = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS37751__FTR_ASTC_HDR2D};                                           // Indirect field from anonymous struct
    TokenDword FtrAstc3D = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS37751__FTR_ASTC3D};                                                  // Indirect field from anonymous struct
    TokenDword FtrFbc = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS42853__FTR_FBC};                                                        // Indirect field from anonymous struct
    TokenDword FtrRendComp = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS54736__FTR_REND_COMP};                                             // Indirect field from anonymous struct
    TokenDword FtrDisplayYTiling = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS54736__FTR_DISPLAY_YTILING};                                 // Indirect field from anonymous struct
    TokenDword FtrS3D = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS66219__FTR_S3D};                                                        // Indirect field from anonymous struct
    TokenDword FtrDisplayEngineS3d = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS66219__FTR_DISPLAY_ENGINE_S3D};                            // Indirect field from anonymous struct
    TokenDword FtrVgt = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS89755__FTR_VGT};                                                        // Indirect field from anonymous struct
    TokenDword FtrAssignedGpuTile = {TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS91822__FTR_ASSIGNED_GPU_TILE};                              // Indirect field from anonymous struct
};
static_assert(std::is_standard_layout_v<TOKSTR__SKU_FEATURE_TABLE>, "");
static_assert(sizeof(TOKSTR__SKU_FEATURE_TABLE) % sizeof(uint32_t) == 0, "");

struct TOKSTR__WA_TABLE {
    TokenVariableLength base;

    TOKSTR__WA_TABLE(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR__WA_TABLE, Wa32bppTileY2DColorNoHAlign4) + sizeof(Wa32bppTileY2DColorNoHAlign4) - offsetof(TOKSTR__WA_TABLE, WaAlignIndexBuffer), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR__WA_TABLE()
        : base(TOK_S_WA_TABLE, 0, sizeof(*this) - sizeof(base)) {}

    TokenDword WaAlignIndexBuffer = {TOK_FBD_WA_TABLE__WA_ALIGN_INDEX_BUFFER};
    TokenDword WaSendMIFLUSHBeforeVFE = {TOK_FBD_WA_TABLE__WA_SEND_MIFLUSHBEFORE_VFE};
    TokenDword WaDisablePerCtxtPreemptionGranularityControl = {TOK_FBD_WA_TABLE__WA_DISABLE_PER_CTXT_PREEMPTION_GRANULARITY_CONTROL};
    TokenDword WaDisableLSQCROPERFforOCL = {TOK_FBD_WA_TABLE__WA_DISABLE_LSQCROPERFFOR_OCL};
    TokenDword WaValign2For96bppFormats = {TOK_FBD_WA_TABLE__WA_VALIGN2FOR96BPP_FORMATS};
    TokenDword WaValign2ForR8G8B8UINTFormat = {TOK_FBD_WA_TABLE__WA_VALIGN2FOR_R8G8B8UINTFORMAT};
    TokenDword WaCSRUncachable = {TOK_FBD_WA_TABLE__WA_CSRUNCACHABLE};
    TokenDword WaDisableFusedThreadScheduling = {TOK_FBD_WA_TABLE__WA_DISABLE_FUSED_THREAD_SCHEDULING};
    TokenDword WaModifyVFEStateAfterGPGPUPreemption = {TOK_FBD_WA_TABLE__WA_MODIFY_VFESTATE_AFTER_GPGPUPREEMPTION};
    TokenDword WaCursor16K = {TOK_FBD_WA_TABLE__WA_CURSOR16K};
    TokenDword Wa8kAlignforAsyncFlip = {TOK_FBD_WA_TABLE__WA8K_ALIGNFOR_ASYNC_FLIP};
    TokenDword Wa29BitDisplayAddrLimit = {TOK_FBD_WA_TABLE__WA29BIT_DISPLAY_ADDR_LIMIT};
    TokenDword WaAlignContextImage = {TOK_FBD_WA_TABLE__WA_ALIGN_CONTEXT_IMAGE};
    TokenDword WaForceGlobalGTT = {TOK_FBD_WA_TABLE__WA_FORCE_GLOBAL_GTT};
    TokenDword WaReportPerfCountForceGlobalGTT = {TOK_FBD_WA_TABLE__WA_REPORT_PERF_COUNT_FORCE_GLOBAL_GTT};
    TokenDword WaOaAddressTranslation = {TOK_FBD_WA_TABLE__WA_OA_ADDRESS_TRANSLATION};
    TokenDword Wa2RowVerticalAlignment = {TOK_FBD_WA_TABLE__WA2ROW_VERTICAL_ALIGNMENT};
    TokenDword WaPpgttAliasGlobalGttSpace = {TOK_FBD_WA_TABLE__WA_PPGTT_ALIAS_GLOBAL_GTT_SPACE};
    TokenDword WaClearFenceRegistersAtDriverInit = {TOK_FBD_WA_TABLE__WA_CLEAR_FENCE_REGISTERS_AT_DRIVER_INIT};
    TokenDword WaRestrictPitch128KB = {TOK_FBD_WA_TABLE__WA_RESTRICT_PITCH128KB};
    TokenDword WaAvoidLLC = {TOK_FBD_WA_TABLE__WA_AVOID_LLC};
    TokenDword WaAvoidL3 = {TOK_FBD_WA_TABLE__WA_AVOID_L3};
    TokenDword Wa16TileFencesOnly = {TOK_FBD_WA_TABLE__WA16TILE_FENCES_ONLY};
    TokenDword Wa16MBOABufferAlignment = {TOK_FBD_WA_TABLE__WA16MBOABUFFER_ALIGNMENT};
    TokenDword WaTranslationTableUnavailable = {TOK_FBD_WA_TABLE__WA_TRANSLATION_TABLE_UNAVAILABLE};
    TokenDword WaNoMinimizedTrivialSurfacePadding = {TOK_FBD_WA_TABLE__WA_NO_MINIMIZED_TRIVIAL_SURFACE_PADDING};
    TokenDword WaNoBufferSamplerPadding = {TOK_FBD_WA_TABLE__WA_NO_BUFFER_SAMPLER_PADDING};
    TokenDword WaSurfaceStatePlanarYOffsetAlignBy2 = {TOK_FBD_WA_TABLE__WA_SURFACE_STATE_PLANAR_YOFFSET_ALIGN_BY2};
    TokenDword WaGttCachingOffByDefault = {TOK_FBD_WA_TABLE__WA_GTT_CACHING_OFF_BY_DEFAULT};
    TokenDword WaTouchAllSvmMemory = {TOK_FBD_WA_TABLE__WA_TOUCH_ALL_SVM_MEMORY};
    TokenDword WaIOBAddressMustBeValidInHwContext = {TOK_FBD_WA_TABLE__WA_IOBADDRESS_MUST_BE_VALID_IN_HW_CONTEXT};
    TokenDword WaFlushTlbAfterCpuGgttWrites = {TOK_FBD_WA_TABLE__WA_FLUSH_TLB_AFTER_CPU_GGTT_WRITES};
    TokenDword WaMsaa8xTileYDepthPitchAlignment = {TOK_FBD_WA_TABLE__WA_MSAA8X_TILE_YDEPTH_PITCH_ALIGNMENT};
    TokenDword WaDisableNullPageAsDummy = {TOK_FBD_WA_TABLE__WA_DISABLE_NULL_PAGE_AS_DUMMY};
    TokenDword WaUseVAlign16OnTileXYBpp816 = {TOK_FBD_WA_TABLE__WA_USE_VALIGN16ON_TILE_XYBPP816};
    TokenDword WaGttPat0 = {TOK_FBD_WA_TABLE__WA_GTT_PAT0};
    TokenDword WaGttPat0WB = {TOK_FBD_WA_TABLE__WA_GTT_PAT0WB};
    TokenDword WaMemTypeIsMaxOfPatAndMocs = {TOK_FBD_WA_TABLE__WA_MEM_TYPE_IS_MAX_OF_PAT_AND_MOCS};
    TokenDword WaGttPat0GttWbOverOsIommuEllcOnly = {TOK_FBD_WA_TABLE__WA_GTT_PAT0GTT_WB_OVER_OS_IOMMU_ELLC_ONLY};
    TokenDword WaAddDummyPageForDisplayPrefetch = {TOK_FBD_WA_TABLE__WA_ADD_DUMMY_PAGE_FOR_DISPLAY_PREFETCH};
    TokenDword WaLLCCachingUnsupported = {TOK_FBD_WA_TABLE__WA_LLCCACHING_UNSUPPORTED};
    TokenDword WaDoubleFastClearWidthAlignment = {TOK_FBD_WA_TABLE__WA_DOUBLE_FAST_CLEAR_WIDTH_ALIGNMENT};
    TokenDword WaCompressedResourceRequiresConstVA21 = {TOK_FBD_WA_TABLE__WA_COMPRESSED_RESOURCE_REQUIRES_CONST_VA21};
    TokenDword WaDisregardPlatformChecks = {TOK_FBD_WA_TABLE__WA_DISREGARD_PLATFORM_CHECKS};
    TokenDword WaLosslessCompressionSurfaceStride = {TOK_FBD_WA_TABLE__WA_LOSSLESS_COMPRESSION_SURFACE_STRIDE};
    TokenDword WaFbcLinearSurfaceStride = {TOK_FBD_WA_TABLE__WA_FBC_LINEAR_SURFACE_STRIDE};
    TokenDword Wa4kAlignUVOffsetNV12LinearSurface = {TOK_FBD_WA_TABLE__WA4K_ALIGN_UVOFFSET_NV12LINEAR_SURFACE};
    TokenDword WaAstcCorruptionForOddCompressedBlockSizeX = {TOK_FBD_WA_TABLE__WA_ASTC_CORRUPTION_FOR_ODD_COMPRESSED_BLOCK_SIZE_X};
    TokenDword WaAuxTable16KGranular = {TOK_FBD_WA_TABLE__WA_AUX_TABLE16KGRANULAR};
    TokenDword WaEncryptedEdramOnlyPartials = {TOK_FBD_WA_TABLE__WA_ENCRYPTED_EDRAM_ONLY_PARTIALS};
    TokenDword WaDisableEdramForDisplayRT = {TOK_FBD_WA_TABLE__WA_DISABLE_EDRAM_FOR_DISPLAY_RT};
    TokenDword WaLimit128BMediaCompr = {TOK_FBD_WA_TABLE__WA_LIMIT128BMEDIA_COMPR};
    TokenDword WaUntypedBufferCompression = {TOK_FBD_WA_TABLE__WA_UNTYPED_BUFFER_COMPRESSION};
    TokenDword WaSamplerCacheFlushBetweenRedescribedSurfaceReads = {TOK_FBD_WA_TABLE__WA_SAMPLER_CACHE_FLUSH_BETWEEN_REDESCRIBED_SURFACE_READS};
    TokenDword WaAlignYUVResourceToLCU = {TOK_FBD_WA_TABLE__WA_ALIGN_YUVRESOURCE_TO_LCU};
    TokenDword Wa32bppTileY2DColorNoHAlign4 = {TOK_FBD_WA_TABLE__WA32BPP_TILE_Y2DCOLOR_NO_HALIGN4};
};
static_assert(std::is_standard_layout_v<TOKSTR__WA_TABLE>, "");
static_assert(sizeof(TOKSTR__WA_TABLE) % sizeof(uint32_t) == 0, "");

struct TOKSTR_PLATFORM_STR {
    TokenVariableLength base;

    TOKSTR_PLATFORM_STR(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR_PLATFORM_STR, eGTType) + sizeof(eGTType) - offsetof(TOKSTR_PLATFORM_STR, eProductFamily), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR_PLATFORM_STR()
        : base(TOK_S_PLATFORM_STR, 0, sizeof(*this) - sizeof(base)) {}

    TokenDword eProductFamily = {TOK_FE_PLATFORM_STR__E_PRODUCT_FAMILY};
    TokenDword ePCHProductFamily = {TOK_FE_PLATFORM_STR__E_PCHPRODUCT_FAMILY};
    TokenDword eDisplayCoreFamily = {TOK_FE_PLATFORM_STR__E_DISPLAY_CORE_FAMILY};
    TokenDword eRenderCoreFamily = {TOK_FE_PLATFORM_STR__E_RENDER_CORE_FAMILY};
    TokenDword ePlatformType = {TOK_FE_PLATFORM_STR__E_PLATFORM_TYPE};
    TokenDword usDeviceID = {TOK_FBW_PLATFORM_STR__US_DEVICE_ID};
    TokenDword usRevId = {TOK_FBW_PLATFORM_STR__US_REV_ID};
    TokenDword usDeviceID_PCH = {TOK_FBW_PLATFORM_STR__US_DEVICE_ID_PCH};
    TokenDword usRevId_PCH = {TOK_FBW_PLATFORM_STR__US_REV_ID_PCH};
    TokenDword eGTType = {TOK_FE_PLATFORM_STR__E_GTTYPE};
};
static_assert(std::is_standard_layout_v<TOKSTR_PLATFORM_STR>, "");
static_assert(sizeof(TOKSTR_PLATFORM_STR) % sizeof(uint32_t) == 0, "");

struct TOKSTR___KMD_CAPS_INFO {
    TokenVariableLength base;

    TOKSTR___KMD_CAPS_INFO(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR___KMD_CAPS_INFO, DriverStoreEnabled) + sizeof(DriverStoreEnabled) - offsetof(TOKSTR___KMD_CAPS_INFO, Gamma_Rgb256x3x16), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR___KMD_CAPS_INFO()
        : base(TOK_S_KMD_CAPS_INFO, 0, sizeof(*this) - sizeof(base)) {}

    TokenDword Gamma_Rgb256x3x16 = {TOK_FBD_KMD_CAPS_INFO__GAMMA_RGB256X3X16};
    TokenDword GDIAcceleration = {TOK_FBD_KMD_CAPS_INFO__GDIACCELERATION};
    TokenDword OsManagedHwContext = {TOK_FBD_KMD_CAPS_INFO__OS_MANAGED_HW_CONTEXT};
    TokenDword GraphicsPreemptionGranularity = {TOK_FBD_KMD_CAPS_INFO__GRAPHICS_PREEMPTION_GRANULARITY};
    TokenDword ComputePreemptionGranularity = {TOK_FBD_KMD_CAPS_INFO__COMPUTE_PREEMPTION_GRANULARITY};
    TokenDword InstrumentationIsEnabled = {TOK_FBD_KMD_CAPS_INFO__INSTRUMENTATION_IS_ENABLED};
    TokenDword DriverStoreEnabled = {TOK_FBD_KMD_CAPS_INFO__DRIVER_STORE_ENABLED};
};
static_assert(std::is_standard_layout_v<TOKSTR___KMD_CAPS_INFO>, "");
static_assert(sizeof(TOKSTR___KMD_CAPS_INFO) % sizeof(uint32_t) == 0, "");

struct TOKSTR___KMD_OVERLAY_OVERRIDE {
    TokenVariableLength base;

    TOKSTR___KMD_OVERLAY_OVERRIDE(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR___KMD_OVERLAY_OVERRIDE, YUY2Overlay) + sizeof(YUY2Overlay) - offsetof(TOKSTR___KMD_OVERLAY_OVERRIDE, OverrideOverlayCaps), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR___KMD_OVERLAY_OVERRIDE()
        : base(TOK_S_KMD_OVERLAY_OVERRIDE, 0, sizeof(*this) - sizeof(base)) {}

    TokenDword OverrideOverlayCaps = {TOK_FBD_KMD_OVERLAY_OVERRIDE__OVERRIDE_OVERLAY_CAPS};
    TokenDword RGBOverlay = {TOK_FBD_KMD_OVERLAY_OVERRIDE__RGBOVERLAY};
    TokenDword YUY2Overlay = {TOK_FBD_KMD_OVERLAY_OVERRIDE__YUY2OVERLAY};
};
static_assert(std::is_standard_layout_v<TOKSTR___KMD_OVERLAY_OVERRIDE>, "");
static_assert(sizeof(TOKSTR___KMD_OVERLAY_OVERRIDE) % sizeof(uint32_t) == 0, "");

struct TOKSTR___KMD_OVERLAY_CAPS_INFO {
    TokenVariableLength base;

    TOKSTR___KMD_OVERLAY_CAPS_INFO(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR___KMD_OVERLAY_CAPS_INFO, MaxHWScalerStride) + sizeof(MaxHWScalerStride) - offsetof(TOKSTR___KMD_OVERLAY_CAPS_INFO, Caps), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR___KMD_OVERLAY_CAPS_INFO()
        : base(TOK_S_KMD_OVERLAY_CAPS_INFO, 0, sizeof(*this) - sizeof(base)) {}

    struct TOKSTR_ANONYMOUS5171 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS5171(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS5171, CapsValue) + sizeof(CapsValue) - offsetof(TOKSTR_ANONYMOUS5171, Caps), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS5171()
            : base(TOK_S_KMD_OVERLAY_CAPS_INFO__ANONYMOUS5171, 0, sizeof(*this) - sizeof(base)) {}

        struct TOKSTR_ANONYMOUS5191 {
            TokenVariableLength base;

            TOKSTR_ANONYMOUS5191(uint16_t tokenId, uint32_t elementId = 0)
                : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS5191, StretchY) + sizeof(StretchY) - offsetof(TOKSTR_ANONYMOUS5191, FullRangeRGB), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

            TOKSTR_ANONYMOUS5191()
                : base(TOK_S_KMD_OVERLAY_CAPS_INFO__ANONYMOUS5171__ANONYMOUS5191, 0, sizeof(*this) - sizeof(base)) {}

            TokenDword FullRangeRGB = {TOK_FBD_KMD_OVERLAY_CAPS_INFO__ANONYMOUS5171__ANONYMOUS5191__FULL_RANGE_RGB};
            TokenDword LimitedRangeRGB = {TOK_FBD_KMD_OVERLAY_CAPS_INFO__ANONYMOUS5171__ANONYMOUS5191__LIMITED_RANGE_RGB};
            TokenDword YCbCr_BT601 = {TOK_FBD_KMD_OVERLAY_CAPS_INFO__ANONYMOUS5171__ANONYMOUS5191__YCB_CR_BT601};
            TokenDword YCbCr_BT709 = {TOK_FBD_KMD_OVERLAY_CAPS_INFO__ANONYMOUS5171__ANONYMOUS5191__YCB_CR_BT709};
            TokenDword YCbCr_BT601_xvYCC = {TOK_FBD_KMD_OVERLAY_CAPS_INFO__ANONYMOUS5171__ANONYMOUS5191__YCB_CR_BT601_XV_YCC};
            TokenDword YCbCr_BT709_xvYCC = {TOK_FBD_KMD_OVERLAY_CAPS_INFO__ANONYMOUS5171__ANONYMOUS5191__YCB_CR_BT709_XV_YCC};
            TokenDword StretchX = {TOK_FBD_KMD_OVERLAY_CAPS_INFO__ANONYMOUS5171__ANONYMOUS5191__STRETCH_X};
            TokenDword StretchY = {TOK_FBD_KMD_OVERLAY_CAPS_INFO__ANONYMOUS5171__ANONYMOUS5191__STRETCH_Y};
        };
        static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS5191>, "");
        static_assert(sizeof(TOKSTR_ANONYMOUS5191) % sizeof(uint32_t) == 0, "");

        TOKSTR_ANONYMOUS5191 Caps = {TOK_FS_KMD_OVERLAY_CAPS_INFO__ANONYMOUS5171__CAPS};
        TokenDword CapsValue = {TOK_FBD_KMD_OVERLAY_CAPS_INFO__ANONYMOUS5171__CAPS_VALUE};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS5171>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS5171) % sizeof(uint32_t) == 0, "");

    TOKSTR_ANONYMOUS5171::TOKSTR_ANONYMOUS5191 Caps = {TOK_FS_KMD_OVERLAY_CAPS_INFO__ANONYMOUS5171__CAPS}; // Indirect field from anonymous struct
    TokenDword CapsValue = {TOK_FBD_KMD_OVERLAY_CAPS_INFO__ANONYMOUS5171__CAPS_VALUE};                     // Indirect field from anonymous struct
    TOKSTR___KMD_OVERLAY_OVERRIDE OVOverride = {TOK_FS_KMD_OVERLAY_CAPS_INFO__OVOVERRIDE};
    TokenDword MaxOverlayDisplayWidth = {TOK_FBD_KMD_OVERLAY_CAPS_INFO__MAX_OVERLAY_DISPLAY_WIDTH};
    TokenDword MaxOverlayDisplayHeight = {TOK_FBD_KMD_OVERLAY_CAPS_INFO__MAX_OVERLAY_DISPLAY_HEIGHT};
    TokenDword HWScalerExists = {TOK_FBC_KMD_OVERLAY_CAPS_INFO__HWSCALER_EXISTS};
    TokenDword MaxHWScalerStride = {TOK_FBD_KMD_OVERLAY_CAPS_INFO__MAX_HWSCALER_STRIDE};
};
static_assert(std::is_standard_layout_v<TOKSTR___KMD_OVERLAY_CAPS_INFO>, "");
static_assert(sizeof(TOKSTR___KMD_OVERLAY_CAPS_INFO) % sizeof(uint32_t) == 0, "");

struct TOKSTR_FRAME_RATE {
    TokenVariableLength base;

    TOKSTR_FRAME_RATE(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR_FRAME_RATE, uiDenominator) + sizeof(uiDenominator) - offsetof(TOKSTR_FRAME_RATE, uiNumerator), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR_FRAME_RATE()
        : base(TOK_S_FRAME_RATE, 0, sizeof(*this) - sizeof(base)) {}

    TokenDword uiNumerator = {TOK_FBD_FRAME_RATE__UI_NUMERATOR};
    TokenDword uiDenominator = {TOK_FBD_FRAME_RATE__UI_DENOMINATOR};
};
static_assert(std::is_standard_layout_v<TOKSTR_FRAME_RATE>, "");
static_assert(sizeof(TOKSTR_FRAME_RATE) % sizeof(uint32_t) == 0, "");

struct TOKSTR__KM_DEFERRED_WAIT_INFO {
    TokenVariableLength base;

    TOKSTR__KM_DEFERRED_WAIT_INFO(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR__KM_DEFERRED_WAIT_INFO, ActiveDisplay) + sizeof(ActiveDisplay) - offsetof(TOKSTR__KM_DEFERRED_WAIT_INFO, FeatureSupported), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR__KM_DEFERRED_WAIT_INFO()
        : base(TOK_S_KM_DEFERRED_WAIT_INFO, 0, sizeof(*this) - sizeof(base)) {}

    TokenDword FeatureSupported = {TOK_FBD_KM_DEFERRED_WAIT_INFO__FEATURE_SUPPORTED};
    TokenDword ActiveDisplay = {TOK_FBD_KM_DEFERRED_WAIT_INFO__ACTIVE_DISPLAY};
};
static_assert(std::is_standard_layout_v<TOKSTR__KM_DEFERRED_WAIT_INFO>, "");
static_assert(sizeof(TOKSTR__KM_DEFERRED_WAIT_INFO) % sizeof(uint32_t) == 0, "");

struct TOKSTR___GMM_GFX_PARTITIONING {
    TokenVariableLength base;

    TOKSTR___GMM_GFX_PARTITIONING(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR___GMM_GFX_PARTITIONING, Heap32) + sizeof(Heap32) - offsetof(TOKSTR___GMM_GFX_PARTITIONING, Standard), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR___GMM_GFX_PARTITIONING()
        : base(TOK_S_GMM_GFX_PARTITIONING, 0, sizeof(*this) - sizeof(base)) {}

    struct TOKSTR_ANONYMOUS7117 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS7117(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS7117, Limit) + sizeof(Limit) - offsetof(TOKSTR_ANONYMOUS7117, Base), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS7117()
            : base(TOK_S_GMM_GFX_PARTITIONING__ANONYMOUS7117, 0, sizeof(*this) - sizeof(base)) {}

        TokenQword Base = {TOK_FBQ_GMM_GFX_PARTITIONING__ANONYMOUS7117__BASE};
        TokenQword Limit = {TOK_FBQ_GMM_GFX_PARTITIONING__ANONYMOUS7117__LIMIT};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS7117>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS7117) % sizeof(uint32_t) == 0, "");

    TOKSTR_ANONYMOUS7117 Standard = {TOK_FS_GMM_GFX_PARTITIONING__STANDARD};
    TOKSTR_ANONYMOUS7117 Standard64KB = {TOK_FS_GMM_GFX_PARTITIONING__STANDARD64KB};
    TOKSTR_ANONYMOUS7117 SVM = {TOK_FS_GMM_GFX_PARTITIONING__SVM};
    TOKSTR_ANONYMOUS7117 Heap32[4] = {{TOK_FS_GMM_GFX_PARTITIONING__HEAP32, 0}, {TOK_FS_GMM_GFX_PARTITIONING__HEAP32, 1}, {TOK_FS_GMM_GFX_PARTITIONING__HEAP32, 2}, {TOK_FS_GMM_GFX_PARTITIONING__HEAP32, 3}};
};
static_assert(std::is_standard_layout_v<TOKSTR___GMM_GFX_PARTITIONING>, "");
static_assert(sizeof(TOKSTR___GMM_GFX_PARTITIONING) % sizeof(uint32_t) == 0, "");

struct TOKSTR__ADAPTER_BDF_ {
    TokenVariableLength base;

    TOKSTR__ADAPTER_BDF_(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR__ADAPTER_BDF_, Data) + sizeof(Data) - offsetof(TOKSTR__ADAPTER_BDF_, Bus), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR__ADAPTER_BDF_()
        : base(TOK_S_ADAPTER_BDF, 0, sizeof(*this) - sizeof(base)) {}

    struct TOKSTR_ANONYMOUS8173 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS8173(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS8173, Data) + sizeof(Data) - offsetof(TOKSTR_ANONYMOUS8173, Bus), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS8173()
            : base(TOK_S_ADAPTER_BDF___ANONYMOUS8173, 0, sizeof(*this) - sizeof(base)) {}

        struct TOKSTR_ANONYMOUS8193 {
            TokenVariableLength base;

            TOKSTR_ANONYMOUS8193(uint16_t tokenId, uint32_t elementId = 0)
                : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS8193, Function) + sizeof(Function) - offsetof(TOKSTR_ANONYMOUS8193, Bus), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

            TOKSTR_ANONYMOUS8193()
                : base(TOK_S_ADAPTER_BDF___ANONYMOUS8173__ANONYMOUS8193, 0, sizeof(*this) - sizeof(base)) {}

            TokenDword Bus = {TOK_FBD_ADAPTER_BDF___ANONYMOUS8173__ANONYMOUS8193__BUS};
            TokenDword Device = {TOK_FBD_ADAPTER_BDF___ANONYMOUS8173__ANONYMOUS8193__DEVICE};
            TokenDword Function = {TOK_FBD_ADAPTER_BDF___ANONYMOUS8173__ANONYMOUS8193__FUNCTION};
        };
        static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS8193>, "");
        static_assert(sizeof(TOKSTR_ANONYMOUS8193) % sizeof(uint32_t) == 0, "");

        TokenDword Bus = {TOK_FBD_ADAPTER_BDF___ANONYMOUS8173__ANONYMOUS8193__BUS};           // Indirect field from anonymous struct
        TokenDword Device = {TOK_FBD_ADAPTER_BDF___ANONYMOUS8173__ANONYMOUS8193__DEVICE};     // Indirect field from anonymous struct
        TokenDword Function = {TOK_FBD_ADAPTER_BDF___ANONYMOUS8173__ANONYMOUS8193__FUNCTION}; // Indirect field from anonymous struct
        TokenDword Data = {TOK_FBD_ADAPTER_BDF___ANONYMOUS8173__DATA};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS8173>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS8173) % sizeof(uint32_t) == 0, "");

    TokenDword Bus = {TOK_FBD_ADAPTER_BDF___ANONYMOUS8173__ANONYMOUS8193__BUS};           // Indirect field from anonymous struct
    TokenDword Device = {TOK_FBD_ADAPTER_BDF___ANONYMOUS8173__ANONYMOUS8193__DEVICE};     // Indirect field from anonymous struct
    TokenDword Function = {TOK_FBD_ADAPTER_BDF___ANONYMOUS8173__ANONYMOUS8193__FUNCTION}; // Indirect field from anonymous struct
    TokenDword Data = {TOK_FBD_ADAPTER_BDF___ANONYMOUS8173__DATA};                        // Indirect field from anonymous struct
};
static_assert(std::is_standard_layout_v<TOKSTR__ADAPTER_BDF_>, "");
static_assert(sizeof(TOKSTR__ADAPTER_BDF_) % sizeof(uint32_t) == 0, "");

struct TOKSTR__ADAPTER_INFO {
    TokenVariableLength base;

    TOKSTR__ADAPTER_INFO(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR__ADAPTER_INFO, stAdapterBDF) + sizeof(stAdapterBDF) - offsetof(TOKSTR__ADAPTER_INFO, KmdVersionInfo), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR__ADAPTER_INFO()
        : base(TOK_S_ADAPTER_INFO, 0, sizeof(*this) - sizeof(base)) {}

    TokenDword KmdVersionInfo = {TOK_FBD_ADAPTER_INFO__KMD_VERSION_INFO};
    TokenArray<128> DeviceRegistryPath = {TOK_FBC_ADAPTER_INFO__DEVICE_REGISTRY_PATH, 8, 512};
    TOKSTR_PLATFORM_STR GfxPlatform = {TOK_FS_ADAPTER_INFO__GFX_PLATFORM};
    TOKSTR__SKU_FEATURE_TABLE SkuTable = {TOK_FS_ADAPTER_INFO__SKU_TABLE};
    TOKSTR__WA_TABLE WaTable = {TOK_FS_ADAPTER_INFO__WA_TABLE};
    TokenDword GfxTimeStampFreq = {TOK_FBD_ADAPTER_INFO__GFX_TIME_STAMP_FREQ};
    TokenDword GfxCoreFrequency = {TOK_FBD_ADAPTER_INFO__GFX_CORE_FREQUENCY};
    TokenDword FSBFrequency = {TOK_FBD_ADAPTER_INFO__FSBFREQUENCY};
    TokenDword MinRenderFreq = {TOK_FBD_ADAPTER_INFO__MIN_RENDER_FREQ};
    TokenDword MaxRenderFreq = {TOK_FBD_ADAPTER_INFO__MAX_RENDER_FREQ};
    TokenDword PackageTdp = {TOK_FBD_ADAPTER_INFO__PACKAGE_TDP};
    TokenDword MaxFillRate = {TOK_FBD_ADAPTER_INFO__MAX_FILL_RATE};
    TokenDword NumberOfEUs = {TOK_FBD_ADAPTER_INFO__NUMBER_OF_EUS};
    TokenDword dwReleaseTarget = {TOK_FBD_ADAPTER_INFO__DW_RELEASE_TARGET};
    TokenDword SizeOfDmaBuffer = {TOK_FBD_ADAPTER_INFO__SIZE_OF_DMA_BUFFER};
    TokenDword PatchLocationListSize = {TOK_FBD_ADAPTER_INFO__PATCH_LOCATION_LIST_SIZE};
    TokenDword AllocationListSize = {TOK_FBD_ADAPTER_INFO__ALLOCATION_LIST_SIZE};
    TokenDword SmallPatchLocationListSize = {TOK_FBD_ADAPTER_INFO__SMALL_PATCH_LOCATION_LIST_SIZE};
    TokenDword DefaultCmdBufferSize = {TOK_FBD_ADAPTER_INFO__DEFAULT_CMD_BUFFER_SIZE};
    TokenQword GfxMemorySize = {TOK_FBQ_ADAPTER_INFO__GFX_MEMORY_SIZE};
    TokenDword SystemMemorySize = {TOK_FBD_ADAPTER_INFO__SYSTEM_MEMORY_SIZE};
    TokenDword CacheLineSize = {TOK_FBD_ADAPTER_INFO__CACHE_LINE_SIZE};
    TokenDword ProcessorFamily = {TOK_FE_ADAPTER_INFO__PROCESSOR_FAMILY};
    TokenDword IsHTSupported = {TOK_FBC_ADAPTER_INFO__IS_HTSUPPORTED};
    TokenDword IsMutiCoreCpu = {TOK_FBC_ADAPTER_INFO__IS_MUTI_CORE_CPU};
    TokenDword IsVTDSupported = {TOK_FBC_ADAPTER_INFO__IS_VTDSUPPORTED};
    TokenDword RegistryPathLength = {TOK_FBD_ADAPTER_INFO__REGISTRY_PATH_LENGTH};
    TokenQword DedicatedVideoMemory = {TOK_FBQ_ADAPTER_INFO__DEDICATED_VIDEO_MEMORY};
    TokenQword SystemSharedMemory = {TOK_FBQ_ADAPTER_INFO__SYSTEM_SHARED_MEMORY};
    TokenQword SystemVideoMemory = {TOK_FBQ_ADAPTER_INFO__SYSTEM_VIDEO_MEMORY};
    TOKSTR_FRAME_RATE OutputFrameRate = {TOK_FS_ADAPTER_INFO__OUTPUT_FRAME_RATE};
    TOKSTR_FRAME_RATE InputFrameRate = {TOK_FS_ADAPTER_INFO__INPUT_FRAME_RATE};
    TOKSTR___KMD_CAPS_INFO Caps = {TOK_FS_ADAPTER_INFO__CAPS};
    TOKSTR___KMD_OVERLAY_CAPS_INFO OverlayCaps = {TOK_FS_ADAPTER_INFO__OVERLAY_CAPS};
    TOKSTR_GT_SYSTEM_INFO SystemInfo = {TOK_FS_ADAPTER_INFO__SYSTEM_INFO};
    TOKSTR__KM_DEFERRED_WAIT_INFO DeferredWaitInfo = {TOK_FS_ADAPTER_INFO__DEFERRED_WAIT_INFO};
    TOKSTR___GMM_GFX_PARTITIONING GfxPartition = {TOK_FS_ADAPTER_INFO__GFX_PARTITION};
    TOKSTR__ADAPTER_BDF_ stAdapterBDF = {TOK_FS_ADAPTER_INFO__ST_ADAPTER_BDF};
};
static_assert(std::is_standard_layout_v<TOKSTR__ADAPTER_INFO>, "");
static_assert(sizeof(TOKSTR__ADAPTER_INFO) % sizeof(uint32_t) == 0, "");

struct TOKSTR__CREATECONTEXT_PVTDATA {
    TokenVariableLength base;

    TOKSTR__CREATECONTEXT_PVTDATA(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR__CREATECONTEXT_PVTDATA, NoRingFlushes) + sizeof(NoRingFlushes) - offsetof(TOKSTR__CREATECONTEXT_PVTDATA, pHwContextId), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR__CREATECONTEXT_PVTDATA()
        : base(TOK_S_CREATECONTEXT_PVTDATA, 0, sizeof(*this) - sizeof(base)) {}

    struct TOKSTR_ANONYMOUS18449 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS18449(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS18449, pHwContextId) + sizeof(pHwContextId) - offsetof(TOKSTR_ANONYMOUS18449, pHwContextId), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS18449()
            : base(TOK_S_CREATECONTEXT_PVTDATA__ANONYMOUS18449, 0, sizeof(*this) - sizeof(base)) {}

        TokenPointer pHwContextId = {TOK_PBQ_CREATECONTEXT_PVTDATA__ANONYMOUS18449__P_HW_CONTEXT_ID};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS18449>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS18449) % sizeof(uint32_t) == 0, "");

    TokenPointer pHwContextId = {TOK_PBQ_CREATECONTEXT_PVTDATA__ANONYMOUS18449__P_HW_CONTEXT_ID}; // Indirect field from anonymous struct
    TokenDword NumberOfHwContextIds = {TOK_FBD_CREATECONTEXT_PVTDATA__NUMBER_OF_HW_CONTEXT_IDS};
    TokenDword ProcessID = {TOK_FBD_CREATECONTEXT_PVTDATA__PROCESS_ID};
    TokenDword IsProtectedProcess = {TOK_FBC_CREATECONTEXT_PVTDATA__IS_PROTECTED_PROCESS};
    TokenDword IsDwm = {TOK_FBC_CREATECONTEXT_PVTDATA__IS_DWM};
    TokenDword IsMediaUsage = {TOK_FBC_CREATECONTEXT_PVTDATA__IS_MEDIA_USAGE};
    TokenDword GpuVAContext = {TOK_FBC_CREATECONTEXT_PVTDATA__GPU_VACONTEXT};
    TokenDword NoRingFlushes = {TOK_FBC_CREATECONTEXT_PVTDATA__NO_RING_FLUSHES};
    TokenDword UmdContextType = {TOK_FBD_CREATECONTEXT_PVTDATA__UMD_CONTEXT_TYPE};
};
static_assert(std::is_standard_layout_v<TOKSTR__CREATECONTEXT_PVTDATA>, "");
static_assert(sizeof(TOKSTR__CREATECONTEXT_PVTDATA) % sizeof(uint32_t) == 0, "");

struct TOKSTR_COMMAND_BUFFER_HEADER_REC {
    TokenVariableLength base;

    TOKSTR_COMMAND_BUFFER_HEADER_REC(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR_COMMAND_BUFFER_HEADER_REC, MonitorFenceValue) + sizeof(MonitorFenceValue) - offsetof(TOKSTR_COMMAND_BUFFER_HEADER_REC, UmdContextType), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR_COMMAND_BUFFER_HEADER_REC()
        : base(TOK_S_COMMAND_BUFFER_HEADER_REC, 0, sizeof(*this) - sizeof(base)) {}

    struct TOKSTR_ANONYMOUS32401 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS32401(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS32401, Tag) + sizeof(Tag) - offsetof(TOKSTR_ANONYMOUS32401, Enable), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS32401()
            : base(TOK_S_COMMAND_BUFFER_HEADER_REC__ANONYMOUS32401, 0, sizeof(*this) - sizeof(base)) {}

        struct TOKSTR_ANONYMOUS32457 {
            TokenVariableLength base;

            TOKSTR_ANONYMOUS32457(uint16_t tokenId, uint32_t elementId = 0)
                : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS32457, Value) + sizeof(Value) - offsetof(TOKSTR_ANONYMOUS32457, Address), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

            TOKSTR_ANONYMOUS32457()
                : base(TOK_S_COMMAND_BUFFER_HEADER_REC__ANONYMOUS32401__ANONYMOUS32457, 0, sizeof(*this) - sizeof(base)) {}

            struct TOKSTR_ANONYMOUS32501 {
                TokenVariableLength base;

                TOKSTR_ANONYMOUS32501(uint16_t tokenId, uint32_t elementId = 0)
                    : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS32501, GfxAddress) + sizeof(GfxAddress) - offsetof(TOKSTR_ANONYMOUS32501, Allocation), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

                TOKSTR_ANONYMOUS32501()
                    : base(TOK_S_COMMAND_BUFFER_HEADER_REC__ANONYMOUS32401__ANONYMOUS32457__ANONYMOUS32501, 0, sizeof(*this) - sizeof(base)) {}

                struct TOKSTR_ANONYMOUS32537 {
                    TokenVariableLength base;

                    TOKSTR_ANONYMOUS32537(uint16_t tokenId, uint32_t elementId = 0)
                        : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS32537, Offset) + sizeof(Offset) - offsetof(TOKSTR_ANONYMOUS32537, Index), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

                    TOKSTR_ANONYMOUS32537()
                        : base(TOK_S_COMMAND_BUFFER_HEADER_REC__ANONYMOUS32401__ANONYMOUS32457__ANONYMOUS32501__ANONYMOUS32537, 0, sizeof(*this) - sizeof(base)) {}

                    TokenDword Index = {TOK_FBD_COMMAND_BUFFER_HEADER_REC__ANONYMOUS32401__ANONYMOUS32457__ANONYMOUS32501__ANONYMOUS32537__INDEX};
                    TokenDword Offset = {TOK_FBD_COMMAND_BUFFER_HEADER_REC__ANONYMOUS32401__ANONYMOUS32457__ANONYMOUS32501__ANONYMOUS32537__OFFSET};
                };
                static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS32537>, "");
                static_assert(sizeof(TOKSTR_ANONYMOUS32537) % sizeof(uint32_t) == 0, "");

                struct TOKSTR_ANONYMOUS32680 {
                    TokenVariableLength base;

                    TOKSTR_ANONYMOUS32680(uint16_t tokenId, uint32_t elementId = 0)
                        : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS32680, HighPart) + sizeof(HighPart) - offsetof(TOKSTR_ANONYMOUS32680, LowPart), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

                    TOKSTR_ANONYMOUS32680()
                        : base(TOK_S_COMMAND_BUFFER_HEADER_REC__ANONYMOUS32401__ANONYMOUS32457__ANONYMOUS32501__ANONYMOUS32680, 0, sizeof(*this) - sizeof(base)) {}

                    TokenDword LowPart = {TOK_FBD_COMMAND_BUFFER_HEADER_REC__ANONYMOUS32401__ANONYMOUS32457__ANONYMOUS32501__ANONYMOUS32680__LOW_PART};
                    TokenDword HighPart = {TOK_FBD_COMMAND_BUFFER_HEADER_REC__ANONYMOUS32401__ANONYMOUS32457__ANONYMOUS32501__ANONYMOUS32680__HIGH_PART};
                };
                static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS32680>, "");
                static_assert(sizeof(TOKSTR_ANONYMOUS32680) % sizeof(uint32_t) == 0, "");

                TOKSTR_ANONYMOUS32537 Allocation = {TOK_FS_COMMAND_BUFFER_HEADER_REC__ANONYMOUS32401__ANONYMOUS32457__ANONYMOUS32501__ALLOCATION};
                TOKSTR_ANONYMOUS32680 GfxAddress = {TOK_FS_COMMAND_BUFFER_HEADER_REC__ANONYMOUS32401__ANONYMOUS32457__ANONYMOUS32501__GFX_ADDRESS};
            };
            static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS32501>, "");
            static_assert(sizeof(TOKSTR_ANONYMOUS32501) % sizeof(uint32_t) == 0, "");

            TOKSTR_ANONYMOUS32501 Address = {TOK_FS_COMMAND_BUFFER_HEADER_REC__ANONYMOUS32401__ANONYMOUS32457__ADDRESS};
            TokenDword Value = {TOK_FBD_COMMAND_BUFFER_HEADER_REC__ANONYMOUS32401__ANONYMOUS32457__VALUE};
        };
        static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS32457>, "");
        static_assert(sizeof(TOKSTR_ANONYMOUS32457) % sizeof(uint32_t) == 0, "");

        TokenDword Enable = {TOK_FBD_COMMAND_BUFFER_HEADER_REC__ANONYMOUS32401__ENABLE};
        TOKSTR_ANONYMOUS32457 Tag = {TOK_FS_COMMAND_BUFFER_HEADER_REC__ANONYMOUS32401__TAG};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS32401>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS32401) % sizeof(uint32_t) == 0, "");

    TokenDword UmdContextType = {TOK_FBD_COMMAND_BUFFER_HEADER_REC__UMD_CONTEXT_TYPE};
    TokenDword UmdPatchList = {TOK_FBD_COMMAND_BUFFER_HEADER_REC__UMD_PATCH_LIST};
    TokenDword UmdRequestedSliceState = {TOK_FBD_COMMAND_BUFFER_HEADER_REC__UMD_REQUESTED_SLICE_STATE};
    TokenDword UmdRequestedSubsliceCount = {TOK_FBD_COMMAND_BUFFER_HEADER_REC__UMD_REQUESTED_SUBSLICE_COUNT};
    TokenDword UmdRequestedEUCount = {TOK_FBD_COMMAND_BUFFER_HEADER_REC__UMD_REQUESTED_EUCOUNT};
    TokenDword UsesResourceStreamer = {TOK_FBD_COMMAND_BUFFER_HEADER_REC__USES_RESOURCE_STREAMER};
    TokenDword NeedsMidBatchPreEmptionSupport = {TOK_FBD_COMMAND_BUFFER_HEADER_REC__NEEDS_MID_BATCH_PRE_EMPTION_SUPPORT};
    TokenDword UsesGPGPUPipeline = {TOK_FBD_COMMAND_BUFFER_HEADER_REC__USES_GPGPUPIPELINE};
    TokenDword RequiresCoherency = {TOK_FBD_COMMAND_BUFFER_HEADER_REC__REQUIRES_COHERENCY};
    TokenDword PerfTag = {TOK_FBD_COMMAND_BUFFER_HEADER_REC__PERF_TAG};
    TokenQword MonitorFenceVA = {TOK_FBQ_COMMAND_BUFFER_HEADER_REC__MONITOR_FENCE_VA};
    TokenQword MonitorFenceValue = {TOK_FBQ_COMMAND_BUFFER_HEADER_REC__MONITOR_FENCE_VALUE};
};
static_assert(std::is_standard_layout_v<TOKSTR_COMMAND_BUFFER_HEADER_REC>, "");
static_assert(sizeof(TOKSTR_COMMAND_BUFFER_HEADER_REC) % sizeof(uint32_t) == 0, "");

struct TOKSTR_GMM_RESOURCE_FLAG_REC {
    TokenVariableLength base;

    TOKSTR_GMM_RESOURCE_FLAG_REC(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR_GMM_RESOURCE_FLAG_REC, Wa) + sizeof(Wa) - offsetof(TOKSTR_GMM_RESOURCE_FLAG_REC, Gpu), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR_GMM_RESOURCE_FLAG_REC()
        : base(TOK_S_GMM_RESOURCE_FLAG_REC, 0, sizeof(*this) - sizeof(base)) {}

    struct TOKSTR_ANONYMOUS1739 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS1739(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS1739, __Remaining) + sizeof(__Remaining) - offsetof(TOKSTR_ANONYMOUS1739, CameraCapture), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS1739()
            : base(TOK_S_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739, 0, sizeof(*this) - sizeof(base)) {}

        TokenDword CameraCapture = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__CAMERA_CAPTURE};
        TokenDword CCS = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__CCS};
        TokenDword ColorDiscard = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__COLOR_DISCARD};
        TokenDword ColorSeparation = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__COLOR_SEPARATION};
        TokenDword ColorSeparationRGBX = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__COLOR_SEPARATION_RGBX};
        TokenDword Constant = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__CONSTANT};
        TokenDword Depth = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__DEPTH};
        TokenDword FlipChain = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__FLIP_CHAIN};
        TokenDword FlipChainPreferred = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__FLIP_CHAIN_PREFERRED};
        TokenDword HistoryBuffer = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__HISTORY_BUFFER};
        TokenDword HiZ = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__HI_Z};
        TokenDword Index = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__INDEX};
        TokenDword IndirectClearColor = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__INDIRECT_CLEAR_COLOR};
        TokenDword InstructionFlat = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__INSTRUCTION_FLAT};
        TokenDword InterlacedScan = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__INTERLACED_SCAN};
        TokenDword MCS = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__MCS};
        TokenDword MMC = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__MMC};
        TokenDword MotionComp = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__MOTION_COMP};
        TokenDword NoRestriction = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__NO_RESTRICTION};
        TokenDword Overlay = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__OVERLAY};
        TokenDword Presentable = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__PRESENTABLE};
        TokenDword ProceduralTexture = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__PROCEDURAL_TEXTURE};
        TokenDword Query = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__QUERY};
        TokenDword RenderTarget = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__RENDER_TARGET};
        TokenDword S3d = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__S3D};
        TokenDword S3dDx = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__S3D_DX};
        TokenDword __S3dNonPacked = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____S3D_NON_PACKED};
        TokenDword __S3dWidi = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____S3D_WIDI};
        TokenDword ScratchFlat = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__SCRATCH_FLAT};
        TokenDword SeparateStencil = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__SEPARATE_STENCIL};
        TokenDword State = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__STATE};
        TokenDword StateDx9ConstantBuffer = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__STATE_DX9CONSTANT_BUFFER};
        TokenDword Stream = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__STREAM};
        TokenDword TextApi = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__TEXT_API};
        TokenDword Texture = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__TEXTURE};
        TokenDword TiledResource = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__TILED_RESOURCE};
        TokenDword TilePool = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__TILE_POOL};
        TokenDword UnifiedAuxSurface = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__UNIFIED_AUX_SURFACE};
        TokenDword Vertex = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__VERTEX};
        TokenDword Video = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__VIDEO};
        TokenDword __NonMsaaTileXCcs = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____NON_MSAA_TILE_XCCS};
        TokenDword __NonMsaaTileYCcs = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____NON_MSAA_TILE_YCCS};
        TokenDword __MsaaTileMcs = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____MSAA_TILE_MCS};
        TokenDword __NonMsaaLinearCCS = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____NON_MSAA_LINEAR_CCS};
        TokenDword __Remaining = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____REMAINING};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS1739>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS1739) % sizeof(uint32_t) == 0, "");

    struct TOKSTR_ANONYMOUS6797 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS6797(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS6797, NotCompressed) + sizeof(NotCompressed) - offsetof(TOKSTR_ANONYMOUS6797, AllowVirtualPadding), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS6797()
            : base(TOK_S_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797, 0, sizeof(*this) - sizeof(base)) {}

        TokenDword AllowVirtualPadding = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__ALLOW_VIRTUAL_PADDING};
        TokenDword BigPage = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__BIG_PAGE};
        TokenDword Cacheable = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__CACHEABLE};
        TokenDword ContigPhysMemoryForiDART = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__CONTIG_PHYS_MEMORY_FORI_DART};
        TokenDword CornerTexelMode = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__CORNER_TEXEL_MODE};
        TokenDword ExistingSysMem = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__EXISTING_SYS_MEM};
        TokenDword ForceResidency = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__FORCE_RESIDENCY};
        TokenDword Gfdt = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__GFDT};
        TokenDword GttMapType = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__GTT_MAP_TYPE};
        TokenDword HardwareProtected = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__HARDWARE_PROTECTED};
        TokenDword KernelModeMapped = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__KERNEL_MODE_MAPPED};
        TokenDword LayoutBelow = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__LAYOUT_BELOW};
        TokenDword LayoutMono = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__LAYOUT_MONO};
        TokenDword LocalOnly = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__LOCAL_ONLY};
        TokenDword Linear = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__LINEAR};
        TokenDword MediaCompressed = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__MEDIA_COMPRESSED};
        TokenDword NoOptimizationPadding = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__NO_OPTIMIZATION_PADDING};
        TokenDword NoPhysMemory = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__NO_PHYS_MEMORY};
        TokenDword NotLockable = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__NOT_LOCKABLE};
        TokenDword NonLocalOnly = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__NON_LOCAL_ONLY};
        TokenDword StdSwizzle = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__STD_SWIZZLE};
        TokenDword PseudoStdSwizzle = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__PSEUDO_STD_SWIZZLE};
        TokenDword Undefined64KBSwizzle = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__UNDEFINED64KBSWIZZLE};
        TokenDword RedecribedPlanes = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__REDECRIBED_PLANES};
        TokenDword RenderCompressed = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__RENDER_COMPRESSED};
        TokenDword Rotated = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__ROTATED};
        TokenDword Shared = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__SHARED};
        TokenDword SoftwareProtected = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__SOFTWARE_PROTECTED};
        TokenDword SVM = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__SVM};
        TokenDword Tile4 = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILE4};
        TokenDword Tile64 = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILE64};
        TokenDword TiledW = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILED_W};
        TokenDword TiledX = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILED_X};
        TokenDword TiledY = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILED_Y};
        TokenDword TiledYf = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILED_YF};
        TokenDword TiledYs = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILED_YS};
        TokenDword WddmProtected = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__WDDM_PROTECTED};
        TokenDword XAdapter = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__XADAPTER};
        TokenDword __PreallocatedResInfo = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797____PREALLOCATED_RES_INFO};
        TokenDword __PreWddm2SVM = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797____PRE_WDDM2SVM};
        TokenDword NotCompressed = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__NOT_COMPRESSED};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS6797>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS6797) % sizeof(uint32_t) == 0, "");

    struct TOKSTR_ANONYMOUS12521 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS12521(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS12521, DeniableLocalOnlyForCompression) + sizeof(DeniableLocalOnlyForCompression) - offsetof(TOKSTR_ANONYMOUS12521, GTMfx2ndLevelBatchRingSizeAlign), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS12521()
            : base(TOK_S_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521, 0, sizeof(*this) - sizeof(base)) {}

        TokenDword GTMfx2ndLevelBatchRingSizeAlign = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__GTMFX2ND_LEVEL_BATCH_RING_SIZE_ALIGN};
        TokenDword ILKNeedAvcMprRowStore32KAlign = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__ILKNEED_AVC_MPR_ROW_STORE32KALIGN};
        TokenDword ILKNeedAvcDmvBuffer32KAlign = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__ILKNEED_AVC_DMV_BUFFER32KALIGN};
        TokenDword NoBufferSamplerPadding = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__NO_BUFFER_SAMPLER_PADDING};
        TokenDword NoLegacyPlanarLinearVideoRestrictions = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__NO_LEGACY_PLANAR_LINEAR_VIDEO_RESTRICTIONS};
        TokenDword CHVAstcSkipVirtualMips = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__CHVASTC_SKIP_VIRTUAL_MIPS};
        TokenDword DisablePackedMipTail = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__DISABLE_PACKED_MIP_TAIL};
        TokenDword __ForceOtherHVALIGN4 = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521____FORCE_OTHER_HVALIGN4};
        TokenDword DisableDisplayCcsClearColor = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__DISABLE_DISPLAY_CCS_CLEAR_COLOR};
        TokenDword DisableDisplayCcsCompression = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__DISABLE_DISPLAY_CCS_COMPRESSION};
        TokenDword PreGen12FastClearOnly = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__PRE_GEN12FAST_CLEAR_ONLY};
        TokenDword ForceStdAllocAlign = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__FORCE_STD_ALLOC_ALIGN};
        TokenDword DeniableLocalOnlyForCompression = {TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__DENIABLE_LOCAL_ONLY_FOR_COMPRESSION};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS12521>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS12521) % sizeof(uint32_t) == 0, "");

    TOKSTR_ANONYMOUS1739 Gpu = {TOK_FS_GMM_RESOURCE_FLAG_REC__GPU};
    TOKSTR_ANONYMOUS6797 Info = {TOK_FS_GMM_RESOURCE_FLAG_REC__INFO};
    TOKSTR_ANONYMOUS12521 Wa = {TOK_FS_GMM_RESOURCE_FLAG_REC__WA};
};
static_assert(std::is_standard_layout_v<TOKSTR_GMM_RESOURCE_FLAG_REC>, "");
static_assert(sizeof(TOKSTR_GMM_RESOURCE_FLAG_REC) % sizeof(uint32_t) == 0, "");

struct TOKSTR_GMM_RESOURCE_MSAA_INFO_REC {
    TokenVariableLength base;

    TOKSTR_GMM_RESOURCE_MSAA_INFO_REC(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR_GMM_RESOURCE_MSAA_INFO_REC, NumSamples) + sizeof(NumSamples) - offsetof(TOKSTR_GMM_RESOURCE_MSAA_INFO_REC, SamplePattern), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR_GMM_RESOURCE_MSAA_INFO_REC()
        : base(TOK_S_GMM_RESOURCE_MSAA_INFO_REC, 0, sizeof(*this) - sizeof(base)) {}

    TokenDword SamplePattern = {TOK_FE_GMM_RESOURCE_MSAA_INFO_REC__SAMPLE_PATTERN};
    TokenDword NumSamples = {TOK_FBD_GMM_RESOURCE_MSAA_INFO_REC__NUM_SAMPLES};
};
static_assert(std::is_standard_layout_v<TOKSTR_GMM_RESOURCE_MSAA_INFO_REC>, "");
static_assert(sizeof(TOKSTR_GMM_RESOURCE_MSAA_INFO_REC) % sizeof(uint32_t) == 0, "");

struct TOKSTR_GMM_RESOURCE_ALIGNMENT_REC {
    TokenVariableLength base;

    TOKSTR_GMM_RESOURCE_ALIGNMENT_REC(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR_GMM_RESOURCE_ALIGNMENT_REC, QPitch) + sizeof(QPitch) - offsetof(TOKSTR_GMM_RESOURCE_ALIGNMENT_REC, ArraySpacingSingleLod), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR_GMM_RESOURCE_ALIGNMENT_REC()
        : base(TOK_S_GMM_RESOURCE_ALIGNMENT_REC, 0, sizeof(*this) - sizeof(base)) {}

    TokenDword ArraySpacingSingleLod = {TOK_FBC_GMM_RESOURCE_ALIGNMENT_REC__ARRAY_SPACING_SINGLE_LOD};
    TokenDword BaseAlignment = {TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__BASE_ALIGNMENT};
    TokenDword HAlign = {TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__HALIGN};
    TokenDword VAlign = {TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__VALIGN};
    TokenDword DAlign = {TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__DALIGN};
    TokenDword MipTailStartLod = {TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__MIP_TAIL_START_LOD};
    TokenDword PackedMipStartLod = {TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__PACKED_MIP_START_LOD};
    TokenDword PackedMipWidth = {TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__PACKED_MIP_WIDTH};
    TokenDword PackedMipHeight = {TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__PACKED_MIP_HEIGHT};
    TokenDword QPitch = {TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__QPITCH};
};
static_assert(std::is_standard_layout_v<TOKSTR_GMM_RESOURCE_ALIGNMENT_REC>, "");
static_assert(sizeof(TOKSTR_GMM_RESOURCE_ALIGNMENT_REC) % sizeof(uint32_t) == 0, "");

struct TOKSTR_GMM_S3D_INFO_REC {
    TokenVariableLength base;

    TOKSTR_GMM_S3D_INFO_REC(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR_GMM_S3D_INFO_REC, IsRFrame) + sizeof(IsRFrame) - offsetof(TOKSTR_GMM_S3D_INFO_REC, DisplayModeHeight), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR_GMM_S3D_INFO_REC()
        : base(TOK_S_GMM_S3D_INFO_REC, 0, sizeof(*this) - sizeof(base)) {}

    TokenDword DisplayModeHeight = {TOK_FBD_GMM_S3D_INFO_REC__DISPLAY_MODE_HEIGHT};
    TokenDword NumBlankActiveLines = {TOK_FBD_GMM_S3D_INFO_REC__NUM_BLANK_ACTIVE_LINES};
    TokenDword RFrameOffset = {TOK_FBD_GMM_S3D_INFO_REC__RFRAME_OFFSET};
    TokenDword BlankAreaOffset = {TOK_FBD_GMM_S3D_INFO_REC__BLANK_AREA_OFFSET};
    TokenDword TallBufferHeight = {TOK_FBD_GMM_S3D_INFO_REC__TALL_BUFFER_HEIGHT};
    TokenDword TallBufferSize = {TOK_FBD_GMM_S3D_INFO_REC__TALL_BUFFER_SIZE};
    TokenDword IsRFrame = {TOK_FBC_GMM_S3D_INFO_REC__IS_RFRAME};
};
static_assert(std::is_standard_layout_v<TOKSTR_GMM_S3D_INFO_REC>, "");
static_assert(sizeof(TOKSTR_GMM_S3D_INFO_REC) % sizeof(uint32_t) == 0, "");

struct TOKSTR_GMM_MULTI_TILE_ARCH_REC {
    TokenVariableLength base;

    TOKSTR_GMM_MULTI_TILE_ARCH_REC(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR_GMM_MULTI_TILE_ARCH_REC, LocalMemPreferredSet) + sizeof(LocalMemPreferredSet) - offsetof(TOKSTR_GMM_MULTI_TILE_ARCH_REC, Enable), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR_GMM_MULTI_TILE_ARCH_REC()
        : base(TOK_S_GMM_MULTI_TILE_ARCH_REC, 0, sizeof(*this) - sizeof(base)) {}

    TokenDword Enable = {TOK_FBC_GMM_MULTI_TILE_ARCH_REC__ENABLE};
    TokenDword TileInstanced = {TOK_FBC_GMM_MULTI_TILE_ARCH_REC__TILE_INSTANCED};
    TokenDword GpuVaMappingSet = {TOK_FBC_GMM_MULTI_TILE_ARCH_REC__GPU_VA_MAPPING_SET};
    TokenDword LocalMemEligibilitySet = {TOK_FBC_GMM_MULTI_TILE_ARCH_REC__LOCAL_MEM_ELIGIBILITY_SET};
    TokenDword LocalMemPreferredSet = {TOK_FBC_GMM_MULTI_TILE_ARCH_REC__LOCAL_MEM_PREFERRED_SET};
};
static_assert(std::is_standard_layout_v<TOKSTR_GMM_MULTI_TILE_ARCH_REC>, "");
static_assert(sizeof(TOKSTR_GMM_MULTI_TILE_ARCH_REC) % sizeof(uint32_t) == 0, "");

struct TOKSTR_GMM_PLANAR_OFFSET_INFO_REC {
    TokenVariableLength base;

    TOKSTR_GMM_PLANAR_OFFSET_INFO_REC(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR_GMM_PLANAR_OFFSET_INFO_REC, IsTileAlignedPlanes) + sizeof(IsTileAlignedPlanes) - offsetof(TOKSTR_GMM_PLANAR_OFFSET_INFO_REC, ArrayQPitch), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR_GMM_PLANAR_OFFSET_INFO_REC()
        : base(TOK_S_GMM_PLANAR_OFFSET_INFO_REC, 0, sizeof(*this) - sizeof(base)) {}

    struct TOKSTR_ANONYMOUS1851 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS1851(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS1851, Height) + sizeof(Height) - offsetof(TOKSTR_ANONYMOUS1851, Height), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS1851()
            : base(TOK_S_GMM_PLANAR_OFFSET_INFO_REC__ANONYMOUS1851, 0, sizeof(*this) - sizeof(base)) {}

        TokenArray<8> Height = {TOK_FBQ_GMM_PLANAR_OFFSET_INFO_REC__ANONYMOUS1851__HEIGHT, 64, 4};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS1851>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS1851) % sizeof(uint32_t) == 0, "");

    TokenQword ArrayQPitch = {TOK_FBQ_GMM_PLANAR_OFFSET_INFO_REC__ARRAY_QPITCH};
    TokenArray<8> X = {TOK_FBQ_GMM_PLANAR_OFFSET_INFO_REC__X, 64, 4};
    TokenArray<8> Y = {TOK_FBQ_GMM_PLANAR_OFFSET_INFO_REC__Y, 64, 4};
    TOKSTR_ANONYMOUS1851 UnAligned = {TOK_FS_GMM_PLANAR_OFFSET_INFO_REC__UN_ALIGNED};
    TokenDword NoOfPlanes = {TOK_FBD_GMM_PLANAR_OFFSET_INFO_REC__NO_OF_PLANES};
    TokenBool IsTileAlignedPlanes = {TOK_FBB_GMM_PLANAR_OFFSET_INFO_REC__IS_TILE_ALIGNED_PLANES};
};
static_assert(std::is_standard_layout_v<TOKSTR_GMM_PLANAR_OFFSET_INFO_REC>, "");
static_assert(sizeof(TOKSTR_GMM_PLANAR_OFFSET_INFO_REC) % sizeof(uint32_t) == 0, "");

struct TOKSTR_GMM_2D_TEXTURE_OFFSET_INFO_REC {
    TokenVariableLength base;

    TOKSTR_GMM_2D_TEXTURE_OFFSET_INFO_REC(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR_GMM_2D_TEXTURE_OFFSET_INFO_REC, Offset) + sizeof(Offset) - offsetof(TOKSTR_GMM_2D_TEXTURE_OFFSET_INFO_REC, ArrayQPitchLock), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR_GMM_2D_TEXTURE_OFFSET_INFO_REC()
        : base(TOK_S_GMM_2D_TEXTURE_OFFSET_INFO_REC, 0, sizeof(*this) - sizeof(base)) {}

    TokenQword ArrayQPitchLock = {TOK_FBQ_GMM_2D_TEXTURE_OFFSET_INFO_REC__ARRAY_QPITCH_LOCK};
    TokenQword ArrayQPitchRender = {TOK_FBQ_GMM_2D_TEXTURE_OFFSET_INFO_REC__ARRAY_QPITCH_RENDER};
    TokenArray<30> Offset = {TOK_FBQ_GMM_2D_TEXTURE_OFFSET_INFO_REC__OFFSET, 64, 15};
};
static_assert(std::is_standard_layout_v<TOKSTR_GMM_2D_TEXTURE_OFFSET_INFO_REC>, "");
static_assert(sizeof(TOKSTR_GMM_2D_TEXTURE_OFFSET_INFO_REC) % sizeof(uint32_t) == 0, "");

struct TOKSTR_GMM_3D_TEXTURE_OFFSET_INFO_REC {
    TokenVariableLength base;

    TOKSTR_GMM_3D_TEXTURE_OFFSET_INFO_REC(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR_GMM_3D_TEXTURE_OFFSET_INFO_REC, Offset) + sizeof(Offset) - offsetof(TOKSTR_GMM_3D_TEXTURE_OFFSET_INFO_REC, Mip0SlicePitch), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR_GMM_3D_TEXTURE_OFFSET_INFO_REC()
        : base(TOK_S_GMM_3D_TEXTURE_OFFSET_INFO_REC, 0, sizeof(*this) - sizeof(base)) {}

    TokenQword Mip0SlicePitch = {TOK_FBQ_GMM_3D_TEXTURE_OFFSET_INFO_REC__MIP0SLICE_PITCH};
    TokenArray<30> Offset = {TOK_FBQ_GMM_3D_TEXTURE_OFFSET_INFO_REC__OFFSET, 64, 15};
};
static_assert(std::is_standard_layout_v<TOKSTR_GMM_3D_TEXTURE_OFFSET_INFO_REC>, "");
static_assert(sizeof(TOKSTR_GMM_3D_TEXTURE_OFFSET_INFO_REC) % sizeof(uint32_t) == 0, "");

struct TOKSTR_GMM_OFFSET_INFO_REC {
    TokenVariableLength base;

    TOKSTR_GMM_OFFSET_INFO_REC(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR_GMM_OFFSET_INFO_REC, Plane) + sizeof(Plane) - offsetof(TOKSTR_GMM_OFFSET_INFO_REC, Texture3DOffsetInfo), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR_GMM_OFFSET_INFO_REC()
        : base(TOK_S_GMM_OFFSET_INFO_REC, 0, sizeof(*this) - sizeof(base)) {}

    struct TOKSTR_ANONYMOUS3429 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS3429(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS3429, Plane) + sizeof(Plane) - offsetof(TOKSTR_ANONYMOUS3429, Texture3DOffsetInfo), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS3429()
            : base(TOK_S_GMM_OFFSET_INFO_REC__ANONYMOUS3429, 0, sizeof(*this) - sizeof(base)) {}

        TOKSTR_GMM_3D_TEXTURE_OFFSET_INFO_REC Texture3DOffsetInfo = {TOK_FS_GMM_OFFSET_INFO_REC__ANONYMOUS3429__TEXTURE3DOFFSET_INFO};
        TOKSTR_GMM_2D_TEXTURE_OFFSET_INFO_REC Texture2DOffsetInfo = {TOK_FS_GMM_OFFSET_INFO_REC__ANONYMOUS3429__TEXTURE2DOFFSET_INFO};
        TOKSTR_GMM_PLANAR_OFFSET_INFO_REC Plane = {TOK_FS_GMM_OFFSET_INFO_REC__ANONYMOUS3429__PLANE};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS3429>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS3429) % sizeof(uint32_t) == 0, "");

    TOKSTR_GMM_3D_TEXTURE_OFFSET_INFO_REC Texture3DOffsetInfo = {TOK_FS_GMM_OFFSET_INFO_REC__ANONYMOUS3429__TEXTURE3DOFFSET_INFO}; // Indirect field from anonymous struct
    TOKSTR_GMM_2D_TEXTURE_OFFSET_INFO_REC Texture2DOffsetInfo = {TOK_FS_GMM_OFFSET_INFO_REC__ANONYMOUS3429__TEXTURE2DOFFSET_INFO}; // Indirect field from anonymous struct
    TOKSTR_GMM_PLANAR_OFFSET_INFO_REC Plane = {TOK_FS_GMM_OFFSET_INFO_REC__ANONYMOUS3429__PLANE};                                  // Indirect field from anonymous struct
};
static_assert(std::is_standard_layout_v<TOKSTR_GMM_OFFSET_INFO_REC>, "");
static_assert(sizeof(TOKSTR_GMM_OFFSET_INFO_REC) % sizeof(uint32_t) == 0, "");

struct TOKSTR_GMM_TEXTURE_INFO_REC {
    TokenVariableLength base;

    TOKSTR_GMM_TEXTURE_INFO_REC(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR_GMM_TEXTURE_INFO_REC, __Platform) + sizeof(__Platform) - offsetof(TOKSTR_GMM_TEXTURE_INFO_REC, Type), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR_GMM_TEXTURE_INFO_REC()
        : base(TOK_S_GMM_TEXTURE_INFO_REC, 0, sizeof(*this) - sizeof(base)) {}

    struct TOKSTR_ANONYMOUS4927 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS4927(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS4927, Usage) + sizeof(Usage) - offsetof(TOKSTR_ANONYMOUS4927, Usage), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS4927()
            : base(TOK_S_GMM_TEXTURE_INFO_REC__ANONYMOUS4927, 0, sizeof(*this) - sizeof(base)) {}

        TokenDword Usage = {TOK_FE_GMM_TEXTURE_INFO_REC__ANONYMOUS4927__USAGE};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS4927>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS4927) % sizeof(uint32_t) == 0, "");

    struct TOKSTR_ANONYMOUS6185 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS6185(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS6185, Evict) + sizeof(Evict) - offsetof(TOKSTR_ANONYMOUS6185, Seg1), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS6185()
            : base(TOK_S_GMM_TEXTURE_INFO_REC__ANONYMOUS6185, 0, sizeof(*this) - sizeof(base)) {}

        TokenDword Seg1 = {TOK_FBD_GMM_TEXTURE_INFO_REC__ANONYMOUS6185__SEG1};
        TokenDword Evict = {TOK_FBD_GMM_TEXTURE_INFO_REC__ANONYMOUS6185__EVICT};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS6185>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS6185) % sizeof(uint32_t) == 0, "");

    struct TOKSTR_ANONYMOUS6590 {
        TokenVariableLength base;

        TOKSTR_ANONYMOUS6590(uint16_t tokenId, uint32_t elementId = 0)
            : base(tokenId, elementId, offsetof(TOKSTR_ANONYMOUS6590, IsPageAligned) + sizeof(IsPageAligned) - offsetof(TOKSTR_ANONYMOUS6590, IsGmmAllocated), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

        TOKSTR_ANONYMOUS6590()
            : base(TOK_S_GMM_TEXTURE_INFO_REC__ANONYMOUS6590, 0, sizeof(*this) - sizeof(base)) {}

        TokenDword IsGmmAllocated = {TOK_FBC_GMM_TEXTURE_INFO_REC__ANONYMOUS6590__IS_GMM_ALLOCATED};
        TokenDword IsPageAligned = {TOK_FBC_GMM_TEXTURE_INFO_REC__ANONYMOUS6590__IS_PAGE_ALIGNED};
    };
    static_assert(std::is_standard_layout_v<TOKSTR_ANONYMOUS6590>, "");
    static_assert(sizeof(TOKSTR_ANONYMOUS6590) % sizeof(uint32_t) == 0, "");

    TokenDword Type = {TOK_FE_GMM_TEXTURE_INFO_REC__TYPE};
    TokenDword Format = {TOK_FE_GMM_TEXTURE_INFO_REC__FORMAT};
    TokenDword BitsPerPixel = {TOK_FBD_GMM_TEXTURE_INFO_REC__BITS_PER_PIXEL};
    TOKSTR_GMM_RESOURCE_FLAG_REC Flags = {TOK_FS_GMM_TEXTURE_INFO_REC__FLAGS};
    TokenQword BaseWidth = {TOK_FBQ_GMM_TEXTURE_INFO_REC__BASE_WIDTH};
    TokenDword BaseHeight = {TOK_FBD_GMM_TEXTURE_INFO_REC__BASE_HEIGHT};
    TokenDword Depth = {TOK_FBD_GMM_TEXTURE_INFO_REC__DEPTH};
    TokenDword MaxLod = {TOK_FBD_GMM_TEXTURE_INFO_REC__MAX_LOD};
    TokenDword ArraySize = {TOK_FBD_GMM_TEXTURE_INFO_REC__ARRAY_SIZE};
    TokenDword CpTag = {TOK_FBD_GMM_TEXTURE_INFO_REC__CP_TAG};
    TOKSTR_ANONYMOUS4927 CachePolicy = {TOK_FS_GMM_TEXTURE_INFO_REC__CACHE_POLICY};
    TOKSTR_GMM_RESOURCE_MSAA_INFO_REC MSAA = {TOK_FS_GMM_TEXTURE_INFO_REC__MSAA};
    TOKSTR_GMM_RESOURCE_ALIGNMENT_REC Alignment = {TOK_FS_GMM_TEXTURE_INFO_REC__ALIGNMENT};
    TokenArray<16> MmcMode = {TOK_FBC_GMM_TEXTURE_INFO_REC__MMC_MODE, 8, 64};
    TokenArray<16> MmcHint = {TOK_FBC_GMM_TEXTURE_INFO_REC__MMC_HINT, 8, 64};
    TokenQword Pitch = {TOK_FBQ_GMM_TEXTURE_INFO_REC__PITCH};
    TokenQword OverridePitch = {TOK_FBQ_GMM_TEXTURE_INFO_REC__OVERRIDE_PITCH};
    TokenQword Size = {TOK_FBQ_GMM_TEXTURE_INFO_REC__SIZE};
    TokenQword CCSize = {TOK_FBQ_GMM_TEXTURE_INFO_REC__CCSIZE};
    TokenQword UnpaddedSize = {TOK_FBQ_GMM_TEXTURE_INFO_REC__UNPADDED_SIZE};
    TokenQword SizeReportToOS = {TOK_FBQ_GMM_TEXTURE_INFO_REC__SIZE_REPORT_TO_OS};
    TOKSTR_GMM_OFFSET_INFO_REC OffsetInfo = {TOK_FS_GMM_TEXTURE_INFO_REC__OFFSET_INFO};
    TokenDword TileMode = {TOK_FE_GMM_TEXTURE_INFO_REC__TILE_MODE};
    TokenDword CCSModeAlign = {TOK_FBD_GMM_TEXTURE_INFO_REC__CCSMODE_ALIGN};
    TokenDword LegacyFlags = {TOK_FBD_GMM_TEXTURE_INFO_REC__LEGACY_FLAGS};
    TOKSTR_GMM_S3D_INFO_REC S3d = {TOK_FS_GMM_TEXTURE_INFO_REC__S3D};
    TOKSTR_ANONYMOUS6185 SegmentOverride = {TOK_FS_GMM_TEXTURE_INFO_REC__SEGMENT_OVERRIDE};
    TokenDword MaximumRenamingListLength = {TOK_FBD_GMM_TEXTURE_INFO_REC__MAXIMUM_RENAMING_LIST_LENGTH};
    TOKSTR_PLATFORM_STR Platform = {TOK_FS_GMM_TEXTURE_INFO_REC__PLATFORM}; // _DEBUG || _RELEASE_INTERNAL
    TOKSTR_ANONYMOUS6590 ExistingSysMem = {TOK_FS_GMM_TEXTURE_INFO_REC__EXISTING_SYS_MEM};
    TOKSTR_PLATFORM_STR __Platform = {TOK_FS_GMM_TEXTURE_INFO_REC____PLATFORM}; // !(_DEBUG || _RELEASE_INTERNAL)
};
static_assert(std::is_standard_layout_v<TOKSTR_GMM_TEXTURE_INFO_REC>, "");
static_assert(sizeof(TOKSTR_GMM_TEXTURE_INFO_REC) % sizeof(uint32_t) == 0, "");

struct TOKSTR_GMM_EXISTING_SYS_MEM_REC {
    TokenVariableLength base;

    TOKSTR_GMM_EXISTING_SYS_MEM_REC(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR_GMM_EXISTING_SYS_MEM_REC, IsGmmAllocated) + sizeof(IsGmmAllocated) - offsetof(TOKSTR_GMM_EXISTING_SYS_MEM_REC, pExistingSysMem), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR_GMM_EXISTING_SYS_MEM_REC()
        : base(TOK_S_GMM_EXISTING_SYS_MEM_REC, 0, sizeof(*this) - sizeof(base)) {}

    TokenQword pExistingSysMem = {TOK_FBQ_GMM_EXISTING_SYS_MEM_REC__P_EXISTING_SYS_MEM};
    TokenQword pVirtAddress = {TOK_FBQ_GMM_EXISTING_SYS_MEM_REC__P_VIRT_ADDRESS};
    TokenQword pGfxAlignedVirtAddress = {TOK_FBQ_GMM_EXISTING_SYS_MEM_REC__P_GFX_ALIGNED_VIRT_ADDRESS};
    TokenQword Size = {TOK_FBQ_GMM_EXISTING_SYS_MEM_REC__SIZE};
    TokenDword IsGmmAllocated = {TOK_FBC_GMM_EXISTING_SYS_MEM_REC__IS_GMM_ALLOCATED};
};
static_assert(std::is_standard_layout_v<TOKSTR_GMM_EXISTING_SYS_MEM_REC>, "");
static_assert(sizeof(TOKSTR_GMM_EXISTING_SYS_MEM_REC) % sizeof(uint32_t) == 0, "");

struct TOKSTR_GmmResourceInfoCommonStruct {
    TokenVariableLength base;

    TOKSTR_GmmResourceInfoCommonStruct(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR_GmmResourceInfoCommonStruct, MultiTileArch) + sizeof(MultiTileArch) - offsetof(TOKSTR_GmmResourceInfoCommonStruct, ClientType), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR_GmmResourceInfoCommonStruct()
        : base(TOK_S_GMM_RESOURCE_INFO_COMMON_STRUCT, 0, sizeof(*this) - sizeof(base)) {}

    TokenDword ClientType = {TOK_FE_GMM_RESOURCE_INFO_COMMON_STRUCT__CLIENT_TYPE};
    TOKSTR_GMM_TEXTURE_INFO_REC Surf = {TOK_FS_GMM_RESOURCE_INFO_COMMON_STRUCT__SURF};
    TOKSTR_GMM_TEXTURE_INFO_REC AuxSurf = {TOK_FS_GMM_RESOURCE_INFO_COMMON_STRUCT__AUX_SURF};
    TOKSTR_GMM_TEXTURE_INFO_REC AuxSecSurf = {TOK_FS_GMM_RESOURCE_INFO_COMMON_STRUCT__AUX_SEC_SURF};
    TokenDword RotateInfo = {TOK_FBD_GMM_RESOURCE_INFO_COMMON_STRUCT__ROTATE_INFO};
    TOKSTR_GMM_EXISTING_SYS_MEM_REC ExistingSysMem = {TOK_FS_GMM_RESOURCE_INFO_COMMON_STRUCT__EXISTING_SYS_MEM};
    TokenQword SvmAddress = {TOK_FBQ_GMM_RESOURCE_INFO_COMMON_STRUCT__SVM_ADDRESS};
    TokenQword pPrivateData = {TOK_FBQ_GMM_RESOURCE_INFO_COMMON_STRUCT__P_PRIVATE_DATA};
    TOKSTR_GMM_MULTI_TILE_ARCH_REC MultiTileArch = {TOK_FS_GMM_RESOURCE_INFO_COMMON_STRUCT__MULTI_TILE_ARCH};
};
static_assert(std::is_standard_layout_v<TOKSTR_GmmResourceInfoCommonStruct>, "");
static_assert(sizeof(TOKSTR_GmmResourceInfoCommonStruct) % sizeof(uint32_t) == 0, "");

struct TOKSTR_GmmResourceInfoWinStruct {
    TokenVariableLength base;

    TOKSTR_GmmResourceInfoWinStruct(uint16_t tokenId, uint32_t elementId = 0)
        : base(tokenId, elementId, offsetof(TOKSTR_GmmResourceInfoWinStruct, GmmResourceInfoCommon) + sizeof(GmmResourceInfoCommon) - offsetof(TOKSTR_GmmResourceInfoWinStruct, GmmResourceInfoCommon), (sizeof(*this) - sizeof(base)) / sizeof(uint32_t)) {}

    TOKSTR_GmmResourceInfoWinStruct()
        : base(TOK_S_GMM_RESOURCE_INFO_WIN_STRUCT, 0, sizeof(*this) - sizeof(base)) {}

    TOKSTR_GmmResourceInfoCommonStruct GmmResourceInfoCommon = {TOK_FS_GMM_RESOURCE_INFO_WIN_STRUCT__GMM_RESOURCE_INFO_COMMON};
};
static_assert(std::is_standard_layout_v<TOKSTR_GmmResourceInfoWinStruct>, "");
static_assert(sizeof(TOKSTR_GmmResourceInfoWinStruct) % sizeof(uint32_t) == 0, "");
