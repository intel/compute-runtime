/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma pack(1)

typedef struct tagBINDING_TABLE_STATE {
    union tagTheStructure {
        struct tagCommon {
            uint32_t Reserved_0 : BITFIELD_RANGE(0, 5);
            uint32_t SurfaceStatePointer : BITFIELD_RANGE(6, 31);
        } Common;
        uint32_t RawData[1];
    } TheStructure;
    typedef enum tagPATCH_CONSTANTS {
        SURFACESTATEPOINTER_BYTEOFFSET = 0x0,
        SURFACESTATEPOINTER_INDEX = 0x0,
    } PATCH_CONSTANTS;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
    }
    static tagBINDING_TABLE_STATE sInit() {
        BINDING_TABLE_STATE state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 1);
        return TheStructure.RawData[index];
    }
    inline const uint32_t &getRawData(const uint32_t index) const {
        DEBUG_BREAK_IF(index >= 1);
        return TheStructure.RawData[index];
    }
    typedef enum tagSURFACESTATEPOINTER {
        SURFACESTATEPOINTER_BIT_SHIFT = 0x6,
        SURFACESTATEPOINTER_ALIGN_SIZE = 0x40,
    } SURFACESTATEPOINTER;
    inline void setSurfaceStatePointer(const uint64_t value) {
        DEBUG_BREAK_IF(value >= 0x100000000);
        TheStructure.Common.SurfaceStatePointer = (uint32_t)value >> SURFACESTATEPOINTER_BIT_SHIFT;
    }
    inline uint32_t getSurfaceStatePointer() const {
        return (TheStructure.Common.SurfaceStatePointer << SURFACESTATEPOINTER_BIT_SHIFT);
    }
} BINDING_TABLE_STATE;
STATIC_ASSERT(4 == sizeof(BINDING_TABLE_STATE));

typedef struct tagMEDIA_SURFACE_STATE {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t CompressionFormat : BITFIELD_RANGE(0, 3);
            uint32_t Reserved_4 : BITFIELD_RANGE(4, 29);
            uint32_t Rotation : BITFIELD_RANGE(30, 31);
            // DWORD 1
            uint32_t Cr_VCb_UPixelOffsetVDirection : BITFIELD_RANGE(0, 1);
            uint32_t PictureStructure : BITFIELD_RANGE(2, 3);
            uint32_t Width : BITFIELD_RANGE(4, 17);
            uint32_t Height : BITFIELD_RANGE(18, 31);
            // DWORD 2
            uint32_t TileMode : BITFIELD_RANGE(0, 1);
            uint32_t HalfPitchForChroma : BITFIELD_RANGE(2, 2);
            uint32_t SurfacePitch : BITFIELD_RANGE(3, 20);
            uint32_t AddressControl : BITFIELD_RANGE(21, 21);
            uint32_t CompressionAccumulationBufferEnable : BITFIELD_RANGE(22, 22);
            uint32_t Reserved_87 : BITFIELD_RANGE(23, 23);
            uint32_t Cr_VCb_UPixelOffsetVDirectionMsb : BITFIELD_RANGE(24, 24);
            uint32_t Cr_VCb_UPixelOffsetUDirection : BITFIELD_RANGE(25, 25);
            uint32_t InterleaveChroma : BITFIELD_RANGE(26, 26);
            uint32_t SurfaceFormat : BITFIELD_RANGE(27, 31);
            // DWORD 3
            uint32_t YOffsetForU_Cb : BITFIELD_RANGE(0, 13);
            uint32_t Reserved_110 : BITFIELD_RANGE(14, 15);
            uint32_t XOffsetForU_Cb : BITFIELD_RANGE(16, 29);
            uint32_t Reserved_126 : BITFIELD_RANGE(30, 31);
            // DWORD 4
            uint32_t Reserved_128;
            // DWORD 5
            uint32_t SurfaceMemoryObjectControlStateEncryptedData : BITFIELD_RANGE(0, 0);
            uint32_t SurfaceMemoryObjectControlStateIndexToMocsTables : BITFIELD_RANGE(1, 6);
            uint32_t Reserved_167 : BITFIELD_RANGE(7, 29);
            uint32_t VerticalLineStrideOffset : BITFIELD_RANGE(30, 30);
            uint32_t VerticalLineStride : BITFIELD_RANGE(31, 31);
            // DWORD 6
            uint32_t SurfaceBaseAddress;
            // DWORD 7
            uint32_t SurfaceBaseAddressHigh : BITFIELD_RANGE(0, 15);
            uint32_t Reserved_240 : BITFIELD_RANGE(16, 31);
        } Common;
        struct tagSurfaceFormatIsOneOfPlanarFormats {
            // DWORD 0
            uint32_t Reserved_0 : BITFIELD_RANGE(0, 15);
            uint32_t YOffset : BITFIELD_RANGE(16, 19);
            uint32_t XOffset : BITFIELD_RANGE(20, 26);
            uint32_t Reserved_27 : BITFIELD_RANGE(27, 31);
            // DWORD 1
            uint32_t Reserved_32;
            // DWORD 2
            uint32_t Reserved_64;
            // DWORD 3
            uint32_t Reserved_96;
            // DWORD 4
            uint32_t Reserved_128;
            // DWORD 5
            uint32_t Reserved_160;
            // DWORD 6
            uint32_t Reserved_192;
            // DWORD 7
            uint32_t Reserved_224;
        } SurfaceFormatIsOneOfPlanarFormats;
        struct tag_SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0 {
            // DWORD 0
            uint32_t Reserved_0;
            // DWORD 1
            uint32_t Reserved_32;
            // DWORD 2
            uint32_t Reserved_64;
            // DWORD 3
            uint32_t Reserved_96;
            // DWORD 4
            uint32_t YOffsetForV_Cr : BITFIELD_RANGE(0, 14);
            uint32_t Reserved_143 : BITFIELD_RANGE(15, 15);
            uint32_t XOffsetForV_Cr : BITFIELD_RANGE(16, 29);
            uint32_t Reserved_158 : BITFIELD_RANGE(30, 31);
            // DWORD 5
            uint32_t Reserved_160;
            // DWORD 6
            uint32_t Reserved_192;
            // DWORD 7
            uint32_t Reserved_224;
        } _SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0;
        uint32_t RawData[8];
    } TheStructure;
    typedef enum tagCOMPRESSION_FORMAT {
        COMPRESSION_FORMAT_CMF_R8 = 0x0,
        COMPRESSION_FORMAT_CMF_R8_G8 = 0x1,
        COMPRESSION_FORMAT_CMF_R8_G8_B8_A8 = 0x2,
        COMPRESSION_FORMAT_CMF_R10_G10_B10_A2 = 0x3,
        COMPRESSION_FORMAT_CMF_R11_G11_B10 = 0x4,
        COMPRESSION_FORMAT_CMF_R16 = 0x5,
        COMPRESSION_FORMAT_CMF_R16_G16 = 0x6,
        COMPRESSION_FORMAT_CMF_R16_G16_B16_A16 = 0x7,
        COMPRESSION_FORMAT_CMF_R32 = 0x8,
        COMPRESSION_FORMAT_CMF_R32_G32 = 0x9,
        COMPRESSION_FORMAT_CMF_R32_G32_B32_A32 = 0xa,
        COMPRESSION_FORMAT_CMF_Y16_U16_Y16_V16 = 0xb,
        COMPRESSION_FORMAT_CMF_ML8 = 0xf,
    } COMPRESSION_FORMAT;
    typedef enum tagROTATION {
        ROTATION_NO_ROTATION_OR_0_DEGREE = 0x0,
        ROTATION_90_DEGREE_ROTATION = 0x1,
        ROTATION_180_DEGREE_ROTATION = 0x2,
        ROTATION_270_DEGREE_ROTATION = 0x3,
    } ROTATION;
    typedef enum tagPICTURE_STRUCTURE {
        PICTURE_STRUCTURE_FRAME_PICTURE = 0x0,
        PICTURE_STRUCTURE_TOP_FIELD_PICTURE = 0x1,
        PICTURE_STRUCTURE_BOTTOM_FIELD_PICTURE = 0x2,
        PICTURE_STRUCTURE_INVALID_NOT_ALLOWED = 0x3,
    } PICTURE_STRUCTURE;
    typedef enum tagTILE_MODE {
        TILE_MODE_TILEMODE_LINEAR = 0x0,
        TILE_MODE_TILES_64K = 0x1,
        TILE_MODE_TILEMODE_XMAJOR = 0x2,
        TILE_MODE_TILEMODE_YMAJOR = 0x3, // patched TILE_MODE_TILEF
    } TILE_MODE;
    typedef enum tagADDRESS_CONTROL {
        ADDRESS_CONTROL_CLAMP = 0x0,
        ADDRESS_CONTROL_MIRROR = 0x1,
    } ADDRESS_CONTROL;
    typedef enum tagSURFACE_FORMAT {
        SURFACE_FORMAT_YCRCB_NORMAL = 0x0,
        SURFACE_FORMAT_YCRCB_SWAPUVY = 0x1,
        SURFACE_FORMAT_YCRCB_SWAPUV = 0x2,
        SURFACE_FORMAT_YCRCB_SWAPY = 0x3,
        SURFACE_FORMAT_PLANAR_420_8 = 0x4,
        SURFACE_FORMAT_Y8_UNORM_VA = 0x5, // patched
        SURFACE_FORMAT_R10G10B10A2_UNORM = 0x8,
        SURFACE_FORMAT_R8G8B8A8_UNORM = 0x9,
        SURFACE_FORMAT_Y1_UNORM = 0x10, // patched
        SURFACE_FORMAT_R8B8_UNORM_CRCB = 0xa,
        SURFACE_FORMAT_R8_UNORM_CRCB = 0xb,
        SURFACE_FORMAT_Y8_UNORM = 0xc,
        SURFACE_FORMAT_A8Y8U8V8_UNORM = 0xd,
        SURFACE_FORMAT_B8G8R8A8_UNORM = 0xe,
        SURFACE_FORMAT_R16G16B16A16 = 0xf,
        SURFACE_FORMAT_PLANAR_422_8 = 0x12,
        SURFACE_FORMAT_PLANAR_420_16 = 0x17,
        SURFACE_FORMAT_R16B16_UNORM_CRCB = 0x18,
        SURFACE_FORMAT_R16_UNORM_CRCB = 0x19,
        SURFACE_FORMAT_Y16_UNORM = 0x1a,
    } SURFACE_FORMAT;
    typedef enum tagSURFACE_MEMORY_OBJECT_CONTROL_STATE {
        SURFACE_MEMORY_OBJECT_CONTROL_STATE_DEFAULTVAUEDESC = 0x0,
    } SURFACE_MEMORY_OBJECT_CONTROL_STATE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.CompressionFormat = COMPRESSION_FORMAT_CMF_R8;
        TheStructure.Common.Rotation = ROTATION_NO_ROTATION_OR_0_DEGREE;
        TheStructure.Common.PictureStructure = PICTURE_STRUCTURE_FRAME_PICTURE;
        TheStructure.Common.TileMode = TILE_MODE_TILEMODE_LINEAR;
        TheStructure.Common.AddressControl = ADDRESS_CONTROL_CLAMP;
        TheStructure.Common.SurfaceFormat = SURFACE_FORMAT_YCRCB_NORMAL;
    }
    static tagMEDIA_SURFACE_STATE sInit() {
        MEDIA_SURFACE_STATE state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 8);
        return TheStructure.RawData[index];
    }
    inline void setCompressionFormat(const COMPRESSION_FORMAT value) {
        TheStructure.Common.CompressionFormat = value;
    }
    inline COMPRESSION_FORMAT getCompressionFormat() const {
        return static_cast<COMPRESSION_FORMAT>(TheStructure.Common.CompressionFormat);
    }
    inline void setRotation(const ROTATION value) {
        TheStructure.Common.Rotation = value;
    }
    inline ROTATION getRotation() const {
        return static_cast<ROTATION>(TheStructure.Common.Rotation);
    }
    inline void setCrVCbUPixelOffsetVDirection(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3);
        TheStructure.Common.Cr_VCb_UPixelOffsetVDirection = value;
    }
    inline uint32_t getCrVCbUPixelOffsetVDirection() const {
        return TheStructure.Common.Cr_VCb_UPixelOffsetVDirection;
    }
    inline void setPictureStructure(const PICTURE_STRUCTURE value) {
        TheStructure.Common.PictureStructure = value;
    }
    inline PICTURE_STRUCTURE getPictureStructure() const {
        return static_cast<PICTURE_STRUCTURE>(TheStructure.Common.PictureStructure);
    }
    inline void setWidth(const uint32_t value) {
        UNRECOVERABLE_IF(value - 1 > 0x3fff);
        TheStructure.Common.Width = value - 1;
    }
    inline uint32_t getWidth() const {
        return TheStructure.Common.Width + 1;
    }
    inline void setHeight(const uint32_t value) {
        UNRECOVERABLE_IF(value - 1 > 0x3fff);
        TheStructure.Common.Height = value - 1;
    }
    inline uint32_t getHeight() const {
        return TheStructure.Common.Height + 1;
    }
    inline void setTileMode(const TILE_MODE value) {
        TheStructure.Common.TileMode = value;
    }
    inline TILE_MODE getTileMode() const {
        return static_cast<TILE_MODE>(TheStructure.Common.TileMode);
    }
    inline void setHalfPitchForChroma(const bool value) {
        TheStructure.Common.HalfPitchForChroma = value;
    }
    inline bool getHalfPitchForChroma() const {
        return TheStructure.Common.HalfPitchForChroma;
    }
    inline void setSurfacePitch(const uint32_t value) {
        UNRECOVERABLE_IF(value - 1 > 0x3ffff);
        TheStructure.Common.SurfacePitch = value - 1;
    }
    inline uint32_t getSurfacePitch() const {
        return TheStructure.Common.SurfacePitch + 1;
    }
    inline void setAddressControl(const ADDRESS_CONTROL value) {
        TheStructure.Common.AddressControl = value;
    }
    inline ADDRESS_CONTROL getAddressControl() const {
        return static_cast<ADDRESS_CONTROL>(TheStructure.Common.AddressControl);
    }
    inline void setCompressionAccumulationBufferEnable(const bool value) {
        TheStructure.Common.CompressionAccumulationBufferEnable = value;
    }
    inline bool getCompressionAccumulationBufferEnable() const {
        return TheStructure.Common.CompressionAccumulationBufferEnable;
    }
    inline void setCrVCbUPixelOffsetVDirectionMsb(const bool value) {
        TheStructure.Common.Cr_VCb_UPixelOffsetVDirectionMsb = value;
    }
    inline bool getCrVCbUPixelOffsetVDirectionMsb() const {
        return TheStructure.Common.Cr_VCb_UPixelOffsetVDirectionMsb;
    }
    inline void setCrVCbUPixelOffsetUDirection(const bool value) {
        TheStructure.Common.Cr_VCb_UPixelOffsetUDirection = value;
    }
    inline bool getCrVCbUPixelOffsetUDirection() const {
        return TheStructure.Common.Cr_VCb_UPixelOffsetUDirection;
    }
    inline void setInterleaveChroma(const bool value) {
        TheStructure.Common.InterleaveChroma = value;
    }
    inline bool getInterleaveChroma() const {
        return TheStructure.Common.InterleaveChroma;
    }
    inline void setSurfaceFormat(const SURFACE_FORMAT value) {
        TheStructure.Common.SurfaceFormat = value;
    }
    inline SURFACE_FORMAT getSurfaceFormat() const {
        return static_cast<SURFACE_FORMAT>(TheStructure.Common.SurfaceFormat);
    }
    inline void setYOffsetForUCb(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3fff);
        TheStructure.Common.YOffsetForU_Cb = value;
    }
    inline uint32_t getYOffsetForUCb() const {
        return TheStructure.Common.YOffsetForU_Cb;
    }
    inline void setXOffsetForUCb(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3fff);
        TheStructure.Common.XOffsetForU_Cb = value;
    }
    inline uint32_t getXOffsetForUCb() const {
        return TheStructure.Common.XOffsetForU_Cb;
    }
    inline void setSurfaceMemoryObjectControlStateEncryptedData(const bool value) {
        TheStructure.Common.SurfaceMemoryObjectControlStateEncryptedData = value;
    }
    inline bool getSurfaceMemoryObjectControlStateEncryptedData() const {
        return TheStructure.Common.SurfaceMemoryObjectControlStateEncryptedData;
    }
    inline void setSurfaceMemoryObjectControlState(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x7f);
        setSurfaceMemoryObjectControlStateEncryptedData(value & 1);
        TheStructure.Common.SurfaceMemoryObjectControlStateIndexToMocsTables = value >> 1;
    }
    inline uint32_t getSurfaceMemoryObjectControlState() const {
        uint32_t mocs = getSurfaceMemoryObjectControlStateEncryptedData();
        return mocs | (TheStructure.Common.SurfaceMemoryObjectControlStateIndexToMocsTables << 1);
    }
    inline void setVerticalLineStrideOffset(const bool value) {
        TheStructure.Common.VerticalLineStrideOffset = value;
    }
    inline bool getVerticalLineStrideOffset() const {
        return TheStructure.Common.VerticalLineStrideOffset;
    }
    inline void setVerticalLineStride(const bool value) {
        TheStructure.Common.VerticalLineStride = value;
    }
    inline bool getVerticalLineStride() const {
        return TheStructure.Common.VerticalLineStride;
    }
    inline void setSurfaceBaseAddress(const uint64_t value) { // patched
        TheStructure.Common.SurfaceBaseAddress = (value & 0xffffffff);
        TheStructure.Common.SurfaceBaseAddressHigh = (value >> 32) & 0xffffffff;
    }
    inline uint64_t getSurfaceBaseAddress() const { // patched
        uint64_t addressLow = TheStructure.Common.SurfaceBaseAddress;
        return (addressLow | static_cast<uint64_t>(TheStructure.Common.SurfaceBaseAddressHigh) << 32);
    }
    inline void setSurfaceBaseAddressHigh(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffff);
        TheStructure.Common.SurfaceBaseAddressHigh = value;
    }
    inline uint32_t getSurfaceBaseAddressHigh() const {
        return TheStructure.Common.SurfaceBaseAddressHigh;
    }
    inline void setYOffset(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xf);
        TheStructure.SurfaceFormatIsOneOfPlanarFormats.YOffset = value;
    }
    inline uint32_t getYOffset() const {
        return TheStructure.SurfaceFormatIsOneOfPlanarFormats.YOffset;
    }
    inline void setXOffset(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x7f);
        TheStructure.SurfaceFormatIsOneOfPlanarFormats.XOffset = value;
    }
    inline uint32_t getXOffset() const {
        return TheStructure.SurfaceFormatIsOneOfPlanarFormats.XOffset;
    }
    inline void setYOffsetForVCr(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x7fff);
        TheStructure._SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0.YOffsetForV_Cr = value;
    }
    inline uint32_t getYOffsetForVCr() const {
        return TheStructure._SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0.YOffsetForV_Cr;
    }
    inline void setXOffsetForVCr(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3fff);
        TheStructure._SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0.XOffsetForV_Cr = value;
    }
    inline uint32_t getXOffsetForVCr() const {
        return TheStructure._SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0.XOffsetForV_Cr;
    }
} MEDIA_SURFACE_STATE;
STATIC_ASSERT(32 == sizeof(MEDIA_SURFACE_STATE));

typedef struct tagMEM_COPY {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 8);
            uint32_t CompressionFormat30 : BITFIELD_RANGE(9, 12);
            uint32_t Reserved_13 : BITFIELD_RANGE(13, 16);
            uint32_t CopyType : BITFIELD_RANGE(17, 18);
            uint32_t Mode : BITFIELD_RANGE(19, 19);
            uint32_t Reserved_20 : BITFIELD_RANGE(20, 21);
            uint32_t InstructionTarget_Opcode : BITFIELD_RANGE(22, 28);
            uint32_t Client : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint32_t TransferWidth : BITFIELD_RANGE(0, 23);
            uint32_t Reserved_56 : BITFIELD_RANGE(24, 31);
            // DWORD 2
            uint32_t TransferHeight : BITFIELD_RANGE(0, 17);
            uint32_t Reserved_82 : BITFIELD_RANGE(18, 31);
            // DWORD 3
            uint32_t SourcePitch : BITFIELD_RANGE(0, 17);
            uint32_t Reserved_114 : BITFIELD_RANGE(18, 31);
            // DWORD 4
            uint32_t DestinationPitch : BITFIELD_RANGE(0, 17);
            uint32_t Reserved_146 : BITFIELD_RANGE(18, 31);
            // DWORD 5
            uint32_t SourceStartAddressLow;
            // DWORD 6
            uint32_t SourceStartAddressHigh;
            // DWORD 7
            uint32_t DestinationStartAddressLow;
            // DWORD 8
            uint32_t DestinationStartAddressHigh;
            // DWORD 9
            uint32_t DestinationEncryptEn : BITFIELD_RANGE(0, 0);
            uint32_t Reserved_289 : BITFIELD_RANGE(1, 2);
            uint32_t DestinationMocs : BITFIELD_RANGE(3, 6);
            uint32_t Reserved_295 : BITFIELD_RANGE(7, 24);
            uint32_t SourceEncryptEn : BITFIELD_RANGE(25, 25);
            uint32_t Reserved_314 : BITFIELD_RANGE(26, 27);
            uint32_t SourceMocs : BITFIELD_RANGE(28, 31);
        } Common;
        uint32_t RawData[10];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x8,
    } DWORD_LENGTH;
    typedef enum tagCOMPRESSION_FORMAT30 {
        COMPRESSION_FORMAT30_CMF_R8 = 0x0,
        COMPRESSION_FORMAT30_CMF_R8_G8 = 0x1,
        COMPRESSION_FORMAT30_CMF_R8_G8_B8_A8 = 0x2,
        COMPRESSION_FORMAT30_CMF_R10_G10_B10_A2 = 0x3,
        COMPRESSION_FORMAT30_CMF_R11_G11_B10 = 0x4,
        COMPRESSION_FORMAT30_CMF_R16 = 0x5,
        COMPRESSION_FORMAT30_CMF_R16_G16 = 0x6,
        COMPRESSION_FORMAT30_CMF_R16_G16_B16_A16 = 0x7,
        COMPRESSION_FORMAT30_CMF_R32 = 0x8,
        COMPRESSION_FORMAT30_CMF_R32_G32 = 0x9,
        COMPRESSION_FORMAT30_CMF_R32_G32_B32_A32 = 0xa,
        COMPRESSION_FORMAT30_CMF_Y16_U16_Y16_V16 = 0xb,
        COMPRESSION_FORMAT30_CMF_ML8 = 0xf,
    } COMPRESSION_FORMAT30;
    typedef enum tagCOPY_TYPE {
        COPY_TYPE_LINEAR_COPY = 0x0,
        COPY_TYPE_MATRIX_COPY = 0x1,
    } COPY_TYPE;
    typedef enum tagMODE {
        MODE_BYTE_COPY = 0x0,
        MODE_PAGE_COPY = 0x1,
    } MODE;
    typedef enum tagINSTRUCTION_TARGETOPCODE {
        INSTRUCTIONTARGET_OPCODE_OPCODE = 0x5a,
    } INSTRUCTION_TARGETOPCODE;
    typedef enum tagCLIENT {
        CLIENT_2D_PROCESSOR = 0x2,
    } CLIENT;
    typedef enum tagDESTINATION_ENCRYPT_EN {
        DESTINATION_ENCRYPT_EN_DISABLE = 0x0,
        DESTINATION_ENCRYPT_EN_ENABLE = 0x1,
    } DESTINATION_ENCRYPT_EN;
    typedef enum tagSOURCE_ENCRYPT_EN {
        SOURCE_ENCRYPT_EN_DISABLE = 0x0,
        SOURCE_ENCRYPT_EN_ENABLE = 0x1,
    } SOURCE_ENCRYPT_EN;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.CompressionFormat30 = COMPRESSION_FORMAT30_CMF_R8;
        TheStructure.Common.CopyType = COPY_TYPE_LINEAR_COPY;
        TheStructure.Common.Mode = MODE_BYTE_COPY;
        TheStructure.Common.InstructionTarget_Opcode = INSTRUCTIONTARGET_OPCODE_OPCODE;
        TheStructure.Common.Client = CLIENT_2D_PROCESSOR;
        TheStructure.Common.DestinationEncryptEn = DESTINATION_ENCRYPT_EN_DISABLE;
        TheStructure.Common.SourceEncryptEn = SOURCE_ENCRYPT_EN_DISABLE;
    }
    static tagMEM_COPY sInit() {
        MEM_COPY state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 10);
        return TheStructure.RawData[index];
    }
    inline void setCompressionFormat(const COMPRESSION_FORMAT30 value) {
        TheStructure.Common.CompressionFormat30 = value;
    }
    inline COMPRESSION_FORMAT30 getCompressionFormat() const {
        return static_cast<COMPRESSION_FORMAT30>(TheStructure.Common.CompressionFormat30);
    }
    inline void setCopyType(const COPY_TYPE value) {
        TheStructure.Common.CopyType = value;
    }
    inline COPY_TYPE getCopyType() const {
        return static_cast<COPY_TYPE>(TheStructure.Common.CopyType);
    }
    inline void setMode(const MODE value) {
        TheStructure.Common.Mode = value;
    }
    inline MODE getMode() const {
        return static_cast<MODE>(TheStructure.Common.Mode);
    }
    inline void setInstructionTargetOpcode(const INSTRUCTION_TARGETOPCODE value) {
        TheStructure.Common.InstructionTarget_Opcode = value;
    }
    inline INSTRUCTION_TARGETOPCODE getInstructionTargetOpcode() const {
        return static_cast<INSTRUCTION_TARGETOPCODE>(TheStructure.Common.InstructionTarget_Opcode);
    }
    inline void setClient(const CLIENT value) {
        TheStructure.Common.Client = value;
    }
    inline CLIENT getClient() const {
        return static_cast<CLIENT>(TheStructure.Common.Client);
    }
    inline void setDestinationX2CoordinateRight(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffffff + 1);
        TheStructure.Common.TransferWidth = value - 1; // patched
    }
    inline uint32_t getDestinationX2CoordinateRight() const {
        return TheStructure.Common.TransferWidth + 1; // patched
    }
    inline void setDestinationY2CoordinateBottom(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3ffff + 1);
        TheStructure.Common.TransferHeight = value - 1;
    }
    inline uint32_t getDestinationY2CoordinateBottom() const {
        return TheStructure.Common.TransferHeight + 1;
    }
    inline void setSourcePitch(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3ffff + 1);
        TheStructure.Common.SourcePitch = value - 1;
    }
    inline uint32_t getSourcePitch() const {
        return TheStructure.Common.SourcePitch + 1;
    }
    inline void setDestinationPitch(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3ffff + 1);
        TheStructure.Common.DestinationPitch = value - 1;
    }
    inline uint32_t getDestinationPitch() const {
        return TheStructure.Common.DestinationPitch + 1;
    }
    inline void setSourceBaseAddress(const uint64_t value) { // patched
        TheStructure.Common.SourceStartAddressLow = value & 0xffffffff;
        TheStructure.Common.SourceStartAddressHigh = (value >> 32) & 0xffffffff;
    }
    inline uint64_t getSourceBaseAddress() const { // patched
        return (static_cast<uint64_t>(TheStructure.Common.SourceStartAddressHigh) << 32) | (TheStructure.Common.SourceStartAddressLow);
    }
    inline void setDestinationBaseAddress(const uint64_t value) { // patched
        TheStructure.Common.DestinationStartAddressLow = value & 0xffffffff;
        TheStructure.Common.DestinationStartAddressHigh = (value >> 32) & 0xffffffff;
    }
    inline uint64_t getDestinationBaseAddress() const { // patched
        return (static_cast<uint64_t>(TheStructure.Common.DestinationStartAddressHigh) << 32) | (TheStructure.Common.DestinationStartAddressLow);
    }
    inline void setDestinationEncryptEn(const DESTINATION_ENCRYPT_EN value) {
        TheStructure.Common.DestinationEncryptEn = value;
    }
    inline DESTINATION_ENCRYPT_EN getDestinationEncryptEn() const {
        return static_cast<DESTINATION_ENCRYPT_EN>(TheStructure.Common.DestinationEncryptEn);
    }
    inline void setDestinationMOCS(const uint32_t value) { // patched
        setDestinationEncryptEn(static_cast<DESTINATION_ENCRYPT_EN>(value & 1));
        UNRECOVERABLE_IF((value >> 1) > 0xf);
        TheStructure.Common.DestinationMocs = value >> 1;
    }
    inline uint32_t getDestinationMOCS() const { // patched
        return (TheStructure.Common.DestinationMocs << 1) | static_cast<uint32_t>(getDestinationEncryptEn());
    }
    inline void setSourceEncryptEn(const SOURCE_ENCRYPT_EN value) {
        TheStructure.Common.SourceEncryptEn = value;
    }
    inline SOURCE_ENCRYPT_EN getSourceEncryptEn() const {
        return static_cast<SOURCE_ENCRYPT_EN>(TheStructure.Common.SourceEncryptEn);
    }
    inline void setSourceMOCS(const uint32_t value) { // patched
        setSourceEncryptEn(static_cast<SOURCE_ENCRYPT_EN>(value & 1));
        UNRECOVERABLE_IF((value >> 1) > 0xf);
        TheStructure.Common.SourceMocs = value >> 1;
    }
    inline uint32_t getSourceMOCS() const { // patched
        return (TheStructure.Common.SourceMocs << 1) | static_cast<uint32_t>(getSourceEncryptEn());
    }
} MEM_COPY;
STATIC_ASSERT(40 == sizeof(MEM_COPY));

typedef struct tagMI_MATH {
    union _DW0 {
        struct _BitField {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t MemoryObjectControlState : BITFIELD_RANGE(8, 14);
            uint32_t Reserved : BITFIELD_RANGE(15, 22);
            uint32_t InstructionOpcode : BITFIELD_RANGE(23, 28);
            uint32_t InstructionType : BITFIELD_RANGE(29, 31);
        } BitField;
        uint32_t Value;
    } DW0;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_MATH = 0x1A,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
} MI_MATH;

typedef struct tagMI_MATH_ALU_INST_INLINE {
    union _DW0 {
        struct _BitField {
            uint32_t Operand2 : BITFIELD_RANGE(0, 9);
            uint32_t Operand1 : BITFIELD_RANGE(10, 19);
            uint32_t ALUOpcode : BITFIELD_RANGE(20, 31);
        } BitField;
        uint32_t Value;
    } DW0;
} MI_MATH_ALU_INST_INLINE;

typedef struct tagPIPE_CONTROL {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t PredicateEnable : BITFIELD_RANGE(8, 8);
            uint32_t DataportFlush : BITFIELD_RANGE(9, 9);
            uint32_t L3ReadOnlyCacheInvalidationEnable : BITFIELD_RANGE(10, 10);
            uint32_t UnTypedDataPortCacheFlush : BITFIELD_RANGE(11, 11);
            uint32_t Reserved_12 : BITFIELD_RANGE(12, 12);
            uint32_t CompressionControlSurface_CcsFlush : BITFIELD_RANGE(13, 13);
            uint32_t WorkloadPartitionIdOffsetEnable : BITFIELD_RANGE(14, 14);
            uint32_t Reserved_15 : BITFIELD_RANGE(15, 15);
            uint32_t _3DCommandSubOpcode : BITFIELD_RANGE(16, 23);
            uint32_t _3DCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t CommandSubtype : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint32_t DepthCacheFlushEnable : BITFIELD_RANGE(0, 0);
            uint32_t StallAtPixelScoreboard : BITFIELD_RANGE(1, 1);
            uint32_t StateCacheInvalidationEnable : BITFIELD_RANGE(2, 2);
            uint32_t ConstantCacheInvalidationEnable : BITFIELD_RANGE(3, 3);
            uint32_t VfCacheInvalidationEnable : BITFIELD_RANGE(4, 4);
            uint32_t DcFlushEnable : BITFIELD_RANGE(5, 5);
            uint32_t ProtectedMemoryApplicationId : BITFIELD_RANGE(6, 6);
            uint32_t PipeControlFlushEnable : BITFIELD_RANGE(7, 7);
            uint32_t NotifyEnable : BITFIELD_RANGE(8, 8);
            uint32_t IndirectStatePointersDisable : BITFIELD_RANGE(9, 9);
            uint32_t TextureCacheInvalidationEnable : BITFIELD_RANGE(10, 10);
            uint32_t InstructionCacheInvalidateEnable : BITFIELD_RANGE(11, 11);
            uint32_t RenderTargetCacheFlushEnable : BITFIELD_RANGE(12, 12);
            uint32_t DepthStallEnable : BITFIELD_RANGE(13, 13);
            uint32_t PostSyncOperation : BITFIELD_RANGE(14, 15);
            uint32_t Reserved_48 : BITFIELD_RANGE(16, 16);
            uint32_t PssStallSyncEnable : BITFIELD_RANGE(17, 17);
            uint32_t TlbInvalidate : BITFIELD_RANGE(18, 18);
            uint32_t DepthStallSyncEnable : BITFIELD_RANGE(19, 19);
            uint32_t CommandStreamerStallEnable : BITFIELD_RANGE(20, 20);
            uint32_t StoreDataIndex : BITFIELD_RANGE(21, 21);
            uint32_t ProtectedMemoryEnable : BITFIELD_RANGE(22, 22);
            uint32_t LriPostSyncOperation : BITFIELD_RANGE(23, 23);
            uint32_t DestinationAddressType : BITFIELD_RANGE(24, 24);
            uint32_t AmfsFlushEnable : BITFIELD_RANGE(25, 25);
            uint32_t Reserved_58 : BITFIELD_RANGE(26, 26);
            uint32_t ProtectedMemoryDisable : BITFIELD_RANGE(27, 27);
            uint32_t Reserved_60 : BITFIELD_RANGE(28, 28);
            uint32_t CommandCacheInvalidateEnable : BITFIELD_RANGE(29, 29);
            uint32_t FlushEuStallBuffer : BITFIELD_RANGE(30, 30);
            uint32_t TbimrForceBatchClosure : BITFIELD_RANGE(31, 31);
            // DWORD 2
            uint32_t Reserved_64 : BITFIELD_RANGE(0, 1);
            uint32_t Address : BITFIELD_RANGE(2, 31);
            // DWORD 3
            uint32_t AddressHigh;
            // DWORD 4
            uint64_t ImmediateData;
        } Common;
        uint32_t RawData[6];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_DWORD_COUNT_N = 0x4,
    } DWORD_LENGTH;
    typedef enum tag_3D_COMMAND_SUB_OPCODE {
        _3D_COMMAND_SUB_OPCODE_PIPE_CONTROL = 0x0,
    } _3D_COMMAND_SUB_OPCODE;
    typedef enum tag_3D_COMMAND_OPCODE {
        _3D_COMMAND_OPCODE_PIPE_CONTROL = 0x2,
    } _3D_COMMAND_OPCODE;
    typedef enum tagCOMMAND_SUBTYPE {
        COMMAND_SUBTYPE_GFXPIPE_3D = 0x3,
    } COMMAND_SUBTYPE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_GFXPIPE = 0x3,
    } COMMAND_TYPE;
    typedef enum tagPOST_SYNC_OPERATION {
        POST_SYNC_OPERATION_NO_WRITE = 0x0,
        POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA = 0x1,
        POST_SYNC_OPERATION_WRITE_PS_DEPTH_COUNT = 0x2,
        POST_SYNC_OPERATION_WRITE_TIMESTAMP = 0x3,
    } POST_SYNC_OPERATION;
    typedef enum tagLRI_POST_SYNC_OPERATION {
        LRI_POST_SYNC_OPERATION_NO_LRI_OPERATION = 0x0,
        LRI_POST_SYNC_OPERATION_MMIO_WRITE_IMMEDIATE_DATA = 0x1,
    } LRI_POST_SYNC_OPERATION;
    typedef enum tagDESTINATION_ADDRESS_TYPE {
        DESTINATION_ADDRESS_TYPE_PPGTT = 0x0,
        DESTINATION_ADDRESS_TYPE_GGTT = 0x1,
    } DESTINATION_ADDRESS_TYPE;
    typedef enum tagFLUSH_EU_STALL_BUFFER {
        FLUSH_EU_STALL_BUFFER_FLUSH_EU_STALL_DISABLE = 0x0,
        FLUSH_EU_STALL_BUFFER_FLUSH_EU_STALL_ENABLE = 0x1,
    } FLUSH_EU_STALL_BUFFER;
    typedef enum tagTBIMR_FORCE_BATCH_CLOSURE {
        TBIMR_FORCE_BATCH_CLOSURE_NO_BATCH_CLOSURE = 0x0,
        TBIMR_FORCE_BATCH_CLOSURE_CLOSE_BATCH = 0x1,
    } TBIMR_FORCE_BATCH_CLOSURE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_DWORD_COUNT_N;
        TheStructure.Common._3DCommandSubOpcode = _3D_COMMAND_SUB_OPCODE_PIPE_CONTROL;
        TheStructure.Common._3DCommandOpcode = _3D_COMMAND_OPCODE_PIPE_CONTROL;
        TheStructure.Common.CommandSubtype = COMMAND_SUBTYPE_GFXPIPE_3D;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
        TheStructure.Common.PostSyncOperation = POST_SYNC_OPERATION_NO_WRITE;
        TheStructure.Common.LriPostSyncOperation = LRI_POST_SYNC_OPERATION_NO_LRI_OPERATION;
        TheStructure.Common.DestinationAddressType = DESTINATION_ADDRESS_TYPE_PPGTT;
        TheStructure.Common.FlushEuStallBuffer = FLUSH_EU_STALL_BUFFER_FLUSH_EU_STALL_DISABLE;
        TheStructure.Common.TbimrForceBatchClosure = TBIMR_FORCE_BATCH_CLOSURE_NO_BATCH_CLOSURE;
        TheStructure.Common.CommandStreamerStallEnable = 1; // Patched
    }
    static tagPIPE_CONTROL sInit() {
        PIPE_CONTROL state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 6);
        return TheStructure.RawData[index];
    }
    inline void setPredicateEnable(const bool value) {
        TheStructure.Common.PredicateEnable = value;
    }
    inline bool getPredicateEnable() const {
        return TheStructure.Common.PredicateEnable;
    }
    inline void setDataportFlush(const bool value) {
        TheStructure.Common.DataportFlush = value;
    }
    inline bool getDataportFlush() const {
        return TheStructure.Common.DataportFlush;
    }
    inline void setL3ReadOnlyCacheInvalidationEnable(const bool value) {
        TheStructure.Common.L3ReadOnlyCacheInvalidationEnable = value;
    }
    inline bool getL3ReadOnlyCacheInvalidationEnable() const {
        return TheStructure.Common.L3ReadOnlyCacheInvalidationEnable;
    }
    inline void setUnTypedDataPortCacheFlush(const bool value) {
        TheStructure.Common.UnTypedDataPortCacheFlush = value;
    }
    inline bool getUnTypedDataPortCacheFlush() const {
        return TheStructure.Common.UnTypedDataPortCacheFlush;
    }
    inline void setCompressionControlSurfaceCcsFlush(const bool value) {
        TheStructure.Common.CompressionControlSurface_CcsFlush = value;
    }
    inline bool getCompressionControlSurfaceCcsFlush() const {
        return TheStructure.Common.CompressionControlSurface_CcsFlush;
    }
    inline void setWorkloadPartitionIdOffsetEnable(const bool value) {
        TheStructure.Common.WorkloadPartitionIdOffsetEnable = value;
    }
    inline bool getWorkloadPartitionIdOffsetEnable() const {
        return TheStructure.Common.WorkloadPartitionIdOffsetEnable;
    }
    inline void setDepthCacheFlushEnable(const bool value) {
        TheStructure.Common.DepthCacheFlushEnable = value;
    }
    inline bool getDepthCacheFlushEnable() const {
        return TheStructure.Common.DepthCacheFlushEnable;
    }
    inline void setStallAtPixelScoreboard(const bool value) {
        TheStructure.Common.StallAtPixelScoreboard = value;
    }
    inline bool getStallAtPixelScoreboard() const {
        return TheStructure.Common.StallAtPixelScoreboard;
    }
    inline void setStateCacheInvalidationEnable(const bool value) {
        TheStructure.Common.StateCacheInvalidationEnable = value;
    }
    inline bool getStateCacheInvalidationEnable() const {
        return TheStructure.Common.StateCacheInvalidationEnable;
    }
    inline void setConstantCacheInvalidationEnable(const bool value) {
        TheStructure.Common.ConstantCacheInvalidationEnable = value;
    }
    inline bool getConstantCacheInvalidationEnable() const {
        return TheStructure.Common.ConstantCacheInvalidationEnable;
    }
    inline void setVfCacheInvalidationEnable(const bool value) {
        TheStructure.Common.VfCacheInvalidationEnable = value;
    }
    inline bool getVfCacheInvalidationEnable() const {
        return TheStructure.Common.VfCacheInvalidationEnable;
    }
    inline void setDcFlushEnable(const bool value) {
        TheStructure.Common.DcFlushEnable = value;
    }
    inline bool getDcFlushEnable() const {
        return TheStructure.Common.DcFlushEnable;
    }
    inline void setProtectedMemoryApplicationId(const bool value) {
        TheStructure.Common.ProtectedMemoryApplicationId = value;
    }
    inline bool getProtectedMemoryApplicationId() const {
        return TheStructure.Common.ProtectedMemoryApplicationId;
    }
    inline void setPipeControlFlushEnable(const bool value) {
        TheStructure.Common.PipeControlFlushEnable = value;
    }
    inline bool getPipeControlFlushEnable() const {
        return TheStructure.Common.PipeControlFlushEnable;
    }
    inline void setNotifyEnable(const bool value) {
        TheStructure.Common.NotifyEnable = value;
    }
    inline bool getNotifyEnable() const {
        return TheStructure.Common.NotifyEnable;
    }
    inline void setIndirectStatePointersDisable(const bool value) {
        TheStructure.Common.IndirectStatePointersDisable = value;
    }
    inline bool getIndirectStatePointersDisable() const {
        return TheStructure.Common.IndirectStatePointersDisable;
    }
    inline void setTextureCacheInvalidationEnable(const bool value) {
        TheStructure.Common.TextureCacheInvalidationEnable = value;
    }
    inline bool getTextureCacheInvalidationEnable() const {
        return TheStructure.Common.TextureCacheInvalidationEnable;
    }
    inline void setInstructionCacheInvalidateEnable(const bool value) {
        TheStructure.Common.InstructionCacheInvalidateEnable = value;
    }
    inline bool getInstructionCacheInvalidateEnable() const {
        return TheStructure.Common.InstructionCacheInvalidateEnable;
    }
    inline void setRenderTargetCacheFlushEnable(const bool value) {
        TheStructure.Common.RenderTargetCacheFlushEnable = value;
    }
    inline bool getRenderTargetCacheFlushEnable() const {
        return TheStructure.Common.RenderTargetCacheFlushEnable;
    }
    inline void setDepthStallEnable(const bool value) {
        TheStructure.Common.DepthStallEnable = value;
    }
    inline bool getDepthStallEnable() const {
        return TheStructure.Common.DepthStallEnable;
    }
    inline void setPostSyncOperation(const POST_SYNC_OPERATION value) {
        TheStructure.Common.PostSyncOperation = value;
    }
    inline POST_SYNC_OPERATION getPostSyncOperation() const {
        return static_cast<POST_SYNC_OPERATION>(TheStructure.Common.PostSyncOperation);
    }
    inline void setPssStallSyncEnable(const bool value) {
        TheStructure.Common.PssStallSyncEnable = value;
    }
    inline bool getPssStallSyncEnable() const {
        return TheStructure.Common.PssStallSyncEnable;
    }
    inline void setTlbInvalidate(const bool value) {
        TheStructure.Common.TlbInvalidate = value;
    }
    inline bool getTlbInvalidate() const {
        return TheStructure.Common.TlbInvalidate;
    }
    inline void setDepthStallSyncEnable(const bool value) {
        TheStructure.Common.DepthStallSyncEnable = value;
    }
    inline bool getDepthStallSyncEnable() const {
        return TheStructure.Common.DepthStallSyncEnable;
    }
    inline void setCommandStreamerStallEnable(const bool value) {
        TheStructure.Common.CommandStreamerStallEnable = value;
    }
    inline bool getCommandStreamerStallEnable() const {
        return TheStructure.Common.CommandStreamerStallEnable;
    }
    inline void setStoreDataIndex(const bool value) {
        TheStructure.Common.StoreDataIndex = value;
    }
    inline bool getStoreDataIndex() const {
        return TheStructure.Common.StoreDataIndex;
    }
    inline void setProtectedMemoryEnable(const bool value) {
        TheStructure.Common.ProtectedMemoryEnable = value;
    }
    inline bool getProtectedMemoryEnable() const {
        return TheStructure.Common.ProtectedMemoryEnable;
    }
    inline void setLriPostSyncOperation(const LRI_POST_SYNC_OPERATION value) {
        TheStructure.Common.LriPostSyncOperation = value;
    }
    inline LRI_POST_SYNC_OPERATION getLriPostSyncOperation() const {
        return static_cast<LRI_POST_SYNC_OPERATION>(TheStructure.Common.LriPostSyncOperation);
    }
    inline void setDestinationAddressType(const DESTINATION_ADDRESS_TYPE value) {
        TheStructure.Common.DestinationAddressType = value;
    }
    inline DESTINATION_ADDRESS_TYPE getDestinationAddressType() const {
        return static_cast<DESTINATION_ADDRESS_TYPE>(TheStructure.Common.DestinationAddressType);
    }
    inline void setAmfsFlushEnable(const bool value) {
        TheStructure.Common.AmfsFlushEnable = value;
    }
    inline bool getAmfsFlushEnable() const {
        return TheStructure.Common.AmfsFlushEnable;
    }
    inline void setProtectedMemoryDisable(const bool value) {
        TheStructure.Common.ProtectedMemoryDisable = value;
    }
    inline bool getProtectedMemoryDisable() const {
        return TheStructure.Common.ProtectedMemoryDisable;
    }
    inline void setCommandCacheInvalidateEnable(const bool value) {
        TheStructure.Common.CommandCacheInvalidateEnable = value;
    }
    inline bool getCommandCacheInvalidateEnable() const {
        return TheStructure.Common.CommandCacheInvalidateEnable;
    }
    inline void setFlushEuStallBuffer(const FLUSH_EU_STALL_BUFFER value) {
        TheStructure.Common.FlushEuStallBuffer = value;
    }
    inline FLUSH_EU_STALL_BUFFER getFlushEuStallBuffer() const {
        return static_cast<FLUSH_EU_STALL_BUFFER>(TheStructure.Common.FlushEuStallBuffer);
    }
    inline void setTbimrForceBatchClosure(const TBIMR_FORCE_BATCH_CLOSURE value) {
        TheStructure.Common.TbimrForceBatchClosure = value;
    }
    inline TBIMR_FORCE_BATCH_CLOSURE getTbimrForceBatchClosure() const {
        return static_cast<TBIMR_FORCE_BATCH_CLOSURE>(TheStructure.Common.TbimrForceBatchClosure);
    }
    typedef enum tagADDRESS {
        ADDRESS_BIT_SHIFT = 0x2,
        ADDRESS_ALIGN_SIZE = 0x4,
    } ADDRESS;
    inline void setAddress(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffffffff);
        TheStructure.Common.Address = value >> ADDRESS_BIT_SHIFT;
    }
    inline uint32_t getAddress() const {
        return TheStructure.Common.Address << ADDRESS_BIT_SHIFT;
    }
    inline void setAddressHigh(const uint32_t value) {
        TheStructure.Common.AddressHigh = value;
    }
    inline uint32_t getAddressHigh() const {
        return TheStructure.Common.AddressHigh;
    }
    inline void setImmediateData(const uint64_t value) {
        TheStructure.Common.ImmediateData = value;
    }
    inline uint64_t getImmediateData() const {
        return TheStructure.Common.ImmediateData;
    }
} PIPE_CONTROL;
STATIC_ASSERT(24 == sizeof(PIPE_CONTROL));

typedef struct tagMI_ATOMIC {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t AtomicOpcode : BITFIELD_RANGE(8, 15);
            uint32_t ReturnDataControl : BITFIELD_RANGE(16, 16);
            uint32_t CsStall : BITFIELD_RANGE(17, 17);
            uint32_t InlineData : BITFIELD_RANGE(18, 18);
            uint32_t DataSize : BITFIELD_RANGE(19, 20);
            uint32_t PostSyncOperation : BITFIELD_RANGE(21, 21);
            uint32_t MemoryType : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint64_t WorkloadPartitionIdOffsetEnable : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_33 : BITFIELD_RANGE(1, 1);
            uint64_t MemoryAddress : BITFIELD_RANGE(2, 63);
            // DWORD 3
            uint32_t Operand1DataDword0;
            // DWORD 4
            uint32_t Operand2DataDword0;
            // DWORD 5
            uint32_t Operand1DataDword1;
            // DWORD 6
            uint32_t Operand2DataDword1;
            // DWORD 7
            uint32_t Operand1DataDword2;
            // DWORD 8
            uint32_t Operand2DataDword2;
            // DWORD 9
            uint32_t Operand1DataDword3;
            // DWORD 10
            uint32_t Operand2DataDword3;
        } Common;
        uint32_t RawData[11];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_INLINE_DATA_0 = 0x1,
        DWORD_LENGTH_INLINE_DATA_1 = 0x9,
    } DWORD_LENGTH;
    typedef enum tagDATA_SIZE {
        DATA_SIZE_DWORD = 0x0,
        DATA_SIZE_QWORD = 0x1,
        DATA_SIZE_OCTWORD = 0x2,
    } DATA_SIZE;
    typedef enum tagPOST_SYNC_OPERATION {
        POST_SYNC_OPERATION_NO_POST_SYNC_OPERATION = 0x0,
        POST_SYNC_OPERATION_POST_SYNC_OPERATION = 0x1,
    } POST_SYNC_OPERATION;
    typedef enum tagMEMORY_TYPE {
        MEMORY_TYPE_PER_PROCESS_GRAPHICS_ADDRESS = 0x0,
        MEMORY_TYPE_GLOBAL_GRAPHICS_ADDRESS = 0x1,
    } MEMORY_TYPE;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_ATOMIC = 0x2f,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    typedef enum tagATOMIC_OPCODES { // patched
        ATOMIC_4B_MOVE = 0x4,
        ATOMIC_4B_INCREMENT = 0x5,
        ATOMIC_4B_DECREMENT = 0x6,
        ATOMIC_8B_MOVE = 0x24,
        ATOMIC_8B_INCREMENT = 0x25,
        ATOMIC_8B_DECREMENT = 0x26,
        ATOMIC_8B_ADD = 0x27,
        ATOMIC_8B_CMP_WR = 0x2E,
    } ATOMIC_OPCODES;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_INLINE_DATA_0;
        TheStructure.Common.DataSize = DATA_SIZE_DWORD;
        TheStructure.Common.PostSyncOperation = POST_SYNC_OPERATION_NO_POST_SYNC_OPERATION;
        TheStructure.Common.MemoryType = MEMORY_TYPE_PER_PROCESS_GRAPHICS_ADDRESS;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_ATOMIC;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_ATOMIC sInit() {
        MI_ATOMIC state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 11);
        return TheStructure.RawData[index];
    }
    inline void setDwordLength(const DWORD_LENGTH value) {
        TheStructure.Common.DwordLength = value;
    }
    inline DWORD_LENGTH getDwordLength() const {
        return static_cast<DWORD_LENGTH>(TheStructure.Common.DwordLength);
    }
    inline void setAtomicOpcode(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xff);
        TheStructure.Common.AtomicOpcode = value;
    }
    inline uint32_t getAtomicOpcode() const {
        return TheStructure.Common.AtomicOpcode;
    }
    inline void setReturnDataControl(const bool value) {
        TheStructure.Common.ReturnDataControl = value;
    }
    inline bool getReturnDataControl() const {
        return TheStructure.Common.ReturnDataControl;
    }
    inline void setCsStall(const bool value) {
        TheStructure.Common.CsStall = value;
    }
    inline bool getCsStall() const {
        return TheStructure.Common.CsStall;
    }
    inline void setInlineData(const bool value) {
        TheStructure.Common.InlineData = value;
    }
    inline bool getInlineData() const {
        return TheStructure.Common.InlineData;
    }
    inline void setDataSize(const DATA_SIZE value) {
        TheStructure.Common.DataSize = value;
    }
    inline DATA_SIZE getDataSize() const {
        return static_cast<DATA_SIZE>(TheStructure.Common.DataSize);
    }
    inline void setPostSyncOperation(const POST_SYNC_OPERATION value) {
        TheStructure.Common.PostSyncOperation = value;
    }
    inline POST_SYNC_OPERATION getPostSyncOperation() const {
        return static_cast<POST_SYNC_OPERATION>(TheStructure.Common.PostSyncOperation);
    }
    inline void setMemoryType(const MEMORY_TYPE value) {
        TheStructure.Common.MemoryType = value;
    }
    inline MEMORY_TYPE getMemoryType() const {
        return static_cast<MEMORY_TYPE>(TheStructure.Common.MemoryType);
    }
    inline void setWorkloadPartitionIdOffsetEnable(const bool value) {
        TheStructure.Common.WorkloadPartitionIdOffsetEnable = value;
    }
    inline bool getWorkloadPartitionIdOffsetEnable() const {
        return TheStructure.Common.WorkloadPartitionIdOffsetEnable;
    }
    typedef enum tagMEMORYADDRESS {
        MEMORYADDRESS_BIT_SHIFT = 0x2,
        MEMORYADDRESS_ALIGN_SIZE = 0x4,
    } MEMORYADDRESS;
    inline void setMemoryAddress(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0xffffffffffffffffL);
        TheStructure.Common.MemoryAddress = value >> MEMORYADDRESS_BIT_SHIFT;
    }
    inline uint64_t getMemoryAddress() const {
        return TheStructure.Common.MemoryAddress << MEMORYADDRESS_BIT_SHIFT;
    }
    inline void setOperand1DataDword0(const uint32_t value) {
        TheStructure.Common.Operand1DataDword0 = value;
    }
    inline uint32_t getOperand1DataDword0() const {
        return TheStructure.Common.Operand1DataDword0;
    }
    inline void setOperand2DataDword0(const uint32_t value) {
        TheStructure.Common.Operand2DataDword0 = value;
    }
    inline uint32_t getOperand2DataDword0() const {
        return TheStructure.Common.Operand2DataDword0;
    }
    inline void setOperand1DataDword1(const uint32_t value) {
        TheStructure.Common.Operand1DataDword1 = value;
    }
    inline uint32_t getOperand1DataDword1() const {
        return TheStructure.Common.Operand1DataDword1;
    }
    inline void setOperand2DataDword1(const uint32_t value) {
        TheStructure.Common.Operand2DataDword1 = value;
    }
    inline uint32_t getOperand2DataDword1() const {
        return TheStructure.Common.Operand2DataDword1;
    }
    inline void setOperand1DataDword2(const uint32_t value) {
        TheStructure.Common.Operand1DataDword2 = value;
    }
    inline uint32_t getOperand1DataDword2() const {
        return TheStructure.Common.Operand1DataDword2;
    }
    inline void setOperand2DataDword2(const uint32_t value) {
        TheStructure.Common.Operand2DataDword2 = value;
    }
    inline uint32_t getOperand2DataDword2() const {
        return TheStructure.Common.Operand2DataDword2;
    }
    inline void setOperand1DataDword3(const uint32_t value) {
        TheStructure.Common.Operand1DataDword3 = value;
    }
    inline uint32_t getOperand1DataDword3() const {
        return TheStructure.Common.Operand1DataDword3;
    }
    inline void setOperand2DataDword3(const uint32_t value) {
        TheStructure.Common.Operand2DataDword3 = value;
    }
    inline uint32_t getOperand2DataDword3() const {
        return TheStructure.Common.Operand2DataDword3;
    }
} MI_ATOMIC;
STATIC_ASSERT(44 == sizeof(MI_ATOMIC));

typedef struct tagMI_BATCH_BUFFER_END {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t EndContext : BITFIELD_RANGE(0, 0);
            uint32_t Reserved_1 : BITFIELD_RANGE(1, 14);
            uint32_t PredicateEnable : BITFIELD_RANGE(15, 15);
            uint32_t Reserved_16 : BITFIELD_RANGE(16, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
        } Common;
        uint32_t RawData[1];
    } TheStructure;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_BATCH_BUFFER_END = 0xa,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_BATCH_BUFFER_END;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_BATCH_BUFFER_END sInit() {
        MI_BATCH_BUFFER_END state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 1);
        return TheStructure.RawData[index];
    }
    inline void setEndContext(const bool value) {
        TheStructure.Common.EndContext = value;
    }
    inline bool getEndContext() const {
        return TheStructure.Common.EndContext;
    }
    inline void setPredicateEnable(const bool value) {
        TheStructure.Common.PredicateEnable = value;
    }
    inline bool getPredicateEnable() const {
        return TheStructure.Common.PredicateEnable;
    }
} MI_BATCH_BUFFER_END;
STATIC_ASSERT(4 == sizeof(MI_BATCH_BUFFER_END));

typedef struct tagMI_LOAD_REGISTER_IMM {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t ByteWriteDisables : BITFIELD_RANGE(8, 11);
            uint32_t ForcePosted : BITFIELD_RANGE(12, 12);
            uint32_t Reserved_13 : BITFIELD_RANGE(13, 16);
            uint32_t MmioRemapEnable : BITFIELD_RANGE(17, 17);
            uint32_t Reserved_18 : BITFIELD_RANGE(18, 18);
            uint32_t AddCsMmioStartOffset : BITFIELD_RANGE(19, 19);
            uint32_t Reserved_20 : BITFIELD_RANGE(20, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint32_t Reserved_32 : BITFIELD_RANGE(0, 1);     // patched
            uint32_t RegisterOffset : BITFIELD_RANGE(2, 22); // patched
            uint32_t Reserved_55 : BITFIELD_RANGE(23, 31);   // patched
            // DWORD 2
            uint32_t DataDword; // patched
        } Common;
        uint32_t RawData[3];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x1,
    } DWORD_LENGTH;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_LOAD_REGISTER_IMM = 0x22,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_LOAD_REGISTER_IMM;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_LOAD_REGISTER_IMM sInit() {
        MI_LOAD_REGISTER_IMM state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 3);
        return TheStructure.RawData[index];
    }
    inline void setByteWriteDisables(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xf);
        TheStructure.Common.ByteWriteDisables = value;
    }
    inline uint32_t getByteWriteDisables() const {
        return TheStructure.Common.ByteWriteDisables;
    }
    inline void setForcePosted(const bool value) {
        TheStructure.Common.ForcePosted = value;
    }
    inline bool getForcePosted() const {
        return TheStructure.Common.ForcePosted;
    }
    inline void setMmioRemapEnable(const bool value) {
        TheStructure.Common.MmioRemapEnable = value;
    }
    inline bool getMmioRemapEnable() const {
        return TheStructure.Common.MmioRemapEnable;
    }
    inline void setAddCsMmioStartOffset(const bool value) {
        TheStructure.Common.AddCsMmioStartOffset = value;
    }
    inline bool getAddCsMmioStartOffset() const {
        return TheStructure.Common.AddCsMmioStartOffset;
    }
    typedef enum tagREGISTEROFFSET {
        REGISTEROFFSET_BIT_SHIFT = 0x2,
        REGISTEROFFSET_ALIGN_SIZE = 0x4,
    } REGISTEROFFSET;
    inline void setRegisterOffset(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x7fffffL);
        TheStructure.Common.RegisterOffset = value >> REGISTEROFFSET_BIT_SHIFT;
    }
    inline uint32_t getRegisterOffset() const {
        return TheStructure.Common.RegisterOffset << REGISTEROFFSET_BIT_SHIFT;
    }
    inline void setDataDword(const uint32_t value) {
        TheStructure.Common.DataDword = value;
    }
    inline uint32_t getDataDword() const {
        return TheStructure.Common.DataDword;
    }
} MI_LOAD_REGISTER_IMM;
STATIC_ASSERT(12 == sizeof(MI_LOAD_REGISTER_IMM));

typedef struct tagMI_NOOP {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t IdentificationNumber : BITFIELD_RANGE(0, 21);
            uint32_t IdentificationNumberRegisterWriteEnable : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
        } Common;
        uint32_t RawData[1];
    } TheStructure;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_NOOP = 0x0,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_NOOP;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_NOOP sInit() {
        MI_NOOP state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 1);
        return TheStructure.RawData[index];
    }
    inline void setIdentificationNumber(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3fffff);
        TheStructure.Common.IdentificationNumber = value;
    }
    inline uint32_t getIdentificationNumber() const {
        return TheStructure.Common.IdentificationNumber;
    }
    inline void setIdentificationNumberRegisterWriteEnable(const bool value) {
        TheStructure.Common.IdentificationNumberRegisterWriteEnable = value;
    }
    inline bool getIdentificationNumberRegisterWriteEnable() const {
        return TheStructure.Common.IdentificationNumberRegisterWriteEnable;
    }
} MI_NOOP;
STATIC_ASSERT(4 == sizeof(MI_NOOP));

typedef struct tagRENDER_SURFACE_STATE {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t CubeFaceEnablePositiveZ : BITFIELD_RANGE(0, 0);
            uint32_t CubeFaceEnableNegativeZ : BITFIELD_RANGE(1, 1);
            uint32_t CubeFaceEnablePositiveY : BITFIELD_RANGE(2, 2);
            uint32_t CubeFaceEnableNegativeY : BITFIELD_RANGE(3, 3);
            uint32_t CubeFaceEnablePositiveX : BITFIELD_RANGE(4, 4);
            uint32_t CubeFaceEnableNegativeX : BITFIELD_RANGE(5, 5);
            uint32_t MediaBoundaryPixelMode : BITFIELD_RANGE(6, 7);
            uint32_t RenderCacheReadWriteMode : BITFIELD_RANGE(8, 8);
            uint32_t EnableSamplerRouteToLsc : BITFIELD_RANGE(9, 9);
            uint32_t VerticalLineStrideOffset : BITFIELD_RANGE(10, 10);
            uint32_t VerticalLineStride : BITFIELD_RANGE(11, 11);
            uint32_t TileMode : BITFIELD_RANGE(12, 13);
            uint32_t SurfaceHorizontalAlignment : BITFIELD_RANGE(14, 15);
            uint32_t SurfaceVerticalAlignment : BITFIELD_RANGE(16, 17);
            uint32_t SurfaceFormat : BITFIELD_RANGE(18, 26);
            uint32_t Reserved_27 : BITFIELD_RANGE(27, 27);
            uint32_t SurfaceArray : BITFIELD_RANGE(28, 28);
            uint32_t SurfaceType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint32_t SurfaceQpitch : BITFIELD_RANGE(0, 14);
            uint32_t Reserved_47 : BITFIELD_RANGE(15, 18);
            uint32_t BaseMipLevel : BITFIELD_RANGE(19, 23);
            uint32_t MemoryObjectControlStateEncryptedData : BITFIELD_RANGE(24, 24);
            uint32_t MemoryObjectControlStateIndexToMocsTables : BITFIELD_RANGE(25, 30);
            uint32_t Reserved_63 : BITFIELD_RANGE(31, 31);
            // DWORD 2
            uint32_t Width : BITFIELD_RANGE(0, 13);
            uint32_t Reserved_78 : BITFIELD_RANGE(14, 15);
            uint32_t Height : BITFIELD_RANGE(16, 29);
            uint32_t Reserved_94 : BITFIELD_RANGE(30, 30);
            uint32_t DepthStencilResource : BITFIELD_RANGE(31, 31);
            // DWORD 3
            uint32_t SurfacePitch : BITFIELD_RANGE(0, 17);
            uint32_t Reserved_114 : BITFIELD_RANGE(18, 20);
            uint32_t Depth : BITFIELD_RANGE(21, 31);
            // DWORD 4
            uint32_t MultisamplePositionPaletteIndex : BITFIELD_RANGE(0, 2);
            uint32_t NumberOfMultisamples : BITFIELD_RANGE(3, 5);
            uint32_t MultisampledSurfaceStorageFormat : BITFIELD_RANGE(6, 6);
            uint32_t RenderTargetViewExtent : BITFIELD_RANGE(7, 17);
            uint32_t MinimumArrayElement : BITFIELD_RANGE(18, 28);
            uint32_t RenderTargetAndSampleUnormRotation : BITFIELD_RANGE(29, 30);
            uint32_t Reserved_159 : BITFIELD_RANGE(31, 31);
            // DWORD 5
            uint32_t MipCountLod : BITFIELD_RANGE(0, 3);
            uint32_t SurfaceMinLod : BITFIELD_RANGE(4, 7);
            uint32_t MipTailStartLod : BITFIELD_RANGE(8, 11);
            uint32_t Reserved_172 : BITFIELD_RANGE(12, 15);
            uint32_t L1CacheControlCachePolicy : BITFIELD_RANGE(16, 18);
            uint32_t Reserved_179 : BITFIELD_RANGE(19, 19);
            uint32_t EwaDisableForCube : BITFIELD_RANGE(20, 20);
            uint32_t YOffset : BITFIELD_RANGE(21, 23);
            uint32_t Reserved_184 : BITFIELD_RANGE(24, 24);
            uint32_t XOffset : BITFIELD_RANGE(25, 31);
            // DWORD 6
            uint32_t AuxiliarySurfaceMode : BITFIELD_RANGE(0, 2);
            uint32_t Reserved_195 : BITFIELD_RANGE(3, 14);
            uint32_t YuvInterpolationEnable : BITFIELD_RANGE(15, 15);
            uint32_t Reserved_208 : BITFIELD_RANGE(16, 31);
            // DWORD 7
            uint32_t ResourceMinLod : BITFIELD_RANGE(0, 11);
            uint32_t Reserved_236 : BITFIELD_RANGE(12, 15);
            uint32_t ShaderChannelSelectAlpha : BITFIELD_RANGE(16, 18);
            uint32_t ShaderChannelSelectBlue : BITFIELD_RANGE(19, 21);
            uint32_t ShaderChannelSelectGreen : BITFIELD_RANGE(22, 24);
            uint32_t ShaderChannelSelectRed : BITFIELD_RANGE(25, 27);
            uint32_t Reserved_252 : BITFIELD_RANGE(28, 31);
            // DWORD 8
            uint64_t SurfaceBaseAddress;
            // DWORD 10
            uint64_t Reserved_320;
            // DWORD 12
            uint32_t CompressionFormat : BITFIELD_RANGE(0, 3);
            uint32_t MipRegionDepthInLog2 : BITFIELD_RANGE(4, 7);
            uint32_t Reserved_392 : BITFIELD_RANGE(8, 31);
            // DWORD 13
            uint32_t Reserved_416 : BITFIELD_RANGE(0, 29);
            uint32_t DisableSamplerBackingByLsc : BITFIELD_RANGE(30, 30);
            uint32_t DisallowLowQualityFiltering : BITFIELD_RANGE(31, 31);
            // DWORD 14
            uint32_t Reserved_448;
            // DWORD 15
            uint32_t Reserved_480;
        } Common;
        struct tag_SurfaceFormatIsPlanar {
            // DWORD 0
            uint32_t Reserved_0;
            // DWORD 1
            uint32_t Reserved_32;
            // DWORD 2
            uint32_t Reserved_64;
            // DWORD 3
            uint32_t Reserved_96;
            // DWORD 4
            uint32_t Reserved_128;
            // DWORD 5
            uint32_t Reserved_160;
            // DWORD 6
            uint32_t YOffsetForUOrUvPlane : BITFIELD_RANGE(0, 13);
            uint32_t Reserved_206 : BITFIELD_RANGE(14, 15);
            uint32_t XOffsetForUOrUvPlane : BITFIELD_RANGE(16, 29);
            uint32_t HalfPitchForChroma : BITFIELD_RANGE(30, 30);
            uint32_t SeparateUvPlaneEnable : BITFIELD_RANGE(31, 31);
            // DWORD 7
            uint32_t Reserved_224;
            // DWORD 8
            uint64_t Reserved_256;
            // DWORD 10
            uint64_t Reserved_320 : BITFIELD_RANGE(0, 31);
            // DWORD 11
            uint64_t YOffsetForVPlane : BITFIELD_RANGE(32, 45);
            uint64_t Reserved_366 : BITFIELD_RANGE(46, 47);
            uint64_t XOffsetForVPlane : BITFIELD_RANGE(48, 61);
            uint64_t Reserved_382 : BITFIELD_RANGE(62, 63);
            // DWORD 12
            uint32_t Reserved_384;
            // DWORD 13
            uint32_t Reserved_416;
            // DWORD 14
            uint32_t Reserved_448;
            // DWORD 15
            uint32_t Reserved_480;
        } _SurfaceFormatIsPlanar;
        struct tag_SurfaceFormatIsnotPlanar {
            // DWORD 0
            uint32_t Reserved_0;
            // DWORD 1
            uint32_t Reserved_32;
            // DWORD 2
            uint32_t Reserved_64;
            // DWORD 3
            uint32_t Reserved_96;
            // DWORD 4
            uint32_t Reserved_128;
            // DWORD 5
            uint32_t Reserved_160;
            // DWORD 6
            uint32_t Reserved_192 : BITFIELD_RANGE(0, 2);
            uint32_t AuxiliarySurfacePitch : BITFIELD_RANGE(3, 12);
            uint32_t Reserved_205 : BITFIELD_RANGE(13, 15);
            uint32_t AuxiliarySurfaceQpitch : BITFIELD_RANGE(16, 30);
            uint32_t Reserved_223 : BITFIELD_RANGE(31, 31);
            // DWORD 7
            uint32_t Reserved_224;
            // DWORD 8
            uint64_t Reserved_256;
            // DWORD 10
            uint64_t Reserved_320;
            // DWORD 12
            uint32_t Reserved_384;
            // DWORD 13
            uint32_t Reserved_416;
            // DWORD 14
            uint32_t Reserved_448;
            // DWORD 15
            uint32_t Reserved_480;
        } _SurfaceFormatIsnotPlanar;
        struct tag_PropertyauxiliarySurfaceModeIsnotAux_NoneAnd_PropertyauxiliarySurfaceModeIsnotAux_Append {
            // DWORD 0
            uint32_t Reserved_0;
            // DWORD 1
            uint32_t Reserved_32;
            // DWORD 2
            uint32_t Reserved_64;
            // DWORD 3
            uint32_t Reserved_96;
            // DWORD 4
            uint32_t Reserved_128;
            // DWORD 5
            uint32_t Reserved_160;
            // DWORD 6
            uint32_t Reserved_192;
            // DWORD 7
            uint32_t Reserved_224;
            // DWORD 8
            uint64_t Reserved_256;
            // DWORD 10
            uint64_t Reserved_320 : BITFIELD_RANGE(0, 11);
            uint64_t AuxiliarySurfaceBaseAddress : BITFIELD_RANGE(12, 63);
            // DWORD 12
            uint32_t Reserved_384;
            // DWORD 13
            uint32_t Reserved_416;
            // DWORD 14
            uint32_t Reserved_448;
            // DWORD 15
            uint32_t Reserved_480;
        } _PropertyauxiliarySurfaceModeIsnotAux_NoneAnd_PropertyauxiliarySurfaceModeIsnotAux_Append;
        struct tagPropertyauxiliarySurfaceModeIsAux_Append {
            // DWORD 0
            uint32_t Reserved_0;
            // DWORD 1
            uint32_t Reserved_32;
            // DWORD 2
            uint32_t Reserved_64;
            // DWORD 3
            uint32_t Reserved_96;
            // DWORD 4
            uint32_t Reserved_128;
            // DWORD 5
            uint32_t Reserved_160;
            // DWORD 6
            uint32_t Reserved_192;
            // DWORD 7
            uint32_t Reserved_224;
            // DWORD 8
            uint64_t Reserved_256 : BITFIELD_RANGE(0, 1);
            uint64_t AppendCounterAddress : BITFIELD_RANGE(2, 63);
            // DWORD 12
            uint32_t Reserved_384;
            // DWORD 13
            uint32_t Reserved_416;
            // DWORD 14
            uint32_t Reserved_448;
            // DWORD 15
            uint32_t Reserved_480;
        } PropertyauxiliarySurfaceModeIsAux_Append;
        struct tagAuxiliarySurfaceModeIsnotAux_Append {
            // DWORD 0
            uint32_t Reserved_0;
            // DWORD 1
            uint32_t Reserved_32;
            // DWORD 2
            uint32_t Reserved_64;
            // DWORD 3
            uint32_t Reserved_96;
            // DWORD 4
            uint32_t Reserved_128;
            // DWORD 5
            uint32_t Reserved_160;
            // DWORD 6
            uint32_t Reserved_192;
            // DWORD 7
            uint32_t Reserved_224;
            // DWORD 8
            uint64_t Reserved_256;
            // DWORD 10
            uint64_t MipRegionWidthInLog2 : BITFIELD_RANGE(0, 3);
            uint64_t MipRegionHeightInLog2 : BITFIELD_RANGE(4, 7);
            uint64_t Reserved_328 : BITFIELD_RANGE(8, 10);
            uint64_t ProceduralTexture : BITFIELD_RANGE(11, 11);
            uint64_t Reserved_332 : BITFIELD_RANGE(12, 63);
            // DWORD 12
            uint32_t Reserved_384;
            // DWORD 13
            uint32_t Reserved_416;
            // DWORD 14
            uint32_t Reserved_448;
            // DWORD 15
            uint32_t Reserved_480;
        } AuxiliarySurfaceModeIsnotAux_Append;
        uint32_t RawData[16];
    } TheStructure;
    typedef enum tagMEDIA_BOUNDARY_PIXEL_MODE {
        MEDIA_BOUNDARY_PIXEL_MODE_NORMAL_MODE = 0x0,
        MEDIA_BOUNDARY_PIXEL_MODE_PROGRESSIVE_FRAME = 0x2,
        MEDIA_BOUNDARY_PIXEL_MODE_INTERLACED_FRAME = 0x3,
    } MEDIA_BOUNDARY_PIXEL_MODE;
    typedef enum tagRENDER_CACHE_READ_WRITE_MODE {
        RENDER_CACHE_READ_WRITE_MODE_WRITE_ONLY_CACHE = 0x0,
        RENDER_CACHE_READ_WRITE_MODE_READ_WRITE_CACHE = 0x1,
    } RENDER_CACHE_READ_WRITE_MODE;
    typedef enum tagTILE_MODE {
        TILE_MODE_LINEAR = 0x0,
        TILE_MODE_TILE64 = 0x1,
        TILE_MODE_XMAJOR = 0x2,
        TILE_MODE_TILE4 = 0x3,
    } TILE_MODE;
    typedef enum tagSURFACE_HORIZONTAL_ALIGNMENT {
        SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_16 = 0x0,
        SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_32 = 0x1,
        SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_64 = 0x2,
        SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_128 = 0x3,
        SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_DEFAULT = SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_128,
    } SURFACE_HORIZONTAL_ALIGNMENT;
    typedef enum tagSURFACE_VERTICAL_ALIGNMENT {
        SURFACE_VERTICAL_ALIGNMENT_VALIGN_4 = 0x1,
        SURFACE_VERTICAL_ALIGNMENT_VALIGN_8 = 0x2,
        SURFACE_VERTICAL_ALIGNMENT_VALIGN_16 = 0x3,
    } SURFACE_VERTICAL_ALIGNMENT;
    typedef enum tagSURFACE_FORMAT {
        SURFACE_FORMAT_R32G32B32A32_FLOAT = 0x0,
        SURFACE_FORMAT_R32G32B32A32_SINT = 0x1,
        SURFACE_FORMAT_R32G32B32A32_UINT = 0x2,
        SURFACE_FORMAT_R32G32B32A32_UNORM = 0x3,
        SURFACE_FORMAT_R32G32B32A32_SNORM = 0x4,
        SURFACE_FORMAT_R64G64_FLOAT = 0x5,
        SURFACE_FORMAT_R32G32B32X32_FLOAT = 0x6,
        SURFACE_FORMAT_R32G32B32A32_SSCALED = 0x7,
        SURFACE_FORMAT_R32G32B32A32_USCALED = 0x8,
        SURFACE_FORMAT_PLANAR_422_8_P208 = 0xc,
        SURFACE_FORMAT_PLANAR_420_8_SAMPLE_8X8 = 0xd,
        SURFACE_FORMAT_PLANAR_411_8 = 0xe,
        SURFACE_FORMAT_PLANAR_422_8 = 0xf,
        SURFACE_FORMAT_R8G8B8A8_UNORM_VDI = 0x10,
        SURFACE_FORMAT_YCRCB_NORMAL_SAMPLE_8X8 = 0x11,
        SURFACE_FORMAT_YCRCB_SWAPUVY_SAMPLE_8X8 = 0x12,
        SURFACE_FORMAT_YCRCB_SWAPUV_SAMPLE_8X8 = 0x13,
        SURFACE_FORMAT_YCRCB_SWAPY_SAMPLE_8X8 = 0x14,
        SURFACE_FORMAT_R32G32B32A32_FLOAT_LD = 0x15,
        SURFACE_FORMAT_PLANAR_420_16_SAMPLE_8X8 = 0x16,
        SURFACE_FORMAT_R16B16_UNORM_SAMPLE_8X8 = 0x17,
        SURFACE_FORMAT_Y16_UNORM_SAMPLE_8X8 = 0x18,
        SURFACE_FORMAT_PLANAR_Y32_UNORM = 0x19,
        SURFACE_FORMAT_R32G32B32A32_SFIXED = 0x20,
        SURFACE_FORMAT_R64G64_PASSTHRU = 0x21,
        SURFACE_FORMAT_R32G32B32_FLOAT = 0x40,
        SURFACE_FORMAT_R32G32B32_SINT = 0x41,
        SURFACE_FORMAT_R32G32B32_UINT = 0x42,
        SURFACE_FORMAT_R32G32B32_UNORM = 0x43,
        SURFACE_FORMAT_R32G32B32_SNORM = 0x44,
        SURFACE_FORMAT_R32G32B32_SSCALED = 0x45,
        SURFACE_FORMAT_R32G32B32_USCALED = 0x46,
        SURFACE_FORMAT_R32G32B32_FLOAT_LD = 0x47,
        SURFACE_FORMAT_R32G32B32_SFIXED = 0x50,
        SURFACE_FORMAT_R16G16B16A16_UNORM = 0x80,
        SURFACE_FORMAT_R16G16B16A16_SNORM = 0x81,
        SURFACE_FORMAT_R16G16B16A16_SINT = 0x82,
        SURFACE_FORMAT_R16G16B16A16_UINT = 0x83,
        SURFACE_FORMAT_R16G16B16A16_FLOAT = 0x84,
        SURFACE_FORMAT_R32G32_FLOAT = 0x85,
        SURFACE_FORMAT_R32G32_SINT = 0x86,
        SURFACE_FORMAT_R32G32_UINT = 0x87,
        SURFACE_FORMAT_R32_FLOAT_X8X24_TYPELESS = 0x88,
        SURFACE_FORMAT_X32_TYPELESS_G8X24_UINT = 0x89,
        SURFACE_FORMAT_L32A32_FLOAT = 0x8a,
        SURFACE_FORMAT_R32G32_UNORM = 0x8b,
        SURFACE_FORMAT_R32G32_SNORM = 0x8c,
        SURFACE_FORMAT_R64_FLOAT = 0x8d,
        SURFACE_FORMAT_R16G16B16X16_UNORM = 0x8e,
        SURFACE_FORMAT_R16G16B16X16_FLOAT = 0x8f,
        SURFACE_FORMAT_A32X32_FLOAT = 0x90,
        SURFACE_FORMAT_L32X32_FLOAT = 0x91,
        SURFACE_FORMAT_I32X32_FLOAT = 0x92,
        SURFACE_FORMAT_R16G16B16A16_SSCALED = 0x93,
        SURFACE_FORMAT_R16G16B16A16_USCALED = 0x94,
        SURFACE_FORMAT_R32G32_SSCALED = 0x95,
        SURFACE_FORMAT_R32G32_USCALED = 0x96,
        SURFACE_FORMAT_R32G32_FLOAT_LD = 0x97,
        SURFACE_FORMAT_R32_FLOAT_X8X24_TYPELESS_LD = 0x98,
        SURFACE_FORMAT_R32G32_SFIXED = 0xa0,
        SURFACE_FORMAT_R64_PASSTHRU = 0xa1,
        SURFACE_FORMAT_B8G8R8A8_UNORM = 0xc0,
        SURFACE_FORMAT_B8G8R8A8_UNORM_SRGB = 0xc1,
        SURFACE_FORMAT_R10G10B10A2_UNORM = 0xc2,
        SURFACE_FORMAT_R10G10B10A2_UNORM_SRGB = 0xc3,
        SURFACE_FORMAT_R10G10B10A2_UINT = 0xc4,
        SURFACE_FORMAT_R10G10B10_SNORM_A2_UNORM = 0xc5,
        SURFACE_FORMAT_R10G10B10A2_UNORM_SAMPLE_8X8 = 0xc6,
        SURFACE_FORMAT_R8G8B8A8_UNORM = 0xc7,
        SURFACE_FORMAT_R8G8B8A8_UNORM_SRGB = 0xc8,
        SURFACE_FORMAT_R8G8B8A8_SNORM = 0xc9,
        SURFACE_FORMAT_R8G8B8A8_SINT = 0xca,
        SURFACE_FORMAT_R8G8B8A8_UINT = 0xcb,
        SURFACE_FORMAT_R16G16_UNORM = 0xcc,
        SURFACE_FORMAT_R16G16_SNORM = 0xcd,
        SURFACE_FORMAT_R16G16_SINT = 0xce,
        SURFACE_FORMAT_R16G16_UINT = 0xcf,
        SURFACE_FORMAT_R16G16_FLOAT = 0xd0,
        SURFACE_FORMAT_B10G10R10A2_UNORM = 0xd1,
        SURFACE_FORMAT_B10G10R10A2_UNORM_SRGB = 0xd2,
        SURFACE_FORMAT_R11G11B10_FLOAT = 0xd3,
        SURFACE_FORMAT_R10G10B10_FLOAT_A2_UNORM = 0xd5,
        SURFACE_FORMAT_R32_SINT = 0xd6,
        SURFACE_FORMAT_R32_UINT = 0xd7,
        SURFACE_FORMAT_R32_FLOAT = 0xd8,
        SURFACE_FORMAT_R24_UNORM_X8_TYPELESS = 0xd9,
        SURFACE_FORMAT_X24_TYPELESS_G8_UINT = 0xda,
        SURFACE_FORMAT_R32_FLOAT_LD = 0xdb,
        SURFACE_FORMAT_R24_UNORM_X8_TYPELESS_LD = 0xdc,
        SURFACE_FORMAT_L32_UNORM = 0xdd,
        SURFACE_FORMAT_A32_UNORM = 0xde,
        SURFACE_FORMAT_L16A16_UNORM = 0xdf,
        SURFACE_FORMAT_I24X8_UNORM = 0xe0,
        SURFACE_FORMAT_L24X8_UNORM = 0xe1,
        SURFACE_FORMAT_A24X8_UNORM = 0xe2,
        SURFACE_FORMAT_I32_FLOAT = 0xe3,
        SURFACE_FORMAT_L32_FLOAT = 0xe4,
        SURFACE_FORMAT_A32_FLOAT = 0xe5,
        SURFACE_FORMAT_X8B8_UNORM_G8R8_SNORM = 0xe6,
        SURFACE_FORMAT_A8X8_UNORM_G8R8_SNORM = 0xe7,
        SURFACE_FORMAT_B8X8_UNORM_G8R8_SNORM = 0xe8,
        SURFACE_FORMAT_B8G8R8X8_UNORM = 0xe9,
        SURFACE_FORMAT_B8G8R8X8_UNORM_SRGB = 0xea,
        SURFACE_FORMAT_R8G8B8X8_UNORM = 0xeb,
        SURFACE_FORMAT_R8G8B8X8_UNORM_SRGB = 0xec,
        SURFACE_FORMAT_R9G9B9E5_SHAREDEXP = 0xed,
        SURFACE_FORMAT_B10G10R10X2_UNORM = 0xee,
        SURFACE_FORMAT_L16A16_FLOAT = 0xf0,
        SURFACE_FORMAT_R32_UNORM = 0xf1,
        SURFACE_FORMAT_R32_SNORM = 0xf2,
        SURFACE_FORMAT_R10G10B10X2_USCALED = 0xf3,
        SURFACE_FORMAT_R8G8B8A8_SSCALED = 0xf4,
        SURFACE_FORMAT_R8G8B8A8_USCALED = 0xf5,
        SURFACE_FORMAT_R16G16_SSCALED = 0xf6,
        SURFACE_FORMAT_R16G16_USCALED = 0xf7,
        SURFACE_FORMAT_R32_SSCALED = 0xf8,
        SURFACE_FORMAT_R32_USCALED = 0xf9,
        SURFACE_FORMAT_R8B8G8A8_UNORM = 0xfa,
        SURFACE_FORMAT_R8G8B8A8_SINT_NOA = 0xfb,
        SURFACE_FORMAT_R8G8B8A8_UINT_NOA = 0xfc,
        SURFACE_FORMAT_R8G8B8A8_UNORM_YUV = 0xfd,
        SURFACE_FORMAT_R8G8B8A8_UNORM_SNCK = 0xfe,
        SURFACE_FORMAT_R8G8B8A8_UNORM_NOA = 0xff,
        SURFACE_FORMAT_B5G6R5_UNORM = 0x100,
        SURFACE_FORMAT_B5G6R5_UNORM_SRGB = 0x101,
        SURFACE_FORMAT_B5G5R5A1_UNORM = 0x102,
        SURFACE_FORMAT_B5G5R5A1_UNORM_SRGB = 0x103,
        SURFACE_FORMAT_B4G4R4A4_UNORM = 0x104,
        SURFACE_FORMAT_B4G4R4A4_UNORM_SRGB = 0x105,
        SURFACE_FORMAT_R8G8_UNORM = 0x106,
        SURFACE_FORMAT_R8G8_SNORM = 0x107,
        SURFACE_FORMAT_R8G8_SINT = 0x108,
        SURFACE_FORMAT_R8G8_UINT = 0x109,
        SURFACE_FORMAT_R16_UNORM = 0x10a,
        SURFACE_FORMAT_R16_SNORM = 0x10b,
        SURFACE_FORMAT_R16_SINT = 0x10c,
        SURFACE_FORMAT_R16_UINT = 0x10d,
        SURFACE_FORMAT_R16_FLOAT = 0x10e,
        SURFACE_FORMAT_A8P8_UNORM_PALETTE0 = 0x10f,
        SURFACE_FORMAT_A8P8_UNORM_PALETTE1 = 0x110,
        SURFACE_FORMAT_I16_UNORM = 0x111,
        SURFACE_FORMAT_L16_UNORM = 0x112,
        SURFACE_FORMAT_A16_UNORM = 0x113,
        SURFACE_FORMAT_L8A8_UNORM = 0x114,
        SURFACE_FORMAT_I16_FLOAT = 0x115,
        SURFACE_FORMAT_L16_FLOAT = 0x116,
        SURFACE_FORMAT_A16_FLOAT = 0x117,
        SURFACE_FORMAT_L8A8_UNORM_SRGB = 0x118,
        SURFACE_FORMAT_R5G5_SNORM_B6_UNORM = 0x119,
        SURFACE_FORMAT_B5G5R5X1_UNORM = 0x11a,
        SURFACE_FORMAT_B5G5R5X1_UNORM_SRGB = 0x11b,
        SURFACE_FORMAT_R8G8_SSCALED = 0x11c,
        SURFACE_FORMAT_R8G8_USCALED = 0x11d,
        SURFACE_FORMAT_R16_SSCALED = 0x11e,
        SURFACE_FORMAT_R16_USCALED = 0x11f,
        SURFACE_FORMAT_R8G8_SNORM_DX9 = 0x120,
        SURFACE_FORMAT_R16_FLOAT_DX9 = 0x121,
        SURFACE_FORMAT_P8A8_UNORM_PALETTE0 = 0x122,
        SURFACE_FORMAT_P8A8_UNORM_PALETTE1 = 0x123,
        SURFACE_FORMAT_A1B5G5R5_UNORM = 0x124,
        SURFACE_FORMAT_A4B4G4R4_UNORM = 0x125,
        SURFACE_FORMAT_L8A8_UINT = 0x126,
        SURFACE_FORMAT_L8A8_SINT = 0x127,
        SURFACE_FORMAT_R8_UNORM = 0x140,
        SURFACE_FORMAT_R8_SNORM = 0x141,
        SURFACE_FORMAT_R8_SINT = 0x142,
        SURFACE_FORMAT_R8_UINT = 0x143,
        SURFACE_FORMAT_A8_UNORM = 0x144,
        SURFACE_FORMAT_I8_UNORM = 0x145,
        SURFACE_FORMAT_L8_UNORM = 0x146,
        SURFACE_FORMAT_P4A4_UNORM_PALETTE0 = 0x147,
        SURFACE_FORMAT_A4P4_UNORM_PALETTE0 = 0x148,
        SURFACE_FORMAT_R8_SSCALED = 0x149,
        SURFACE_FORMAT_R8_USCALED = 0x14a,
        SURFACE_FORMAT_P8_UNORM_PALETTE0 = 0x14b,
        SURFACE_FORMAT_L8_UNORM_SRGB = 0x14c,
        SURFACE_FORMAT_P8_UNORM_PALETTE1 = 0x14d,
        SURFACE_FORMAT_P4A4_UNORM_PALETTE1 = 0x14e,
        SURFACE_FORMAT_A4P4_UNORM_PALETTE1 = 0x14f,
        SURFACE_FORMAT_Y8_UNORM = 0x150,
        SURFACE_FORMAT_L8_UINT = 0x152,
        SURFACE_FORMAT_L8_SINT = 0x153,
        SURFACE_FORMAT_I8_UINT = 0x154,
        SURFACE_FORMAT_I8_SINT = 0x155,
        SURFACE_FORMAT_DXT1_RGB_SRGB = 0x180,
        SURFACE_FORMAT_R1_UNORM = 0x181,
        SURFACE_FORMAT_YCRCB_NORMAL = 0x182,
        SURFACE_FORMAT_YCRCB_SWAPUVY = 0x183,
        SURFACE_FORMAT_P2_UNORM_PALETTE0 = 0x184,
        SURFACE_FORMAT_P2_UNORM_PALETTE1 = 0x185,
        SURFACE_FORMAT_BC1_UNORM = 0x186,
        SURFACE_FORMAT_BC2_UNORM = 0x187,
        SURFACE_FORMAT_BC3_UNORM = 0x188,
        SURFACE_FORMAT_BC4_UNORM = 0x189,
        SURFACE_FORMAT_BC5_UNORM = 0x18a,
        SURFACE_FORMAT_BC1_UNORM_SRGB = 0x18b,
        SURFACE_FORMAT_BC2_UNORM_SRGB = 0x18c,
        SURFACE_FORMAT_BC3_UNORM_SRGB = 0x18d,
        SURFACE_FORMAT_MONO8 = 0x18e,
        SURFACE_FORMAT_YCRCB_SWAPUV = 0x18f,
        SURFACE_FORMAT_YCRCB_SWAPY = 0x190,
        SURFACE_FORMAT_DXT1_RGB = 0x191,
        SURFACE_FORMAT_R8G8B8_UNORM = 0x193,
        SURFACE_FORMAT_R8G8B8_SNORM = 0x194,
        SURFACE_FORMAT_R8G8B8_SSCALED = 0x195,
        SURFACE_FORMAT_R8G8B8_USCALED = 0x196,
        SURFACE_FORMAT_R64G64B64A64_FLOAT = 0x197,
        SURFACE_FORMAT_R64G64B64_FLOAT = 0x198,
        SURFACE_FORMAT_BC4_SNORM = 0x199,
        SURFACE_FORMAT_BC5_SNORM = 0x19a,
        SURFACE_FORMAT_R16G16B16_FLOAT = 0x19b,
        SURFACE_FORMAT_R16G16B16_UNORM = 0x19c,
        SURFACE_FORMAT_R16G16B16_SNORM = 0x19d,
        SURFACE_FORMAT_R16G16B16_SSCALED = 0x19e,
        SURFACE_FORMAT_R16G16B16_USCALED = 0x19f,
        SURFACE_FORMAT_R8B8_UNORM = 0x1a0,
        SURFACE_FORMAT_BC6H_SF16 = 0x1a1,
        SURFACE_FORMAT_BC7_UNORM = 0x1a2,
        SURFACE_FORMAT_BC7_UNORM_SRGB = 0x1a3,
        SURFACE_FORMAT_BC6H_UF16 = 0x1a4,
        SURFACE_FORMAT_PLANAR_420_8 = 0x1a5,
        SURFACE_FORMAT_PLANAR_420_16 = 0x1a6,
        SURFACE_FORMAT_R8G8B8_UNORM_SRGB = 0x1a8,
        SURFACE_FORMAT_ETC1_RGB8 = 0x1a9,
        SURFACE_FORMAT_ETC2_RGB8 = 0x1aa,
        SURFACE_FORMAT_EAC_R11 = 0x1ab,
        SURFACE_FORMAT_EAC_RG11 = 0x1ac,
        SURFACE_FORMAT_EAC_SIGNED_R11 = 0x1ad,
        SURFACE_FORMAT_EAC_SIGNED_RG11 = 0x1ae,
        SURFACE_FORMAT_ETC2_SRGB8 = 0x1af,
        SURFACE_FORMAT_R16G16B16_UINT = 0x1b0,
        SURFACE_FORMAT_R16G16B16_SINT = 0x1b1,
        SURFACE_FORMAT_R32_SFIXED = 0x1b2,
        SURFACE_FORMAT_R10G10B10A2_SNORM = 0x1b3,
        SURFACE_FORMAT_R10G10B10A2_USCALED = 0x1b4,
        SURFACE_FORMAT_R10G10B10A2_SSCALED = 0x1b5,
        SURFACE_FORMAT_R10G10B10A2_SINT = 0x1b6,
        SURFACE_FORMAT_B10G10R10A2_SNORM = 0x1b7,
        SURFACE_FORMAT_B10G10R10A2_USCALED = 0x1b8,
        SURFACE_FORMAT_B10G10R10A2_SSCALED = 0x1b9,
        SURFACE_FORMAT_B10G10R10A2_UINT = 0x1ba,
        SURFACE_FORMAT_B10G10R10A2_SINT = 0x1bb,
        SURFACE_FORMAT_R64G64B64A64_PASSTHRU = 0x1bc,
        SURFACE_FORMAT_R64G64B64_PASSTHRU = 0x1bd,
        SURFACE_FORMAT_ETC2_RGB8_PTA = 0x1c0,
        SURFACE_FORMAT_ETC2_SRGB8_PTA = 0x1c1,
        SURFACE_FORMAT_ETC2_EAC_RGBA8 = 0x1c2,
        SURFACE_FORMAT_ETC2_EAC_SRGB8_A8 = 0x1c3,
        SURFACE_FORMAT_R8G8B8_UINT = 0x1c8,
        SURFACE_FORMAT_R8G8B8_SINT = 0x1c9,
        SURFACE_FORMAT_RAW = 0x1ff,
    } SURFACE_FORMAT;
    typedef enum tagSURFACE_TYPE {
        SURFACE_TYPE_SURFTYPE_1D = 0x0,
        SURFACE_TYPE_SURFTYPE_2D = 0x1,
        SURFACE_TYPE_SURFTYPE_3D = 0x2,
        SURFACE_TYPE_SURFTYPE_CUBE = 0x3,
        SURFACE_TYPE_SURFTYPE_BUFFER = 0x4,
        SURFACE_TYPE_SURFTYPE_RES5 = 0x5,
        SURFACE_TYPE_SURFTYPE_SCRATCH = 0x6,
        SURFACE_TYPE_SURFTYPE_NULL = 0x7,
    } SURFACE_TYPE;
    typedef enum tagSAMPLE_TAP_DISCARD_DISABLE {
        SAMPLE_TAP_DISCARD_DISABLE_DISABLE = 0x0,
        SAMPLE_TAP_DISCARD_DISABLE_ENABLE = 0x1,
    } SAMPLE_TAP_DISCARD_DISABLE;
    typedef enum tagNUMBER_OF_MULTISAMPLES {
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1 = 0x0,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_2 = 0x1,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_4 = 0x2,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_8 = 0x3,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_16 = 0x4,
    } NUMBER_OF_MULTISAMPLES;
    typedef enum tagMULTISAMPLED_SURFACE_STORAGE_FORMAT {
        MULTISAMPLED_SURFACE_STORAGE_FORMAT_MSS = 0x0,
        MULTISAMPLED_SURFACE_STORAGE_FORMAT_DEPTH_STENCIL = 0x1,
    } MULTISAMPLED_SURFACE_STORAGE_FORMAT;
    typedef enum tagRENDER_TARGET_AND_SAMPLE_UNORM_ROTATION {
        RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION_0DEG = 0x0,
        RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION_90DEG = 0x1,
        RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION_180DEG = 0x2,
        RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION_270DEG = 0x3,
    } RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION;
    typedef enum tagCOHERENCY_TYPE {
        COHERENCY_TYPE_GPU_COHERENT = 0x0, // patched from COHERENCY_TYPE_SINGLE_GPU_COHERENT
        COHERENCY_TYPE_IA_COHERENT = 0x1,  // patched from COHERENCY_TYPE_SYSTEM_COHERENT
        COHERENCY_TYPE_MULTI_GPU_COHERENT = 0x2,
    } COHERENCY_TYPE;
    typedef enum tagL1_CACHE_CONTROL {
        L1_CACHE_CONTROL_WBP = 0x0,
        L1_CACHE_CONTROL_UC = 0x1,
        L1_CACHE_CONTROL_WB = 0x2,
        L1_CACHE_CONTROL_WT = 0x3,
        L1_CACHE_CONTROL_WS = 0x4,
    } L1_CACHE_CONTROL;
    typedef enum tagAUXILIARY_SURFACE_MODE {
        AUXILIARY_SURFACE_MODE_AUX_NONE = 0x0,
        AUXILIARY_SURFACE_MODE_AUX_APPEND = 0x1,
        AUXILIARY_SURFACE_MODE_AUX_MCS = 0x2,
        AUXILIARY_SURFACE_MODE_AUX_AMFS = 0x3,
    } AUXILIARY_SURFACE_MODE;
    typedef enum tagHALF_PITCH_FOR_CHROMA {
        HALF_PITCH_FOR_CHROMA_DISABLE = 0x0,
        HALF_PITCH_FOR_CHROMA_ENABLE = 0x1,
    } HALF_PITCH_FOR_CHROMA;
    typedef enum tagSHADER_CHANNEL_SELECT {
        SHADER_CHANNEL_SELECT_ZERO = 0x0,
        SHADER_CHANNEL_SELECT_ONE = 0x1,
        SHADER_CHANNEL_SELECT_RED = 0x4,
        SHADER_CHANNEL_SELECT_GREEN = 0x5,
        SHADER_CHANNEL_SELECT_BLUE = 0x6,
        SHADER_CHANNEL_SELECT_ALPHA = 0x7,
    } SHADER_CHANNEL_SELECT;
    typedef enum tagCOMPRESSION_FORMAT {
        COMPRESSION_FORMAT_CMF_R8 = 0x0,
        COMPRESSION_FORMAT_CMF_R8_G8 = 0x1,
        COMPRESSION_FORMAT_CMF_R8_G8_B8_A8 = 0x2,
        COMPRESSION_FORMAT_CMF_R10_G10_B10_A2 = 0x3,
        COMPRESSION_FORMAT_CMF_R11_G11_B10 = 0x4,
        COMPRESSION_FORMAT_CMF_R16 = 0x5,
        COMPRESSION_FORMAT_CMF_R16_G16 = 0x6,
        COMPRESSION_FORMAT_CMF_R16_G16_B16_A16 = 0x7,
        COMPRESSION_FORMAT_CMF_R32 = 0x8,
        COMPRESSION_FORMAT_CMF_R32_G32 = 0x9,
        COMPRESSION_FORMAT_CMF_R32_G32_B32_A32 = 0xa,
        COMPRESSION_FORMAT_CMF_Y16_U16_Y16_V16 = 0xb,
        COMPRESSION_FORMAT_CMF_ML8 = 0xf,
    } COMPRESSION_FORMAT;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.MediaBoundaryPixelMode = MEDIA_BOUNDARY_PIXEL_MODE_NORMAL_MODE;
        TheStructure.Common.RenderCacheReadWriteMode = RENDER_CACHE_READ_WRITE_MODE_WRITE_ONLY_CACHE;
        TheStructure.Common.TileMode = TILE_MODE_LINEAR;
        TheStructure.Common.SurfaceHorizontalAlignment = SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_16;
        TheStructure.Common.SurfaceVerticalAlignment = SURFACE_VERTICAL_ALIGNMENT_VALIGN_4;
        TheStructure.Common.SurfaceType = SURFACE_TYPE_SURFTYPE_1D;
        TheStructure.Common.NumberOfMultisamples = NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1;
        TheStructure.Common.MultisampledSurfaceStorageFormat = MULTISAMPLED_SURFACE_STORAGE_FORMAT_MSS;
        TheStructure.Common.RenderTargetAndSampleUnormRotation = RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION_0DEG;
        TheStructure.Common.L1CacheControlCachePolicy = L1_CACHE_CONTROL_WBP;
        TheStructure.Common.AuxiliarySurfaceMode = AUXILIARY_SURFACE_MODE_AUX_NONE;
        TheStructure.Common.CompressionFormat = COMPRESSION_FORMAT_CMF_R8;
        TheStructure._SurfaceFormatIsPlanar.HalfPitchForChroma = HALF_PITCH_FOR_CHROMA_DISABLE;
    }
    static tagRENDER_SURFACE_STATE sInit() {
        RENDER_SURFACE_STATE state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 16);
        return TheStructure.RawData[index];
    }
    inline void setCubeFaceEnablePositiveZ(const bool value) {
        TheStructure.Common.CubeFaceEnablePositiveZ = value;
    }
    inline bool getCubeFaceEnablePositiveZ() const {
        return TheStructure.Common.CubeFaceEnablePositiveZ;
    }
    inline void setCubeFaceEnableNegativeZ(const bool value) {
        TheStructure.Common.CubeFaceEnableNegativeZ = value;
    }
    inline bool getCubeFaceEnableNegativeZ() const {
        return TheStructure.Common.CubeFaceEnableNegativeZ;
    }
    inline void setCubeFaceEnablePositiveY(const bool value) {
        TheStructure.Common.CubeFaceEnablePositiveY = value;
    }
    inline bool getCubeFaceEnablePositiveY() const {
        return TheStructure.Common.CubeFaceEnablePositiveY;
    }
    inline void setCubeFaceEnableNegativeY(const bool value) {
        TheStructure.Common.CubeFaceEnableNegativeY = value;
    }
    inline bool getCubeFaceEnableNegativeY() const {
        return TheStructure.Common.CubeFaceEnableNegativeY;
    }
    inline void setCubeFaceEnablePositiveX(const bool value) {
        TheStructure.Common.CubeFaceEnablePositiveX = value;
    }
    inline bool getCubeFaceEnablePositiveX() const {
        return TheStructure.Common.CubeFaceEnablePositiveX;
    }
    inline void setCubeFaceEnableNegativeX(const bool value) {
        TheStructure.Common.CubeFaceEnableNegativeX = value;
    }
    inline bool getCubeFaceEnableNegativeX() const {
        return TheStructure.Common.CubeFaceEnableNegativeX;
    }
    inline void setMediaBoundaryPixelMode(const MEDIA_BOUNDARY_PIXEL_MODE value) {
        TheStructure.Common.MediaBoundaryPixelMode = value;
    }
    inline MEDIA_BOUNDARY_PIXEL_MODE getMediaBoundaryPixelMode() const {
        return static_cast<MEDIA_BOUNDARY_PIXEL_MODE>(TheStructure.Common.MediaBoundaryPixelMode);
    }
    inline void setRenderCacheReadWriteMode(const RENDER_CACHE_READ_WRITE_MODE value) {
        TheStructure.Common.RenderCacheReadWriteMode = value;
    }
    inline RENDER_CACHE_READ_WRITE_MODE getRenderCacheReadWriteMode() const {
        return static_cast<RENDER_CACHE_READ_WRITE_MODE>(TheStructure.Common.RenderCacheReadWriteMode);
    }
    inline void setEnableSamplerRouteToLsc(const bool value) {
        TheStructure.Common.EnableSamplerRouteToLsc = value;
    }
    inline bool getEnableSamplerRouteToLsc() const {
        return TheStructure.Common.EnableSamplerRouteToLsc;
    }
    inline void setVerticalLineStrideOffset(const bool value) {
        TheStructure.Common.VerticalLineStrideOffset = value;
    }
    inline bool getVerticalLineStrideOffset() const {
        return TheStructure.Common.VerticalLineStrideOffset;
    }
    inline void setVerticalLineStride(const bool value) {
        TheStructure.Common.VerticalLineStride = value;
    }
    inline bool getVerticalLineStride() const {
        return TheStructure.Common.VerticalLineStride;
    }
    inline void setTileMode(const TILE_MODE value) {
        TheStructure.Common.TileMode = value;
    }
    inline TILE_MODE getTileMode() const {
        return static_cast<TILE_MODE>(TheStructure.Common.TileMode);
    }
    inline void setSurfaceHorizontalAlignment(const SURFACE_HORIZONTAL_ALIGNMENT value) {
        TheStructure.Common.SurfaceHorizontalAlignment = value;
    }
    inline SURFACE_HORIZONTAL_ALIGNMENT getSurfaceHorizontalAlignment() const {
        return static_cast<SURFACE_HORIZONTAL_ALIGNMENT>(TheStructure.Common.SurfaceHorizontalAlignment);
    }
    inline void setSurfaceVerticalAlignment(const SURFACE_VERTICAL_ALIGNMENT value) {
        TheStructure.Common.SurfaceVerticalAlignment = value;
    }
    inline SURFACE_VERTICAL_ALIGNMENT getSurfaceVerticalAlignment() const {
        return static_cast<SURFACE_VERTICAL_ALIGNMENT>(TheStructure.Common.SurfaceVerticalAlignment);
    }
    inline void setSurfaceFormat(const SURFACE_FORMAT value) {
        TheStructure.Common.SurfaceFormat = value;
    }
    inline SURFACE_FORMAT getSurfaceFormat() const {
        return static_cast<SURFACE_FORMAT>(TheStructure.Common.SurfaceFormat);
    }
    inline void setSurfaceArray(const bool value) {
        TheStructure.Common.SurfaceArray = value;
    }
    inline bool getSurfaceArray() const {
        return TheStructure.Common.SurfaceArray;
    }
    inline void setSurfaceType(const SURFACE_TYPE value) {
        TheStructure.Common.SurfaceType = value;
    }
    inline SURFACE_TYPE getSurfaceType() const {
        return static_cast<SURFACE_TYPE>(TheStructure.Common.SurfaceType);
    }
    typedef enum tagSURFACEQPITCH {
        SURFACEQPITCH_BIT_SHIFT = 0x2,
        SURFACEQPITCH_ALIGN_SIZE = 0x4,
    } SURFACEQPITCH;
    inline void setSurfaceQPitch(const uint32_t value) {
        UNRECOVERABLE_IF((value >> SURFACEQPITCH_BIT_SHIFT) > 0x1ffff);
        TheStructure.Common.SurfaceQpitch = value >> SURFACEQPITCH_BIT_SHIFT;
    }
    inline uint32_t getSurfaceQPitch() const {
        return TheStructure.Common.SurfaceQpitch << SURFACEQPITCH_BIT_SHIFT;
    }
    inline void setBaseMipLevel(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x1f);
        TheStructure.Common.BaseMipLevel = value;
    }
    inline uint32_t getBaseMipLevel() const {
        return TheStructure.Common.BaseMipLevel;
    }
    inline void setMemoryObjectControlStateEncryptedData(const bool value) {
        TheStructure.Common.MemoryObjectControlStateEncryptedData = value;
    }
    inline bool getMemoryObjectControlStateEncryptedData() const {
        return TheStructure.Common.MemoryObjectControlStateEncryptedData;
    }
    inline void setMemoryObjectControlState(const uint32_t value) {
        TheStructure.Common.MemoryObjectControlStateEncryptedData = value;
        TheStructure.Common.MemoryObjectControlStateIndexToMocsTables = (value >> 1);
    }
    inline uint32_t getMemoryObjectControlState() const {
        uint32_t mocs = TheStructure.Common.MemoryObjectControlStateEncryptedData;
        mocs |= (TheStructure.Common.MemoryObjectControlStateIndexToMocsTables << 1);
        return (mocs);
    }
    inline void setWidth(const uint32_t value) {
        TheStructure.Common.Width = value - 1;
    }
    inline uint32_t getWidth() const {
        return TheStructure.Common.Width + 1;
    }
    inline void setHeight(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3fff + 1);
        TheStructure.Common.Height = value - 1;
    }
    inline uint32_t getHeight() const {
        return TheStructure.Common.Height + 1;
    }
    inline void setDepthStencilResource(const bool value) {
        TheStructure.Common.DepthStencilResource = value;
    }
    inline bool getDepthStencilResource() const {
        return TheStructure.Common.DepthStencilResource;
    }
    inline void setSurfacePitch(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3ffff + 1);
        TheStructure.Common.SurfacePitch = value - 1;
    }
    inline uint32_t getSurfacePitch() const {
        return TheStructure.Common.SurfacePitch + 1;
    }
    inline void setDepth(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x7ff + 1);
        TheStructure.Common.Depth = value - 1;
    }
    inline uint32_t getDepth() const {
        return TheStructure.Common.Depth + 1;
    }
    inline void setMultisamplePositionPaletteIndex(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x7);
        TheStructure.Common.MultisamplePositionPaletteIndex = value;
    }
    inline uint32_t getMultisamplePositionPaletteIndex() const {
        return TheStructure.Common.MultisamplePositionPaletteIndex;
    }
    inline void setNumberOfMultisamples(const NUMBER_OF_MULTISAMPLES value) {
        TheStructure.Common.NumberOfMultisamples = value;
    }
    inline NUMBER_OF_MULTISAMPLES getNumberOfMultisamples() const {
        return static_cast<NUMBER_OF_MULTISAMPLES>(TheStructure.Common.NumberOfMultisamples);
    }
    inline void setMultisampledSurfaceStorageFormat(const MULTISAMPLED_SURFACE_STORAGE_FORMAT value) {
        TheStructure.Common.MultisampledSurfaceStorageFormat = value;
    }
    inline MULTISAMPLED_SURFACE_STORAGE_FORMAT getMultisampledSurfaceStorageFormat() const {
        return static_cast<MULTISAMPLED_SURFACE_STORAGE_FORMAT>(TheStructure.Common.MultisampledSurfaceStorageFormat);
    }
    inline void setRenderTargetViewExtent(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x7ff + 1);
        TheStructure.Common.RenderTargetViewExtent = value - 1;
    }
    inline uint32_t getRenderTargetViewExtent() const {
        return TheStructure.Common.RenderTargetViewExtent + 1;
    }
    inline void setMinimumArrayElement(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x7ff);
        TheStructure.Common.MinimumArrayElement = value;
    }
    inline uint32_t getMinimumArrayElement() const {
        return TheStructure.Common.MinimumArrayElement;
    }
    inline void setRenderTargetAndSampleUnormRotation(const RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION value) {
        TheStructure.Common.RenderTargetAndSampleUnormRotation = value;
    }
    inline RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION getRenderTargetAndSampleUnormRotation() const {
        return static_cast<RENDER_TARGET_AND_SAMPLE_UNORM_ROTATION>(TheStructure.Common.RenderTargetAndSampleUnormRotation);
    }
    inline void setMIPCountLOD(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xf);
        TheStructure.Common.MipCountLod = value;
    }
    inline uint32_t getMIPCountLOD() const {
        return TheStructure.Common.MipCountLod;
    }
    inline void setSurfaceMinLOD(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xf);
        TheStructure.Common.SurfaceMinLod = value;
    }
    inline uint32_t getSurfaceMinLOD() const {
        return TheStructure.Common.SurfaceMinLod;
    }
    inline void setMipTailStartLOD(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xf);
        TheStructure.Common.MipTailStartLod = value;
    }
    inline uint32_t getMipTailStartLOD() const {
        return TheStructure.Common.MipTailStartLod;
    }
    inline void setL1CacheControlCachePolicy(const L1_CACHE_CONTROL value) {
        TheStructure.Common.L1CacheControlCachePolicy = value;
    }
    inline L1_CACHE_CONTROL getL1CacheControlCachePolicy() const {
        return static_cast<L1_CACHE_CONTROL>(TheStructure.Common.L1CacheControlCachePolicy);
    }
    inline void setEwaDisableForCube(const bool value) {
        TheStructure.Common.EwaDisableForCube = value;
    }
    inline bool getEwaDisableForCube() const {
        return TheStructure.Common.EwaDisableForCube;
    }
    typedef enum tagYOFFSET {
        YOFFSET_BIT_SHIFT = 0x2,
        YOFFSET_ALIGN_SIZE = 0x4,
    } YOFFSET;
    inline void setYOffset(const uint32_t value) {
        UNRECOVERABLE_IF((value >> YOFFSET_BIT_SHIFT) > 0x1f);
        TheStructure.Common.YOffset = value >> YOFFSET_BIT_SHIFT;
    }
    inline uint32_t getYOffset() const {
        return TheStructure.Common.YOffset << YOFFSET_BIT_SHIFT;
    }
    typedef enum tagXOFFSET {
        XOFFSET_BIT_SHIFT = 0x2,
        XOFFSET_ALIGN_SIZE = 0x4,
    } XOFFSET;
    inline void setXOffset(const uint32_t value) {
        UNRECOVERABLE_IF((value >> XOFFSET_BIT_SHIFT) > 0x1ff);
        TheStructure.Common.XOffset = value >> XOFFSET_BIT_SHIFT;
    }
    inline uint32_t getXOffset() const {
        return TheStructure.Common.XOffset << XOFFSET_BIT_SHIFT;
    }
    inline void setAuxiliarySurfaceMode(const AUXILIARY_SURFACE_MODE value) {
        TheStructure.Common.AuxiliarySurfaceMode = value;
    }
    inline AUXILIARY_SURFACE_MODE getAuxiliarySurfaceMode() const {
        return static_cast<AUXILIARY_SURFACE_MODE>(TheStructure.Common.AuxiliarySurfaceMode);
    }
    inline void setYuvInterpolationEnable(const bool value) {
        TheStructure.Common.YuvInterpolationEnable = value;
    }
    inline bool getYuvInterpolationEnable() const {
        return TheStructure.Common.YuvInterpolationEnable;
    }
    inline void setResourceMinLod(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xfff);
        TheStructure.Common.ResourceMinLod = value;
    }
    inline uint32_t getResourceMinLod() const {
        return TheStructure.Common.ResourceMinLod;
    }
    inline void setShaderChannelSelectAlpha(const SHADER_CHANNEL_SELECT value) {
        TheStructure.Common.ShaderChannelSelectAlpha = value;
    }
    inline SHADER_CHANNEL_SELECT getShaderChannelSelectAlpha() const {
        return static_cast<SHADER_CHANNEL_SELECT>(TheStructure.Common.ShaderChannelSelectAlpha);
    }
    inline void setShaderChannelSelectBlue(const SHADER_CHANNEL_SELECT value) {
        TheStructure.Common.ShaderChannelSelectBlue = value;
    }
    inline SHADER_CHANNEL_SELECT getShaderChannelSelectBlue() const {
        return static_cast<SHADER_CHANNEL_SELECT>(TheStructure.Common.ShaderChannelSelectBlue);
    }
    inline void setShaderChannelSelectGreen(const SHADER_CHANNEL_SELECT value) {
        TheStructure.Common.ShaderChannelSelectGreen = value;
    }
    inline SHADER_CHANNEL_SELECT getShaderChannelSelectGreen() const {
        return static_cast<SHADER_CHANNEL_SELECT>(TheStructure.Common.ShaderChannelSelectGreen);
    }
    inline void setShaderChannelSelectRed(const SHADER_CHANNEL_SELECT value) {
        TheStructure.Common.ShaderChannelSelectRed = value;
    }
    inline SHADER_CHANNEL_SELECT getShaderChannelSelectRed() const {
        return static_cast<SHADER_CHANNEL_SELECT>(TheStructure.Common.ShaderChannelSelectRed);
    }
    inline void setSurfaceBaseAddress(const uint64_t value) {
        TheStructure.Common.SurfaceBaseAddress = value;
    }
    inline uint64_t getSurfaceBaseAddress() const {
        return TheStructure.Common.SurfaceBaseAddress;
    }
    inline void setCompressionFormat(const uint32_t value) {
        TheStructure.Common.CompressionFormat = value;
    }
    inline COMPRESSION_FORMAT getCompressionFormat() const {
        return static_cast<COMPRESSION_FORMAT>(TheStructure.Common.CompressionFormat);
    }
    inline void setMipRegionDepthInLog2(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xf);
        TheStructure.Common.MipRegionDepthInLog2 = value;
    }
    inline uint32_t getMipRegionDepthInLog2() const {
        return TheStructure.Common.MipRegionDepthInLog2;
    }
    inline void setDisableSamplerBackingByLsc(const bool value) {
        TheStructure.Common.DisableSamplerBackingByLsc = value;
    }
    inline bool getDisableSamplerBackingByLsc() const {
        return TheStructure.Common.DisableSamplerBackingByLsc;
    }
    inline void setDisallowLowQualityFiltering(const bool value) {
        TheStructure.Common.DisallowLowQualityFiltering = value;
    }
    inline bool getDisallowLowQualityFiltering() const {
        return TheStructure.Common.DisallowLowQualityFiltering;
    }
    inline void setYOffsetForUOrUvPlane(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3fff);
        TheStructure._SurfaceFormatIsPlanar.YOffsetForUOrUvPlane = value;
    }
    inline uint32_t getYOffsetForUOrUvPlane() const {
        return TheStructure._SurfaceFormatIsPlanar.YOffsetForUOrUvPlane;
    }
    inline void setXOffsetForUOrUvPlane(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3fff);
        TheStructure._SurfaceFormatIsPlanar.XOffsetForUOrUvPlane = value;
    }
    inline uint32_t getXOffsetForUOrUvPlane() const {
        return TheStructure._SurfaceFormatIsPlanar.XOffsetForUOrUvPlane;
    }
    inline void setHalfPitchForChroma(const HALF_PITCH_FOR_CHROMA value) {
        TheStructure._SurfaceFormatIsPlanar.HalfPitchForChroma = value;
    }
    inline HALF_PITCH_FOR_CHROMA getHalfPitchForChroma() const {
        return static_cast<HALF_PITCH_FOR_CHROMA>(TheStructure._SurfaceFormatIsPlanar.HalfPitchForChroma);
    }
    inline void setSeparateUvPlaneEnable(const bool value) {
        TheStructure._SurfaceFormatIsPlanar.SeparateUvPlaneEnable = value;
    }
    inline bool getSeparateUvPlaneEnable() const {
        return TheStructure._SurfaceFormatIsPlanar.SeparateUvPlaneEnable;
    }
    inline void setYOffsetForVPlane(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0x3fffL);
        TheStructure._SurfaceFormatIsPlanar.YOffsetForVPlane = value;
    }
    inline uint64_t getYOffsetForVPlane() const {
        return TheStructure._SurfaceFormatIsPlanar.YOffsetForVPlane;
    }
    inline void setXOffsetForVPlane(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0x3fffL);
        TheStructure._SurfaceFormatIsPlanar.XOffsetForVPlane = value;
    }
    inline uint64_t getXOffsetForVPlane() const {
        return TheStructure._SurfaceFormatIsPlanar.XOffsetForVPlane;
    }
    inline void setAuxiliarySurfacePitch(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3ff + 1);
        TheStructure._SurfaceFormatIsnotPlanar.AuxiliarySurfacePitch = value - 1;
    }
    inline uint32_t getAuxiliarySurfacePitch() const {
        return TheStructure._SurfaceFormatIsnotPlanar.AuxiliarySurfacePitch + 1;
    }
    typedef enum tagAUXILIARYSURFACEQPITCH {
        AUXILIARYSURFACEQPITCH_BIT_SHIFT = 0x2,
        AUXILIARYSURFACEQPITCH_ALIGN_SIZE = 0x4,
    } AUXILIARYSURFACEQPITCH;
    inline void setAuxiliarySurfaceQPitch(const uint32_t value) {
        UNRECOVERABLE_IF((value >> AUXILIARYSURFACEQPITCH_BIT_SHIFT) > 0x1ffff);
        TheStructure._SurfaceFormatIsnotPlanar.AuxiliarySurfaceQpitch = value >> AUXILIARYSURFACEQPITCH_BIT_SHIFT;
    }
    inline uint32_t getAuxiliarySurfaceQPitch() const {
        return TheStructure._SurfaceFormatIsnotPlanar.AuxiliarySurfaceQpitch << AUXILIARYSURFACEQPITCH_BIT_SHIFT;
    }
    typedef enum tagAUXILIARYSURFACEBASEADDRESS {
        AUXILIARYSURFACEBASEADDRESS_BIT_SHIFT = 0xc,
        AUXILIARYSURFACEBASEADDRESS_ALIGN_SIZE = 0x1000,
    } AUXILIARYSURFACEBASEADDRESS;
    inline void setAuxiliarySurfaceBaseAddress(const uint64_t value) {
        UNRECOVERABLE_IF((value >> AUXILIARYSURFACEBASEADDRESS_BIT_SHIFT) > 0xffffffffffffffffL);
        TheStructure._PropertyauxiliarySurfaceModeIsnotAux_NoneAnd_PropertyauxiliarySurfaceModeIsnotAux_Append.AuxiliarySurfaceBaseAddress = value >> AUXILIARYSURFACEBASEADDRESS_BIT_SHIFT;
    }
    inline uint64_t getAuxiliarySurfaceBaseAddress() const {
        return TheStructure._PropertyauxiliarySurfaceModeIsnotAux_NoneAnd_PropertyauxiliarySurfaceModeIsnotAux_Append.AuxiliarySurfaceBaseAddress << AUXILIARYSURFACEBASEADDRESS_BIT_SHIFT;
    }
    typedef enum tagAPPENDCOUNTERADDRESS {
        APPENDCOUNTERADDRESS_BIT_SHIFT = 0x2,
        APPENDCOUNTERADDRESS_ALIGN_SIZE = 0x4,
    } APPENDCOUNTERADDRESS;
    inline void setAppendCounterAddress(const uint64_t value) {
        UNRECOVERABLE_IF((value >> APPENDCOUNTERADDRESS_BIT_SHIFT) > 0xffffffffffffffffL);
        TheStructure.PropertyauxiliarySurfaceModeIsAux_Append.AppendCounterAddress = value >> APPENDCOUNTERADDRESS_BIT_SHIFT;
    }
    inline uint64_t getAppendCounterAddress() const {
        return TheStructure.PropertyauxiliarySurfaceModeIsAux_Append.AppendCounterAddress << APPENDCOUNTERADDRESS_BIT_SHIFT;
    }
    inline void setMipRegionWidthInLog2(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0xfL);
        TheStructure.AuxiliarySurfaceModeIsnotAux_Append.MipRegionWidthInLog2 = value;
    }
    inline uint64_t getMipRegionWidthInLog2() const {
        return TheStructure.AuxiliarySurfaceModeIsnotAux_Append.MipRegionWidthInLog2;
    }
    inline void setMipRegionHeightInLog2(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0xfL);
        TheStructure.AuxiliarySurfaceModeIsnotAux_Append.MipRegionHeightInLog2 = value;
    }
    inline uint64_t getMipRegionHeightInLog2() const {
        return TheStructure.AuxiliarySurfaceModeIsnotAux_Append.MipRegionHeightInLog2;
    }
    inline void setProceduralTexture(const bool value) {
        TheStructure.AuxiliarySurfaceModeIsnotAux_Append.ProceduralTexture = value;
    }
    inline bool getProceduralTexture() const {
        return TheStructure.AuxiliarySurfaceModeIsnotAux_Append.ProceduralTexture;
    }
} RENDER_SURFACE_STATE;
STATIC_ASSERT(64 == sizeof(RENDER_SURFACE_STATE));

typedef struct tagSAMPLER_STATE {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t LodAlgorithm : BITFIELD_RANGE(0, 0);
            uint32_t TextureLodBias : BITFIELD_RANGE(1, 13);
            uint32_t MinModeFilter : BITFIELD_RANGE(14, 16);
            uint32_t MagModeFilter : BITFIELD_RANGE(17, 19);
            uint32_t MipModeFilter : BITFIELD_RANGE(20, 21);
            uint32_t Reserved_22 : BITFIELD_RANGE(22, 25);
            uint32_t LowQualityCubeCornerModeEnable : BITFIELD_RANGE(26, 26);
            uint32_t LodPreclampMode : BITFIELD_RANGE(27, 28);
            uint32_t Reserved_29 : BITFIELD_RANGE(29, 29);
            uint32_t CpsLodCompensationEnable : BITFIELD_RANGE(30, 30);
            uint32_t SamplerDisable : BITFIELD_RANGE(31, 31);
            // DWORD 1
            uint32_t CubeSurfaceControlMode : BITFIELD_RANGE(0, 0);
            uint32_t ShadowFunction : BITFIELD_RANGE(1, 3);
            uint32_t ChromakeyMode : BITFIELD_RANGE(4, 4);
            uint32_t ChromakeyIndex : BITFIELD_RANGE(5, 6);
            uint32_t ChromakeyEnable : BITFIELD_RANGE(7, 7);
            uint32_t MaxLod : BITFIELD_RANGE(8, 19);
            uint32_t MinLod : BITFIELD_RANGE(20, 31);
            // DWORD 2
            uint32_t LodClampMagnificationMode : BITFIELD_RANGE(0, 0);
            uint32_t SrgbDecode : BITFIELD_RANGE(1, 1);
            uint32_t ReturnFilterWeightForNullTexels : BITFIELD_RANGE(2, 2);
            uint32_t ReturnFilterWeightForBorderTexels : BITFIELD_RANGE(3, 3);
            uint32_t Reserved_68 : BITFIELD_RANGE(4, 4);
            uint32_t ForceGather4Behavior : BITFIELD_RANGE(5, 5);
            uint32_t IndirectStatePointer : BITFIELD_RANGE(6, 23);
            uint32_t ExtendedIndirectStatePointer : BITFIELD_RANGE(24, 31);
            // DWORD 3
            uint32_t TczAddressControlMode : BITFIELD_RANGE(0, 2);
            uint32_t TcyAddressControlMode : BITFIELD_RANGE(3, 5);
            uint32_t TcxAddressControlMode : BITFIELD_RANGE(6, 8);
            uint32_t ReductionTypeEnable : BITFIELD_RANGE(9, 9);
            uint32_t NonNormalizedCoordinateEnable : BITFIELD_RANGE(10, 10);
            uint32_t MipLinearFilterQuality : BITFIELD_RANGE(11, 12);
            uint32_t RAddressMinFilterRoundingEnable : BITFIELD_RANGE(13, 13);
            uint32_t RAddressMagFilterRoundingEnable : BITFIELD_RANGE(14, 14);
            uint32_t VAddressMinFilterRoundingEnable : BITFIELD_RANGE(15, 15);
            uint32_t VAddressMagFilterRoundingEnable : BITFIELD_RANGE(16, 16);
            uint32_t UAddressMinFilterRoundingEnable : BITFIELD_RANGE(17, 17);
            uint32_t UAddressMagFilterRoundingEnable : BITFIELD_RANGE(18, 18);
            uint32_t MaximumAnisotropy : BITFIELD_RANGE(19, 21);
            uint32_t ReductionType : BITFIELD_RANGE(22, 23);
            uint32_t AllowLowQualityLodCalculation : BITFIELD_RANGE(24, 24);
            uint32_t Reserved_121 : BITFIELD_RANGE(25, 25);
            uint32_t LowQualityFilter : BITFIELD_RANGE(26, 26);
            uint32_t Reserved_123 : BITFIELD_RANGE(27, 31);
        } Common;
        uint32_t RawData[4];
    } TheStructure;
    typedef enum tagLOD_ALGORITHM {
        LOD_ALGORITHM_LEGACY = 0x0,
        LOD_ALGORITHM_EWA_APPROXIMATION = 0x1,
    } LOD_ALGORITHM;
    typedef enum tagMIN_MODE_FILTER {
        MIN_MODE_FILTER_NEAREST = 0x0,
        MIN_MODE_FILTER_LINEAR = 0x1,
        MIN_MODE_FILTER_ANISOTROPIC = 0x2,
    } MIN_MODE_FILTER;
    typedef enum tagMAG_MODE_FILTER {
        MAG_MODE_FILTER_NEAREST = 0x0,
        MAG_MODE_FILTER_LINEAR = 0x1,
        MAG_MODE_FILTER_ANISOTROPIC = 0x2,
    } MAG_MODE_FILTER;
    typedef enum tagMIP_MODE_FILTER {
        MIP_MODE_FILTER_NONE = 0x0,
        MIP_MODE_FILTER_NEAREST = 0x1,
        MIP_MODE_FILTER_LINEAR = 0x3,
    } MIP_MODE_FILTER;
    typedef enum tagLOW_QUALITY_CUBE_CORNER_MODE_ENABLE {
        LOW_QUALITY_CUBE_CORNER_MODE_ENABLE_DISABLE = 0x0,
        LOW_QUALITY_CUBE_CORNER_MODE_ENABLE_ENABLE = 0x1,
    } LOW_QUALITY_CUBE_CORNER_MODE_ENABLE;
    typedef enum tagLOD_PRECLAMP_MODE {
        LOD_PRECLAMP_MODE_NONE = 0x0,
        LOD_PRECLAMP_MODE_OGL = 0x2,
        LOD_PRECLAMP_MODE_D3D = 0x3,
    } LOD_PRECLAMP_MODE;
    typedef enum tagCUBE_SURFACE_CONTROL_MODE {
        CUBE_SURFACE_CONTROL_MODE_PROGRAMMED = 0x0,
        CUBE_SURFACE_CONTROL_MODE_OVERRIDE = 0x1,
    } CUBE_SURFACE_CONTROL_MODE;
    typedef enum tagSHADOW_FUNCTION {
        SHADOW_FUNCTION_PREFILTEROP_ALWAYS = 0x0,
        SHADOW_FUNCTION_PREFILTEROP_NEVER = 0x1,
        SHADOW_FUNCTION_PREFILTEROP_LESS = 0x2,
        SHADOW_FUNCTION_PREFILTEROP_EQUAL = 0x3,
        SHADOW_FUNCTION_PREFILTEROP_LEQUAL = 0x4,
        SHADOW_FUNCTION_PREFILTEROP_GREATER = 0x5,
        SHADOW_FUNCTION_PREFILTEROP_NOTEQUAL = 0x6,
        SHADOW_FUNCTION_PREFILTEROP_GEQUAL = 0x7,
    } SHADOW_FUNCTION;
    typedef enum tagCHROMAKEY_MODE {
        CHROMAKEY_MODE_KEYFILTER_KILL_ON_ANY_MATCH = 0x0,
        CHROMAKEY_MODE_KEYFILTER_REPLACE_BLACK = 0x1,
    } CHROMAKEY_MODE;
    typedef enum tagLOD_CLAMP_MAGNIFICATION_MODE {
        LOD_CLAMP_MAGNIFICATION_MODE_MIPNONE = 0x0,
        LOD_CLAMP_MAGNIFICATION_MODE_MIPFILTER = 0x1,
    } LOD_CLAMP_MAGNIFICATION_MODE;
    typedef enum tagSRGB_DECODE {
        SRGB_DECODE_DECODE_EXT = 0x0,
        SRGB_DECODE_SKIP_DECODE_EXT = 0x1,
    } SRGB_DECODE;
    typedef enum tagRETURN_FILTER_WEIGHT_FOR_NULL_TEXELS {
        RETURN_FILTER_WEIGHT_FOR_NULL_TEXELS_DISABLE = 0x0,
        RETURN_FILTER_WEIGHT_FOR_NULL_TEXELS_ENABLE = 0x1,
    } RETURN_FILTER_WEIGHT_FOR_NULL_TEXELS;
    typedef enum tagRETURN_FILTER_WEIGHT_FOR_BORDER_TEXELS {
        RETURN_FILTER_WEIGHT_FOR_BORDER_TEXELS_DISABLE = 0x0,
        RETURN_FILTER_WEIGHT_FOR_BORDER_TEXELS_ENABLE = 0x1,
    } RETURN_FILTER_WEIGHT_FOR_BORDER_TEXELS;
    typedef enum tagTEXTURE_COORDINATE_MODE {
        TEXTURE_COORDINATE_MODE_WRAP = 0x0,
        TEXTURE_COORDINATE_MODE_MIRROR = 0x1,
        TEXTURE_COORDINATE_MODE_CLAMP = 0x2,
        TEXTURE_COORDINATE_MODE_CUBE = 0x3,
        TEXTURE_COORDINATE_MODE_CLAMP_BORDER = 0x4,
        TEXTURE_COORDINATE_MODE_MIRROR_ONCE = 0x5,
        TEXTURE_COORDINATE_MODE_HALF_BORDER = 0x6,
        TEXTURE_COORDINATE_MODE_MIRROR_101 = 0x7,
    } TEXTURE_COORDINATE_MODE;
    typedef enum tagMIP_LINEAR_FILTER_QUALITY {
        MIP_LINEAR_FILTER_QUALITY_FULL_QUALITY = 0x0,
        MIP_LINEAR_FILTER_QUALITY_HIGH_QUALITY = 0x1,
        MIP_LINEAR_FILTER_QUALITY_MEDIUM_QUALITY = 0x2,
        MIP_LINEAR_FILTER_QUALITY_LOW_QUALITY = 0x3,
    } MIP_LINEAR_FILTER_QUALITY;
    typedef enum tagMAXIMUM_ANISOTROPY {
        MAXIMUM_ANISOTROPY_RATIO_21 = 0x0,
        MAXIMUM_ANISOTROPY_RATIO_41 = 0x1,
        MAXIMUM_ANISOTROPY_RATIO_61 = 0x2,
        MAXIMUM_ANISOTROPY_RATIO_81 = 0x3,
        MAXIMUM_ANISOTROPY_RATIO_101 = 0x4,
        MAXIMUM_ANISOTROPY_RATIO_121 = 0x5,
        MAXIMUM_ANISOTROPY_RATIO_141 = 0x6,
        MAXIMUM_ANISOTROPY_RATIO_161 = 0x7,
    } MAXIMUM_ANISOTROPY;
    typedef enum tagREDUCTION_TYPE {
        REDUCTION_TYPE_STD_FILTER = 0x0,
        REDUCTION_TYPE_COMPARISON = 0x1,
        REDUCTION_TYPE_MINIMUM = 0x2,
        REDUCTION_TYPE_MAXIMUM = 0x3,
    } REDUCTION_TYPE;
    typedef enum tagLOW_QUALITY_FILTER {
        LOW_QUALITY_FILTER_DISABLE = 0x0,
        LOW_QUALITY_FILTER_ENABLE = 0x1,
    } LOW_QUALITY_FILTER;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.LodAlgorithm = LOD_ALGORITHM_LEGACY;
        TheStructure.Common.MinModeFilter = MIN_MODE_FILTER_NEAREST;
        TheStructure.Common.MagModeFilter = MAG_MODE_FILTER_NEAREST;
        TheStructure.Common.MipModeFilter = MIP_MODE_FILTER_NONE;
        TheStructure.Common.LowQualityCubeCornerModeEnable = LOW_QUALITY_CUBE_CORNER_MODE_ENABLE_DISABLE;
        TheStructure.Common.LodPreclampMode = LOD_PRECLAMP_MODE_NONE;
        TheStructure.Common.CubeSurfaceControlMode = CUBE_SURFACE_CONTROL_MODE_PROGRAMMED;
        TheStructure.Common.ShadowFunction = SHADOW_FUNCTION_PREFILTEROP_ALWAYS;
        TheStructure.Common.ChromakeyMode = CHROMAKEY_MODE_KEYFILTER_KILL_ON_ANY_MATCH;
        TheStructure.Common.LodClampMagnificationMode = LOD_CLAMP_MAGNIFICATION_MODE_MIPNONE;
        TheStructure.Common.SrgbDecode = SRGB_DECODE_DECODE_EXT;
        TheStructure.Common.ReturnFilterWeightForNullTexels = RETURN_FILTER_WEIGHT_FOR_NULL_TEXELS_DISABLE;
        TheStructure.Common.ReturnFilterWeightForBorderTexels = RETURN_FILTER_WEIGHT_FOR_BORDER_TEXELS_DISABLE;
        TheStructure.Common.TczAddressControlMode = TEXTURE_COORDINATE_MODE_WRAP;
        TheStructure.Common.TcyAddressControlMode = TEXTURE_COORDINATE_MODE_WRAP;
        TheStructure.Common.TcxAddressControlMode = TEXTURE_COORDINATE_MODE_WRAP;
        TheStructure.Common.MipLinearFilterQuality = MIP_LINEAR_FILTER_QUALITY_FULL_QUALITY;
        TheStructure.Common.MaximumAnisotropy = MAXIMUM_ANISOTROPY_RATIO_21;
        TheStructure.Common.ReductionType = REDUCTION_TYPE_STD_FILTER;
    }
    static tagSAMPLER_STATE sInit() {
        SAMPLER_STATE state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 4);
        return TheStructure.RawData[index];
    }
    inline void setLodAlgorithm(const LOD_ALGORITHM value) {
        TheStructure.Common.LodAlgorithm = value;
    }
    inline LOD_ALGORITHM getLodAlgorithm() const {
        return static_cast<LOD_ALGORITHM>(TheStructure.Common.LodAlgorithm);
    }
    inline void setTextureLodBias(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x1fff);
        TheStructure.Common.TextureLodBias = value;
    }
    inline uint32_t getTextureLodBias() const {
        return TheStructure.Common.TextureLodBias;
    }
    inline void setMinModeFilter(const MIN_MODE_FILTER value) {
        TheStructure.Common.MinModeFilter = value;
    }
    inline MIN_MODE_FILTER getMinModeFilter() const {
        return static_cast<MIN_MODE_FILTER>(TheStructure.Common.MinModeFilter);
    }
    inline void setMagModeFilter(const MAG_MODE_FILTER value) {
        TheStructure.Common.MagModeFilter = value;
    }
    inline MAG_MODE_FILTER getMagModeFilter() const {
        return static_cast<MAG_MODE_FILTER>(TheStructure.Common.MagModeFilter);
    }
    inline void setMipModeFilter(const MIP_MODE_FILTER value) {
        TheStructure.Common.MipModeFilter = value;
    }
    inline MIP_MODE_FILTER getMipModeFilter() const {
        return static_cast<MIP_MODE_FILTER>(TheStructure.Common.MipModeFilter);
    }
    inline void setLowQualityCubeCornerModeEnable(const LOW_QUALITY_CUBE_CORNER_MODE_ENABLE value) {
        TheStructure.Common.LowQualityCubeCornerModeEnable = value;
    }
    inline LOW_QUALITY_CUBE_CORNER_MODE_ENABLE getLowQualityCubeCornerModeEnable() const {
        return static_cast<LOW_QUALITY_CUBE_CORNER_MODE_ENABLE>(TheStructure.Common.LowQualityCubeCornerModeEnable);
    }
    inline void setLodPreclampMode(const LOD_PRECLAMP_MODE value) {
        TheStructure.Common.LodPreclampMode = value;
    }
    inline LOD_PRECLAMP_MODE getLodPreclampMode() const {
        return static_cast<LOD_PRECLAMP_MODE>(TheStructure.Common.LodPreclampMode);
    }
    inline void setCpsLodCompensationEnable(const bool value) {
        TheStructure.Common.CpsLodCompensationEnable = value;
    }
    inline bool getCpsLodCompensationEnable() const {
        return TheStructure.Common.CpsLodCompensationEnable;
    }
    inline void setSamplerDisable(const bool value) {
        TheStructure.Common.SamplerDisable = value;
    }
    inline bool getSamplerDisable() const {
        return TheStructure.Common.SamplerDisable;
    }
    inline void setCubeSurfaceControlMode(const CUBE_SURFACE_CONTROL_MODE value) {
        TheStructure.Common.CubeSurfaceControlMode = value;
    }
    inline CUBE_SURFACE_CONTROL_MODE getCubeSurfaceControlMode() const {
        return static_cast<CUBE_SURFACE_CONTROL_MODE>(TheStructure.Common.CubeSurfaceControlMode);
    }
    inline void setShadowFunction(const SHADOW_FUNCTION value) {
        TheStructure.Common.ShadowFunction = value;
    }
    inline SHADOW_FUNCTION getShadowFunction() const {
        return static_cast<SHADOW_FUNCTION>(TheStructure.Common.ShadowFunction);
    }
    inline void setChromakeyMode(const CHROMAKEY_MODE value) {
        TheStructure.Common.ChromakeyMode = value;
    }
    inline CHROMAKEY_MODE getChromakeyMode() const {
        return static_cast<CHROMAKEY_MODE>(TheStructure.Common.ChromakeyMode);
    }
    inline void setChromakeyIndex(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3);
        TheStructure.Common.ChromakeyIndex = value;
    }
    inline uint32_t getChromakeyIndex() const {
        return TheStructure.Common.ChromakeyIndex;
    }
    inline void setChromakeyEnable(const bool value) {
        TheStructure.Common.ChromakeyEnable = value;
    }
    inline bool getChromakeyEnable() const {
        return TheStructure.Common.ChromakeyEnable;
    }
    inline void setMaxLod(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xfff);
        TheStructure.Common.MaxLod = value;
    }
    inline uint32_t getMaxLod() const {
        return TheStructure.Common.MaxLod;
    }
    inline void setMinLod(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xfff);
        TheStructure.Common.MinLod = value;
    }
    inline uint32_t getMinLod() const {
        return TheStructure.Common.MinLod;
    }
    inline void setLodClampMagnificationMode(const LOD_CLAMP_MAGNIFICATION_MODE value) {
        TheStructure.Common.LodClampMagnificationMode = value;
    }
    inline LOD_CLAMP_MAGNIFICATION_MODE getLodClampMagnificationMode() const {
        return static_cast<LOD_CLAMP_MAGNIFICATION_MODE>(TheStructure.Common.LodClampMagnificationMode);
    }
    inline void setSrgbDecode(const SRGB_DECODE value) {
        TheStructure.Common.SrgbDecode = value;
    }
    inline SRGB_DECODE getSrgbDecode() const {
        return static_cast<SRGB_DECODE>(TheStructure.Common.SrgbDecode);
    }
    inline void setReturnFilterWeightForNullTexels(const RETURN_FILTER_WEIGHT_FOR_NULL_TEXELS value) {
        TheStructure.Common.ReturnFilterWeightForNullTexels = value;
    }
    inline RETURN_FILTER_WEIGHT_FOR_NULL_TEXELS getReturnFilterWeightForNullTexels() const {
        return static_cast<RETURN_FILTER_WEIGHT_FOR_NULL_TEXELS>(TheStructure.Common.ReturnFilterWeightForNullTexels);
    }
    inline void setReturnFilterWeightForBorderTexels(const RETURN_FILTER_WEIGHT_FOR_BORDER_TEXELS value) {
        TheStructure.Common.ReturnFilterWeightForBorderTexels = value;
    }
    inline RETURN_FILTER_WEIGHT_FOR_BORDER_TEXELS getReturnFilterWeightForBorderTexels() const {
        return static_cast<RETURN_FILTER_WEIGHT_FOR_BORDER_TEXELS>(TheStructure.Common.ReturnFilterWeightForBorderTexels);
    }
    inline void setForceGather4Behavior(const bool value) {
        TheStructure.Common.ForceGather4Behavior = value;
    }
    inline bool getForceGather4Behavior() const {
        return TheStructure.Common.ForceGather4Behavior;
    }
    typedef enum tagINDIRECTSTATEPOINTER {
        INDIRECTSTATEPOINTER_BIT_SHIFT = 0x6,
        INDIRECTSTATEPOINTER_ALIGN_SIZE = 0x40,
    } INDIRECTSTATEPOINTER;
    inline void setIndirectStatePointer(const uint32_t value) {
        UNRECOVERABLE_IF(value & 0x3f);
        TheStructure.Common.IndirectStatePointer = static_cast<uint32_t>(value & 0xffffc0) >> INDIRECTSTATEPOINTER_BIT_SHIFT;
        setExtendedIndirectStatePointer(value);
    }
    inline void setExtendedIndirectStatePointer(const uint32_t value) {
        TheStructure.Common.ExtendedIndirectStatePointer = static_cast<uint32_t>(value & 0xff000000) >> 24;
    }
    inline uint64_t getIndirectStatePointer() const {
        return (TheStructure.Common.ExtendedIndirectStatePointer << 24) | (TheStructure.Common.IndirectStatePointer << INDIRECTSTATEPOINTER_BIT_SHIFT);
    }
    inline uint32_t getExtendedIndirectStatePointer() const {
        return TheStructure.Common.ExtendedIndirectStatePointer;
    }
    inline void setTczAddressControlMode(const TEXTURE_COORDINATE_MODE value) {
        TheStructure.Common.TczAddressControlMode = value;
    }
    inline TEXTURE_COORDINATE_MODE getTczAddressControlMode() const {
        return static_cast<TEXTURE_COORDINATE_MODE>(TheStructure.Common.TczAddressControlMode);
    }
    inline void setTcyAddressControlMode(const TEXTURE_COORDINATE_MODE value) {
        TheStructure.Common.TcyAddressControlMode = value;
    }
    inline TEXTURE_COORDINATE_MODE getTcyAddressControlMode() const {
        return static_cast<TEXTURE_COORDINATE_MODE>(TheStructure.Common.TcyAddressControlMode);
    }
    inline void setTcxAddressControlMode(const TEXTURE_COORDINATE_MODE value) {
        TheStructure.Common.TcxAddressControlMode = value;
    }
    inline TEXTURE_COORDINATE_MODE getTcxAddressControlMode() const {
        return static_cast<TEXTURE_COORDINATE_MODE>(TheStructure.Common.TcxAddressControlMode);
    }
    inline void setReductionTypeEnable(const bool value) {
        TheStructure.Common.ReductionTypeEnable = value;
    }
    inline bool getReductionTypeEnable() const {
        return TheStructure.Common.ReductionTypeEnable;
    }
    inline void setNonNormalizedCoordinateEnable(const bool value) {
        TheStructure.Common.NonNormalizedCoordinateEnable = value;
    }
    inline bool getNonNormalizedCoordinateEnable() const {
        return TheStructure.Common.NonNormalizedCoordinateEnable;
    }
    inline void setMipLinearFilterQuality(const MIP_LINEAR_FILTER_QUALITY value) {
        TheStructure.Common.MipLinearFilterQuality = value;
    }
    inline MIP_LINEAR_FILTER_QUALITY getMipLinearFilterQuality() const {
        return static_cast<MIP_LINEAR_FILTER_QUALITY>(TheStructure.Common.MipLinearFilterQuality);
    }
    inline void setRAddressMinFilterRoundingEnable(const bool value) {
        TheStructure.Common.RAddressMinFilterRoundingEnable = value;
    }
    inline bool getRAddressMinFilterRoundingEnable() const {
        return TheStructure.Common.RAddressMinFilterRoundingEnable;
    }
    inline void setRAddressMagFilterRoundingEnable(const bool value) {
        TheStructure.Common.RAddressMagFilterRoundingEnable = value;
    }
    inline bool getRAddressMagFilterRoundingEnable() const {
        return TheStructure.Common.RAddressMagFilterRoundingEnable;
    }
    inline void setVAddressMinFilterRoundingEnable(const bool value) {
        TheStructure.Common.VAddressMinFilterRoundingEnable = value;
    }
    inline bool getVAddressMinFilterRoundingEnable() const {
        return TheStructure.Common.VAddressMinFilterRoundingEnable;
    }
    inline void setVAddressMagFilterRoundingEnable(const bool value) {
        TheStructure.Common.VAddressMagFilterRoundingEnable = value;
    }
    inline bool getVAddressMagFilterRoundingEnable() const {
        return TheStructure.Common.VAddressMagFilterRoundingEnable;
    }
    inline void setUAddressMinFilterRoundingEnable(const bool value) {
        TheStructure.Common.UAddressMinFilterRoundingEnable = value;
    }
    inline bool getUAddressMinFilterRoundingEnable() const {
        return TheStructure.Common.UAddressMinFilterRoundingEnable;
    }
    inline void setUAddressMagFilterRoundingEnable(const bool value) {
        TheStructure.Common.UAddressMagFilterRoundingEnable = value;
    }
    inline bool getUAddressMagFilterRoundingEnable() const {
        return TheStructure.Common.UAddressMagFilterRoundingEnable;
    }
    inline void setMaximumAnisotropy(const MAXIMUM_ANISOTROPY value) {
        TheStructure.Common.MaximumAnisotropy = value;
    }
    inline MAXIMUM_ANISOTROPY getMaximumAnisotropy() const {
        return static_cast<MAXIMUM_ANISOTROPY>(TheStructure.Common.MaximumAnisotropy);
    }
    inline void setReductionType(const REDUCTION_TYPE value) {
        TheStructure.Common.ReductionType = value;
    }
    inline REDUCTION_TYPE getReductionType() const {
        return static_cast<REDUCTION_TYPE>(TheStructure.Common.ReductionType);
    }
    inline void setAllowLowQualityLodCalculation(const bool value) {
        TheStructure.Common.AllowLowQualityLodCalculation = value;
    }
    inline bool getAllowLowQualityLodCalculation() const {
        return TheStructure.Common.AllowLowQualityLodCalculation;
    }
    inline void setLowQualityFilter(const bool value) {
        TheStructure.Common.LowQualityFilter = value;
    }
    inline bool getLowQualityFilter() const {
        return TheStructure.Common.LowQualityFilter;
    }
} SAMPLER_STATE;
STATIC_ASSERT(16 == sizeof(SAMPLER_STATE));

typedef struct tagSTATE_BASE_ADDRESS {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 15);
            uint32_t _3DCommandSubOpcode : BITFIELD_RANGE(16, 23);
            uint32_t _3DCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t CommandSubtype : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint64_t GeneralStateBaseAddressModifyEnable : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_33 : BITFIELD_RANGE(1, 3);
            uint64_t GeneralStateMemoryObjectControlStateEncryptedData : BITFIELD_RANGE(4, 4);
            uint64_t GeneralStateMemoryObjectControlStateIndexToMocsTables : BITFIELD_RANGE(5, 10);
            uint64_t Reserved_43 : BITFIELD_RANGE(11, 11);
            uint64_t GeneralStateBaseAddress : BITFIELD_RANGE(12, 63);
            // DWORD 3
            uint32_t Reserved_96 : BITFIELD_RANGE(0, 15);
            uint32_t StatelessDataPortAccessMemoryObjectControlStateEncryptedData : BITFIELD_RANGE(16, 16);
            uint32_t StatelessDataPortAccessMemoryObjectControlStateIndexToMocsTables : BITFIELD_RANGE(17, 22);
            uint32_t L1CacheControlCachePolicy : BITFIELD_RANGE(23, 25);
            uint32_t Reserved_122 : BITFIELD_RANGE(26, 31);
            // DWORD 4
            uint64_t SurfaceStateBaseAddressModifyEnable : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_129 : BITFIELD_RANGE(1, 3);
            uint64_t SurfaceStateMemoryObjectControlStateEncryptedData : BITFIELD_RANGE(4, 4);
            uint64_t SurfaceStateMemoryObjectControlStateIndexToMocsTables : BITFIELD_RANGE(5, 10);
            uint64_t Reserved_139 : BITFIELD_RANGE(11, 11);
            uint64_t SurfaceStateBaseAddress : BITFIELD_RANGE(12, 63);
            // DWORD 6
            uint64_t DynamicStateBaseAddressModifyEnable : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_193 : BITFIELD_RANGE(1, 3);
            uint64_t DynamicStateMemoryObjectControlStateEncryptedData : BITFIELD_RANGE(4, 4);
            uint64_t DynamicStateMemoryObjectControlStateIndexToMocsTables : BITFIELD_RANGE(5, 10);
            uint64_t Reserved_203 : BITFIELD_RANGE(11, 11);
            uint64_t DynamicStateBaseAddress : BITFIELD_RANGE(12, 63);
            // DWORD 8, 9
            uint64_t Reserved_256;
            // DWORD 10
            uint64_t InstructionBaseAddressModifyEnable : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_321 : BITFIELD_RANGE(1, 3);
            uint64_t InstructionMemoryObjectControlStateEncryptedData : BITFIELD_RANGE(4, 4);
            uint64_t InstructionMemoryObjectControlStateIndexToMocsTables : BITFIELD_RANGE(5, 10);
            uint64_t Reserved_331 : BITFIELD_RANGE(11, 11);
            uint64_t InstructionBaseAddress : BITFIELD_RANGE(12, 63);
            // DWORD 12
            uint32_t GeneralStateBufferSizeModifyEnable : BITFIELD_RANGE(0, 0);
            uint32_t Reserved_385 : BITFIELD_RANGE(1, 11);
            uint32_t GeneralStateBufferSize : BITFIELD_RANGE(12, 31);
            // DWORD 13
            uint32_t DynamicStateBufferSizeModifyEnable : BITFIELD_RANGE(0, 0);
            uint32_t Reserved_417 : BITFIELD_RANGE(1, 11);
            uint32_t DynamicStateBufferSize : BITFIELD_RANGE(12, 31);
            // DWORD 14
            uint32_t Reserved_448;
            // DWORD 15
            uint32_t InstructionBufferSizeModifyEnable : BITFIELD_RANGE(0, 0);
            uint32_t Reserved_481 : BITFIELD_RANGE(1, 11);
            uint32_t InstructionBufferSize : BITFIELD_RANGE(12, 31);
            // DWORD 16
            uint64_t BindlessSurfaceStateBaseAddressModifyEnable : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_513 : BITFIELD_RANGE(1, 3);
            uint64_t BindlessSurfaceStateMemoryObjectControlStateEncryptedData : BITFIELD_RANGE(4, 4);
            uint64_t BindlessSurfaceStateMemoryObjectControlStateIndexToMocsTables : BITFIELD_RANGE(5, 10);
            uint64_t Reserved_523 : BITFIELD_RANGE(11, 11);
            uint64_t BindlessSurfaceStateBaseAddress : BITFIELD_RANGE(12, 63);
            // DWORD 18
            uint32_t BindlessSurfaceStateSize;
            // DWORD 19
            uint64_t BindlessSamplerStateBaseAddressModifyEnable : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_609 : BITFIELD_RANGE(1, 3);
            uint64_t BindlessSamplerStateMemoryObjectControlStateEncryptedData : BITFIELD_RANGE(4, 4);
            uint64_t BindlessSamplerStateMemoryObjectControlStateIndexToMocsTables : BITFIELD_RANGE(5, 10);
            uint64_t Reserved_619 : BITFIELD_RANGE(11, 11);
            uint64_t BindlessSamplerStateBaseAddress : BITFIELD_RANGE(12, 63);
            // DWORD 21
            uint32_t Reserved_672 : BITFIELD_RANGE(0, 11);
            uint32_t BindlessSamplerStateBufferSize : BITFIELD_RANGE(12, 31);
        } Common;
        uint32_t RawData[22];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_DWORD_COUNT_N = 0x14,
    } DWORD_LENGTH;
    typedef enum tag_3D_COMMAND_SUB_OPCODE {
        _3D_COMMAND_SUB_OPCODE_STATE_BASE_ADDRESS = 0x1,
    } _3D_COMMAND_SUB_OPCODE;
    typedef enum tag_3D_COMMAND_OPCODE {
        _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED = 0x1,
    } _3D_COMMAND_OPCODE;
    typedef enum tagCOMMAND_SUBTYPE {
        COMMAND_SUBTYPE_GFXPIPE_COMMON = 0x0,
    } COMMAND_SUBTYPE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_GFXPIPE = 0x3,
    } COMMAND_TYPE;
    typedef enum tagL1_CACHE_CONTROL {
        L1_CACHE_CONTROL_WBP = 0x0,
        L1_CACHE_CONTROL_UC = 0x1,
        L1_CACHE_CONTROL_WB = 0x2,
        L1_CACHE_CONTROL_WT = 0x3,
        L1_CACHE_CONTROL_WS = 0x4,
    } L1_CACHE_CONTROL;
    typedef enum tagPATCH_CONSTANTS { // patched
        GENERALSTATEBASEADDRESS_BYTEOFFSET = 0x4,
        GENERALSTATEBASEADDRESS_INDEX = 0x1,
        SURFACESTATEBASEADDRESS_BYTEOFFSET = 0x10,
        SURFACESTATEBASEADDRESS_INDEX = 0x4,
        DYNAMICSTATEBASEADDRESS_BYTEOFFSET = 0x18,
        DYNAMICSTATEBASEADDRESS_INDEX = 0x6,
        INSTRUCTIONBASEADDRESS_BYTEOFFSET = 0x28,
        INSTRUCTIONBASEADDRESS_INDEX = 0xa,
        BINDLESSSURFACESTATEBASEADDRESS_BYTEOFFSET = 0x40,
        BINDLESSSURFACESTATEBASEADDRESS_INDEX = 0x10,
        BINDLESSSAMPLERSTATEBASEADDRESS_BYTEOFFSET = 0x4c,
        BINDLESSSAMPLERSTATEBASEADDRESS_INDEX = 0x13,
    } PATCH_CONSTANTS;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_DWORD_COUNT_N;
        TheStructure.Common._3DCommandSubOpcode = _3D_COMMAND_SUB_OPCODE_STATE_BASE_ADDRESS;
        TheStructure.Common._3DCommandOpcode = _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED;
        TheStructure.Common.CommandSubtype = COMMAND_SUBTYPE_GFXPIPE_COMMON;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
        TheStructure.Common.L1CacheControlCachePolicy = L1_CACHE_CONTROL_WBP;
    }
    static tagSTATE_BASE_ADDRESS sInit() {
        STATE_BASE_ADDRESS state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 22);
        return TheStructure.RawData[index];
    }
    inline void setGeneralStateBaseAddressModifyEnable(const bool value) {
        TheStructure.Common.GeneralStateBaseAddressModifyEnable = value;
    }
    inline bool getGeneralStateBaseAddressModifyEnable() const {
        return TheStructure.Common.GeneralStateBaseAddressModifyEnable;
    }
    inline void setGeneralStateMemoryObjectControlStateEncryptedData(const bool value) {
        TheStructure.Common.GeneralStateMemoryObjectControlStateEncryptedData = value;
    }
    inline bool getGeneralStateMemoryObjectControlStateEncryptedData() const {
        return TheStructure.Common.GeneralStateMemoryObjectControlStateEncryptedData;
    }
    inline void setGeneralStateMemoryObjectControlState(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0x7fL);
        setGeneralStateMemoryObjectControlStateEncryptedData(value & 1);
        TheStructure.Common.GeneralStateMemoryObjectControlStateIndexToMocsTables = value >> 1;
    }
    inline uint64_t getGeneralStateMemoryObjectControlState() const {
        uint64_t mocs = getGeneralStateMemoryObjectControlStateEncryptedData();
        return mocs | (TheStructure.Common.GeneralStateMemoryObjectControlStateIndexToMocsTables << 1);
    }
    typedef enum tagGENERALSTATEBASEADDRESS {
        GENERALSTATEBASEADDRESS_BIT_SHIFT = 0xc,
        GENERALSTATEBASEADDRESS_ALIGN_SIZE = 0x1000,
    } GENERALSTATEBASEADDRESS;
    inline void setGeneralStateBaseAddress(const uint64_t value) {
        UNRECOVERABLE_IF((value >> GENERALSTATEBASEADDRESS_BIT_SHIFT) > 0xffffffffffffffffL);
        TheStructure.Common.GeneralStateBaseAddress = value >> GENERALSTATEBASEADDRESS_BIT_SHIFT;
    }
    inline uint64_t getGeneralStateBaseAddress() const {
        return TheStructure.Common.GeneralStateBaseAddress << GENERALSTATEBASEADDRESS_BIT_SHIFT;
    }
    inline void setStatelessDataPortAccessMemoryObjectControlStateEncryptedData(const bool value) {
        TheStructure.Common.StatelessDataPortAccessMemoryObjectControlStateEncryptedData = value;
    }
    inline bool getStatelessDataPortAccessMemoryObjectControlStateEncryptedData() const {
        return TheStructure.Common.StatelessDataPortAccessMemoryObjectControlStateEncryptedData;
    }
    inline void setStatelessDataPortAccessMemoryObjectControlState(const uint32_t value) { // patched
        setStatelessDataPortAccessMemoryObjectControlStateEncryptedData(value & 1);
        TheStructure.Common.StatelessDataPortAccessMemoryObjectControlStateIndexToMocsTables = value >> 1;
    }
    inline uint32_t getStatelessDataPortAccessMemoryObjectControlState() const { // patched
        uint32_t mocs = getStatelessDataPortAccessMemoryObjectControlStateEncryptedData();
        return (TheStructure.Common.StatelessDataPortAccessMemoryObjectControlStateIndexToMocsTables << 1) | mocs;
    }
    inline void setL1CacheControlCachePolicy(const L1_CACHE_CONTROL value) {
        TheStructure.Common.L1CacheControlCachePolicy = value;
    }
    inline L1_CACHE_CONTROL getL1CacheControlCachePolicy() const {
        return static_cast<L1_CACHE_CONTROL>(TheStructure.Common.L1CacheControlCachePolicy);
    }
    inline void setSurfaceStateBaseAddressModifyEnable(const bool value) {
        TheStructure.Common.SurfaceStateBaseAddressModifyEnable = value;
    }
    inline bool getSurfaceStateBaseAddressModifyEnable() const {
        return TheStructure.Common.SurfaceStateBaseAddressModifyEnable;
    }
    inline void setSurfaceStateMemoryObjectControlStateEncryptedData(const bool value) {
        TheStructure.Common.SurfaceStateMemoryObjectControlStateEncryptedData = value;
    }
    inline bool getSurfaceStateMemoryObjectControlStateEncryptedData() const {
        return TheStructure.Common.SurfaceStateMemoryObjectControlStateEncryptedData;
    }
    inline void setSurfaceStateMemoryObjectControlState(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0x7fL);
        setSurfaceStateMemoryObjectControlStateEncryptedData(value & 1);
        TheStructure.Common.SurfaceStateMemoryObjectControlStateIndexToMocsTables = value >> 1;
    }
    inline uint64_t getSurfaceStateMemoryObjectControlState() const {
        uint64_t mocs = getSurfaceStateMemoryObjectControlStateEncryptedData();
        return mocs | (TheStructure.Common.SurfaceStateMemoryObjectControlStateIndexToMocsTables << 1);
    }
    typedef enum tagSURFACESTATEBASEADDRESS {
        SURFACESTATEBASEADDRESS_BIT_SHIFT = 0xc,
        SURFACESTATEBASEADDRESS_ALIGN_SIZE = 0x1000,
    } SURFACESTATEBASEADDRESS;
    inline void setSurfaceStateBaseAddress(const uint64_t value) {
        UNRECOVERABLE_IF((value >> SURFACESTATEBASEADDRESS_BIT_SHIFT) > 0xffffffffffffffffL);
        TheStructure.Common.SurfaceStateBaseAddress = value >> SURFACESTATEBASEADDRESS_BIT_SHIFT;
    }
    inline uint64_t getSurfaceStateBaseAddress() const {
        return TheStructure.Common.SurfaceStateBaseAddress << SURFACESTATEBASEADDRESS_BIT_SHIFT;
    }
    inline void setDynamicStateBaseAddressModifyEnable(const bool value) {
        TheStructure.Common.DynamicStateBaseAddressModifyEnable = value;
    }
    inline bool getDynamicStateBaseAddressModifyEnable() const {
        return TheStructure.Common.DynamicStateBaseAddressModifyEnable;
    }
    inline void setDynamicStateMemoryObjectControlStateEncryptedData(const bool value) {
        TheStructure.Common.DynamicStateMemoryObjectControlStateEncryptedData = value;
    }
    inline bool getDynamicStateMemoryObjectControlStateEncryptedData() const {
        return TheStructure.Common.DynamicStateMemoryObjectControlStateEncryptedData;
    }
    inline void setDynamicStateMemoryObjectControlState(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0x7fL);
        setDynamicStateMemoryObjectControlStateEncryptedData(value & 1);
        TheStructure.Common.DynamicStateMemoryObjectControlStateIndexToMocsTables = value >> 1;
    }
    inline uint64_t getDynamicStateMemoryObjectControlState() const {
        uint64_t mocs = getDynamicStateMemoryObjectControlStateEncryptedData();
        return mocs | TheStructure.Common.DynamicStateMemoryObjectControlStateIndexToMocsTables << 1;
    }
    typedef enum tagDYNAMICSTATEBASEADDRESS {
        DYNAMICSTATEBASEADDRESS_BIT_SHIFT = 0xc,
        DYNAMICSTATEBASEADDRESS_ALIGN_SIZE = 0x1000,
    } DYNAMICSTATEBASEADDRESS;
    inline void setDynamicStateBaseAddress(const uint64_t value) {
        UNRECOVERABLE_IF((value >> DYNAMICSTATEBASEADDRESS_BIT_SHIFT) > 0xffffffffffffffffL);
        TheStructure.Common.DynamicStateBaseAddress = value >> DYNAMICSTATEBASEADDRESS_BIT_SHIFT;
    }
    inline uint64_t getDynamicStateBaseAddress() const {
        return TheStructure.Common.DynamicStateBaseAddress << DYNAMICSTATEBASEADDRESS_BIT_SHIFT;
    }
    inline void setInstructionBaseAddressModifyEnable(const bool value) {
        TheStructure.Common.InstructionBaseAddressModifyEnable = value;
    }
    inline bool getInstructionBaseAddressModifyEnable() const {
        return TheStructure.Common.InstructionBaseAddressModifyEnable;
    }
    inline void setInstructionMemoryObjectControlStateEncryptedData(const bool value) {
        TheStructure.Common.InstructionMemoryObjectControlStateEncryptedData = value;
    }
    inline bool getInstructionMemoryObjectControlStateEncryptedData() const {
        return TheStructure.Common.InstructionMemoryObjectControlStateEncryptedData;
    }
    inline void setInstructionMemoryObjectControlState(const uint64_t value) { // patched
        setInstructionMemoryObjectControlStateEncryptedData(value & 1);
        TheStructure.Common.InstructionMemoryObjectControlStateIndexToMocsTables = value >> 1;
    }
    inline uint64_t getInstructionMemoryObjectControlState() const { // patched
        uint64_t mocs = getInstructionMemoryObjectControlStateEncryptedData();
        return (TheStructure.Common.InstructionMemoryObjectControlStateIndexToMocsTables << 1) | mocs;
    }
    typedef enum tagINSTRUCTIONBASEADDRESS {
        INSTRUCTIONBASEADDRESS_BIT_SHIFT = 0xc,
        INSTRUCTIONBASEADDRESS_ALIGN_SIZE = 0x1000,
    } INSTRUCTIONBASEADDRESS;
    inline void setInstructionBaseAddress(const uint64_t value) {
        UNRECOVERABLE_IF((value >> INSTRUCTIONBASEADDRESS_BIT_SHIFT) > 0xffffffffffffffffL);
        TheStructure.Common.InstructionBaseAddress = value >> INSTRUCTIONBASEADDRESS_BIT_SHIFT;
    }
    inline uint64_t getInstructionBaseAddress() const {
        return TheStructure.Common.InstructionBaseAddress << INSTRUCTIONBASEADDRESS_BIT_SHIFT;
    }
    inline void setGeneralStateBufferSizeModifyEnable(const bool value) {
        TheStructure.Common.GeneralStateBufferSizeModifyEnable = value;
    }
    inline bool getGeneralStateBufferSizeModifyEnable() const {
        return TheStructure.Common.GeneralStateBufferSizeModifyEnable;
    }
    inline void setGeneralStateBufferSize(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xfffff);
        TheStructure.Common.GeneralStateBufferSize = value;
    }
    inline uint32_t getGeneralStateBufferSize() const {
        return TheStructure.Common.GeneralStateBufferSize;
    }
    inline void setDynamicStateBufferSizeModifyEnable(const bool value) {
        TheStructure.Common.DynamicStateBufferSizeModifyEnable = value;
    }
    inline bool getDynamicStateBufferSizeModifyEnable() const {
        return TheStructure.Common.DynamicStateBufferSizeModifyEnable;
    }
    inline void setDynamicStateBufferSize(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xfffff);
        TheStructure.Common.DynamicStateBufferSize = value;
    }
    inline uint32_t getDynamicStateBufferSize() const {
        return TheStructure.Common.DynamicStateBufferSize;
    }
    inline void setInstructionBufferSizeModifyEnable(const bool value) {
        TheStructure.Common.InstructionBufferSizeModifyEnable = value;
    }
    inline bool getInstructionBufferSizeModifyEnable() const {
        return TheStructure.Common.InstructionBufferSizeModifyEnable;
    }
    inline void setInstructionBufferSize(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xfffff);
        TheStructure.Common.InstructionBufferSize = value;
    }
    inline uint32_t getInstructionBufferSize() const {
        return TheStructure.Common.InstructionBufferSize;
    }
    inline void setBindlessSurfaceStateBaseAddressModifyEnable(const bool value) {
        TheStructure.Common.BindlessSurfaceStateBaseAddressModifyEnable = value;
    }
    inline bool getBindlessSurfaceStateBaseAddressModifyEnable() const {
        return TheStructure.Common.BindlessSurfaceStateBaseAddressModifyEnable;
    }
    inline void setBindlessSurfaceStateMemoryObjectControlStateEncryptedData(const bool value) {
        TheStructure.Common.BindlessSurfaceStateMemoryObjectControlStateEncryptedData = value;
    }
    inline bool getBindlessSurfaceStateMemoryObjectControlStateEncryptedData() const {
        return TheStructure.Common.BindlessSurfaceStateMemoryObjectControlStateEncryptedData;
    }
    inline void setBindlessSurfaceStateMemoryObjectControlState(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0x7fL);
        setBindlessSurfaceStateMemoryObjectControlStateEncryptedData(value & 1);
        TheStructure.Common.BindlessSurfaceStateMemoryObjectControlStateIndexToMocsTables = value >> 1;
    }
    inline uint64_t getBindlessSurfaceStateMemoryObjectControlState() const {
        uint64_t mocs = getBindlessSurfaceStateMemoryObjectControlStateEncryptedData();
        return mocs | (TheStructure.Common.BindlessSurfaceStateMemoryObjectControlStateIndexToMocsTables << 1);
    }
    typedef enum tagBINDLESSSURFACESTATEBASEADDRESS {
        BINDLESSSURFACESTATEBASEADDRESS_BIT_SHIFT = 0xc,
        BINDLESSSURFACESTATEBASEADDRESS_ALIGN_SIZE = 0x1000,
    } BINDLESSSURFACESTATEBASEADDRESS;
    inline void setBindlessSurfaceStateBaseAddress(const uint64_t value) {
        UNRECOVERABLE_IF((value >> BINDLESSSURFACESTATEBASEADDRESS_BIT_SHIFT) > 0xffffffffffffffffL);
        TheStructure.Common.BindlessSurfaceStateBaseAddress = value >> BINDLESSSURFACESTATEBASEADDRESS_BIT_SHIFT;
    }
    inline uint64_t getBindlessSurfaceStateBaseAddress() const {
        return TheStructure.Common.BindlessSurfaceStateBaseAddress << BINDLESSSURFACESTATEBASEADDRESS_BIT_SHIFT;
    }
    typedef enum tagBINDLESSSURFACESTATESIZE {
        BINDLESSSURFACESTATESIZE_BIT_SHIFT = 0x6,
        BINDLESSSURFACESTATESIZE_ALIGN_SIZE = 0x40,
    } BINDLESSSURFACESTATESIZE;
    inline void setBindlessSurfaceStateSize(const uint32_t value) {
        TheStructure.Common.BindlessSurfaceStateSize = value - 1; // patched based on description- incorrect format in bspec
    }
    inline uint32_t getBindlessSurfaceStateSize() const {
        return TheStructure.Common.BindlessSurfaceStateSize + 1;
    }
    inline void setBindlessSamplerStateBaseAddressModifyEnable(const bool value) {
        TheStructure.Common.BindlessSamplerStateBaseAddressModifyEnable = value;
    }
    inline bool getBindlessSamplerStateBaseAddressModifyEnable() const {
        return TheStructure.Common.BindlessSamplerStateBaseAddressModifyEnable;
    }
    inline void setBindlessSamplerStateMemoryObjectControlStateEncryptedData(const bool value) {
        TheStructure.Common.BindlessSamplerStateMemoryObjectControlStateEncryptedData = value;
    }
    inline bool getBindlessSamplerStateMemoryObjectControlStateEncryptedData() const {
        return TheStructure.Common.BindlessSamplerStateMemoryObjectControlStateEncryptedData;
    }
    inline void setBindlessSamplerStateMemoryObjectControlState(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0x7fL);
        setBindlessSamplerStateMemoryObjectControlStateEncryptedData(value & 1);
        TheStructure.Common.BindlessSamplerStateMemoryObjectControlStateIndexToMocsTables = value >> 1;
    }
    inline uint64_t getBindlessSamplerStateMemoryObjectControlState() const {
        uint64_t mocs = getBindlessSamplerStateMemoryObjectControlStateEncryptedData();
        return mocs | (TheStructure.Common.BindlessSamplerStateMemoryObjectControlStateIndexToMocsTables << 1);
    }
    typedef enum tagBINDLESSSAMPLERSTATEBASEADDRESS {
        BINDLESSSAMPLERSTATEBASEADDRESS_BIT_SHIFT = 0xc,
        BINDLESSSAMPLERSTATEBASEADDRESS_ALIGN_SIZE = 0x1000,
    } BINDLESSSAMPLERSTATEBASEADDRESS;
    inline void setBindlessSamplerStateBaseAddress(const uint64_t value) {
        UNRECOVERABLE_IF((value >> BINDLESSSAMPLERSTATEBASEADDRESS_BIT_SHIFT) > 0xffffffffffffffffL);
        TheStructure.Common.BindlessSamplerStateBaseAddress = value >> BINDLESSSAMPLERSTATEBASEADDRESS_BIT_SHIFT;
    }
    inline uint64_t getBindlessSamplerStateBaseAddress() const {
        return TheStructure.Common.BindlessSamplerStateBaseAddress << BINDLESSSAMPLERSTATEBASEADDRESS_BIT_SHIFT;
    }
    inline void setBindlessSamplerStateBufferSize(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xfffff);
        TheStructure.Common.BindlessSamplerStateBufferSize = value;
    }
    inline uint32_t getBindlessSamplerStateBufferSize() const {
        return TheStructure.Common.BindlessSamplerStateBufferSize;
    }
} STATE_BASE_ADDRESS;
STATIC_ASSERT(88 == sizeof(STATE_BASE_ADDRESS));

typedef struct tagMI_REPORT_PERF_COUNT {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 5);
            uint32_t Reserved_6 : BITFIELD_RANGE(6, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint64_t UseGlobalGtt : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_33 : BITFIELD_RANGE(1, 3);
            uint64_t CoreModeEnable : BITFIELD_RANGE(4, 4);
            uint64_t Reserved_37 : BITFIELD_RANGE(5, 5);
            uint64_t MemoryAddress : BITFIELD_RANGE(6, 63);
            // DWORD 3
            uint32_t ReportId;
        } Common;
        uint32_t RawData[4];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x2,
    } DWORD_LENGTH;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_REPORT_PERF_COUNT = 0x28,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_REPORT_PERF_COUNT;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_REPORT_PERF_COUNT sInit() {
        MI_REPORT_PERF_COUNT state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 4);
        return TheStructure.RawData[index];
    }
    inline void setUseGlobalGtt(const bool value) {
        TheStructure.Common.UseGlobalGtt = value;
    }
    inline bool getUseGlobalGtt() const {
        return TheStructure.Common.UseGlobalGtt;
    }
    inline void setCoreModeEnable(const bool value) {
        TheStructure.Common.CoreModeEnable = value;
    }
    inline bool getCoreModeEnable() const {
        return TheStructure.Common.CoreModeEnable;
    }
    typedef enum tagMEMORYADDRESS {
        MEMORYADDRESS_BIT_SHIFT = 0x6,
        MEMORYADDRESS_ALIGN_SIZE = 0x40,
    } MEMORYADDRESS;
    inline void setMemoryAddress(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0xffffffffffffffffL);
        TheStructure.Common.MemoryAddress = value >> MEMORYADDRESS_BIT_SHIFT;
    }
    inline uint64_t getMemoryAddress() const {
        return TheStructure.Common.MemoryAddress << MEMORYADDRESS_BIT_SHIFT;
    }
    inline void setReportId(const uint32_t value) {
        TheStructure.Common.ReportId = value;
    }
    inline uint32_t getReportId() const {
        return TheStructure.Common.ReportId;
    }
} MI_REPORT_PERF_COUNT;
STATIC_ASSERT(16 == sizeof(MI_REPORT_PERF_COUNT));

typedef struct tagMI_USER_INTERRUPT {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t Reserved_0 : BITFIELD_RANGE(0, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
        } Common;
        uint32_t RawData[1];
    } TheStructure;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_USER_INTERRUPT = 0x2,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_USER_INTERRUPT;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_USER_INTERRUPT sInit() {
        MI_USER_INTERRUPT state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 1);
        return TheStructure.RawData[index];
    }
} MI_USER_INTERRUPT;
STATIC_ASSERT(4 == sizeof(MI_USER_INTERRUPT));

typedef struct tagMI_SET_PREDICATE {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t PredicateEnable : BITFIELD_RANGE(0, 3);
            uint32_t PredicateEnableWparid : BITFIELD_RANGE(4, 5);
            uint32_t Reserved_6 : BITFIELD_RANGE(6, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
        } Common;
        uint32_t RawData[1];
    } TheStructure;
    typedef enum tagPREDICATE_ENABLE {
        PREDICATE_ENABLE_PREDICATE_DISABLE = 0x0, // patched PREDICATE_ENABLE_NOOP_NEVER
        PREDICATE_ENABLE_NOOP_ON_RESULT2_CLEAR = 0x1,
        PREDICATE_ENABLE_NOOP_ON_RESULT2_SET = 0x2,
        PREDICATE_ENABLE_NOOP_ON_RESULT_CLEAR = 0x3,
        PREDICATE_ENABLE_NOOP_ON_RESULT_SET = 0x4,
        PREDICATE_ENABLE_NOOP_ALWAYS = 0xf,
    } PREDICATE_ENABLE;
    typedef enum tagPREDICATE_ENABLE_WPARID {
        PREDICATE_ENABLE_WPARID_NOOP_NEVER = 0x0,
        PREDICATE_ENABLE_WPARID_NOOP_ON_ZERO_VALUE = 0x1,
        PREDICATE_ENABLE_WPARID_NOOP_ON_NON_ZERO_VALUE = 0x2,
    } PREDICATE_ENABLE_WPARID;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_SET_PREDICATE = 0x1,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.PredicateEnable = PREDICATE_ENABLE_PREDICATE_DISABLE;
        TheStructure.Common.PredicateEnableWparid = PREDICATE_ENABLE_WPARID_NOOP_NEVER;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_SET_PREDICATE;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_SET_PREDICATE sInit() {
        MI_SET_PREDICATE state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 1);
        return TheStructure.RawData[index];
    }
    inline void setPredicateEnable(const PREDICATE_ENABLE value) {
        TheStructure.Common.PredicateEnable = value;
    }
    inline PREDICATE_ENABLE getPredicateEnable() const {
        return static_cast<PREDICATE_ENABLE>(TheStructure.Common.PredicateEnable);
    }
    inline void setPredicateEnableWparid(const PREDICATE_ENABLE_WPARID value) {
        TheStructure.Common.PredicateEnableWparid = value;
    }
    inline PREDICATE_ENABLE_WPARID getPredicateEnableWparid() const {
        return static_cast<PREDICATE_ENABLE_WPARID>(TheStructure.Common.PredicateEnableWparid);
    }
} MI_SET_PREDICATE;
STATIC_ASSERT(4 == sizeof(MI_SET_PREDICATE));

typedef struct tagMI_CONDITIONAL_BATCH_BUFFER_END {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 11);
            uint32_t CompareOperation : BITFIELD_RANGE(12, 14);
            uint32_t PredicateEnable : BITFIELD_RANGE(15, 15);
            uint32_t Reserved_16 : BITFIELD_RANGE(16, 17);
            uint32_t EndCurrentBatchBufferLevel : BITFIELD_RANGE(18, 18);
            uint32_t CompareMaskMode : BITFIELD_RANGE(19, 19);
            uint32_t CompareBufferEncryptedMemoryReadEnable : BITFIELD_RANGE(20, 20);
            uint32_t CompareSemaphore : BITFIELD_RANGE(21, 21);
            uint32_t UseGlobalGtt : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint32_t CompareDataDword;
            // DWORD 2
            uint64_t Reserved_64 : BITFIELD_RANGE(0, 2);
            uint64_t CompareAddress : BITFIELD_RANGE(3, 63);
        } Common;
        uint32_t RawData[4];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x2,
    } DWORD_LENGTH;
    typedef enum tagCOMPARE_OPERATION {
        COMPARE_OPERATION_MAD_GREATER_THAN_IDD = 0x0,
        COMPARE_OPERATION_MAD_GREATER_THAN_OR_EQUAL_IDD = 0x1,
        COMPARE_OPERATION_MAD_LESS_THAN_IDD = 0x2,
        COMPARE_OPERATION_MAD_LESS_THAN_OR_EQUAL_IDD = 0x3,
        COMPARE_OPERATION_MAD_EQUAL_IDD = 0x4,
        COMPARE_OPERATION_MAD_NOT_EQUAL_IDD = 0x5,
    } COMPARE_OPERATION;
    typedef enum tagCOMPARE_MASK_MODE {
        COMPARE_MASK_MODE_COMPARE_MASK_MODE_DISABLED = 0x0,
        COMPARE_MASK_MODE_COMPARE_MASK_MODE_ENABLED = 0x1,
    } COMPARE_MASK_MODE;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_CONDITIONAL_BATCH_BUFFER_END = 0x36,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.CompareOperation = COMPARE_OPERATION_MAD_GREATER_THAN_IDD;
        TheStructure.Common.CompareMaskMode = COMPARE_MASK_MODE_COMPARE_MASK_MODE_DISABLED;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_CONDITIONAL_BATCH_BUFFER_END;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_CONDITIONAL_BATCH_BUFFER_END sInit() {
        MI_CONDITIONAL_BATCH_BUFFER_END state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 4);
        return TheStructure.RawData[index];
    }
    inline void setCompareOperation(const COMPARE_OPERATION value) {
        TheStructure.Common.CompareOperation = value;
    }
    inline COMPARE_OPERATION getCompareOperation() const {
        return static_cast<COMPARE_OPERATION>(TheStructure.Common.CompareOperation);
    }
    inline void setPredicateEnable(const bool value) {
        TheStructure.Common.PredicateEnable = value;
    }
    inline bool getPredicateEnable() const {
        return TheStructure.Common.PredicateEnable;
    }
    inline void setEndCurrentBatchBufferLevel(const bool value) {
        TheStructure.Common.EndCurrentBatchBufferLevel = value;
    }
    inline bool getEndCurrentBatchBufferLevel() const {
        return TheStructure.Common.EndCurrentBatchBufferLevel;
    }
    inline void setCompareMaskMode(const COMPARE_MASK_MODE value) {
        TheStructure.Common.CompareMaskMode = value;
    }
    inline COMPARE_MASK_MODE getCompareMaskMode() const {
        return static_cast<COMPARE_MASK_MODE>(TheStructure.Common.CompareMaskMode);
    }
    inline void setCompareBufferEncryptedMemoryReadEnable(const bool value) {
        TheStructure.Common.CompareBufferEncryptedMemoryReadEnable = value;
    }
    inline bool getCompareBufferEncryptedMemoryReadEnable() const {
        return TheStructure.Common.CompareBufferEncryptedMemoryReadEnable;
    }
    inline void setCompareSemaphore(const bool value) {
        TheStructure.Common.CompareSemaphore = value;
    }
    inline bool getCompareSemaphore() const {
        return TheStructure.Common.CompareSemaphore;
    }
    inline void setUseGlobalGtt(const bool value) {
        TheStructure.Common.UseGlobalGtt = value;
    }
    inline bool getUseGlobalGtt() const {
        return TheStructure.Common.UseGlobalGtt;
    }
    inline void setCompareDataDword(const uint32_t value) {
        TheStructure.Common.CompareDataDword = value;
    }
    inline uint32_t getCompareDataDword() const {
        return TheStructure.Common.CompareDataDword;
    }
    typedef enum tagCOMPAREADDRESS {
        COMPAREADDRESS_BIT_SHIFT = 0x3,
        COMPAREADDRESS_ALIGN_SIZE = 0x8,
    } COMPAREADDRESS;
    inline void setCompareAddress(const uint64_t value) {
        UNRECOVERABLE_IF((value >> COMPAREADDRESS_BIT_SHIFT) > 0xffffffffffffffffL);
        TheStructure.Common.CompareAddress = value >> COMPAREADDRESS_BIT_SHIFT;
    }
    inline uint64_t getCompareAddress() const {
        return TheStructure.Common.CompareAddress << COMPAREADDRESS_BIT_SHIFT;
    }
} MI_CONDITIONAL_BATCH_BUFFER_END;
STATIC_ASSERT(16 == sizeof(MI_CONDITIONAL_BATCH_BUFFER_END));

typedef struct tagXY_BLOCK_COPY_BLT {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 8);
            uint32_t NumberOfMultisamples : BITFIELD_RANGE(9, 11);
            uint32_t SpecialModeOfOperation : BITFIELD_RANGE(12, 13);
            uint32_t Reserved_14 : BITFIELD_RANGE(14, 18);
            uint32_t ColorDepth : BITFIELD_RANGE(19, 21);
            uint32_t InstructionTarget_Opcode : BITFIELD_RANGE(22, 28);
            uint32_t Client : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint32_t DestinationPitch : BITFIELD_RANGE(0, 17);
            uint32_t Reserved_50 : BITFIELD_RANGE(18, 20);
            uint32_t DestinationEncryptEn : BITFIELD_RANGE(21, 21);
            uint32_t Reserved_54 : BITFIELD_RANGE(22, 23);
            uint32_t DestinationMocs : BITFIELD_RANGE(24, 27);
            uint32_t Reserved_60 : BITFIELD_RANGE(28, 29);
            uint32_t DestinationTiling : BITFIELD_RANGE(30, 31);
            // DWORD 2
            uint32_t DestinationX1Coordinate_Left : BITFIELD_RANGE(0, 15);
            uint32_t DestinationY1Coordinate_Top : BITFIELD_RANGE(16, 31);
            // DWORD 3
            uint32_t DestinationX2Coordinate_Right : BITFIELD_RANGE(0, 15);
            uint32_t DestinationY2Coordinate_Bottom : BITFIELD_RANGE(16, 31);
            // DWORD 4
            uint64_t DestinationBaseAddress;
            // DWORD 6
            uint32_t DestinationXOffset : BITFIELD_RANGE(0, 13);
            uint32_t Reserved_206 : BITFIELD_RANGE(14, 15);
            uint32_t DestinationYOffset : BITFIELD_RANGE(16, 29);
            uint32_t Reserved_222 : BITFIELD_RANGE(30, 30);
            uint32_t DestinationTargetMemory : BITFIELD_RANGE(31, 31);
            // DWORD 7
            uint32_t SourceX1Coordinate_Left : BITFIELD_RANGE(0, 15);
            uint32_t SourceY1Coordinate_Top : BITFIELD_RANGE(16, 31);
            // DWORD 8
            uint32_t SourcePitch : BITFIELD_RANGE(0, 17);
            uint32_t Reserved_274 : BITFIELD_RANGE(18, 20);
            uint32_t SourceEncryptEn : BITFIELD_RANGE(21, 21);
            uint32_t Reserved_278 : BITFIELD_RANGE(22, 23);
            uint32_t SourceMocs : BITFIELD_RANGE(24, 27);
            uint32_t Reserved_284 : BITFIELD_RANGE(28, 29);
            uint32_t SourceTiling : BITFIELD_RANGE(30, 31);
            // DWORD 9
            uint64_t SourceBaseAddress;
            // DWORD 11
            uint32_t SourceXOffset : BITFIELD_RANGE(0, 13);
            uint32_t Reserved_366 : BITFIELD_RANGE(14, 15);
            uint32_t SourceYOffset : BITFIELD_RANGE(16, 29);
            uint32_t Reserved_382 : BITFIELD_RANGE(30, 30);
            uint32_t SourceTargetMemory : BITFIELD_RANGE(31, 31);
            // DWORD 12
            uint32_t SourceCompressionFormat : BITFIELD_RANGE(0, 3);
            uint32_t Reserved_388 : BITFIELD_RANGE(4, 31);
            // DWORD 13
            uint32_t Reserved_416;
            // DWORD 14
            uint32_t DestinationCompressionFormat : BITFIELD_RANGE(0, 3);
            uint32_t Reserved_452 : BITFIELD_RANGE(4, 31);
            // DWORD 15
            uint32_t Reserved_480;
            // DWORD 16
            uint32_t DestinationSurfaceHeight : BITFIELD_RANGE(0, 13);
            uint32_t DestinationSurfaceWidth : BITFIELD_RANGE(14, 27);
            uint32_t Reserved_540 : BITFIELD_RANGE(28, 28);
            uint32_t DestinationSurfaceType : BITFIELD_RANGE(29, 31);
            // DWORD 17
            uint32_t DestinationLod : BITFIELD_RANGE(0, 3);
            uint32_t DestinationSurfaceQpitch : BITFIELD_RANGE(4, 18);
            uint32_t Reserved_563 : BITFIELD_RANGE(19, 20);
            uint32_t DestinationSurfaceDepth : BITFIELD_RANGE(21, 31);
            // DWORD 18
            uint32_t DestinationHorizontalAlign : BITFIELD_RANGE(0, 1);
            uint32_t Reserved_578 : BITFIELD_RANGE(2, 2);
            uint32_t DestinationVerticalAlign : BITFIELD_RANGE(3, 4);
            uint32_t Reserved_581 : BITFIELD_RANGE(5, 7);
            uint32_t DestinationMipTailStartLod : BITFIELD_RANGE(8, 11);
            uint32_t Reserved_588 : BITFIELD_RANGE(12, 20);
            uint32_t DestinationArrayIndex : BITFIELD_RANGE(21, 31);
            // DWORD 19
            uint32_t SourceSurfaceHeight : BITFIELD_RANGE(0, 13);
            uint32_t SourceSurfaceWidth : BITFIELD_RANGE(14, 27);
            uint32_t Reserved_636 : BITFIELD_RANGE(28, 28);
            uint32_t SourceSurfaceType : BITFIELD_RANGE(29, 31);
            // DWORD 20
            uint32_t SourceLod : BITFIELD_RANGE(0, 3);
            uint32_t SourceSurfaceQpitch : BITFIELD_RANGE(4, 18);
            uint32_t Reserved_659 : BITFIELD_RANGE(19, 20);
            uint32_t SourceSurfaceDepth : BITFIELD_RANGE(21, 31);
            // DWORD 21
            uint32_t SourceHorizontalAlign : BITFIELD_RANGE(0, 1);
            uint32_t Reserved_674 : BITFIELD_RANGE(2, 2);
            uint32_t SourceVerticalAlign : BITFIELD_RANGE(3, 4);
            uint32_t Reserved_677 : BITFIELD_RANGE(5, 7);
            uint32_t SourceMipTailStartLod : BITFIELD_RANGE(8, 11);
            uint32_t Reserved_684 : BITFIELD_RANGE(12, 20);
            uint32_t SourceArrayIndex : BITFIELD_RANGE(21, 31);
        } Common;
        uint32_t RawData[22];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x14,
    } DWORD_LENGTH;
    typedef enum tagNUMBER_OF_MULTISAMPLES {
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1 = 0x0,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_2 = 0x1,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_4 = 0x2,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_8 = 0x3,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_16 = 0x4,
    } NUMBER_OF_MULTISAMPLES;
    typedef enum tagSPECIAL_MODE_OF_OPERATION {
        SPECIAL_MODE_OF_OPERATION_NONE = 0x0,
        SPECIAL_MODE_OF_OPERATION_FULL_RESOLVE = 0x1,
        SPECIAL_MODE_OF_OPERATION_PARTIAL_RESOLVE = 0x2,
    } SPECIAL_MODE_OF_OPERATION;
    typedef enum tagCOLOR_DEPTH {
        COLOR_DEPTH_8_BIT_COLOR = 0x0,
        COLOR_DEPTH_16_BIT_COLOR = 0x1,
        COLOR_DEPTH_32_BIT_COLOR = 0x2,
        COLOR_DEPTH_64_BIT_COLOR = 0x3,
        COLOR_DEPTH_96_BIT_COLOR_ONLY_LINEAR_CASE_IS_SUPPORTED = 0x4,
        COLOR_DEPTH_128_BIT_COLOR = 0x5,
    } COLOR_DEPTH;
    typedef enum tagINSTRUCTION_TARGETOPCODE {
        INSTRUCTIONTARGET_OPCODE_OPCODE = 0x41, // patched
    } INSTRUCTION_TARGETOPCODE;
    typedef enum tagCLIENT {
        CLIENT_2D_PROCESSOR = 0x2,
    } CLIENT;
    typedef enum tagDESTINATION_ENCRYPT_EN {
        DESTINATION_ENCRYPT_EN_DISABLE = 0x0,
        DESTINATION_ENCRYPT_EN_ENABLE = 0x1,
    } DESTINATION_ENCRYPT_EN;
    typedef enum tagDESTINATION_TILING {
        DESTINATION_TILING_LINEAR = 0x0,
        DESTINATION_TILING_XMAJOR = 0x1,
        DESTINATION_TILING_TILE4 = 0x2,
        DESTINATION_TILING_TILE64 = 0x3,
    } DESTINATION_TILING;
    typedef enum tagDESTINATION_TARGET_MEMORY {
        DESTINATION_TARGET_MEMORY_LOCAL_MEM = 0x0,
        DESTINATION_TARGET_MEMORY_SYSTEM_MEM = 0x1,
    } DESTINATION_TARGET_MEMORY;

    typedef enum tagTARGET_MEMORY {
        TARGET_MEMORY_LOCAL_MEM = 0,
        TARGET_MEMORY_SYSTEM_MEM = 1,
    } TARGET_MEMORY; // patched

    typedef enum tagSOURCE_ENCRYPT_EN {
        SOURCE_ENCRYPT_EN_DISABLE = 0x0,
        SOURCE_ENCRYPT_EN_ENABLE = 0x1,
    } SOURCE_ENCRYPT_EN;
    typedef enum tagSOURCE_TILING {
        SOURCE_TILING_LINEAR = 0x0,
        SOURCE_TILING_XMAJOR = 0x1,
        SOURCE_TILING_TILE4 = 0x2,
        SOURCE_TILING_TILE64 = 0x3,
    } SOURCE_TILING;

    typedef enum tagTILING {
        TILING_LINEAR = 0x0,
        TILING_XMAJOR = 0x1,
        TILING_TILE4 = 0x2,
        TILING_TILE64 = 0x3,
    } TILING; // patched

    typedef enum tagSOURCE_TARGET_MEMORY {
        SOURCE_TARGET_MEMORY_LOCAL_MEM = 0x0,
        SOURCE_TARGET_MEMORY_SYSTEM_MEM = 0x1,
    } SOURCE_TARGET_MEMORY;
    typedef enum tagSOURCE_COMPRESSION_FORMAT {
        SOURCE_COMPRESSION_FORMAT_CMF_R8 = 0x0,
        SOURCE_COMPRESSION_FORMAT_CMF_R8_G8 = 0x1,
        SOURCE_COMPRESSION_FORMAT_CMF_R8_G8_B8_A8 = 0x2,
        SOURCE_COMPRESSION_FORMAT_CMF_R10_G10_B10_A2 = 0x3,
        SOURCE_COMPRESSION_FORMAT_CMF_R11_G11_B10 = 0x4,
        SOURCE_COMPRESSION_FORMAT_CMF_R16 = 0x5,
        SOURCE_COMPRESSION_FORMAT_CMF_R16_G16 = 0x6,
        SOURCE_COMPRESSION_FORMAT_CMF_R16_G16_B16_A16 = 0x7,
        SOURCE_COMPRESSION_FORMAT_CMF_R32 = 0x8,
        SOURCE_COMPRESSION_FORMAT_CMF_R32_G32 = 0x9,
        SOURCE_COMPRESSION_FORMAT_CMF_R32_G32_B32_A32 = 0xa,
        SOURCE_COMPRESSION_FORMAT_CMF_Y16_U16_Y16_V16 = 0xb,
        SOURCE_COMPRESSION_FORMAT_CMF_ML8 = 0xf,
    } SOURCE_COMPRESSION_FORMAT;
    typedef enum tagDESTINATION_COMPRESSION_FORMAT {
        DESTINATION_COMPRESSION_FORMAT_CMF_R8 = 0x0,
        DESTINATION_COMPRESSION_FORMAT_CMF_R8_G8 = 0x1,
        DESTINATION_COMPRESSION_FORMAT_CMF_R8_G8_B8_A8 = 0x2,
        DESTINATION_COMPRESSION_FORMAT_CMF_R10_G10_B10_A2 = 0x3,
        DESTINATION_COMPRESSION_FORMAT_CMF_R11_G11_B10 = 0x4,
        DESTINATION_COMPRESSION_FORMAT_CMF_R16 = 0x5,
        DESTINATION_COMPRESSION_FORMAT_CMF_R16_G16 = 0x6,
        DESTINATION_COMPRESSION_FORMAT_CMF_R16_G16_B16_A16 = 0x7,
        DESTINATION_COMPRESSION_FORMAT_CMF_R32 = 0x8,
        DESTINATION_COMPRESSION_FORMAT_CMF_R32_G32 = 0x9,
        DESTINATION_COMPRESSION_FORMAT_CMF_R32_G32_B32_A32 = 0xa,
        DESTINATION_COMPRESSION_FORMAT_CMF_Y16_U16_Y16_V16 = 0xb,
        DESTINATION_COMPRESSION_FORMAT_CMF_ML8 = 0xf,
    } DESTINATION_COMPRESSION_FORMAT;
    typedef enum tagDESTINATION_SURFACE_TYPE {
        DESTINATION_SURFACE_TYPE_SURFTYPE_1D = 0x0,
        DESTINATION_SURFACE_TYPE_SURFTYPE_2D = 0x1,
        DESTINATION_SURFACE_TYPE_SURFTYPE_3D = 0x2,
        DESTINATION_SURFACE_TYPE_SURFTYPE_CUBE = 0x3,
    } DESTINATION_SURFACE_TYPE;
    typedef enum tagSOURCE_SURFACE_TYPE {
        SOURCE_SURFACE_TYPE_SURFTYPE_1D = 0x0,
        SOURCE_SURFACE_TYPE_SURFTYPE_2D = 0x1,
        SOURCE_SURFACE_TYPE_SURFTYPE_3D = 0x2,
        SOURCE_SURFACE_TYPE_SURFTYPE_CUBE = 0x3,
    } SOURCE_SURFACE_TYPE;
    typedef enum tagSURFACE_TYPE {
        SURFACE_TYPE_SURFTYPE_1D = 0x0,
        SURFACE_TYPE_SURFTYPE_2D = 0x1,
        SURFACE_TYPE_SURFTYPE_3D = 0x2,
        SURFACE_TYPE_SURFTYPE_CUBE = 0x3,
    } SURFACE_TYPE; // patched
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.NumberOfMultisamples = NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1;
        TheStructure.Common.SpecialModeOfOperation = SPECIAL_MODE_OF_OPERATION_NONE;
        TheStructure.Common.ColorDepth = COLOR_DEPTH_8_BIT_COLOR;
        TheStructure.Common.InstructionTarget_Opcode = INSTRUCTIONTARGET_OPCODE_OPCODE;
        TheStructure.Common.Client = CLIENT_2D_PROCESSOR;
        TheStructure.Common.DestinationEncryptEn = DESTINATION_ENCRYPT_EN_DISABLE;
        TheStructure.Common.DestinationTiling = DESTINATION_TILING_LINEAR;
        TheStructure.Common.DestinationTargetMemory = DESTINATION_TARGET_MEMORY_LOCAL_MEM;
        TheStructure.Common.SourceEncryptEn = SOURCE_ENCRYPT_EN_DISABLE;
        TheStructure.Common.SourceTiling = SOURCE_TILING_LINEAR;
        TheStructure.Common.SourceTargetMemory = SOURCE_TARGET_MEMORY_LOCAL_MEM;
        TheStructure.Common.SourceCompressionFormat = SOURCE_COMPRESSION_FORMAT_CMF_R8;
        TheStructure.Common.DestinationCompressionFormat = DESTINATION_COMPRESSION_FORMAT_CMF_R8;
        TheStructure.Common.DestinationSurfaceType = DESTINATION_SURFACE_TYPE_SURFTYPE_1D;
        TheStructure.Common.SourceSurfaceType = SOURCE_SURFACE_TYPE_SURFTYPE_1D;
    }
    static tagXY_BLOCK_COPY_BLT sInit() {
        XY_BLOCK_COPY_BLT state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 22);
        return TheStructure.RawData[index];
    }
    inline void setNumberOfMultisamples(const NUMBER_OF_MULTISAMPLES value) {
        TheStructure.Common.NumberOfMultisamples = value;
    }
    inline NUMBER_OF_MULTISAMPLES getNumberOfMultisamples() const {
        return static_cast<NUMBER_OF_MULTISAMPLES>(TheStructure.Common.NumberOfMultisamples);
    }
    inline void setSpecialModeOfOperation(const SPECIAL_MODE_OF_OPERATION value) {
        TheStructure.Common.SpecialModeOfOperation = value;
    }
    inline SPECIAL_MODE_OF_OPERATION getSpecialModeOfOperation() const {
        return static_cast<SPECIAL_MODE_OF_OPERATION>(TheStructure.Common.SpecialModeOfOperation);
    }
    inline void setColorDepth(const COLOR_DEPTH value) {
        TheStructure.Common.ColorDepth = value;
    }
    inline COLOR_DEPTH getColorDepth() const {
        return static_cast<COLOR_DEPTH>(TheStructure.Common.ColorDepth);
    }
    inline void setInstructionTargetOpcode(const INSTRUCTION_TARGETOPCODE value) {
        TheStructure.Common.InstructionTarget_Opcode = value;
    }
    inline INSTRUCTION_TARGETOPCODE getInstructionTargetOpcode() const {
        return static_cast<INSTRUCTION_TARGETOPCODE>(TheStructure.Common.InstructionTarget_Opcode);
    }
    inline void setClient(const CLIENT value) {
        TheStructure.Common.Client = value;
    }
    inline CLIENT getClient() const {
        return static_cast<CLIENT>(TheStructure.Common.Client);
    }
    inline void setDestinationPitch(const uint32_t value) {
        UNRECOVERABLE_IF(value - 1 > 0x3ffff);
        TheStructure.Common.DestinationPitch = value - 1;
    }
    inline uint32_t getDestinationPitch() const {
        return TheStructure.Common.DestinationPitch + 1;
    }
    inline void setDestinationEncryptEn(const DESTINATION_ENCRYPT_EN value) {
        TheStructure.Common.DestinationEncryptEn = value;
    }
    inline DESTINATION_ENCRYPT_EN getDestinationEncryptEn() const {
        return static_cast<DESTINATION_ENCRYPT_EN>(TheStructure.Common.DestinationEncryptEn);
    }
    inline void setDestinationMOCS(const uint32_t value) { // patched
        UNRECOVERABLE_IF(value > 0x1f);
        setDestinationEncryptEn(static_cast<DESTINATION_ENCRYPT_EN>(value & 1));
        TheStructure.Common.DestinationMocs = value >> 1;
    }
    inline uint32_t getDestinationMOCS() const { // patched
        uint32_t mocs = static_cast<uint32_t>(getDestinationEncryptEn());
        return mocs | (TheStructure.Common.DestinationMocs << 1);
    }
    inline void setDestinationTiling(const TILING value) { // patched
        TheStructure.Common.DestinationTiling = value;
    }
    inline TILING getDestinationTiling() const { // patched
        return static_cast<TILING>(TheStructure.Common.DestinationTiling);
    }
    inline void setDestinationX1CoordinateLeft(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffff);
        TheStructure.Common.DestinationX1Coordinate_Left = value;
    }
    inline uint32_t getDestinationX1CoordinateLeft() const {
        return TheStructure.Common.DestinationX1Coordinate_Left;
    }
    inline void setDestinationY1CoordinateTop(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffff);
        TheStructure.Common.DestinationY1Coordinate_Top = value;
    }
    inline uint32_t getDestinationY1CoordinateTop() const {
        return TheStructure.Common.DestinationY1Coordinate_Top;
    }
    inline void setDestinationX2CoordinateRight(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffff);
        TheStructure.Common.DestinationX2Coordinate_Right = value;
    }
    inline uint32_t getDestinationX2CoordinateRight() const {
        return TheStructure.Common.DestinationX2Coordinate_Right;
    }
    inline void setDestinationY2CoordinateBottom(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffff);
        TheStructure.Common.DestinationY2Coordinate_Bottom = value;
    }
    inline uint32_t getDestinationY2CoordinateBottom() const {
        return TheStructure.Common.DestinationY2Coordinate_Bottom;
    }
    inline void setDestinationBaseAddress(const uint64_t value) {
        TheStructure.Common.DestinationBaseAddress = value;
    }
    inline uint64_t getDestinationBaseAddress() const {
        return TheStructure.Common.DestinationBaseAddress;
    }
    inline void setDestinationXOffset(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3fff);
        TheStructure.Common.DestinationXOffset = value;
    }
    inline uint32_t getDestinationXOffset() const {
        return TheStructure.Common.DestinationXOffset;
    }
    inline void setDestinationYOffset(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3fff);
        TheStructure.Common.DestinationYOffset = value;
    }
    inline uint32_t getDestinationYOffset() const {
        return TheStructure.Common.DestinationYOffset;
    }
    inline void setDestinationTargetMemory(const TARGET_MEMORY value) { // patched
        TheStructure.Common.DestinationTargetMemory = value;
    }
    inline TARGET_MEMORY getDestinationTargetMemory() const { // patched
        return static_cast<TARGET_MEMORY>(TheStructure.Common.DestinationTargetMemory);
    }
    inline void setSourceX1CoordinateLeft(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffff);
        TheStructure.Common.SourceX1Coordinate_Left = value;
    }
    inline uint32_t getSourceX1CoordinateLeft() const {
        return TheStructure.Common.SourceX1Coordinate_Left;
    }
    inline void setSourceY1CoordinateTop(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffff);
        TheStructure.Common.SourceY1Coordinate_Top = value;
    }
    inline uint32_t getSourceY1CoordinateTop() const {
        return TheStructure.Common.SourceY1Coordinate_Top;
    }
    inline void setSourcePitch(const uint32_t value) {
        UNRECOVERABLE_IF(value - 1 > 0x3ffff);
        TheStructure.Common.SourcePitch = value - 1;
    }
    inline uint32_t getSourcePitch() const {
        return TheStructure.Common.SourcePitch + 1;
    }
    inline void setSourceEncryptEn(const SOURCE_ENCRYPT_EN value) {
        TheStructure.Common.SourceEncryptEn = value;
    }
    inline SOURCE_ENCRYPT_EN getSourceEncryptEn() const {
        return static_cast<SOURCE_ENCRYPT_EN>(TheStructure.Common.SourceEncryptEn);
    }
    inline void setSourceMOCS(const uint32_t value) { // patched
        UNRECOVERABLE_IF(value > 0x1f);
        setSourceEncryptEn(static_cast<SOURCE_ENCRYPT_EN>(value & 1));
        TheStructure.Common.SourceMocs = value >> 1;
    }
    inline uint32_t getSourceMOCS() const { // patched
        uint32_t mocs = static_cast<uint32_t>(getSourceEncryptEn());
        return mocs | (TheStructure.Common.SourceMocs << 1);
    }
    inline void setSourceTiling(const TILING value) { // patched
        TheStructure.Common.SourceTiling = value;
    }
    inline TILING getSourceTiling() const { // patched
        return static_cast<TILING>(TheStructure.Common.SourceTiling);
    }
    inline void setSourceBaseAddress(const uint64_t value) {
        TheStructure.Common.SourceBaseAddress = value;
    }
    inline uint64_t getSourceBaseAddress() const {
        return TheStructure.Common.SourceBaseAddress;
    }
    inline void setSourceXOffset(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3fff);
        TheStructure.Common.SourceXOffset = value;
    }
    inline uint32_t getSourceXOffset() const {
        return TheStructure.Common.SourceXOffset;
    }
    inline void setSourceYOffset(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3fff);
        TheStructure.Common.SourceYOffset = value;
    }
    inline uint32_t getSourceYOffset() const {
        return TheStructure.Common.SourceYOffset;
    }
    inline void setSourceTargetMemory(const TARGET_MEMORY value) { // patched
        TheStructure.Common.SourceTargetMemory = value;
    }
    inline TARGET_MEMORY getSourceTargetMemory() const { // patched
        return static_cast<TARGET_MEMORY>(TheStructure.Common.SourceTargetMemory);
    }
    inline void setSourceCompressionFormat(const uint32_t value) { // patched
        TheStructure.Common.SourceCompressionFormat = value;
    }
    inline uint32_t getSourceCompressionFormat() const { // patched
        return TheStructure.Common.SourceCompressionFormat;
    }
    inline void setDestinationCompressionFormat(const uint32_t value) { // patched
        TheStructure.Common.DestinationCompressionFormat = value;
    }
    inline uint32_t getDestinationCompressionFormat() const { // patched
        return TheStructure.Common.DestinationCompressionFormat;
    }
    inline void setDestinationSurfaceHeight(const uint32_t value) {
        UNRECOVERABLE_IF(value - 1 > 0x3fff);
        TheStructure.Common.DestinationSurfaceHeight = value - 1;
    }
    inline uint32_t getDestinationSurfaceHeight() const {
        return TheStructure.Common.DestinationSurfaceHeight + 1;
    }
    inline void setDestinationSurfaceWidth(const uint32_t value) {
        UNRECOVERABLE_IF(value - 1 > 0x3fff);
        TheStructure.Common.DestinationSurfaceWidth = value - 1;
    }
    inline uint32_t getDestinationSurfaceWidth() const {
        return TheStructure.Common.DestinationSurfaceWidth + 1;
    }
    inline void setDestinationSurfaceType(const SURFACE_TYPE value) { // patched
        TheStructure.Common.DestinationSurfaceType = value;
    }
    inline SURFACE_TYPE getDestinationSurfaceType() const { // patched
        return static_cast<SURFACE_TYPE>(TheStructure.Common.DestinationSurfaceType);
    }
    inline void setDestinationLod(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xf);
        TheStructure.Common.DestinationLod = value;
    }
    inline uint32_t getDestinationLod() const {
        return TheStructure.Common.DestinationLod;
    }
    inline void setDestinationSurfaceQpitch(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x7fff);
        TheStructure.Common.DestinationSurfaceQpitch = value;
    }
    inline uint32_t getDestinationSurfaceQpitch() const {
        return TheStructure.Common.DestinationSurfaceQpitch;
    }
    inline void setDestinationSurfaceDepth(const uint32_t value) {
        UNRECOVERABLE_IF(value - 1 > 0x7ff);
        TheStructure.Common.DestinationSurfaceDepth = value - 1;
    }
    inline uint32_t getDestinationSurfaceDepth() const {
        return TheStructure.Common.DestinationSurfaceDepth + 1;
    }
    inline void setDestinationHorizontalAlign(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3);
        TheStructure.Common.DestinationHorizontalAlign = value;
    }
    inline uint32_t getDestinationHorizontalAlign() const {
        return TheStructure.Common.DestinationHorizontalAlign;
    }
    inline void setDestinationVerticalAlign(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3);
        TheStructure.Common.DestinationVerticalAlign = value;
    }
    inline uint32_t getDestinationVerticalAlign() const {
        return TheStructure.Common.DestinationVerticalAlign;
    }
    inline void setDestinationMipTailStartLOD(const uint32_t value) { // patched
        UNRECOVERABLE_IF(value > 0xf);
        TheStructure.Common.DestinationMipTailStartLod = value;
    }
    inline uint32_t getDestinationMipTailStartLOD() const { // patched
        return TheStructure.Common.DestinationMipTailStartLod;
    }
    inline void setDestinationArrayIndex(const uint32_t value) {
        UNRECOVERABLE_IF(value - 1 > 0x7ff);
        TheStructure.Common.DestinationArrayIndex = value - 1;
    }
    inline uint32_t getDestinationArrayIndex() const {
        return TheStructure.Common.DestinationArrayIndex + 1;
    }
    inline void setSourceSurfaceHeight(const uint32_t value) {
        UNRECOVERABLE_IF(value - 1 > 0x3fff);
        TheStructure.Common.SourceSurfaceHeight = value - 1;
    }
    inline uint32_t getSourceSurfaceHeight() const {
        return TheStructure.Common.SourceSurfaceHeight + 1;
    }
    inline void setSourceSurfaceWidth(const uint32_t value) {
        UNRECOVERABLE_IF(value - 1 > 0x3fff);
        TheStructure.Common.SourceSurfaceWidth = value - 1;
    }
    inline uint32_t getSourceSurfaceWidth() const {
        return TheStructure.Common.SourceSurfaceWidth + 1;
    }
    inline void setSourceSurfaceType(const SURFACE_TYPE value) { // patched
        TheStructure.Common.SourceSurfaceType = value;
    }
    inline SURFACE_TYPE getSourceSurfaceType() const { // patched
        return static_cast<SURFACE_TYPE>(TheStructure.Common.SourceSurfaceType);
    }
    inline void setSourceLod(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xf);
        TheStructure.Common.SourceLod = value;
    }
    inline uint32_t getSourceLod() const {
        return TheStructure.Common.SourceLod;
    }
    inline void setSourceSurfaceQpitch(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x7fff);
        TheStructure.Common.SourceSurfaceQpitch = value;
    }
    inline uint32_t getSourceSurfaceQpitch() const {
        return TheStructure.Common.SourceSurfaceQpitch;
    }
    inline void setSourceSurfaceDepth(const uint32_t value) {
        UNRECOVERABLE_IF(value - 1 > 0x7ff);
        TheStructure.Common.SourceSurfaceDepth = value - 1;
    }
    inline uint32_t getSourceSurfaceDepth() const {
        return TheStructure.Common.SourceSurfaceDepth + 1;
    }
    inline void setSourceHorizontalAlign(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3);
        TheStructure.Common.SourceHorizontalAlign = value;
    }
    inline uint32_t getSourceHorizontalAlign() const {
        return TheStructure.Common.SourceHorizontalAlign;
    }
    inline void setSourceVerticalAlign(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3);
        TheStructure.Common.SourceVerticalAlign = value;
    }
    inline uint32_t getSourceVerticalAlign() const {
        return TheStructure.Common.SourceVerticalAlign;
    }
    inline void setSourceMipTailStartLOD(const uint32_t value) { // patched
        UNRECOVERABLE_IF(value > 0xf);
        TheStructure.Common.SourceMipTailStartLod = value;
    }
    inline uint32_t getSourceMipTailStartLOD() const { // patched
        return TheStructure.Common.SourceMipTailStartLod;
    }
    inline void setSourceArrayIndex(const uint32_t value) {
        UNRECOVERABLE_IF(value - 1 > 0x7ff);
        TheStructure.Common.SourceArrayIndex = value - 1;
    }
    inline uint32_t getSourceArrayIndex() const {
        return TheStructure.Common.SourceArrayIndex + 1;
    }
} XY_BLOCK_COPY_BLT;
STATIC_ASSERT(88 == sizeof(XY_BLOCK_COPY_BLT));

typedef struct tagXY_FAST_COLOR_BLT {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 8);
            uint32_t NumberOfMultisamples : BITFIELD_RANGE(9, 11);
            uint32_t SpecialModeOfOperation : BITFIELD_RANGE(12, 13);
            uint32_t Reserved_14 : BITFIELD_RANGE(14, 18);
            uint32_t ColorDepth : BITFIELD_RANGE(19, 21);
            uint32_t InstructionTarget_Opcode : BITFIELD_RANGE(22, 28);
            uint32_t Client : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint32_t DestinationPitch : BITFIELD_RANGE(0, 17);
            uint32_t Reserved_50 : BITFIELD_RANGE(18, 20);
            uint32_t DestinationEncryptEn : BITFIELD_RANGE(21, 21);
            uint32_t Reserved_54 : BITFIELD_RANGE(22, 23);
            uint32_t DestinationMocs : BITFIELD_RANGE(24, 27);
            uint32_t Reserved_60 : BITFIELD_RANGE(28, 29);
            uint32_t DestinationTiling : BITFIELD_RANGE(30, 31);
            // DWORD 2
            uint32_t DestinationX1Coordinate_Left : BITFIELD_RANGE(0, 15);
            uint32_t DestinationY1Coordinate_Top : BITFIELD_RANGE(16, 31);
            // DWORD 3
            uint32_t DestinationX2Coordinate_Right : BITFIELD_RANGE(0, 15);
            uint32_t DestinationY2Coordinate_Bottom : BITFIELD_RANGE(16, 31);
            // DWORD 4
            uint64_t DestinationBaseAddress;
            // DWORD 6
            uint32_t DestinationXOffset : BITFIELD_RANGE(0, 13);
            uint32_t Reserved_206 : BITFIELD_RANGE(14, 15);
            uint32_t DestinationYOffset : BITFIELD_RANGE(16, 29);
            uint32_t Reserved_222 : BITFIELD_RANGE(30, 30);
            uint32_t DestinationTargetMemory : BITFIELD_RANGE(31, 31);
            // DWORD 7..10
            uint32_t FillColorValue[4]; // patched
            // DWORD 11
            uint32_t DestinationCompressionFormat : BITFIELD_RANGE(0, 3);
            uint32_t Reserved_356 : BITFIELD_RANGE(4, 31);
            // DWORD 12
            uint32_t SurfaceFormat : BITFIELD_RANGE(0, 8);
            uint32_t Reserved_393 : BITFIELD_RANGE(9, 28);
            uint32_t AuxSpecialOperationsMode : BITFIELD_RANGE(29, 31);
            // DWORD 13
            uint32_t DestinationSurfaceHeight : BITFIELD_RANGE(0, 13);
            uint32_t DestinationSurfaceWidth : BITFIELD_RANGE(14, 27);
            uint32_t Reserved_444 : BITFIELD_RANGE(28, 28);
            uint32_t DestinationSurfaceType : BITFIELD_RANGE(29, 31);
            // DWORD 14
            uint32_t DestinationLod : BITFIELD_RANGE(0, 3);
            uint32_t DestinationSurfaceQpitch : BITFIELD_RANGE(4, 18);
            uint32_t Reserved_467 : BITFIELD_RANGE(19, 20);
            uint32_t DestinationSurfaceDepth : BITFIELD_RANGE(21, 31);
            // DWORD 15
            uint32_t DestinationHorizontalAlign : BITFIELD_RANGE(0, 1);
            uint32_t Reserved_482 : BITFIELD_RANGE(2, 2);
            uint32_t DestinationVerticalAlign : BITFIELD_RANGE(3, 4);
            uint32_t Reserved_485 : BITFIELD_RANGE(5, 7);
            uint32_t DestinationMipTailStartLod : BITFIELD_RANGE(8, 11);
            uint32_t Reserved_492 : BITFIELD_RANGE(12, 17);
            uint32_t DestinationDepthStencilResource : BITFIELD_RANGE(18, 18);
            uint32_t Reserved_499 : BITFIELD_RANGE(19, 20);
            uint32_t DestinationArrayIndex : BITFIELD_RANGE(21, 31);
        } Common;
        uint32_t RawData[16];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0xe,
    } DWORD_LENGTH;
    typedef enum tagNUMBER_OF_MULTISAMPLES {
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1 = 0x0,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_2 = 0x1,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_4 = 0x2,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_8 = 0x3,
        NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_16 = 0x4,
    } NUMBER_OF_MULTISAMPLES;
    typedef enum tagSPECIAL_MODE_OF_OPERATION {
        SPECIAL_MODE_OF_OPERATION_NONE = 0x0,
        SPECIAL_MODE_OF_OPERATION_USE_AUX_SPECIAL_OPERATIONS_MODE = 0x1,
    } SPECIAL_MODE_OF_OPERATION;
    typedef enum tagCOLOR_DEPTH {
        COLOR_DEPTH_8_BIT_COLOR = 0x0,
        COLOR_DEPTH_16_BIT_COLOR = 0x1,
        COLOR_DEPTH_32_BIT_COLOR = 0x2,
        COLOR_DEPTH_64_BIT_COLOR = 0x3,
        COLOR_DEPTH_96_BIT_COLOR_ONLY_SUPPORTED_FOR_LINEAR_CASE = 0x4,
        COLOR_DEPTH_128_BIT_COLOR = 0x5,
    } COLOR_DEPTH;
    typedef enum tagINSTRUCTION_TARGETOPCODE {
        INSTRUCTIONTARGET_OPCODE_OPCODE = 0x44,
    } INSTRUCTION_TARGETOPCODE;
    typedef enum tagCLIENT {
        CLIENT_2D_PROCESSOR = 0x2,
    } CLIENT;
    typedef enum tagDESTINATION_ENCRYPT_EN {
        DESTINATION_ENCRYPT_EN_DISABLE = 0x0,
        DESTINATION_ENCRYPT_EN_ENABLE = 0x1,
    } DESTINATION_ENCRYPT_EN;
    typedef enum tagDESTINATION_TILING {
        DESTINATION_TILING_LINEAR = 0x0,
        DESTINATION_TILING_XMAJOR = 0x1,
        DESTINATION_TILING_TILE4 = 0x2,
        DESTINATION_TILING_TILE64 = 0x3,
    } DESTINATION_TILING;
    typedef enum tagDESTINATION_TARGET_MEMORY {
        DESTINATION_TARGET_MEMORY_LOCAL_MEM = 0x0,
        DESTINATION_TARGET_MEMORY_SYSTEM_MEM = 0x1,
    } DESTINATION_TARGET_MEMORY;
    typedef enum tagDESTINATION_COMPRESSION_FORMAT {
        DESTINATION_COMPRESSION_FORMAT_CMF_R8 = 0x0,
        DESTINATION_COMPRESSION_FORMAT_CMF_R8_G8 = 0x1,
        DESTINATION_COMPRESSION_FORMAT_CMF_R8_G8_B8_A8 = 0x2,
        DESTINATION_COMPRESSION_FORMAT_CMF_R10_G10_B10_A2 = 0x3,
        DESTINATION_COMPRESSION_FORMAT_CMF_R11_G11_B10 = 0x4,
        DESTINATION_COMPRESSION_FORMAT_CMF_R16 = 0x5,
        DESTINATION_COMPRESSION_FORMAT_CMF_R16_G16 = 0x6,
        DESTINATION_COMPRESSION_FORMAT_CMF_R16_G16_B16_A16 = 0x7,
        DESTINATION_COMPRESSION_FORMAT_CMF_R32 = 0x8,
        DESTINATION_COMPRESSION_FORMAT_CMF_R32_G32 = 0x9,
        DESTINATION_COMPRESSION_FORMAT_CMF_R32_G32_B32_A32 = 0xa,
        DESTINATION_COMPRESSION_FORMAT_CMF_Y16_U16_Y16_V16 = 0xb,
        DESTINATION_COMPRESSION_FORMAT_CMF_ML8 = 0xf,
    } DESTINATION_COMPRESSION_FORMAT;
    typedef enum tagSURFACE_FORMAT {
        SURFACE_FORMAT_R32G32B32A32_FLOAT = 0x0,
        SURFACE_FORMAT_R32G32B32A32_SINT = 0x1,
        SURFACE_FORMAT_R32G32B32A32_UINT = 0x2,
        SURFACE_FORMAT_R32G32B32A32_UNORM = 0x3,
        SURFACE_FORMAT_R32G32B32A32_SNORM = 0x4,
        SURFACE_FORMAT_R64G64_FLOAT = 0x5,
        SURFACE_FORMAT_R32G32B32X32_FLOAT = 0x6,
        SURFACE_FORMAT_R32G32B32A32_SSCALED = 0x7,
        SURFACE_FORMAT_R32G32B32A32_USCALED = 0x8,
        SURFACE_FORMAT_PLANAR_422_8_P208 = 0xc,
        SURFACE_FORMAT_PLANAR_420_8_SAMPLE_8X8 = 0xd,
        SURFACE_FORMAT_PLANAR_411_8 = 0xe,
        SURFACE_FORMAT_PLANAR_422_8 = 0xf,
        SURFACE_FORMAT_R8G8B8A8_UNORM_VDI = 0x10,
        SURFACE_FORMAT_YCRCB_NORMAL_SAMPLE_8X8 = 0x11,
        SURFACE_FORMAT_YCRCB_SWAPUVY_SAMPLE_8X8 = 0x12,
        SURFACE_FORMAT_YCRCB_SWAPUV_SAMPLE_8X8 = 0x13,
        SURFACE_FORMAT_YCRCB_SWAPY_SAMPLE_8X8 = 0x14,
        SURFACE_FORMAT_R32G32B32A32_FLOAT_LD = 0x15,
        SURFACE_FORMAT_PLANAR_420_16_SAMPLE_8X8 = 0x16,
        SURFACE_FORMAT_R16B16_UNORM_SAMPLE_8X8 = 0x17,
        SURFACE_FORMAT_Y16_UNORM_SAMPLE_8X8 = 0x18,
        SURFACE_FORMAT_PLANAR_Y32_UNORM = 0x19,
        SURFACE_FORMAT_R32G32B32A32_SFIXED = 0x20,
        SURFACE_FORMAT_R64G64_PASSTHRU = 0x21,
        SURFACE_FORMAT_R32G32B32_FLOAT = 0x40,
        SURFACE_FORMAT_R32G32B32_SINT = 0x41,
        SURFACE_FORMAT_R32G32B32_UINT = 0x42,
        SURFACE_FORMAT_R32G32B32_UNORM = 0x43,
        SURFACE_FORMAT_R32G32B32_SNORM = 0x44,
        SURFACE_FORMAT_R32G32B32_SSCALED = 0x45,
        SURFACE_FORMAT_R32G32B32_USCALED = 0x46,
        SURFACE_FORMAT_R32G32B32_FLOAT_LD = 0x47,
        SURFACE_FORMAT_R32G32B32_SFIXED = 0x50,
        SURFACE_FORMAT_R16G16B16A16_UNORM = 0x80,
        SURFACE_FORMAT_R16G16B16A16_SNORM = 0x81,
        SURFACE_FORMAT_R16G16B16A16_SINT = 0x82,
        SURFACE_FORMAT_R16G16B16A16_UINT = 0x83,
        SURFACE_FORMAT_R16G16B16A16_FLOAT = 0x84,
        SURFACE_FORMAT_R32G32_FLOAT = 0x85,
        SURFACE_FORMAT_R32G32_SINT = 0x86,
        SURFACE_FORMAT_R32G32_UINT = 0x87,
        SURFACE_FORMAT_R32_FLOAT_X8X24_TYPELESS = 0x88,
        SURFACE_FORMAT_X32_TYPELESS_G8X24_UINT = 0x89,
        SURFACE_FORMAT_L32A32_FLOAT = 0x8a,
        SURFACE_FORMAT_R32G32_UNORM = 0x8b,
        SURFACE_FORMAT_R32G32_SNORM = 0x8c,
        SURFACE_FORMAT_R64_FLOAT = 0x8d,
        SURFACE_FORMAT_R16G16B16X16_UNORM = 0x8e,
        SURFACE_FORMAT_R16G16B16X16_FLOAT = 0x8f,
        SURFACE_FORMAT_A32X32_FLOAT = 0x90,
        SURFACE_FORMAT_L32X32_FLOAT = 0x91,
        SURFACE_FORMAT_I32X32_FLOAT = 0x92,
        SURFACE_FORMAT_R16G16B16A16_SSCALED = 0x93,
        SURFACE_FORMAT_R16G16B16A16_USCALED = 0x94,
        SURFACE_FORMAT_R32G32_SSCALED = 0x95,
        SURFACE_FORMAT_R32G32_USCALED = 0x96,
        SURFACE_FORMAT_R32G32_FLOAT_LD = 0x97,
        SURFACE_FORMAT_R32_FLOAT_X8X24_TYPELESS_LD = 0x98,
        SURFACE_FORMAT_R32G32_SFIXED = 0xa0,
        SURFACE_FORMAT_R64_PASSTHRU = 0xa1,
        SURFACE_FORMAT_B8G8R8A8_UNORM = 0xc0,
        SURFACE_FORMAT_B8G8R8A8_UNORM_SRGB = 0xc1,
        SURFACE_FORMAT_R10G10B10A2_UNORM = 0xc2,
        SURFACE_FORMAT_R10G10B10A2_UNORM_SRGB = 0xc3,
        SURFACE_FORMAT_R10G10B10A2_UINT = 0xc4,
        SURFACE_FORMAT_R10G10B10_SNORM_A2_UNORM = 0xc5,
        SURFACE_FORMAT_R10G10B10A2_UNORM_SAMPLE_8X8 = 0xc6,
        SURFACE_FORMAT_R8G8B8A8_UNORM = 0xc7,
        SURFACE_FORMAT_R8G8B8A8_UNORM_SRGB = 0xc8,
        SURFACE_FORMAT_R8G8B8A8_SNORM = 0xc9,
        SURFACE_FORMAT_R8G8B8A8_SINT = 0xca,
        SURFACE_FORMAT_R8G8B8A8_UINT = 0xcb,
        SURFACE_FORMAT_R16G16_UNORM = 0xcc,
        SURFACE_FORMAT_R16G16_SNORM = 0xcd,
        SURFACE_FORMAT_R16G16_SINT = 0xce,
        SURFACE_FORMAT_R16G16_UINT = 0xcf,
        SURFACE_FORMAT_R16G16_FLOAT = 0xd0,
        SURFACE_FORMAT_B10G10R10A2_UNORM = 0xd1,
        SURFACE_FORMAT_B10G10R10A2_UNORM_SRGB = 0xd2,
        SURFACE_FORMAT_R11G11B10_FLOAT = 0xd3,
        SURFACE_FORMAT_R10G10B10_FLOAT_A2_UNORM = 0xd5,
        SURFACE_FORMAT_R32_SINT = 0xd6,
        SURFACE_FORMAT_R32_UINT = 0xd7,
        SURFACE_FORMAT_R32_FLOAT = 0xd8,
        SURFACE_FORMAT_R24_UNORM_X8_TYPELESS = 0xd9,
        SURFACE_FORMAT_X24_TYPELESS_G8_UINT = 0xda,
        SURFACE_FORMAT_R32_FLOAT_LD = 0xdb,
        SURFACE_FORMAT_R24_UNORM_X8_TYPELESS_LD = 0xdc,
        SURFACE_FORMAT_L32_UNORM = 0xdd,
        SURFACE_FORMAT_A32_UNORM = 0xde,
        SURFACE_FORMAT_L16A16_UNORM = 0xdf,
        SURFACE_FORMAT_I24X8_UNORM = 0xe0,
        SURFACE_FORMAT_L24X8_UNORM = 0xe1,
        SURFACE_FORMAT_A24X8_UNORM = 0xe2,
        SURFACE_FORMAT_I32_FLOAT = 0xe3,
        SURFACE_FORMAT_L32_FLOAT = 0xe4,
        SURFACE_FORMAT_A32_FLOAT = 0xe5,
        SURFACE_FORMAT_X8B8_UNORM_G8R8_SNORM = 0xe6,
        SURFACE_FORMAT_A8X8_UNORM_G8R8_SNORM = 0xe7,
        SURFACE_FORMAT_B8X8_UNORM_G8R8_SNORM = 0xe8,
        SURFACE_FORMAT_B8G8R8X8_UNORM = 0xe9,
        SURFACE_FORMAT_B8G8R8X8_UNORM_SRGB = 0xea,
        SURFACE_FORMAT_R8G8B8X8_UNORM = 0xeb,
        SURFACE_FORMAT_R8G8B8X8_UNORM_SRGB = 0xec,
        SURFACE_FORMAT_R9G9B9E5_SHAREDEXP = 0xed,
        SURFACE_FORMAT_B10G10R10X2_UNORM = 0xee,
        SURFACE_FORMAT_L16A16_FLOAT = 0xf0,
        SURFACE_FORMAT_R32_UNORM = 0xf1,
        SURFACE_FORMAT_R32_SNORM = 0xf2,
        SURFACE_FORMAT_R10G10B10X2_USCALED = 0xf3,
        SURFACE_FORMAT_R8G8B8A8_SSCALED = 0xf4,
        SURFACE_FORMAT_R8G8B8A8_USCALED = 0xf5,
        SURFACE_FORMAT_R16G16_SSCALED = 0xf6,
        SURFACE_FORMAT_R16G16_USCALED = 0xf7,
        SURFACE_FORMAT_R32_SSCALED = 0xf8,
        SURFACE_FORMAT_R32_USCALED = 0xf9,
        SURFACE_FORMAT_R8B8G8A8_UNORM = 0xfa,
        SURFACE_FORMAT_R8G8B8A8_SINT_NOA = 0xfb,
        SURFACE_FORMAT_R8G8B8A8_UINT_NOA = 0xfc,
        SURFACE_FORMAT_R8G8B8A8_UNORM_YUV = 0xfd,
        SURFACE_FORMAT_R8G8B8A8_UNORM_SNCK = 0xfe,
        SURFACE_FORMAT_R8G8B8A8_UNORM_NOA = 0xff,
        SURFACE_FORMAT_B5G6R5_UNORM = 0x100,
        SURFACE_FORMAT_B5G6R5_UNORM_SRGB = 0x101,
        SURFACE_FORMAT_B5G5R5A1_UNORM = 0x102,
        SURFACE_FORMAT_B5G5R5A1_UNORM_SRGB = 0x103,
        SURFACE_FORMAT_B4G4R4A4_UNORM = 0x104,
        SURFACE_FORMAT_B4G4R4A4_UNORM_SRGB = 0x105,
        SURFACE_FORMAT_R8G8_UNORM = 0x106,
        SURFACE_FORMAT_R8G8_SNORM = 0x107,
        SURFACE_FORMAT_R8G8_SINT = 0x108,
        SURFACE_FORMAT_R8G8_UINT = 0x109,
        SURFACE_FORMAT_R16_UNORM = 0x10a,
        SURFACE_FORMAT_R16_SNORM = 0x10b,
        SURFACE_FORMAT_R16_SINT = 0x10c,
        SURFACE_FORMAT_R16_UINT = 0x10d,
        SURFACE_FORMAT_R16_FLOAT = 0x10e,
        SURFACE_FORMAT_A8P8_UNORM_PALETTE0 = 0x10f,
        SURFACE_FORMAT_A8P8_UNORM_PALETTE1 = 0x110,
        SURFACE_FORMAT_I16_UNORM = 0x111,
        SURFACE_FORMAT_L16_UNORM = 0x112,
        SURFACE_FORMAT_A16_UNORM = 0x113,
        SURFACE_FORMAT_L8A8_UNORM = 0x114,
        SURFACE_FORMAT_I16_FLOAT = 0x115,
        SURFACE_FORMAT_L16_FLOAT = 0x116,
        SURFACE_FORMAT_A16_FLOAT = 0x117,
        SURFACE_FORMAT_L8A8_UNORM_SRGB = 0x118,
        SURFACE_FORMAT_R5G5_SNORM_B6_UNORM = 0x119,
        SURFACE_FORMAT_B5G5R5X1_UNORM = 0x11a,
        SURFACE_FORMAT_B5G5R5X1_UNORM_SRGB = 0x11b,
        SURFACE_FORMAT_R8G8_SSCALED = 0x11c,
        SURFACE_FORMAT_R8G8_USCALED = 0x11d,
        SURFACE_FORMAT_R16_SSCALED = 0x11e,
        SURFACE_FORMAT_R16_USCALED = 0x11f,
        SURFACE_FORMAT_R8G8_SNORM_DX9 = 0x120,
        SURFACE_FORMAT_R16_FLOAT_DX9 = 0x121,
        SURFACE_FORMAT_P8A8_UNORM_PALETTE0 = 0x122,
        SURFACE_FORMAT_P8A8_UNORM_PALETTE1 = 0x123,
        SURFACE_FORMAT_A1B5G5R5_UNORM = 0x124,
        SURFACE_FORMAT_A4B4G4R4_UNORM = 0x125,
        SURFACE_FORMAT_L8A8_UINT = 0x126,
        SURFACE_FORMAT_L8A8_SINT = 0x127,
        SURFACE_FORMAT_R8_UNORM = 0x140,
        SURFACE_FORMAT_R8_SNORM = 0x141,
        SURFACE_FORMAT_R8_SINT = 0x142,
        SURFACE_FORMAT_R8_UINT = 0x143,
        SURFACE_FORMAT_A8_UNORM = 0x144,
        SURFACE_FORMAT_I8_UNORM = 0x145,
        SURFACE_FORMAT_L8_UNORM = 0x146,
        SURFACE_FORMAT_P4A4_UNORM_PALETTE0 = 0x147,
        SURFACE_FORMAT_A4P4_UNORM_PALETTE0 = 0x148,
        SURFACE_FORMAT_R8_SSCALED = 0x149,
        SURFACE_FORMAT_R8_USCALED = 0x14a,
        SURFACE_FORMAT_P8_UNORM_PALETTE0 = 0x14b,
        SURFACE_FORMAT_L8_UNORM_SRGB = 0x14c,
        SURFACE_FORMAT_P8_UNORM_PALETTE1 = 0x14d,
        SURFACE_FORMAT_P4A4_UNORM_PALETTE1 = 0x14e,
        SURFACE_FORMAT_A4P4_UNORM_PALETTE1 = 0x14f,
        SURFACE_FORMAT_Y8_UNORM = 0x150,
        SURFACE_FORMAT_L8_UINT = 0x152,
        SURFACE_FORMAT_L8_SINT = 0x153,
        SURFACE_FORMAT_I8_UINT = 0x154,
        SURFACE_FORMAT_I8_SINT = 0x155,
        SURFACE_FORMAT_DXT1_RGB_SRGB = 0x180,
        SURFACE_FORMAT_R1_UNORM = 0x181,
        SURFACE_FORMAT_YCRCB_NORMAL = 0x182,
        SURFACE_FORMAT_YCRCB_SWAPUVY = 0x183,
        SURFACE_FORMAT_P2_UNORM_PALETTE0 = 0x184,
        SURFACE_FORMAT_P2_UNORM_PALETTE1 = 0x185,
        SURFACE_FORMAT_BC1_UNORM = 0x186,
        SURFACE_FORMAT_BC2_UNORM = 0x187,
        SURFACE_FORMAT_BC3_UNORM = 0x188,
        SURFACE_FORMAT_BC4_UNORM = 0x189,
        SURFACE_FORMAT_BC5_UNORM = 0x18a,
        SURFACE_FORMAT_BC1_UNORM_SRGB = 0x18b,
        SURFACE_FORMAT_BC2_UNORM_SRGB = 0x18c,
        SURFACE_FORMAT_BC3_UNORM_SRGB = 0x18d,
        SURFACE_FORMAT_MONO8 = 0x18e,
        SURFACE_FORMAT_YCRCB_SWAPUV = 0x18f,
        SURFACE_FORMAT_YCRCB_SWAPY = 0x190,
        SURFACE_FORMAT_DXT1_RGB = 0x191,
        SURFACE_FORMAT_R8G8B8_UNORM = 0x193,
        SURFACE_FORMAT_R8G8B8_SNORM = 0x194,
        SURFACE_FORMAT_R8G8B8_SSCALED = 0x195,
        SURFACE_FORMAT_R8G8B8_USCALED = 0x196,
        SURFACE_FORMAT_R64G64B64A64_FLOAT = 0x197,
        SURFACE_FORMAT_R64G64B64_FLOAT = 0x198,
        SURFACE_FORMAT_BC4_SNORM = 0x199,
        SURFACE_FORMAT_BC5_SNORM = 0x19a,
        SURFACE_FORMAT_R16G16B16_FLOAT = 0x19b,
        SURFACE_FORMAT_R16G16B16_UNORM = 0x19c,
        SURFACE_FORMAT_R16G16B16_SNORM = 0x19d,
        SURFACE_FORMAT_R16G16B16_SSCALED = 0x19e,
        SURFACE_FORMAT_R16G16B16_USCALED = 0x19f,
        SURFACE_FORMAT_R8B8_UNORM = 0x1a0,
        SURFACE_FORMAT_BC6H_SF16 = 0x1a1,
        SURFACE_FORMAT_BC7_UNORM = 0x1a2,
        SURFACE_FORMAT_BC7_UNORM_SRGB = 0x1a3,
        SURFACE_FORMAT_BC6H_UF16 = 0x1a4,
        SURFACE_FORMAT_PLANAR_420_8 = 0x1a5,
        SURFACE_FORMAT_PLANAR_420_16 = 0x1a6,
        SURFACE_FORMAT_R8G8B8_UNORM_SRGB = 0x1a8,
        SURFACE_FORMAT_ETC1_RGB8 = 0x1a9,
        SURFACE_FORMAT_ETC2_RGB8 = 0x1aa,
        SURFACE_FORMAT_EAC_R11 = 0x1ab,
        SURFACE_FORMAT_EAC_RG11 = 0x1ac,
        SURFACE_FORMAT_EAC_SIGNED_R11 = 0x1ad,
        SURFACE_FORMAT_EAC_SIGNED_RG11 = 0x1ae,
        SURFACE_FORMAT_ETC2_SRGB8 = 0x1af,
        SURFACE_FORMAT_R16G16B16_UINT = 0x1b0,
        SURFACE_FORMAT_R16G16B16_SINT = 0x1b1,
        SURFACE_FORMAT_R32_SFIXED = 0x1b2,
        SURFACE_FORMAT_R10G10B10A2_SNORM = 0x1b3,
        SURFACE_FORMAT_R10G10B10A2_USCALED = 0x1b4,
        SURFACE_FORMAT_R10G10B10A2_SSCALED = 0x1b5,
        SURFACE_FORMAT_R10G10B10A2_SINT = 0x1b6,
        SURFACE_FORMAT_B10G10R10A2_SNORM = 0x1b7,
        SURFACE_FORMAT_B10G10R10A2_USCALED = 0x1b8,
        SURFACE_FORMAT_B10G10R10A2_SSCALED = 0x1b9,
        SURFACE_FORMAT_B10G10R10A2_UINT = 0x1ba,
        SURFACE_FORMAT_B10G10R10A2_SINT = 0x1bb,
        SURFACE_FORMAT_R64G64B64A64_PASSTHRU = 0x1bc,
        SURFACE_FORMAT_R64G64B64_PASSTHRU = 0x1bd,
        SURFACE_FORMAT_ETC2_RGB8_PTA = 0x1c0,
        SURFACE_FORMAT_ETC2_SRGB8_PTA = 0x1c1,
        SURFACE_FORMAT_ETC2_EAC_RGBA8 = 0x1c2,
        SURFACE_FORMAT_ETC2_EAC_SRGB8_A8 = 0x1c3,
        SURFACE_FORMAT_R8G8B8_UINT = 0x1c8,
        SURFACE_FORMAT_R8G8B8_SINT = 0x1c9,
        SURFACE_FORMAT_RAW = 0x1ff,
    } SURFACE_FORMAT;
    typedef enum tagAUX_SPECIAL_OPERATIONS_MODE {
        AUX_SPECIAL_OPERATIONS_MODE_FAST_CLEAR_HW_FORMAT_CONVERSION = 0x0,
        AUX_SPECIAL_OPERATIONS_MODE_FAST_CLEAR_BYPASS_HW_FORMAT_CONVERSION = 0x1,
        AUX_SPECIAL_OPERATIONS_MODE_FORCE_UNCOMPRESS = 0x2,
    } AUX_SPECIAL_OPERATIONS_MODE;
    typedef enum tagDESTINATION_SURFACE_TYPE {
        DESTINATION_SURFACE_TYPE_SURFTYPE_1D = 0x0,
        DESTINATION_SURFACE_TYPE_SURFTYPE_2D = 0x1,
        DESTINATION_SURFACE_TYPE_SURFTYPE_3D = 0x2,
        DESTINATION_SURFACE_TYPE_SURFTYPE_CUBE = 0x3,
    } DESTINATION_SURFACE_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.NumberOfMultisamples = NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1;
        TheStructure.Common.SpecialModeOfOperation = SPECIAL_MODE_OF_OPERATION_NONE;
        TheStructure.Common.ColorDepth = COLOR_DEPTH_8_BIT_COLOR;
        TheStructure.Common.InstructionTarget_Opcode = INSTRUCTIONTARGET_OPCODE_OPCODE;
        TheStructure.Common.Client = CLIENT_2D_PROCESSOR;
        TheStructure.Common.DestinationEncryptEn = DESTINATION_ENCRYPT_EN_DISABLE;
        TheStructure.Common.DestinationTiling = DESTINATION_TILING_LINEAR;
        TheStructure.Common.DestinationTargetMemory = DESTINATION_TARGET_MEMORY_LOCAL_MEM;
        TheStructure.Common.DestinationCompressionFormat = DESTINATION_COMPRESSION_FORMAT_CMF_R8;
        TheStructure.Common.AuxSpecialOperationsMode = AUX_SPECIAL_OPERATIONS_MODE_FAST_CLEAR_HW_FORMAT_CONVERSION;
        TheStructure.Common.DestinationSurfaceType = DESTINATION_SURFACE_TYPE_SURFTYPE_1D;
    }
    static tagXY_FAST_COLOR_BLT sInit() {
        XY_FAST_COLOR_BLT state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 16);
        return TheStructure.RawData[index];
    }
    inline void setNumberOfMultisamples(const NUMBER_OF_MULTISAMPLES value) {
        TheStructure.Common.NumberOfMultisamples = value;
    }
    inline NUMBER_OF_MULTISAMPLES getNumberOfMultisamples() const {
        return static_cast<NUMBER_OF_MULTISAMPLES>(TheStructure.Common.NumberOfMultisamples);
    }
    inline void setSpecialModeOfOperation(const SPECIAL_MODE_OF_OPERATION value) {
        TheStructure.Common.SpecialModeOfOperation = value;
    }
    inline SPECIAL_MODE_OF_OPERATION getSpecialModeOfOperation() const {
        return static_cast<SPECIAL_MODE_OF_OPERATION>(TheStructure.Common.SpecialModeOfOperation);
    }
    inline void setColorDepth(const COLOR_DEPTH value) {
        TheStructure.Common.ColorDepth = value;
    }
    inline COLOR_DEPTH getColorDepth() const {
        return static_cast<COLOR_DEPTH>(TheStructure.Common.ColorDepth);
    }
    inline void setInstructionTargetOpcode(const INSTRUCTION_TARGETOPCODE value) {
        TheStructure.Common.InstructionTarget_Opcode = value;
    }
    inline INSTRUCTION_TARGETOPCODE getInstructionTargetOpcode() const {
        return static_cast<INSTRUCTION_TARGETOPCODE>(TheStructure.Common.InstructionTarget_Opcode);
    }
    inline void setClient(const CLIENT value) {
        TheStructure.Common.Client = value;
    }
    inline CLIENT getClient() const {
        return static_cast<CLIENT>(TheStructure.Common.Client);
    }
    inline void setDestinationPitch(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3ffff + 1); // patched
        TheStructure.Common.DestinationPitch = value - 1;
    }
    inline uint32_t getDestinationPitch() const {
        return TheStructure.Common.DestinationPitch + 1;
    }
    inline void setDestinationEncryptEn(const DESTINATION_ENCRYPT_EN value) {
        TheStructure.Common.DestinationEncryptEn = value;
    }
    inline DESTINATION_ENCRYPT_EN getDestinationEncryptEn() const {
        return static_cast<DESTINATION_ENCRYPT_EN>(TheStructure.Common.DestinationEncryptEn);
    }
    inline void setDestinationMOCS(const uint32_t value) { // patched
        UNRECOVERABLE_IF(value > 0x1f);
        setDestinationEncryptEn(static_cast<DESTINATION_ENCRYPT_EN>(value & 1));
        TheStructure.Common.DestinationMocs = value >> 1;
    }
    inline uint32_t getDestinationMOCS() const { // patched
        uint32_t mocs = static_cast<uint32_t>(getDestinationEncryptEn());
        return mocs | (TheStructure.Common.DestinationMocs << 1);
    }
    inline void setDestinationTiling(const DESTINATION_TILING value) {
        TheStructure.Common.DestinationTiling = value;
    }
    inline DESTINATION_TILING getDestinationTiling() const {
        return static_cast<DESTINATION_TILING>(TheStructure.Common.DestinationTiling);
    }
    inline void setDestinationX1CoordinateLeft(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffff);
        TheStructure.Common.DestinationX1Coordinate_Left = value;
    }
    inline uint32_t getDestinationX1CoordinateLeft() const {
        return TheStructure.Common.DestinationX1Coordinate_Left;
    }
    inline void setDestinationY1CoordinateTop(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffff);
        TheStructure.Common.DestinationY1Coordinate_Top = value;
    }
    inline uint32_t getDestinationY1CoordinateTop() const {
        return TheStructure.Common.DestinationY1Coordinate_Top;
    }
    inline void setDestinationX2CoordinateRight(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffff);
        TheStructure.Common.DestinationX2Coordinate_Right = value;
    }
    inline uint32_t getDestinationX2CoordinateRight() const {
        return TheStructure.Common.DestinationX2Coordinate_Right;
    }
    inline void setDestinationY2CoordinateBottom(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffff);
        TheStructure.Common.DestinationY2Coordinate_Bottom = value;
    }
    inline uint32_t getDestinationY2CoordinateBottom() const {
        return TheStructure.Common.DestinationY2Coordinate_Bottom;
    }
    inline void setDestinationBaseAddress(const uint64_t value) {
        TheStructure.Common.DestinationBaseAddress = value;
    }
    inline uint64_t getDestinationBaseAddress() const {
        return TheStructure.Common.DestinationBaseAddress;
    }
    inline void setDestinationXOffset(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3fff);
        TheStructure.Common.DestinationXOffset = value;
    }
    inline uint32_t getDestinationXOffset() const {
        return TheStructure.Common.DestinationXOffset;
    }
    inline void setDestinationYOffset(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3fff);
        TheStructure.Common.DestinationYOffset = value;
    }
    inline uint32_t getDestinationYOffset() const {
        return TheStructure.Common.DestinationYOffset;
    }
    inline void setDestinationTargetMemory(const DESTINATION_TARGET_MEMORY value) {
        TheStructure.Common.DestinationTargetMemory = value;
    }
    inline DESTINATION_TARGET_MEMORY getDestinationTargetMemory() const {
        return static_cast<DESTINATION_TARGET_MEMORY>(TheStructure.Common.DestinationTargetMemory);
    }
    inline void setFillColor(const uint32_t *value) { // patched
        TheStructure.Common.FillColorValue[0] = value[0];
        TheStructure.Common.FillColorValue[1] = value[1];
        TheStructure.Common.FillColorValue[2] = value[2];
        TheStructure.Common.FillColorValue[3] = value[3];
    }
    inline void setDestinationCompressionFormat(const uint32_t value) { // patched
        TheStructure.Common.DestinationCompressionFormat = value;
    }
    inline uint32_t getDestinationCompressionFormat() const { // patched
        return TheStructure.Common.DestinationCompressionFormat;
    }
    inline void setSurfaceFormat(const SURFACE_FORMAT value) {
        TheStructure.Common.SurfaceFormat = value;
    }
    inline SURFACE_FORMAT getSurfaceFormat() const {
        return static_cast<SURFACE_FORMAT>(TheStructure.Common.SurfaceFormat);
    }
    inline void setAuxSpecialOperationsMode(const AUX_SPECIAL_OPERATIONS_MODE value) {
        TheStructure.Common.AuxSpecialOperationsMode = value;
    }
    inline AUX_SPECIAL_OPERATIONS_MODE getAuxSpecialOperationsMode() const {
        return static_cast<AUX_SPECIAL_OPERATIONS_MODE>(TheStructure.Common.AuxSpecialOperationsMode);
    }
    inline void setDestinationSurfaceHeight(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3fff);
        TheStructure.Common.DestinationSurfaceHeight = value - 1;
    }
    inline uint32_t getDestinationSurfaceHeight() const {
        return TheStructure.Common.DestinationSurfaceHeight + 1;
    }
    inline void setDestinationSurfaceWidth(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3fff);
        TheStructure.Common.DestinationSurfaceWidth = value - 1;
    }
    inline uint32_t getDestinationSurfaceWidth() const {
        return TheStructure.Common.DestinationSurfaceWidth + 1;
    }
    inline void setDestinationSurfaceType(const DESTINATION_SURFACE_TYPE value) {
        TheStructure.Common.DestinationSurfaceType = value;
    }
    inline DESTINATION_SURFACE_TYPE getDestinationSurfaceType() const {
        return static_cast<DESTINATION_SURFACE_TYPE>(TheStructure.Common.DestinationSurfaceType);
    }
    inline void setDestinationLod(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xf);
        TheStructure.Common.DestinationLod = value;
    }
    inline uint32_t getDestinationLod() const {
        return TheStructure.Common.DestinationLod;
    }
    inline void setDestinationSurfaceQpitch(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x7fff);
        TheStructure.Common.DestinationSurfaceQpitch = value;
    }
    inline uint32_t getDestinationSurfaceQpitch() const {
        return TheStructure.Common.DestinationSurfaceQpitch;
    }
    inline void setDestinationSurfaceDepth(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x7ff);
        TheStructure.Common.DestinationSurfaceDepth = value - 1;
    }
    inline uint32_t getDestinationSurfaceDepth() const {
        return TheStructure.Common.DestinationSurfaceDepth + 1;
    }
    inline void setDestinationHorizontalAlign(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3);
        TheStructure.Common.DestinationHorizontalAlign = value;
    }
    inline uint32_t getDestinationHorizontalAlign() const {
        return TheStructure.Common.DestinationHorizontalAlign;
    }
    inline void setDestinationVerticalAlign(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3);
        TheStructure.Common.DestinationVerticalAlign = value;
    }
    inline uint32_t getDestinationVerticalAlign() const {
        return TheStructure.Common.DestinationVerticalAlign;
    }
    inline void setDestinationMipTailStartLod(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xf);
        TheStructure.Common.DestinationMipTailStartLod = value;
    }
    inline uint32_t getDestinationMipTailStartLod() const {
        return TheStructure.Common.DestinationMipTailStartLod;
    }
    inline void setDestinationDepthStencilResource(const bool value) {
        TheStructure.Common.DestinationDepthStencilResource = value;
    }
    inline bool getDestinationDepthStencilResource() const {
        return TheStructure.Common.DestinationDepthStencilResource;
    }
    inline void setDestinationArrayIndex(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x7ff);
        TheStructure.Common.DestinationArrayIndex = value - 1;
    }
    inline uint32_t getDestinationArrayIndex() const {
        return TheStructure.Common.DestinationArrayIndex + 1;
    }
} XY_FAST_COLOR_BLT;
STATIC_ASSERT(64 == sizeof(XY_FAST_COLOR_BLT));

typedef struct tagMI_FLUSH_DW {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 5);
            uint32_t Reserved_6 : BITFIELD_RANGE(6, 7);
            uint32_t NotifyEnable : BITFIELD_RANGE(8, 8);
            uint32_t FlushLlc : BITFIELD_RANGE(9, 9);
            uint32_t Reserved_10 : BITFIELD_RANGE(10, 13);
            uint32_t PostSyncOperation : BITFIELD_RANGE(14, 15);
            uint32_t FlushCcs : BITFIELD_RANGE(16, 16);
            uint32_t Reserved_17 : BITFIELD_RANGE(17, 17);
            uint32_t TlbInvalidate : BITFIELD_RANGE(18, 18);
            uint32_t Reserved_19 : BITFIELD_RANGE(19, 20);
            uint32_t StoreDataIndex : BITFIELD_RANGE(21, 21);
            uint32_t Reserved_22 : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1..2
            uint64_t Reserved_32 : BITFIELD_RANGE(0, 1);
            uint64_t DestinationAddressType : BITFIELD_RANGE(2, 2);
            uint64_t DestinationAddress : BITFIELD_RANGE(3, 63);
            // DWORD 3..4
            uint64_t ImmediateData;
        } Common;
        uint32_t RawData[5];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1_ = 0x3,
    } DWORD_LENGTH;
    typedef enum tagPOST_SYNC_OPERATION {
        POST_SYNC_OPERATION_NO_WRITE = 0x0,
        POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD = 0x1,
        POST_SYNC_OPERATION_WRITE_TIMESTAMP_REGISTER = 0x3,
    } POST_SYNC_OPERATION;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_FLUSH_DW = 0x26,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    typedef enum tagDESTINATION_ADDRESS_TYPE {
        DESTINATION_ADDRESS_TYPE_PPGTT = 0x0,
        DESTINATION_ADDRESS_TYPE_GGTT = 0x1,
    } DESTINATION_ADDRESS_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1_;
        TheStructure.Common.PostSyncOperation = POST_SYNC_OPERATION_NO_WRITE;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_FLUSH_DW;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
        TheStructure.Common.DestinationAddressType = DESTINATION_ADDRESS_TYPE_PPGTT;
    }
    static tagMI_FLUSH_DW sInit() {
        MI_FLUSH_DW state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 5);
        return TheStructure.RawData[index];
    }
    inline void setNotifyEnable(const bool value) {
        TheStructure.Common.NotifyEnable = value;
    }
    inline bool getNotifyEnable() const {
        return TheStructure.Common.NotifyEnable;
    }
    inline void setFlushLlc(const bool value) {
        TheStructure.Common.FlushLlc = value;
    }
    inline bool getFlushLlc() const {
        return TheStructure.Common.FlushLlc;
    }
    inline void setPostSyncOperation(const POST_SYNC_OPERATION value) {
        TheStructure.Common.PostSyncOperation = value;
    }
    inline POST_SYNC_OPERATION getPostSyncOperation() const {
        return static_cast<POST_SYNC_OPERATION>(TheStructure.Common.PostSyncOperation);
    }
    inline void setFlushCcs(const bool value) {
        TheStructure.Common.FlushCcs = value;
    }
    inline bool getFlushCcs() const {
        return TheStructure.Common.FlushCcs;
    }
    inline void setTlbInvalidate(const bool value) {
        TheStructure.Common.TlbInvalidate = value;
    }
    inline bool getTlbInvalidate() const {
        return TheStructure.Common.TlbInvalidate;
    }
    inline void setStoreDataIndex(const bool value) {
        TheStructure.Common.StoreDataIndex = value;
    }
    inline bool getStoreDataIndex() const {
        return TheStructure.Common.StoreDataIndex;
    }
    inline void setDestinationAddressType(const DESTINATION_ADDRESS_TYPE value) {
        TheStructure.Common.DestinationAddressType = value;
    }
    inline DESTINATION_ADDRESS_TYPE getDestinationAddressType() const {
        return static_cast<DESTINATION_ADDRESS_TYPE>(TheStructure.Common.DestinationAddressType);
    }
    typedef enum tagDESTINATIONADDRESS {
        DESTINATIONADDRESS_BIT_SHIFT = 0x3,
        DESTINATIONADDRESS_ALIGN_SIZE = 0x8,
    } DESTINATIONADDRESS;
    inline void setDestinationAddress(const uint64_t value) {
        TheStructure.Common.DestinationAddress = value >> DESTINATIONADDRESS_BIT_SHIFT;
    }
    inline uint64_t getDestinationAddress() const {
        return TheStructure.Common.DestinationAddress << DESTINATIONADDRESS_BIT_SHIFT;
    }
    inline void setImmediateData(const uint64_t value) {
        TheStructure.Common.ImmediateData = value;
    }
    inline uint64_t getImmediateData() const {
        return TheStructure.Common.ImmediateData;
    }
} MI_FLUSH_DW;
STATIC_ASSERT(20 == sizeof(MI_FLUSH_DW));

typedef struct tag_3DSTATE_BTD {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 15);
            uint32_t _3DCommandSubOpcode : BITFIELD_RANGE(16, 23);
            uint32_t _3DCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t CommandSubtype : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint32_t DispatchTimeoutCounter : BITFIELD_RANGE(0, 2);
            uint32_t Reserved_35 : BITFIELD_RANGE(3, 6);
            uint32_t ControlsTheMaximumNumberOfOutstandingRayqueriesPerSs : BITFIELD_RANGE(7, 8);
            uint32_t DynamicStackManagementMechanismMissPenalty : BITFIELD_RANGE(9, 11);
            uint32_t DynamicStackManagementMechanismHitReward : BITFIELD_RANGE(12, 14);
            uint32_t DynamicStackManagementMechanismScalingFactor : BITFIELD_RANGE(15, 17);
            uint32_t DynamicStackManagementMechanismReductionCap : BITFIELD_RANGE(18, 20);
            uint32_t Reserved_53 : BITFIELD_RANGE(21, 29);
            uint32_t RtMemStructures64BModeEnable : BITFIELD_RANGE(30, 30);
            uint32_t BtdMidThreadPreemption : BITFIELD_RANGE(31, 31);
            // DWORD 2
            uint64_t PerDssMemoryBackedBufferSize : BITFIELD_RANGE(0, 2);
            uint64_t Reserved_67 : BITFIELD_RANGE(3, 9);
            uint64_t MemoryBackedBufferBasePointer : BITFIELD_RANGE(10, 63);
            // DWORD 4
            uint64_t Reserved_128 : BITFIELD_RANGE(0, 9);
            uint64_t ScratchSpaceBuffer : BITFIELD_RANGE(10, 31);
            // DWORD 5
            uint64_t Reserved_160 : BITFIELD_RANGE(32, 63);
        } Common;
        uint32_t RawData[6];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_DWORD_COUNT_N = 0x4,
    } DWORD_LENGTH;
    typedef enum tag_3D_COMMAND_SUB_OPCODE {
        _3D_COMMAND_SUB_OPCODE_3DSTATE_BTD = 0x6,
    } _3D_COMMAND_SUB_OPCODE;
    typedef enum tag_3D_COMMAND_OPCODE {
        _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED = 0x1,
    } _3D_COMMAND_OPCODE;
    typedef enum tagCOMMAND_SUBTYPE {
        COMMAND_SUBTYPE_GFXPIPE_COMMON = 0x0,
    } COMMAND_SUBTYPE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_GFXPIPE = 0x3,
    } COMMAND_TYPE;
    typedef enum tagDISPATCH_TIMEOUT_COUNTER {
        DISPATCH_TIMEOUT_COUNTER_64 = 0x0,
        DISPATCH_TIMEOUT_COUNTER_128 = 0x1,
        DISPATCH_TIMEOUT_COUNTER_192 = 0x2,
        DISPATCH_TIMEOUT_COUNTER_256 = 0x3,
        DISPATCH_TIMEOUT_COUNTER_512 = 0x4,
        DISPATCH_TIMEOUT_COUNTER_1024 = 0x5,
        DISPATCH_TIMEOUT_COUNTER_2048 = 0x6,
        DISPATCH_TIMEOUT_COUNTER_4096 = 0x7,
    } DISPATCH_TIMEOUT_COUNTER;
    typedef enum tagCONTROLS_THE_MAXIMUM_NUMBER_OF_OUTSTANDING_RAYQUERIES_PER_SS {
        CONTROLS_THE_MAXIMUM_NUMBER_OF_OUTSTANDING_RAYQUERIES_PER_SS_128 = 0x0,
        CONTROLS_THE_MAXIMUM_NUMBER_OF_OUTSTANDING_RAYQUERIES_PER_SS_256 = 0x1,
        CONTROLS_THE_MAXIMUM_NUMBER_OF_OUTSTANDING_RAYQUERIES_PER_SS_512 = 0x2,
        CONTROLS_THE_MAXIMUM_NUMBER_OF_OUTSTANDING_RAYQUERIES_PER_SS_1024 = 0x3,
    } CONTROLS_THE_MAXIMUM_NUMBER_OF_OUTSTANDING_RAYQUERIES_PER_SS;
    typedef enum tagDYNAMIC_STACK_MANAGEMENT_MECHANISM_MISS_PENALTY { // patched
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_MISS_PENALTY_1 = 0x0,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_MISS_PENALTY_2 = 0x1,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_MISS_PENALTY_4 = 0x2,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_MISS_PENALTY_8 = 0x3,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_MISS_PENALTY_16 = 0x4,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_MISS_PENALTY_32 = 0x5,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_MISS_PENALTY_64 = 0x6,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_MISS_PENALTY_128 = 0x7,
    } DYNAMIC_STACK_MANAGEMENT_MECHANISM_MISS_PENALTY;
    typedef enum tagDYNAMIC_STACK_MANAGEMENT_MECHANISM_HIT_REWARD { // patched
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_HIT_REWARD_1 = 0x0,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_HIT_REWARD_2 = 0x1,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_HIT_REWARD_4 = 0x2,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_HIT_REWARD_8 = 0x3,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_HIT_REWARD_16 = 0x4,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_HIT_REWARD_32 = 0x5,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_HIT_REWARD_64 = 0x6,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_HIT_REWARD_128 = 0x7,
    } DYNAMIC_STACK_MANAGEMENT_MECHANISM_HIT_REWARD;
    typedef enum tagDYNAMIC_STACK_MANAGEMENT_MECHANISM_SCALING_FACTOR { // patched
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_SCALING_FACTOR_1 = 0x0,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_SCALING_FACTOR_2 = 0x1,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_SCALING_FACTOR_4 = 0x2,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_SCALING_FACTOR_8 = 0x3,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_SCALING_FACTOR_16 = 0x4,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_SCALING_FACTOR_32 = 0x5,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_SCALING_FACTOR_64 = 0x6,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_SCALING_FACTOR_128 = 0x7,
    } DYNAMIC_STACK_MANAGEMENT_MECHANISM_SCALING_FACTOR;
    typedef enum tagDYNAMIC_STACK_MANAGEMENT_MECHANISM_REDUCTION_CAP { // patched
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_REDUCTION_CAP_128 = 0x0,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_REDUCTION_CAP_256 = 0x1,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_REDUCTION_CAP_384 = 0x2,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_REDUCTION_CAP_512 = 0x3,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_REDUCTION_CAP_640 = 0x4,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_REDUCTION_CAP_768 = 0x5,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_REDUCTION_CAP_896 = 0x6,
        DYNAMIC_STACK_MANAGEMENT_MECHANISM_REDUCTION_CAP_1024 = 0x7,
    } DYNAMIC_STACK_MANAGEMENT_MECHANISM_REDUCTION_CAP;
    typedef enum tagPER_DSS_MEMORY_BACKED_BUFFER_SIZE {
        PER_DSS_MEMORY_BACKED_BUFFER_SIZE_2KB = 0x0,
        PER_DSS_MEMORY_BACKED_BUFFER_SIZE_4KB = 0x1,
        PER_DSS_MEMORY_BACKED_BUFFER_SIZE_8KB = 0x2,
        PER_DSS_MEMORY_BACKED_BUFFER_SIZE_16KB = 0x3,
        PER_DSS_MEMORY_BACKED_BUFFER_SIZE_32KB = 0x4,
        PER_DSS_MEMORY_BACKED_BUFFER_SIZE_64KB = 0x5,
        PER_DSS_MEMORY_BACKED_BUFFER_SIZE_128KB = 0x6,
    } PER_DSS_MEMORY_BACKED_BUFFER_SIZE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_DWORD_COUNT_N;
        TheStructure.Common._3DCommandSubOpcode = _3D_COMMAND_SUB_OPCODE_3DSTATE_BTD;
        TheStructure.Common._3DCommandOpcode = _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED;
        TheStructure.Common.CommandSubtype = COMMAND_SUBTYPE_GFXPIPE_COMMON;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
        TheStructure.Common.DispatchTimeoutCounter = DISPATCH_TIMEOUT_COUNTER_64;
        TheStructure.Common.ControlsTheMaximumNumberOfOutstandingRayqueriesPerSs = CONTROLS_THE_MAXIMUM_NUMBER_OF_OUTSTANDING_RAYQUERIES_PER_SS_128;
        TheStructure.Common.DynamicStackManagementMechanismMissPenalty = DYNAMIC_STACK_MANAGEMENT_MECHANISM_MISS_PENALTY_16;     // patched
        TheStructure.Common.DynamicStackManagementMechanismHitReward = DYNAMIC_STACK_MANAGEMENT_MECHANISM_HIT_REWARD_1;          // patched
        TheStructure.Common.DynamicStackManagementMechanismScalingFactor = DYNAMIC_STACK_MANAGEMENT_MECHANISM_SCALING_FACTOR_16; // patched
        TheStructure.Common.DynamicStackManagementMechanismReductionCap = DYNAMIC_STACK_MANAGEMENT_MECHANISM_REDUCTION_CAP_256;  // patched
        TheStructure.Common.PerDssMemoryBackedBufferSize = PER_DSS_MEMORY_BACKED_BUFFER_SIZE_128KB;
    }
    static tag_3DSTATE_BTD sInit() {
        _3DSTATE_BTD state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 6);
        return TheStructure.RawData[index];
    }
    inline void setDispatchTimeoutCounter(const DISPATCH_TIMEOUT_COUNTER value) {
        TheStructure.Common.DispatchTimeoutCounter = value;
    }
    inline DISPATCH_TIMEOUT_COUNTER getDispatchTimeoutCounter() const {
        return static_cast<DISPATCH_TIMEOUT_COUNTER>(TheStructure.Common.DispatchTimeoutCounter);
    }
    inline void setControlsTheMaximumNumberOfOutstandingRayqueriesPerSs(const CONTROLS_THE_MAXIMUM_NUMBER_OF_OUTSTANDING_RAYQUERIES_PER_SS value) {
        TheStructure.Common.ControlsTheMaximumNumberOfOutstandingRayqueriesPerSs = value;
    }
    inline CONTROLS_THE_MAXIMUM_NUMBER_OF_OUTSTANDING_RAYQUERIES_PER_SS getControlsTheMaximumNumberOfOutstandingRayqueriesPerSs() const {
        return static_cast<CONTROLS_THE_MAXIMUM_NUMBER_OF_OUTSTANDING_RAYQUERIES_PER_SS>(TheStructure.Common.ControlsTheMaximumNumberOfOutstandingRayqueriesPerSs);
    }
    inline void setDynamicStackManagementMechanismMissPenalty(const DYNAMIC_STACK_MANAGEMENT_MECHANISM_MISS_PENALTY value) { // patched
        TheStructure.Common.DynamicStackManagementMechanismMissPenalty = value;
    }
    inline DYNAMIC_STACK_MANAGEMENT_MECHANISM_MISS_PENALTY getDynamicStackManagementMechanismMissPenalty() const { // patched
        return static_cast<DYNAMIC_STACK_MANAGEMENT_MECHANISM_MISS_PENALTY>(TheStructure.Common.DynamicStackManagementMechanismMissPenalty);
    }
    inline void setDynamicStackManagementMechanismHitReward(const DYNAMIC_STACK_MANAGEMENT_MECHANISM_HIT_REWARD value) { // patched
        TheStructure.Common.DynamicStackManagementMechanismHitReward = value;
    }
    inline DYNAMIC_STACK_MANAGEMENT_MECHANISM_HIT_REWARD getDynamicStackManagementMechanismHitReward() const { // patched
        return static_cast<DYNAMIC_STACK_MANAGEMENT_MECHANISM_HIT_REWARD>(TheStructure.Common.DynamicStackManagementMechanismHitReward);
    }
    inline void setDynamicStackManagementMechanismScalingFactor(const DYNAMIC_STACK_MANAGEMENT_MECHANISM_SCALING_FACTOR value) { // patched
        TheStructure.Common.DynamicStackManagementMechanismScalingFactor = value;
    }
    inline DYNAMIC_STACK_MANAGEMENT_MECHANISM_SCALING_FACTOR getDynamicStackManagementMechanismScalingFactor() const { // patched
        return static_cast<DYNAMIC_STACK_MANAGEMENT_MECHANISM_SCALING_FACTOR>(TheStructure.Common.DynamicStackManagementMechanismScalingFactor);
    }
    inline void setDynamicStackManagementMechanismReductionCap(const DYNAMIC_STACK_MANAGEMENT_MECHANISM_REDUCTION_CAP value) { // patched
        TheStructure.Common.DynamicStackManagementMechanismReductionCap = value;
    }
    inline DYNAMIC_STACK_MANAGEMENT_MECHANISM_REDUCTION_CAP getDynamicStackManagementMechanismReductionCap() const { // patched
        return static_cast<DYNAMIC_STACK_MANAGEMENT_MECHANISM_REDUCTION_CAP>(TheStructure.Common.DynamicStackManagementMechanismReductionCap);
    }
    inline void setRtMemStructures64BModeEnable(const bool value) {
        TheStructure.Common.RtMemStructures64BModeEnable = value;
    }
    inline bool getRtMemStructures64BModeEnable() const {
        return TheStructure.Common.RtMemStructures64BModeEnable;
    }
    inline void setBtdMidThreadPreemption(const bool value) {
        TheStructure.Common.BtdMidThreadPreemption = value;
    }
    inline bool getBtdMidThreadPreemption() const {
        return TheStructure.Common.BtdMidThreadPreemption;
    }
    inline void setPerDssMemoryBackedBufferSize(const PER_DSS_MEMORY_BACKED_BUFFER_SIZE value) {
        TheStructure.Common.PerDssMemoryBackedBufferSize = value;
    }
    inline PER_DSS_MEMORY_BACKED_BUFFER_SIZE getPerDssMemoryBackedBufferSize() const {
        return static_cast<PER_DSS_MEMORY_BACKED_BUFFER_SIZE>(TheStructure.Common.PerDssMemoryBackedBufferSize);
    }
    typedef enum tagMEMORYBACKEDBUFFERBASEPOINTER {
        MEMORYBACKEDBUFFERBASEPOINTER_BIT_SHIFT = 0xa,
        MEMORYBACKEDBUFFERBASEPOINTER_ALIGN_SIZE = 0x400,
    } MEMORYBACKEDBUFFERBASEPOINTER;
    inline void setMemoryBackedBufferBasePointer(const uint64_t value) {
        TheStructure.Common.MemoryBackedBufferBasePointer = value >> MEMORYBACKEDBUFFERBASEPOINTER_BIT_SHIFT;
    }
    inline uint64_t getMemoryBackedBufferBasePointer() const {
        return TheStructure.Common.MemoryBackedBufferBasePointer << MEMORYBACKEDBUFFERBASEPOINTER_BIT_SHIFT;
    }
    typedef enum tagSCRATCHSPACEBUFFER {
        SCRATCHSPACEBUFFER_BIT_SHIFT = 0x6,
        SCRATCHSPACEBUFFER_ALIGN_SIZE = 0x40,
    } SCRATCHSPACEBUFFER;
    inline void setScratchSpaceBuffer(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0xfffffff);
        TheStructure.Common.ScratchSpaceBuffer = static_cast<uint32_t>(value) >> SCRATCHSPACEBUFFER_BIT_SHIFT;
    }
    inline uint64_t getScratchSpaceBuffer() const {
        return TheStructure.Common.ScratchSpaceBuffer << SCRATCHSPACEBUFFER_BIT_SHIFT;
    }
} _3DSTATE_BTD;
STATIC_ASSERT(24 == sizeof(_3DSTATE_BTD));
STATIC_ASSERT(NEO::TypeTraits::isPodV<_3DSTATE_BTD>); // patched

typedef struct tagGRF {
    union tagTheStructure {
        float fRegs[16];
        uint32_t dwRegs[16];
        uint16_t wRegs[32];
        uint32_t RawData[16];
    } TheStructure;
} GRF;
STATIC_ASSERT(64 == sizeof(GRF));

typedef struct tagPOSTSYNC_DATA {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t Operation : BITFIELD_RANGE(0, 1);
            uint32_t DataportPipelineFlush : BITFIELD_RANGE(2, 2);
            uint32_t Reserved_3 : BITFIELD_RANGE(3, 3);
            uint32_t MocsEncryptedData : BITFIELD_RANGE(4, 4);
            uint32_t MocsIndexToMocsTables : BITFIELD_RANGE(5, 10);
            uint32_t SystemMemoryFenceRequest : BITFIELD_RANGE(11, 11);
            uint32_t DataportSubsliceCacheFlush : BITFIELD_RANGE(12, 12);
            uint32_t Reserved_13 : BITFIELD_RANGE(13, 31);
            // DWORD 1
            uint64_t DestinationAddress;
            // DWORD 3
            uint64_t ImmediateData;
        } Common;
        uint32_t RawData[5];
    } TheStructure;
    typedef enum tagOPERATION {
        OPERATION_NO_WRITE = 0x0,
        OPERATION_WRITE_IMMEDIATE_DATA = 0x1,
        OPERATION_WRITE_TIMESTAMP = 0x3,
    } OPERATION;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.Operation = OPERATION_NO_WRITE;
    }
    static tagPOSTSYNC_DATA sInit() {
        POSTSYNC_DATA state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 5);
        return TheStructure.RawData[index];
    }
    inline void setOperation(const OPERATION value) {
        TheStructure.Common.Operation = value;
    }
    inline OPERATION getOperation() const {
        return static_cast<OPERATION>(TheStructure.Common.Operation);
    }
    inline void setDataportPipelineFlush(const bool value) {
        TheStructure.Common.DataportPipelineFlush = value;
    }
    inline bool getDataportPipelineFlush() const {
        return TheStructure.Common.DataportPipelineFlush;
    }
    inline void setMocsEncryptedData(const bool value) {
        TheStructure.Common.MocsEncryptedData = value;
    }
    inline bool getMocsEncryptedData() const {
        return TheStructure.Common.MocsEncryptedData;
    }
    inline void setMocs(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x7f);
        setMocsEncryptedData(value & 1);
        TheStructure.Common.MocsIndexToMocsTables = value >> 1;
    }
    inline uint32_t getMocs() const {
        uint32_t mocs = getMocsEncryptedData();
        return mocs | TheStructure.Common.MocsIndexToMocsTables << 1;
    }
    inline void setSystemMemoryFenceRequest(const bool value) {
        TheStructure.Common.SystemMemoryFenceRequest = value;
    }
    inline bool getSystemMemoryFenceRequest() const {
        return TheStructure.Common.SystemMemoryFenceRequest;
    }
    inline void setDataportSubsliceCacheFlush(const bool value) {
        TheStructure.Common.DataportSubsliceCacheFlush = value;
    }
    inline bool getDataportSubsliceCacheFlush() const {
        return TheStructure.Common.DataportSubsliceCacheFlush;
    }
    inline void setDestinationAddress(const uint64_t value) {
        TheStructure.Common.DestinationAddress = value;
    }
    inline uint64_t getDestinationAddress() const {
        return TheStructure.Common.DestinationAddress;
    }
    inline void setImmediateData(const uint64_t value) {
        TheStructure.Common.ImmediateData = value;
    }
    inline uint64_t getImmediateData() const {
        return TheStructure.Common.ImmediateData;
    }
} POSTSYNC_DATA;
STATIC_ASSERT(20 == sizeof(POSTSYNC_DATA));

typedef struct tagINTERFACE_DESCRIPTOR_DATA {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint64_t Reserved_0 : BITFIELD_RANGE(0, 5);
            uint64_t KernelStartPointer : BITFIELD_RANGE(6, 31);
            // DWORD 1
            uint64_t Reserved_32 : BITFIELD_RANGE(32, 63);
            // DWORD 2
            uint32_t EuThreadSchedulingModeOverride : BITFIELD_RANGE(0, 1);
            uint32_t Reserved_66 : BITFIELD_RANGE(2, 6);
            uint32_t SoftwareExceptionEnable : BITFIELD_RANGE(7, 7);
            uint32_t Reserved_72 : BITFIELD_RANGE(8, 12);
            uint32_t IllegalOpcodeExceptionEnable : BITFIELD_RANGE(13, 13);
            uint32_t Reserved_78 : BITFIELD_RANGE(14, 15);
            uint32_t FloatingPointMode : BITFIELD_RANGE(16, 16);
            uint32_t Reserved_81 : BITFIELD_RANGE(17, 17);
            uint32_t SingleProgramFlow : BITFIELD_RANGE(18, 18);
            uint32_t DenormMode : BITFIELD_RANGE(19, 19);
            uint32_t ThreadPreemption : BITFIELD_RANGE(20, 20);
            uint32_t Reserved_85 : BITFIELD_RANGE(21, 25);
            uint32_t RegistersPerThread : BITFIELD_RANGE(26, 30);
            uint32_t Reserved_95 : BITFIELD_RANGE(31, 31);
            // DWORD 3
            uint32_t Reserved_96 : BITFIELD_RANGE(0, 1);
            uint32_t SamplerCount : BITFIELD_RANGE(2, 4);
            uint32_t SamplerStatePointer : BITFIELD_RANGE(5, 31);
            // DWORD 4
            uint32_t BindingTableEntryCount : BITFIELD_RANGE(0, 4);
            uint32_t BindingTablePointer : BITFIELD_RANGE(5, 20);
            uint32_t Reserved_149 : BITFIELD_RANGE(21, 31);
            // DWORD 5
            uint32_t NumberOfThreadsInGpgpuThreadGroup : BITFIELD_RANGE(0, 9);
            uint32_t Reserved_170 : BITFIELD_RANGE(10, 12);
            uint32_t ThreadGroupForwardProgressGuarantee : BITFIELD_RANGE(13, 13);
            uint32_t Reserved_174 : BITFIELD_RANGE(14, 15);
            uint32_t SharedLocalMemorySize : BITFIELD_RANGE(16, 20);
            uint32_t Reserved_181 : BITFIELD_RANGE(21, 21);
            uint32_t RoundingMode : BITFIELD_RANGE(22, 23);
            uint32_t Reserved_184 : BITFIELD_RANGE(24, 25);
            uint32_t ThreadGroupDispatchSize : BITFIELD_RANGE(26, 27);
            uint32_t NumberOfBarriers : BITFIELD_RANGE(28, 30);
            uint32_t BtdMode : BITFIELD_RANGE(31, 31);
            // DWORD 6
            uint32_t Reserved_192 : BITFIELD_RANGE(0, 7);
            uint32_t ZPassAsyncComputeThreadLimit : BITFIELD_RANGE(8, 10);
            uint32_t Reserved_203 : BITFIELD_RANGE(11, 11);
            uint32_t NpZAsyncThrottleSettings : BITFIELD_RANGE(12, 13);
            uint32_t Reserved_206 : BITFIELD_RANGE(14, 15);
            uint32_t PsAsyncThreadLimit : BITFIELD_RANGE(16, 18);
            uint32_t Reserved_211 : BITFIELD_RANGE(19, 31);
            // DWORD 7
            uint32_t PreferredSlmAllocationSize : BITFIELD_RANGE(0, 3);
            uint32_t Reserved_228 : BITFIELD_RANGE(4, 31);
        } Common;
        uint32_t RawData[8];
    } TheStructure;
    typedef enum tagEU_THREAD_SCHEDULING_MODE_OVERRIDE {
        EU_THREAD_SCHEDULING_MODE_OVERRIDE_HW_DEFAULT = 0x0,
        EU_THREAD_SCHEDULING_MODE_OVERRIDE_OLDEST_FIRST = 0x1,
        EU_THREAD_SCHEDULING_MODE_OVERRIDE_ROUND_ROBIN = 0x2,
        EU_THREAD_SCHEDULING_MODE_OVERRIDE_STALL_BASED_ROUND_ROBIN = 0x3,
    } EU_THREAD_SCHEDULING_MODE_OVERRIDE;
    typedef enum tagFLOATING_POINT_MODE {
        FLOATING_POINT_MODE_IEEE_754 = 0x0,
        FLOATING_POINT_MODE_ALTERNATE = 0x1,
    } FLOATING_POINT_MODE;
    typedef enum tagSINGLE_PROGRAM_FLOW {
        SINGLE_PROGRAM_FLOW_MULTIPLE = 0x0,
        SINGLE_PROGRAM_FLOW_SINGLE = 0x1,
    } SINGLE_PROGRAM_FLOW;
    typedef enum tagDENORM_MODE {
        DENORM_MODE_FTZ = 0x0,
        DENORM_MODE_SETBYKERNEL = 0x1,
    } DENORM_MODE;
    typedef enum tagREGISTERS_PER_THREAD {
        REGISTERS_PER_THREAD_REGISTERS_32 = 0x0,
        REGISTERS_PER_THREAD_REGISTERS_64 = 0x1,
        REGISTERS_PER_THREAD_REGISTERS_96 = 0x2,
        REGISTERS_PER_THREAD_REGISTERS_128 = 0x3,
        REGISTERS_PER_THREAD_REGISTERS_160 = 0x4,
        REGISTERS_PER_THREAD_REGISTERS_192 = 0x5,
        REGISTERS_PER_THREAD_REGISTERS_256 = 0x7,
        REGISTERS_PER_THREAD_REGISTERS_512 = 0xf, // patched
    } REGISTERS_PER_THREAD;
    typedef enum tagSAMPLER_COUNT {
        SAMPLER_COUNT_NO_SAMPLERS_USED = 0x0,
        SAMPLER_COUNT_BETWEEN_1_AND_4_SAMPLERS_USED = 0x1,
        SAMPLER_COUNT_BETWEEN_5_AND_8_SAMPLERS_USED = 0x2,
        SAMPLER_COUNT_BETWEEN_9_AND_12_SAMPLERS_USED = 0x3,
        SAMPLER_COUNT_BETWEEN_13_AND_16_SAMPLERS_USED = 0x4,
    } SAMPLER_COUNT;
    typedef enum tagBINDING_TABLE_ENTRY_COUNT {
        BINDING_TABLE_ENTRY_COUNT_PREFETCH_DISABLED = 0x0,
        BINDING_TABLE_ENTRY_COUNT_PREFETCH_COUNT_MIN = 0x1,
        BINDING_TABLE_ENTRY_COUNT_PREFETCH_COUNT_MAX = 0x1f,
    } BINDING_TABLE_ENTRY_COUNT;
    typedef enum tagSHARED_LOCAL_MEMORY_SIZE {
        SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_0K = 0x0,
        SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_1K = 0x1,
        SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_2K = 0x2,
        SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_4K = 0x3,
        SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_8K = 0x4,
        SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_16K = 0x5,
        SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_32K = 0x6,
        SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_64K = 0x7,
        SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_24K = 0x8,
        SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_48K = 0x9,
        SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_96K = 0xa,
        SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_128K = 0xb,
        SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_192K = 0xc,
        SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_256K = 0xd,
        SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_384K = 0xe,
    } SHARED_LOCAL_MEMORY_SIZE;
    typedef enum tagROUNDING_MODE {
        ROUNDING_MODE_RTNE = 0x0,
        ROUNDING_MODE_RU = 0x1,
        ROUNDING_MODE_RD = 0x2,
        ROUNDING_MODE_RTZ = 0x3,
    } ROUNDING_MODE;
    typedef enum tagTHREAD_GROUP_DISPATCH_SIZE {
        THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_8 = 0x0,
        THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_4 = 0x1,
        THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_2 = 0x2,
        THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1 = 0x3,
    } THREAD_GROUP_DISPATCH_SIZE;
    typedef enum tagNUMBER_OF_BARRIERS {
        NUMBER_OF_BARRIERS_NONE = 0x0,
        NUMBER_OF_BARRIERS_B1 = 0x1,
        NUMBER_OF_BARRIERS_B2 = 0x2,
        NUMBER_OF_BARRIERS_B4 = 0x3,
        NUMBER_OF_BARRIERS_B8 = 0x4,
        NUMBER_OF_BARRIERS_B16 = 0x5,
        NUMBER_OF_BARRIERS_B24 = 0x6,
        NUMBER_OF_BARRIERS_B32 = 0x7,
    } NUMBER_OF_BARRIERS;
    typedef enum tagBTD_MODE {
        BTD_MODE_DISABLE = 0x0,
        BTD_MODE_ENABLE = 0x1,
    } BTD_MODE;
    typedef enum tagZ_PASS_ASYNC_COMPUTE_THREAD_LIMIT {
        Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60 = 0x0,
        Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_64 = 0x1,
        Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_56 = 0x2,
        Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_48 = 0x3,
        Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_40 = 0x4,
        Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_32 = 0x5,
        Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_24 = 0x6,
        Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_16 = 0x7,
    } Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT;
    typedef enum tagNP_Z_ASYNC_THROTTLE_SETTINGS {
        NP_Z_ASYNC_THROTTLE_SETTINGS_MAX_32 = 0x1,
        NP_Z_ASYNC_THROTTLE_SETTINGS_MAX_40 = 0x2,
        NP_Z_ASYNC_THROTTLE_SETTINGS_MAX_48 = 0x3,
    } NP_Z_ASYNC_THROTTLE_SETTINGS;
    typedef enum tagPS_ASYNC_THREAD_LIMIT {
        PS_ASYNC_THREAD_LIMIT_DISABLED = 0x0,
        PS_ASYNC_THREAD_LIMIT_MAX_2 = 0x1,
        PS_ASYNC_THREAD_LIMIT_MAX_8 = 0x2,
        PS_ASYNC_THREAD_LIMIT_MAX_16 = 0x3,
        PS_ASYNC_THREAD_LIMIT_MAX_24 = 0x4,
        PS_ASYNC_THREAD_LIMIT_MAX_32 = 0x5,
        PS_ASYNC_THREAD_LIMIT_MAX_40 = 0x6,
        PS_ASYNC_THREAD_LIMIT_MAX_48 = 0x7,
    } PS_ASYNC_THREAD_LIMIT;
    typedef enum tagPREFERRED_SLM_ALLOCATION_SIZE {
        PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_0K = 0x0,
        PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_16K = 0x1,
        PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_32K = 0x2,
        PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_64K = 0x3,
        PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_96K = 0x4,
        PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_128K = 0x5,
        PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_160K = 0x6,
        PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_192K = 0x7,
        PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_224K = 0x8,
        PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_256K = 0x9,
        PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_384K = 0xa,
    } PREFERRED_SLM_ALLOCATION_SIZE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.EuThreadSchedulingModeOverride = EU_THREAD_SCHEDULING_MODE_OVERRIDE_STALL_BASED_ROUND_ROBIN; // patched
        TheStructure.Common.FloatingPointMode = FLOATING_POINT_MODE_IEEE_754;
        TheStructure.Common.SingleProgramFlow = SINGLE_PROGRAM_FLOW_MULTIPLE;
        TheStructure.Common.DenormMode = DENORM_MODE_FTZ;
        TheStructure.Common.RegistersPerThread = REGISTERS_PER_THREAD_REGISTERS_128;
        TheStructure.Common.SamplerCount = SAMPLER_COUNT_NO_SAMPLERS_USED;
        TheStructure.Common.BindingTableEntryCount = BINDING_TABLE_ENTRY_COUNT_PREFETCH_DISABLED;
        TheStructure.Common.SharedLocalMemorySize = SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_0K;
        TheStructure.Common.RoundingMode = ROUNDING_MODE_RTNE;
        TheStructure.Common.ThreadGroupDispatchSize = THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_8;
        TheStructure.Common.NumberOfBarriers = NUMBER_OF_BARRIERS_NONE;
        TheStructure.Common.BtdMode = BTD_MODE_DISABLE;
        TheStructure.Common.ZPassAsyncComputeThreadLimit = Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60;
        TheStructure.Common.PsAsyncThreadLimit = PS_ASYNC_THREAD_LIMIT_DISABLED;
        TheStructure.Common.PreferredSlmAllocationSize = PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_0K;
    }
    static tagINTERFACE_DESCRIPTOR_DATA sInit() {
        INTERFACE_DESCRIPTOR_DATA state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 8);
        return TheStructure.RawData[index];
    }
    typedef enum tagKERNELSTARTPOINTER {
        KERNELSTARTPOINTER_BIT_SHIFT = 0x6,
        KERNELSTARTPOINTER_ALIGN_SIZE = 0x40,
    } KERNELSTARTPOINTER;
    inline void setKernelStartPointer(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0xffffffff);
        TheStructure.Common.KernelStartPointer = static_cast<uint32_t>(value) >> KERNELSTARTPOINTER_BIT_SHIFT;
    }
    inline uint64_t getKernelStartPointer() const {
        return static_cast<uint64_t>(TheStructure.Common.KernelStartPointer) << KERNELSTARTPOINTER_BIT_SHIFT; // patched
    }
    inline void setEuThreadSchedulingModeOverride(const EU_THREAD_SCHEDULING_MODE_OVERRIDE value) {
        TheStructure.Common.EuThreadSchedulingModeOverride = value;
    }
    inline EU_THREAD_SCHEDULING_MODE_OVERRIDE getEuThreadSchedulingModeOverride() const {
        return static_cast<EU_THREAD_SCHEDULING_MODE_OVERRIDE>(TheStructure.Common.EuThreadSchedulingModeOverride);
    }
    inline void setSoftwareExceptionEnable(const bool value) {
        TheStructure.Common.SoftwareExceptionEnable = value;
    }
    inline bool getSoftwareExceptionEnable() const {
        return TheStructure.Common.SoftwareExceptionEnable;
    }
    inline void setIllegalOpcodeExceptionEnable(const bool value) {
        TheStructure.Common.IllegalOpcodeExceptionEnable = value;
    }
    inline bool getIllegalOpcodeExceptionEnable() const {
        return TheStructure.Common.IllegalOpcodeExceptionEnable;
    }
    inline void setFloatingPointMode(const FLOATING_POINT_MODE value) {
        TheStructure.Common.FloatingPointMode = value;
    }
    inline FLOATING_POINT_MODE getFloatingPointMode() const {
        return static_cast<FLOATING_POINT_MODE>(TheStructure.Common.FloatingPointMode);
    }
    inline void setSingleProgramFlow(const SINGLE_PROGRAM_FLOW value) {
        TheStructure.Common.SingleProgramFlow = value;
    }
    inline SINGLE_PROGRAM_FLOW getSingleProgramFlow() const {
        return static_cast<SINGLE_PROGRAM_FLOW>(TheStructure.Common.SingleProgramFlow);
    }
    inline void setDenormMode(const DENORM_MODE value) {
        TheStructure.Common.DenormMode = value;
    }
    inline DENORM_MODE getDenormMode() const {
        return static_cast<DENORM_MODE>(TheStructure.Common.DenormMode);
    }
    inline void setThreadPreemption(const bool value) {
        TheStructure.Common.ThreadPreemption = value;
    }
    inline bool getThreadPreemption() const {
        return TheStructure.Common.ThreadPreemption;
    }
    inline void setRegistersPerThread(const REGISTERS_PER_THREAD value) {
        TheStructure.Common.RegistersPerThread = value;
    }
    inline REGISTERS_PER_THREAD getRegistersPerThread() const {
        return static_cast<REGISTERS_PER_THREAD>(TheStructure.Common.RegistersPerThread);
    }
    inline void setSamplerCount(const SAMPLER_COUNT value) {
        TheStructure.Common.SamplerCount = value;
    }
    inline SAMPLER_COUNT getSamplerCount() const {
        return static_cast<SAMPLER_COUNT>(TheStructure.Common.SamplerCount);
    }
    typedef enum tagSAMPLERSTATEPOINTER {
        SAMPLERSTATEPOINTER_BIT_SHIFT = 0x5,
        SAMPLERSTATEPOINTER_ALIGN_SIZE = 0x20,
    } SAMPLERSTATEPOINTER;
    inline void setSamplerStatePointer(const uint32_t value) {
        TheStructure.Common.SamplerStatePointer = static_cast<uint32_t>(value) >> SAMPLERSTATEPOINTER_BIT_SHIFT;
    }
    inline uint32_t getSamplerStatePointer() const {
        return TheStructure.Common.SamplerStatePointer << SAMPLERSTATEPOINTER_BIT_SHIFT;
    }
    inline void setBindingTableEntryCount(const uint32_t value) { // patched
        TheStructure.Common.BindingTableEntryCount = value;
    }
    inline uint32_t getBindingTableEntryCount() const { // patched
        return TheStructure.Common.BindingTableEntryCount;
    }
    typedef enum tagBINDINGTABLEPOINTER {
        BINDINGTABLEPOINTER_BIT_SHIFT = 0x5,
        BINDINGTABLEPOINTER_ALIGN_SIZE = 0x20,
    } BINDINGTABLEPOINTER;
    inline void setBindingTablePointer(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x1fffff);
        TheStructure.Common.BindingTablePointer = static_cast<uint32_t>(value) >> BINDINGTABLEPOINTER_BIT_SHIFT;
    }
    inline uint32_t getBindingTablePointer() const {
        return TheStructure.Common.BindingTablePointer << BINDINGTABLEPOINTER_BIT_SHIFT;
    }
    inline void setNumberOfThreadsInGpgpuThreadGroup(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3ff);
        TheStructure.Common.NumberOfThreadsInGpgpuThreadGroup = value;
    }
    inline uint32_t getNumberOfThreadsInGpgpuThreadGroup() const {
        return TheStructure.Common.NumberOfThreadsInGpgpuThreadGroup;
    }
    inline void setThreadGroupForwardProgressGuarantee(const bool value) {
        TheStructure.Common.ThreadGroupForwardProgressGuarantee = value;
    }
    inline bool getThreadGroupForwardProgressGuarantee() const {
        return TheStructure.Common.ThreadGroupForwardProgressGuarantee;
    }
    inline void setSharedLocalMemorySize(const uint32_t value) { // patched
        TheStructure.Common.SharedLocalMemorySize = value;
    }
    inline uint32_t getSharedLocalMemorySize() const { // patched
        return static_cast<uint32_t>(TheStructure.Common.SharedLocalMemorySize);
    }
    inline void setRoundingMode(const ROUNDING_MODE value) {
        TheStructure.Common.RoundingMode = value;
    }
    inline ROUNDING_MODE getRoundingMode() const {
        return static_cast<ROUNDING_MODE>(TheStructure.Common.RoundingMode);
    }
    inline void setThreadGroupDispatchSize(const THREAD_GROUP_DISPATCH_SIZE value) {
        TheStructure.Common.ThreadGroupDispatchSize = value;
    }
    inline THREAD_GROUP_DISPATCH_SIZE getThreadGroupDispatchSize() const {
        return static_cast<THREAD_GROUP_DISPATCH_SIZE>(TheStructure.Common.ThreadGroupDispatchSize);
    }
    inline void setNumberOfBarriers(const NUMBER_OF_BARRIERS value) {
        TheStructure.Common.NumberOfBarriers = value;
    }
    inline NUMBER_OF_BARRIERS getNumberOfBarriers() const {
        return static_cast<NUMBER_OF_BARRIERS>(TheStructure.Common.NumberOfBarriers);
    }
    inline void setBtdMode(const BTD_MODE value) {
        TheStructure.Common.BtdMode = value;
    }
    inline BTD_MODE getBtdMode() const {
        return static_cast<BTD_MODE>(TheStructure.Common.BtdMode);
    }
    inline void setZPassAsyncComputeThreadLimit(const Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT value) {
        TheStructure.Common.ZPassAsyncComputeThreadLimit = value;
    }
    inline Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT getZPassAsyncComputeThreadLimit() const {
        return static_cast<Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT>(TheStructure.Common.ZPassAsyncComputeThreadLimit);
    }
    inline void setNpZAsyncThrottleSettings(const NP_Z_ASYNC_THROTTLE_SETTINGS value) {
        TheStructure.Common.NpZAsyncThrottleSettings = value;
    }
    inline NP_Z_ASYNC_THROTTLE_SETTINGS getNpZAsyncThrottleSettings() const {
        return static_cast<NP_Z_ASYNC_THROTTLE_SETTINGS>(TheStructure.Common.NpZAsyncThrottleSettings);
    }
    inline void setPsAsyncThreadLimit(const PS_ASYNC_THREAD_LIMIT value) {
        TheStructure.Common.PsAsyncThreadLimit = value;
    }
    inline PS_ASYNC_THREAD_LIMIT getPsAsyncThreadLimit() const {
        return static_cast<PS_ASYNC_THREAD_LIMIT>(TheStructure.Common.PsAsyncThreadLimit);
    }
    inline void setPreferredSlmAllocationSize(const PREFERRED_SLM_ALLOCATION_SIZE value) {
        TheStructure.Common.PreferredSlmAllocationSize = value;
    }
    inline PREFERRED_SLM_ALLOCATION_SIZE getPreferredSlmAllocationSize() const {
        return static_cast<PREFERRED_SLM_ALLOCATION_SIZE>(TheStructure.Common.PreferredSlmAllocationSize);
    }
} INTERFACE_DESCRIPTOR_DATA;
STATIC_ASSERT(32 == sizeof(INTERFACE_DESCRIPTOR_DATA));

typedef struct tagCOMPUTE_WALKER {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t PredicateEnable : BITFIELD_RANGE(8, 8);
            uint32_t WorkloadPartitionEnable : BITFIELD_RANGE(9, 9);
            uint32_t IndirectParameterEnable : BITFIELD_RANGE(10, 10);
            uint32_t Reserved_11 : BITFIELD_RANGE(11, 12);
            uint32_t DispatchComplete : BITFIELD_RANGE(13, 13);
            uint32_t Reserved_14 : BITFIELD_RANGE(14, 14);
            uint32_t CfeSubopcodeVariant : BITFIELD_RANGE(15, 17);
            uint32_t CfeSubopcode : BITFIELD_RANGE(18, 23);
            uint32_t ComputeCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t Pipeline : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint32_t Reserved_32 : BITFIELD_RANGE(0, 7);
            uint32_t DebugObjectId : BITFIELD_RANGE(8, 31);
            // DWORD 2
            uint32_t IndirectDataLength : BITFIELD_RANGE(0, 16);
            uint32_t L3PrefetchDisable : BITFIELD_RANGE(17, 17);
            uint32_t PartitionDispatchParameter : BITFIELD_RANGE(18, 29);
            uint32_t PartitionType : BITFIELD_RANGE(30, 31);
            // DWORD 3
            uint32_t Reserved_96 : BITFIELD_RANGE(0, 5);
            uint32_t IndirectDataStartAddress : BITFIELD_RANGE(6, 31);
            // DWORD 4
            uint32_t ComputeDispatchAllWalkerEnable : BITFIELD_RANGE(0, 0);
            uint32_t Reserved_129 : BITFIELD_RANGE(1, 2);
            uint32_t ThreadGroupBatchSize : BITFIELD_RANGE(3, 4);
            uint32_t DispatchWalkOrder : BITFIELD_RANGE(5, 6);
            uint32_t Reserved_135 : BITFIELD_RANGE(7, 16);
            uint32_t MessageSimd : BITFIELD_RANGE(17, 18);
            uint32_t TileLayout : BITFIELD_RANGE(19, 21);
            uint32_t WalkOrder : BITFIELD_RANGE(22, 24);
            uint32_t EmitInlineParameter : BITFIELD_RANGE(25, 25);
            uint32_t EmitLocal : BITFIELD_RANGE(26, 28);
            uint32_t GenerateLocalId : BITFIELD_RANGE(29, 29);
            uint32_t SimdSize : BITFIELD_RANGE(30, 31);
            // DWORD 5
            uint32_t ExecutionMask;
            // DWORD 6
            uint32_t LocalXMaximum : BITFIELD_RANGE(0, 9);
            uint32_t LocalYMaximum : BITFIELD_RANGE(10, 19);
            uint32_t LocalZMaximum : BITFIELD_RANGE(20, 29);
            uint32_t Reserved_222 : BITFIELD_RANGE(30, 31);
            // DWORD 7
            uint32_t ThreadGroupIdXDimension;
            // DWORD 8
            uint32_t ThreadGroupIdYDimension;
            // DWORD 9
            uint32_t ThreadGroupIdZDimension;
            // DWORD 10
            uint32_t ThreadGroupIdStartingX;
            // DWORD 11
            uint32_t ThreadGroupIdStartingY;
            // DWORD 12
            uint32_t ThreadGroupIdStartingZ;
            // DWORD 13
            uint64_t PartitionId : BITFIELD_RANGE(0, 31);
            // DWORD 14
            uint64_t PartitionSize : BITFIELD_RANGE(32, 63);
            // DWORD 15
            uint32_t PreemptX;
            // DWORD 16
            uint32_t PreemptY;
            // DWORD 17
            uint32_t PreemptZ;
            // DWORD 18
            uint32_t WalkerId : BITFIELD_RANGE(0, 3);
            uint32_t Reserved_580 : BITFIELD_RANGE(4, 7);
            uint32_t OverDispatchTgCount : BITFIELD_RANGE(8, 23);
            uint32_t Reserved_600 : BITFIELD_RANGE(24, 31);
            // DWORD 19
            INTERFACE_DESCRIPTOR_DATA InterfaceDescriptor;
            // DWORD 27
            POSTSYNC_DATA PostSync;
            // DWORD 32
            uint32_t InlineData[8];
        } Common;
        uint32_t RawData[40];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_FIXED_SIZE = 0x26,
    } DWORD_LENGTH;
    typedef enum tagCFE_SUBOPCODE_VARIANT {
        CFE_SUBOPCODE_VARIANT_STANDARD = 0x0,
        CFE_SUBOPCODE_VARIANT_PASS1_RESUME = 0x1,
        CFE_SUBOPCODE_VARIANT_PASS2_RESUME = 0x2,
        CFE_SUBOPCODE_VARIANT_BTD_PASS2 = 0x3,
        CFE_SUBOPCODE_VARIANT_PASS1_PASS2_RESUME = 0x4,
        CFE_SUBOPCODE_VARIANT_TG_RESUME = 0x5,
        CFE_SUBOPCODE_VARIANT_W_DONE = 0x7,
    } CFE_SUBOPCODE_VARIANT;
    typedef enum tagCFE_SUBOPCODE {
        CFE_SUBOPCODE_COMPUTE_WALKER = 0x2,
    } CFE_SUBOPCODE;
    typedef enum tagCOMPUTE_COMMAND_OPCODE {
        COMPUTE_COMMAND_OPCODE_NEW_CFE_COMMAND = 0x2,
    } COMPUTE_COMMAND_OPCODE;
    typedef enum tagPIPELINE {
        PIPELINE_COMPUTE = 0x2,
    } PIPELINE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_GFXPIPE = 0x3,
    } COMMAND_TYPE;
    typedef enum tagPARTITION_TYPE {
        PARTITION_TYPE_DISABLED = 0x0,
        PARTITION_TYPE_X = 0x1,
        PARTITION_TYPE_Y = 0x2,
        PARTITION_TYPE_Z = 0x3,
    } PARTITION_TYPE;
    typedef enum tagTHREAD_GROUP_BATCH_SIZE {
        THREAD_GROUP_BATCH_SIZE_TG_BATCH_1 = 0x0,
        THREAD_GROUP_BATCH_SIZE_TG_BATCH_2 = 0x1,
        THREAD_GROUP_BATCH_SIZE_TG_BATCH_4 = 0x2,
        THREAD_GROUP_BATCH_SIZE_TG_BATCH_8 = 0x3,
    } THREAD_GROUP_BATCH_SIZE;
    typedef enum tagDISPATCH_WALK_ORDER {
        DISPATCH_WALK_ORDER_LINEAR_WALK = 0x0,
        DISPATCH_WALK_ORDER_Y_ORDER_WALK = 0x1,
        DISPATCH_WALK_ORDER_MORTON_WALK = 0x2,
    } DISPATCH_WALK_ORDER;
    typedef enum tagMESSAGE_SIMD {
        MESSAGE_SIMD_SIMT16 = 0x1,
        MESSAGE_SIMD_SIMT32 = 0x2,
    } MESSAGE_SIMD;
    typedef enum tagTILE_LAYOUT {
        TILE_LAYOUT_LINEAR = 0x0,
        TILE_LAYOUT_TILEY_32BPE = 0x1,
        TILE_LAYOUT_TILEY_64BPE = 0x2,
        TILE_LAYOUT_TILEY_128BPE = 0x3,
    } TILE_LAYOUT;
    typedef enum tagWALK_ORDER {
        WALK_ORDER_WALK_012 = 0x0,
        WALK_ORDER_WALK_021 = 0x1,
        WALK_ORDER_WALK_102 = 0x2,
        WALK_ORDER_WALK_120 = 0x3,
        WALK_ORDER_WALK_201 = 0x4,
        WALK_ORDER_WALK_210 = 0x5,
    } WALK_ORDER;
    typedef enum tagEMIT_LOCAL {
        EMIT_LOCAL_EMIT_NONE = 0x0,
        EMIT_LOCAL_EMIT_X = 0x1,
        EMIT_LOCAL_EMIT_XY = 0x3,
        EMIT_LOCAL_EMIT_XYZ = 0x7,
    } EMIT_LOCAL;
    typedef enum tagSIMD_SIZE {
        SIMD_SIZE_SIMT16 = 0x1,
        SIMD_SIZE_SIMT32 = 0x2,
    } SIMD_SIZE;
    typedef enum tagPARTITION_ID {
        PARTITION_ID_SUPPORTED_MIN = 0x0,
        PARTITION_ID_SUPPORTED_MAX = 0xf,
    } PARTITION_ID;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_FIXED_SIZE;
        TheStructure.Common.CfeSubopcodeVariant = CFE_SUBOPCODE_VARIANT_STANDARD;
        TheStructure.Common.CfeSubopcode = CFE_SUBOPCODE_COMPUTE_WALKER;
        TheStructure.Common.ComputeCommandOpcode = COMPUTE_COMMAND_OPCODE_NEW_CFE_COMMAND;
        TheStructure.Common.Pipeline = PIPELINE_COMPUTE;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
        TheStructure.Common.PartitionType = PARTITION_TYPE_DISABLED;
        TheStructure.Common.ThreadGroupBatchSize = THREAD_GROUP_BATCH_SIZE_TG_BATCH_1;
        TheStructure.Common.DispatchWalkOrder = DISPATCH_WALK_ORDER_LINEAR_WALK;
        TheStructure.Common.TileLayout = TILE_LAYOUT_LINEAR;
        TheStructure.Common.WalkOrder = WALK_ORDER_WALK_012;
        TheStructure.Common.EmitLocal = EMIT_LOCAL_EMIT_NONE;
        TheStructure.Common.InterfaceDescriptor.init();
        TheStructure.Common.PostSync.init();
        for (uint32_t i = 0; i < 8; i++) {
            TheStructure.Common.InlineData[i] = 0;
        }
    }
    static tagCOMPUTE_WALKER sInit() {
        COMPUTE_WALKER state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 40);
        return TheStructure.RawData[index];
    }
    inline void setDwordLength(const DWORD_LENGTH value) {
        TheStructure.Common.DwordLength = value;
    }
    inline DWORD_LENGTH getDwordLength() const {
        return static_cast<DWORD_LENGTH>(TheStructure.Common.DwordLength);
    }
    inline void setPredicateEnable(const bool value) {
        TheStructure.Common.PredicateEnable = value;
    }
    inline bool getPredicateEnable() const {
        return TheStructure.Common.PredicateEnable;
    }
    inline void setWorkloadPartitionEnable(const bool value) {
        TheStructure.Common.WorkloadPartitionEnable = value;
    }
    inline bool getWorkloadPartitionEnable() const {
        return TheStructure.Common.WorkloadPartitionEnable;
    }
    inline void setIndirectParameterEnable(const bool value) {
        TheStructure.Common.IndirectParameterEnable = value;
    }
    inline bool getIndirectParameterEnable() const {
        return TheStructure.Common.IndirectParameterEnable;
    }
    inline void setDispatchComplete(const bool value) {
        TheStructure.Common.DispatchComplete = value;
    }
    inline bool getDispatchComplete() const {
        return TheStructure.Common.DispatchComplete;
    }
    inline void setCfeSubopcodeVariant(const CFE_SUBOPCODE_VARIANT value) {
        TheStructure.Common.CfeSubopcodeVariant = value;
    }
    inline CFE_SUBOPCODE_VARIANT getCfeSubopcodeVariant() const {
        return static_cast<CFE_SUBOPCODE_VARIANT>(TheStructure.Common.CfeSubopcodeVariant);
    }
    inline void setCfeSubopcode(const CFE_SUBOPCODE value) {
        TheStructure.Common.CfeSubopcode = value;
    }
    inline CFE_SUBOPCODE getCfeSubopcode() const {
        return static_cast<CFE_SUBOPCODE>(TheStructure.Common.CfeSubopcode);
    }
    inline void setComputeCommandOpcode(const COMPUTE_COMMAND_OPCODE value) {
        TheStructure.Common.ComputeCommandOpcode = value;
    }
    inline COMPUTE_COMMAND_OPCODE getComputeCommandOpcode() const {
        return static_cast<COMPUTE_COMMAND_OPCODE>(TheStructure.Common.ComputeCommandOpcode);
    }
    inline void setDebugObjectId(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffffff);
        TheStructure.Common.DebugObjectId = value;
    }
    inline uint32_t getDebugObjectId() const {
        return TheStructure.Common.DebugObjectId;
    }
    inline void setIndirectDataLength(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x1ffff);
        TheStructure.Common.IndirectDataLength = value;
    }
    inline uint32_t getIndirectDataLength() const {
        return TheStructure.Common.IndirectDataLength;
    }
    inline void setL3PrefetchDisable(const bool value) {
        TheStructure.Common.L3PrefetchDisable = value;
    }
    inline bool getL3PrefetchDisable() const {
        return TheStructure.Common.L3PrefetchDisable;
    }
    inline void setPartitionDispatchParameter(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xfff);
        TheStructure.Common.PartitionDispatchParameter = value;
    }
    inline uint32_t getPartitionDispatchParameter() const {
        return TheStructure.Common.PartitionDispatchParameter;
    }
    inline void setPartitionType(const PARTITION_TYPE value) {
        TheStructure.Common.PartitionType = value;
    }
    inline PARTITION_TYPE getPartitionType() const {
        return static_cast<PARTITION_TYPE>(TheStructure.Common.PartitionType);
    }
    typedef enum tagINDIRECTDATASTARTADDRESS {
        INDIRECTDATASTARTADDRESS_BIT_SHIFT = 0x6,
        INDIRECTDATASTARTADDRESS_ALIGN_SIZE = 0x40,
    } INDIRECTDATASTARTADDRESS;
    inline void setIndirectDataStartAddress(const uint32_t value) {
        TheStructure.Common.IndirectDataStartAddress = value >> INDIRECTDATASTARTADDRESS_BIT_SHIFT;
    }
    inline uint32_t getIndirectDataStartAddress() const {
        return TheStructure.Common.IndirectDataStartAddress << INDIRECTDATASTARTADDRESS_BIT_SHIFT;
    }
    inline void setComputeDispatchAllWalkerEnable(const bool value) {
        TheStructure.Common.ComputeDispatchAllWalkerEnable = value;
    }
    inline bool getComputeDispatchAllWalkerEnable() const {
        return TheStructure.Common.ComputeDispatchAllWalkerEnable;
    }
    inline void setThreadGroupBatchSize(const THREAD_GROUP_BATCH_SIZE value) {
        TheStructure.Common.ThreadGroupBatchSize = value;
    }
    inline THREAD_GROUP_BATCH_SIZE getThreadGroupBatchSize() const {
        return static_cast<THREAD_GROUP_BATCH_SIZE>(TheStructure.Common.ThreadGroupBatchSize);
    }
    inline void setDispatchWalkOrder(const DISPATCH_WALK_ORDER value) {
        TheStructure.Common.DispatchWalkOrder = value;
    }
    inline DISPATCH_WALK_ORDER getDispatchWalkOrder() const {
        return static_cast<DISPATCH_WALK_ORDER>(TheStructure.Common.DispatchWalkOrder);
    }
    inline void setMessageSimd(const uint32_t value) { // patched
        TheStructure.Common.MessageSimd = value;
    }
    inline uint32_t getMessageSimd() const { // patched
        return TheStructure.Common.MessageSimd;
    }
    inline void setTileLayout(const TILE_LAYOUT value) {
        TheStructure.Common.TileLayout = value;
    }
    inline TILE_LAYOUT getTileLayout() const {
        return static_cast<TILE_LAYOUT>(TheStructure.Common.TileLayout);
    }
    inline void setWalkOrder(const uint32_t value) { // patched
        TheStructure.Common.WalkOrder = value;
    }
    inline uint32_t getWalkOrder() const { // patched
        return TheStructure.Common.WalkOrder;
    }
    inline void setEmitInlineParameter(const bool value) {
        TheStructure.Common.EmitInlineParameter = value;
    }
    inline bool getEmitInlineParameter() const {
        return TheStructure.Common.EmitInlineParameter;
    }
    inline void setEmitLocalId(const uint32_t value) { // patched
        TheStructure.Common.EmitLocal = value;
    }
    inline uint32_t getEmitLocalId() const { // patched
        return TheStructure.Common.EmitLocal;
    }
    inline void setGenerateLocalId(const bool value) {
        TheStructure.Common.GenerateLocalId = value;
    }
    inline bool getGenerateLocalId() const {
        return TheStructure.Common.GenerateLocalId;
    }
    inline void setSimdSize(const SIMD_SIZE value) {
        TheStructure.Common.SimdSize = value;
    }
    inline SIMD_SIZE getSimdSize() const {
        return static_cast<SIMD_SIZE>(TheStructure.Common.SimdSize);
    }
    inline void setExecutionMask(const uint32_t value) {
        TheStructure.Common.ExecutionMask = value;
    }
    inline uint32_t getExecutionMask() const {
        return TheStructure.Common.ExecutionMask;
    }
    inline void setLocalXMaximum(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3ff);
        TheStructure.Common.LocalXMaximum = value;
    }
    inline uint32_t getLocalXMaximum() const {
        return TheStructure.Common.LocalXMaximum;
    }
    inline void setLocalYMaximum(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3ff);
        TheStructure.Common.LocalYMaximum = value;
    }
    inline uint32_t getLocalYMaximum() const {
        return TheStructure.Common.LocalYMaximum;
    }
    inline void setLocalZMaximum(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3ff);
        TheStructure.Common.LocalZMaximum = value;
    }
    inline uint32_t getLocalZMaximum() const {
        return TheStructure.Common.LocalZMaximum;
    }
    inline void setThreadGroupIdXDimension(const uint32_t value) {
        TheStructure.Common.ThreadGroupIdXDimension = value;
    }
    inline uint32_t getThreadGroupIdXDimension() const {
        return TheStructure.Common.ThreadGroupIdXDimension;
    }
    inline void setThreadGroupIdYDimension(const uint32_t value) {
        TheStructure.Common.ThreadGroupIdYDimension = value;
    }
    inline uint32_t getThreadGroupIdYDimension() const {
        return TheStructure.Common.ThreadGroupIdYDimension;
    }
    inline void setThreadGroupIdZDimension(const uint32_t value) {
        TheStructure.Common.ThreadGroupIdZDimension = value;
    }
    inline uint32_t getThreadGroupIdZDimension() const {
        return TheStructure.Common.ThreadGroupIdZDimension;
    }
    inline void setThreadGroupIdStartingX(const uint32_t value) {
        TheStructure.Common.ThreadGroupIdStartingX = value;
    }
    inline uint32_t getThreadGroupIdStartingX() const {
        return TheStructure.Common.ThreadGroupIdStartingX;
    }
    inline void setThreadGroupIdStartingY(const uint32_t value) {
        TheStructure.Common.ThreadGroupIdStartingY = value;
    }
    inline uint32_t getThreadGroupIdStartingY() const {
        return TheStructure.Common.ThreadGroupIdStartingY;
    }
    inline void setThreadGroupIdStartingZ(const uint32_t value) {
        TheStructure.Common.ThreadGroupIdStartingZ = value;
    }
    inline uint32_t getThreadGroupIdStartingZ() const {
        return TheStructure.Common.ThreadGroupIdStartingZ;
    }
    inline void setPartitionId(const uint64_t value) {
        TheStructure.Common.PartitionId = value;
    }
    inline uint64_t getPartitionId() const {
        return TheStructure.Common.PartitionId;
    }
    inline void setPartitionSize(const uint64_t value) {
        TheStructure.Common.PartitionSize = value;
    }
    inline uint64_t getPartitionSize() const {
        return TheStructure.Common.PartitionSize;
    }
    inline void setPreemptX(const uint32_t value) {
        TheStructure.Common.PreemptX = value;
    }
    inline uint32_t getPreemptX() const {
        return TheStructure.Common.PreemptX;
    }
    inline void setPreemptY(const uint32_t value) {
        TheStructure.Common.PreemptY = value;
    }
    inline uint32_t getPreemptY() const {
        return TheStructure.Common.PreemptY;
    }
    inline void setPreemptZ(const uint32_t value) {
        TheStructure.Common.PreemptZ = value;
    }
    inline uint32_t getPreemptZ() const {
        return TheStructure.Common.PreemptZ;
    }
    inline void setWalkerId(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xf);
        TheStructure.Common.WalkerId = value;
    }
    inline uint32_t getWalkerId() const {
        return TheStructure.Common.WalkerId;
    }
    inline void setOverDispatchTgCount(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffff);
        TheStructure.Common.OverDispatchTgCount = value;
    }
    inline uint32_t getOverDispatchTgCount() const {
        return TheStructure.Common.OverDispatchTgCount;
    }
    inline void setInterfaceDescriptor(const INTERFACE_DESCRIPTOR_DATA &value) {
        TheStructure.Common.InterfaceDescriptor = value;
    }
    inline INTERFACE_DESCRIPTOR_DATA &getInterfaceDescriptor() {
        return TheStructure.Common.InterfaceDescriptor;
    }
    inline void setPostSync(const POSTSYNC_DATA &value) {
        TheStructure.Common.PostSync = value;
    }
    inline POSTSYNC_DATA &getPostSync() {
        return TheStructure.Common.PostSync;
    }
    inline uint32_t *getInlineDataPointer() {
        return reinterpret_cast<uint32_t *>(&TheStructure.Common.InlineData);
    }
    static constexpr uint32_t getInlineDataSize() {
        return sizeof(TheStructure.Common.InlineData);
    }
    using InterfaceDescriptorType = std::decay_t<decltype(TheStructure.Common.InterfaceDescriptor)>; // patched
} COMPUTE_WALKER;
STATIC_ASSERT(160 == sizeof(COMPUTE_WALKER));

typedef struct tagCFE_STATE {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 15);
            uint32_t CfeSubopcodeVariant : BITFIELD_RANGE(16, 17);
            uint32_t CfeSubopcode : BITFIELD_RANGE(18, 23);
            uint32_t ComputeCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t Pipeline : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint64_t Reserved_32 : BITFIELD_RANGE(0, 9);
            uint64_t ScratchSpaceBuffer : BITFIELD_RANGE(10, 31);
            // DWORD 2
            uint64_t Reserved_64 : BITFIELD_RANGE(32, 63);
            // DWORD 3
            uint32_t StackIdControl : BITFIELD_RANGE(0, 1);
            uint32_t DynamicStackIdControl : BITFIELD_RANGE(2, 2);
            uint32_t Reserved_99 : BITFIELD_RANGE(3, 10);
            uint32_t ComputeOverdispatchDisable : BITFIELD_RANGE(11, 11);
            uint32_t ComputeDispatchAllWalkerEnable : BITFIELD_RANGE(12, 12);
            uint32_t SingleSliceDispatchCcsMode : BITFIELD_RANGE(13, 13);
            uint32_t OverDispatchControl : BITFIELD_RANGE(14, 15);
            uint32_t MaximumNumberOfThreads : BITFIELD_RANGE(16, 31);
            // DWORD 4
            uint32_t Reserved_128;
            // DWORD 5
            uint32_t ResumeIndicatorDebugkey : BITFIELD_RANGE(0, 0);
            uint32_t WalkerNumberDebugkey : BITFIELD_RANGE(1, 10);
            uint32_t Reserved_171 : BITFIELD_RANGE(11, 31);
        } Common;
        uint32_t RawData[6];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_DWORD_COUNT_N = 0x4,
    } DWORD_LENGTH;
    typedef enum tagCFE_SUBOPCODE_VARIANT {
        CFE_SUBOPCODE_VARIANT_STANDARD = 0x0,
    } CFE_SUBOPCODE_VARIANT;
    typedef enum tagCFE_SUBOPCODE {
        CFE_SUBOPCODE_CFE_STATE = 0x0,
    } CFE_SUBOPCODE;
    typedef enum tagCOMPUTE_COMMAND_OPCODE {
        COMPUTE_COMMAND_OPCODE_NEW_CFE_COMMAND = 0x2,
    } COMPUTE_COMMAND_OPCODE;
    typedef enum tagPIPELINE {
        PIPELINE_COMPUTE = 0x2,
    } PIPELINE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_GFXPIPE = 0x3,
    } COMMAND_TYPE;
    typedef enum tagSTACK_ID_CONTROL {
        STACK_ID_CONTROL_2K = 0x0,
        STACK_ID_CONTROL_1K = 0x1,
        STACK_ID_CONTROL_512 = 0x2,
        STACK_ID_CONTROL_256 = 0x3,
    } STACK_ID_CONTROL;
    typedef enum tagDYNAMIC_STACK_ID_CONTROL {
        DYNAMIC_STACK_ID_CONTROL_DISABLED = 0x0,
        DYNAMIC_STACK_ID_CONTROL_ENABLED = 0x1,
    } DYNAMIC_STACK_ID_CONTROL;
    typedef enum tagOVER_DISPATCH_CONTROL {
        OVER_DISPATCH_CONTROL_NONE = 0x0,
        OVER_DISPATCH_CONTROL_LOW = 0x1,
        OVER_DISPATCH_CONTROL_NORMAL = 0x2,
        OVER_DISPATCH_CONTROL_HIGH = 0x3,
    } OVER_DISPATCH_CONTROL;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_DWORD_COUNT_N;
        TheStructure.Common.CfeSubopcodeVariant = CFE_SUBOPCODE_VARIANT_STANDARD;
        TheStructure.Common.CfeSubopcode = CFE_SUBOPCODE_CFE_STATE;
        TheStructure.Common.ComputeCommandOpcode = COMPUTE_COMMAND_OPCODE_NEW_CFE_COMMAND;
        TheStructure.Common.Pipeline = PIPELINE_COMPUTE;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
        TheStructure.Common.StackIdControl = STACK_ID_CONTROL_2K;
        TheStructure.Common.DynamicStackIdControl = DYNAMIC_STACK_ID_CONTROL_ENABLED;
        TheStructure.Common.OverDispatchControl = OVER_DISPATCH_CONTROL_NORMAL;
    }
    static tagCFE_STATE sInit() {
        CFE_STATE state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 6);
        return TheStructure.RawData[index];
    }
    inline void setCfeSubopcodeVariant(const CFE_SUBOPCODE_VARIANT value) {
        TheStructure.Common.CfeSubopcodeVariant = value;
    }
    inline CFE_SUBOPCODE_VARIANT getCfeSubopcodeVariant() const {
        return static_cast<CFE_SUBOPCODE_VARIANT>(TheStructure.Common.CfeSubopcodeVariant);
    }
    inline void setCfeSubopcode(const CFE_SUBOPCODE value) {
        TheStructure.Common.CfeSubopcode = value;
    }
    inline CFE_SUBOPCODE getCfeSubopcode() const {
        return static_cast<CFE_SUBOPCODE>(TheStructure.Common.CfeSubopcode);
    }
    inline void setComputeCommandOpcode(const COMPUTE_COMMAND_OPCODE value) {
        TheStructure.Common.ComputeCommandOpcode = value;
    }
    inline COMPUTE_COMMAND_OPCODE getComputeCommandOpcode() const {
        return static_cast<COMPUTE_COMMAND_OPCODE>(TheStructure.Common.ComputeCommandOpcode);
    }
    typedef enum tagSCRATCHSPACEBUFFER {
        SCRATCHSPACEBUFFER_BIT_SHIFT = 0x6,
        SCRATCHSPACEBUFFER_ALIGN_SIZE = 0x40,
    } SCRATCHSPACEBUFFER;
    inline void setScratchSpaceBuffer(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0xfffffff);
        TheStructure.Common.ScratchSpaceBuffer = static_cast<uint32_t>(value) >> SCRATCHSPACEBUFFER_BIT_SHIFT;
    }
    inline uint64_t getScratchSpaceBuffer() const {
        return TheStructure.Common.ScratchSpaceBuffer << SCRATCHSPACEBUFFER_BIT_SHIFT;
    }
    inline void setStackIdControl(const STACK_ID_CONTROL value) {
        TheStructure.Common.StackIdControl = value;
    }
    inline STACK_ID_CONTROL getStackIdControl() const {
        return static_cast<STACK_ID_CONTROL>(TheStructure.Common.StackIdControl);
    }
    inline void setDynamicStackIdControl(const DYNAMIC_STACK_ID_CONTROL value) {
        TheStructure.Common.DynamicStackIdControl = value;
    }
    inline DYNAMIC_STACK_ID_CONTROL getDynamicStackIdControl() const {
        return static_cast<DYNAMIC_STACK_ID_CONTROL>(TheStructure.Common.DynamicStackIdControl);
    }
    inline void setComputeOverdispatchDisable(const bool value) {
        TheStructure.Common.ComputeOverdispatchDisable = value;
    }
    inline bool getComputeOverdispatchDisable() const {
        return TheStructure.Common.ComputeOverdispatchDisable;
    }
    inline void setComputeDispatchAllWalkerEnable(const bool value) {
        TheStructure.Common.ComputeDispatchAllWalkerEnable = value;
    }
    inline bool getComputeDispatchAllWalkerEnable() const {
        return TheStructure.Common.ComputeDispatchAllWalkerEnable;
    }
    inline void setSingleSliceDispatchCcsMode(const bool value) {
        TheStructure.Common.SingleSliceDispatchCcsMode = value;
    }
    inline bool getSingleSliceDispatchCcsMode() const {
        return TheStructure.Common.SingleSliceDispatchCcsMode;
    }
    inline void setOverDispatchControl(const OVER_DISPATCH_CONTROL value) {
        TheStructure.Common.OverDispatchControl = value;
    }
    inline OVER_DISPATCH_CONTROL getOverDispatchControl() const {
        return static_cast<OVER_DISPATCH_CONTROL>(TheStructure.Common.OverDispatchControl);
    }
    inline void setMaximumNumberOfThreads(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffff);
        TheStructure.Common.MaximumNumberOfThreads = value;
    }
    inline uint32_t getMaximumNumberOfThreads() const {
        return TheStructure.Common.MaximumNumberOfThreads;
    }
    inline void setResumeIndicatorDebugkey(const bool value) {
        TheStructure.Common.ResumeIndicatorDebugkey = value;
    }
    inline bool getResumeIndicatorDebugkey() const {
        return TheStructure.Common.ResumeIndicatorDebugkey;
    }
    inline void setWalkerNumberDebugkey(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x3ff);
        TheStructure.Common.WalkerNumberDebugkey = value;
    }
    inline uint32_t getWalkerNumberDebugkey() const {
        return TheStructure.Common.WalkerNumberDebugkey;
    }
} CFE_STATE;
STATIC_ASSERT(24 == sizeof(CFE_STATE));

typedef struct tagMI_ARB_CHECK {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t PreParserDisable : BITFIELD_RANGE(0, 0);
            uint32_t Reserved_1 : BITFIELD_RANGE(1, 7);
            uint32_t MaskBits : BITFIELD_RANGE(8, 15);
            uint32_t Reserved_16 : BITFIELD_RANGE(16, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
        } Common;
        uint32_t RawData[1];
    } TheStructure;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_ARB_CHECK = 0x5,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_ARB_CHECK;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_ARB_CHECK sInit() {
        MI_ARB_CHECK state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 1);
        return TheStructure.RawData[index];
    }
    inline void setPreParserDisable(const bool value) { // patched
        TheStructure.Common.MaskBits |= 0x1;            // PreParserDisable is at bit0, so set bit0 of mask to 1
        TheStructure.Common.PreParserDisable = value;
    }
    inline bool getPreParserDisable() const {
        return TheStructure.Common.PreParserDisable;
    }
    inline void setMaskBits(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xff);
        TheStructure.Common.MaskBits = value;
    }
    inline uint32_t getMaskBits() const {
        return TheStructure.Common.MaskBits;
    }
} MI_ARB_CHECK;
STATIC_ASSERT(4 == sizeof(MI_ARB_CHECK));

typedef struct tagMI_BATCH_BUFFER_START {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t AddressSpaceIndicator : BITFIELD_RANGE(8, 8);
            uint32_t Reserved_9 : BITFIELD_RANGE(9, 14);
            uint32_t PredicationEnable : BITFIELD_RANGE(15, 15);
            uint32_t Reserved_16 : BITFIELD_RANGE(16, 17);
            uint32_t IndirectAddressEnable : BITFIELD_RANGE(18, 18);
            uint32_t EnableCommandCache : BITFIELD_RANGE(19, 19);
            uint32_t Reserved_20 : BITFIELD_RANGE(20, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint64_t Reserved_32 : BITFIELD_RANGE(0, 1);
            uint64_t BatchBufferStartAddress : BITFIELD_RANGE(2, 47);
            uint64_t Reserved_80 : BITFIELD_RANGE(48, 63);
        } Common;
        struct tagMi_Mode_Nestedbatchbufferenableis0 {
            // DWORD 0
            uint32_t Reserved_0 : BITFIELD_RANGE(0, 21);
            uint32_t SecondLevelBatchBuffer : BITFIELD_RANGE(22, 22);
            uint32_t Reserved_23 : BITFIELD_RANGE(23, 31);
            // DWORD 1
            uint64_t Reserved_32 : BITFIELD_RANGE(0, 47);
            uint64_t Reserved_80 : BITFIELD_RANGE(48, 63);
        } Mi_Mode_Nestedbatchbufferenableis0;
        struct tagMi_Mode_Nestedbatchbufferenableis1 {
            // DWORD 0
            uint32_t Reserved_0 : BITFIELD_RANGE(0, 21);
            uint32_t NestedLevelBatchBuffer : BITFIELD_RANGE(22, 22);
            uint32_t Reserved_23 : BITFIELD_RANGE(23, 31);
            // DWORD 1
            uint64_t Reserved_32 : BITFIELD_RANGE(0, 47);
            uint64_t Reserved_80 : BITFIELD_RANGE(48, 63);
        } Mi_Mode_Nestedbatchbufferenableis1;
        uint32_t RawData[3];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x1,
    } DWORD_LENGTH;
    typedef enum tagADDRESS_SPACE_INDICATOR {
        ADDRESS_SPACE_INDICATOR_GGTT = 0x0,
        ADDRESS_SPACE_INDICATOR_PPGTT = 0x1,
    } ADDRESS_SPACE_INDICATOR;
    typedef enum tagNESTED_LEVEL_BATCH_BUFFER {
        NESTED_LEVEL_BATCH_BUFFER_CHAIN = 0x0,
        NESTED_LEVEL_BATCH_BUFFER_NESTED = 0x1,
    } NESTED_LEVEL_BATCH_BUFFER;
    typedef enum tagSECOND_LEVEL_BATCH_BUFFER {
        SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH = 0x0,
        SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH = 0x1,
    } SECOND_LEVEL_BATCH_BUFFER;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_BATCH_BUFFER_START = 0x31,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.AddressSpaceIndicator = ADDRESS_SPACE_INDICATOR_GGTT;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_BATCH_BUFFER_START;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
        TheStructure.Mi_Mode_Nestedbatchbufferenableis0.SecondLevelBatchBuffer = SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH;
        TheStructure.Mi_Mode_Nestedbatchbufferenableis1.NestedLevelBatchBuffer = NESTED_LEVEL_BATCH_BUFFER_CHAIN;
    }
    static tagMI_BATCH_BUFFER_START sInit() {
        MI_BATCH_BUFFER_START state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 3);
        return TheStructure.RawData[index];
    }
    inline void setAddressSpaceIndicator(const ADDRESS_SPACE_INDICATOR value) {
        TheStructure.Common.AddressSpaceIndicator = value;
    }
    inline ADDRESS_SPACE_INDICATOR getAddressSpaceIndicator() const {
        return static_cast<ADDRESS_SPACE_INDICATOR>(TheStructure.Common.AddressSpaceIndicator);
    }
    inline void setPredicationEnable(const bool value) {
        TheStructure.Common.PredicationEnable = value;
    }
    inline bool getPredicationEnable() const {
        return TheStructure.Common.PredicationEnable;
    }
    inline void setIndirectAddressEnable(const bool value) {
        TheStructure.Common.IndirectAddressEnable = value;
    }
    inline bool getIndirectAddressEnable() const {
        return TheStructure.Common.IndirectAddressEnable;
    }
    inline void setEnableCommandCache(const bool value) {
        TheStructure.Common.EnableCommandCache = value;
    }
    inline bool getEnableCommandCache() const {
        return TheStructure.Common.EnableCommandCache;
    }
    typedef enum tagBATCHBUFFERSTARTADDRESS {
        BATCHBUFFERSTARTADDRESS_BIT_SHIFT = 0x2,
        BATCHBUFFERSTARTADDRESS_ALIGN_SIZE = 0x4,
    } BATCHBUFFERSTARTADDRESS;
    inline void setBatchBufferStartAddress(const uint64_t value) {
        TheStructure.Common.BatchBufferStartAddress = value >> BATCHBUFFERSTARTADDRESS_BIT_SHIFT;
    }
    inline uint64_t getBatchBufferStartAddress() const {
        return TheStructure.Common.BatchBufferStartAddress << BATCHBUFFERSTARTADDRESS_BIT_SHIFT;
    }
    inline void setSecondLevelBatchBuffer(const SECOND_LEVEL_BATCH_BUFFER value) {
        TheStructure.Mi_Mode_Nestedbatchbufferenableis0.SecondLevelBatchBuffer = value;
    }
    inline SECOND_LEVEL_BATCH_BUFFER getSecondLevelBatchBuffer() const {
        return static_cast<SECOND_LEVEL_BATCH_BUFFER>(TheStructure.Mi_Mode_Nestedbatchbufferenableis0.SecondLevelBatchBuffer);
    }
    inline void setNestedLevelBatchBuffer(const NESTED_LEVEL_BATCH_BUFFER value) {
        TheStructure.Mi_Mode_Nestedbatchbufferenableis1.NestedLevelBatchBuffer = value;
    }
    inline NESTED_LEVEL_BATCH_BUFFER getNestedLevelBatchBuffer() const {
        return static_cast<NESTED_LEVEL_BATCH_BUFFER>(TheStructure.Mi_Mode_Nestedbatchbufferenableis1.NestedLevelBatchBuffer);
    }
} MI_BATCH_BUFFER_START;
STATIC_ASSERT(12 == sizeof(MI_BATCH_BUFFER_START));

typedef struct tagMI_LOAD_REGISTER_MEM {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 15);
            uint32_t WorkloadPartitionIdOffsetEnable : BITFIELD_RANGE(16, 16);
            uint32_t MmioRemapEnable : BITFIELD_RANGE(17, 17);
            uint32_t Reserved_18 : BITFIELD_RANGE(18, 18);
            uint32_t AddCsMmioStartOffset : BITFIELD_RANGE(19, 19);
            uint32_t Reserved_20 : BITFIELD_RANGE(20, 20);
            uint32_t AsyncModeEnable : BITFIELD_RANGE(21, 21);
            uint32_t UseGlobalGtt : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint32_t Reserved_32 : BITFIELD_RANGE(0, 1);
            uint32_t RegisterAddress : BITFIELD_RANGE(2, 22);
            uint32_t Reserved_55 : BITFIELD_RANGE(23, 31);
            // DWORD 2
            uint64_t Reserved_64 : BITFIELD_RANGE(0, 1);
            uint64_t MemoryAddress : BITFIELD_RANGE(2, 63);
        } Common;
        uint32_t RawData[4];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x2,
    } DWORD_LENGTH;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_LOAD_REGISTER_MEM = 0x29,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_LOAD_REGISTER_MEM;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_LOAD_REGISTER_MEM sInit() {
        MI_LOAD_REGISTER_MEM state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 4);
        return TheStructure.RawData[index];
    }
    inline void setWorkloadPartitionIdOffsetEnable(const bool value) {
        TheStructure.Common.WorkloadPartitionIdOffsetEnable = value;
    }
    inline bool getWorkloadPartitionIdOffsetEnable() const {
        return TheStructure.Common.WorkloadPartitionIdOffsetEnable;
    }
    inline void setMmioRemapEnable(const bool value) {
        TheStructure.Common.MmioRemapEnable = value;
    }
    inline bool getMmioRemapEnable() const {
        return TheStructure.Common.MmioRemapEnable;
    }
    inline void setAddCsMmioStartOffset(const bool value) {
        TheStructure.Common.AddCsMmioStartOffset = value;
    }
    inline bool getAddCsMmioStartOffset() const {
        return TheStructure.Common.AddCsMmioStartOffset;
    }
    inline void setAsyncModeEnable(const bool value) {
        TheStructure.Common.AsyncModeEnable = value;
    }
    inline bool getAsyncModeEnable() const {
        return TheStructure.Common.AsyncModeEnable;
    }
    inline void setUseGlobalGtt(const bool value) {
        TheStructure.Common.UseGlobalGtt = value;
    }
    inline bool getUseGlobalGtt() const {
        return TheStructure.Common.UseGlobalGtt;
    }
    typedef enum tagREGISTERADDRESS {
        REGISTERADDRESS_BIT_SHIFT = 0x2,
        REGISTERADDRESS_ALIGN_SIZE = 0x4,
    } REGISTERADDRESS;
    inline void setRegisterAddress(const uint32_t value) {
        UNRECOVERABLE_IF((value >> REGISTERADDRESS_BIT_SHIFT) > 0x7fffff);
        TheStructure.Common.RegisterAddress = value >> REGISTERADDRESS_BIT_SHIFT;
    }
    inline uint32_t getRegisterAddress() const {
        return TheStructure.Common.RegisterAddress << REGISTERADDRESS_BIT_SHIFT;
    }
    typedef enum tagMEMORYADDRESS {
        MEMORYADDRESS_BIT_SHIFT = 0x2,
        MEMORYADDRESS_ALIGN_SIZE = 0x4,
    } MEMORYADDRESS;
    inline void setMemoryAddress(const uint64_t value) {
        UNRECOVERABLE_IF((value >> MEMORYADDRESS_BIT_SHIFT) > 0xffffffffffffffffL);
        TheStructure.Common.MemoryAddress = value >> MEMORYADDRESS_BIT_SHIFT;
    }
    inline uint64_t getMemoryAddress() const {
        return TheStructure.Common.MemoryAddress << MEMORYADDRESS_BIT_SHIFT;
    }
} MI_LOAD_REGISTER_MEM;
STATIC_ASSERT(16 == sizeof(MI_LOAD_REGISTER_MEM));

typedef struct tagMI_LOAD_REGISTER_REG {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 15);
            uint32_t MmioRemapEnableSource : BITFIELD_RANGE(16, 16);
            uint32_t MmioRemapEnableDestination : BITFIELD_RANGE(17, 17);
            uint32_t AddCsMmioStartOffsetSource : BITFIELD_RANGE(18, 18);
            uint32_t AddCsMmioStartOffsetDestination : BITFIELD_RANGE(19, 19);
            uint32_t Reserved_20 : BITFIELD_RANGE(20, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint32_t Reserved_32 : BITFIELD_RANGE(0, 1);
            uint32_t SourceRegisterAddress : BITFIELD_RANGE(2, 22);
            uint32_t Reserved_55 : BITFIELD_RANGE(23, 31);
            // DWORD 2
            uint32_t Reserved_64 : BITFIELD_RANGE(0, 1);
            uint32_t DestinationRegisterAddress : BITFIELD_RANGE(2, 22);
            uint32_t Reserved_87 : BITFIELD_RANGE(23, 31);
        } Common;
        uint32_t RawData[3];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x1,
    } DWORD_LENGTH;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_LOAD_REGISTER_REG = 0x2a,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_LOAD_REGISTER_REG;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_LOAD_REGISTER_REG sInit() {
        MI_LOAD_REGISTER_REG state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 3);
        return TheStructure.RawData[index];
    }
    inline void setMmioRemapEnableSource(const bool value) {
        TheStructure.Common.MmioRemapEnableSource = value;
    }
    inline bool getMmioRemapEnableSource() const {
        return TheStructure.Common.MmioRemapEnableSource;
    }
    inline void setMmioRemapEnableDestination(const bool value) {
        TheStructure.Common.MmioRemapEnableDestination = value;
    }
    inline bool getMmioRemapEnableDestination() const {
        return TheStructure.Common.MmioRemapEnableDestination;
    }
    inline void setAddCsMmioStartOffsetSource(const bool value) {
        TheStructure.Common.AddCsMmioStartOffsetSource = value;
    }
    inline bool getAddCsMmioStartOffsetSource() const {
        return TheStructure.Common.AddCsMmioStartOffsetSource;
    }
    inline void setAddCsMmioStartOffsetDestination(const bool value) {
        TheStructure.Common.AddCsMmioStartOffsetDestination = value;
    }
    inline bool getAddCsMmioStartOffsetDestination() const {
        return TheStructure.Common.AddCsMmioStartOffsetDestination;
    }
    typedef enum tagSOURCEREGISTERADDRESS {
        SOURCEREGISTERADDRESS_BIT_SHIFT = 0x2,
        SOURCEREGISTERADDRESS_ALIGN_SIZE = 0x4,
    } SOURCEREGISTERADDRESS;
    inline void setSourceRegisterAddress(const uint32_t value) {
        UNRECOVERABLE_IF((value >> SOURCEREGISTERADDRESS_BIT_SHIFT) > 0x7fffff);
        TheStructure.Common.SourceRegisterAddress = value >> SOURCEREGISTERADDRESS_BIT_SHIFT;
    }
    inline uint32_t getSourceRegisterAddress() const {
        return TheStructure.Common.SourceRegisterAddress << SOURCEREGISTERADDRESS_BIT_SHIFT;
    }
    typedef enum tagDESTINATIONREGISTERADDRESS {
        DESTINATIONREGISTERADDRESS_BIT_SHIFT = 0x2,
        DESTINATIONREGISTERADDRESS_ALIGN_SIZE = 0x4,
    } DESTINATIONREGISTERADDRESS;
    inline void setDestinationRegisterAddress(const uint32_t value) {
        UNRECOVERABLE_IF((value >> DESTINATIONREGISTERADDRESS_BIT_SHIFT) > 0x7fffff);
        TheStructure.Common.DestinationRegisterAddress = value >> DESTINATIONREGISTERADDRESS_BIT_SHIFT;
    }
    inline uint32_t getDestinationRegisterAddress() const {
        return TheStructure.Common.DestinationRegisterAddress << DESTINATIONREGISTERADDRESS_BIT_SHIFT;
    }
} MI_LOAD_REGISTER_REG;
STATIC_ASSERT(12 == sizeof(MI_LOAD_REGISTER_REG));

typedef struct tagMI_SEMAPHORE_WAIT {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 11);
            uint32_t CompareOperation : BITFIELD_RANGE(12, 14);
            uint32_t WaitMode : BITFIELD_RANGE(15, 15);
            uint32_t RegisterPollMode : BITFIELD_RANGE(16, 16);
            uint32_t IndirectSemaphoreDataDword : BITFIELD_RANGE(17, 17);
            uint32_t WorkloadPartitionIdOffsetEnable : BITFIELD_RANGE(18, 18);
            uint32_t Reserved_19 : BITFIELD_RANGE(19, 21);
            uint32_t MemoryType : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint32_t SemaphoreDataDword;
            // DWORD 2
            uint64_t Reserved_64 : BITFIELD_RANGE(0, 1);
            uint64_t SemaphoreAddress : BITFIELD_RANGE(2, 63);
            // DWORD 4
            uint32_t Reserved_128 : BITFIELD_RANGE(0, 1);
            uint32_t WaitTokenNumber : BITFIELD_RANGE(2, 9);
            uint32_t Reserved_138 : BITFIELD_RANGE(10, 31);
        } Common;
        uint32_t RawData[5];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x3,
    } DWORD_LENGTH;
    typedef enum tagCOMPARE_OPERATION {
        COMPARE_OPERATION_SAD_GREATER_THAN_SDD = 0x0,
        COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD = 0x1,
        COMPARE_OPERATION_SAD_LESS_THAN_SDD = 0x2,
        COMPARE_OPERATION_SAD_LESS_THAN_OR_EQUAL_SDD = 0x3,
        COMPARE_OPERATION_SAD_EQUAL_SDD = 0x4,
        COMPARE_OPERATION_SAD_NOT_EQUAL_SDD = 0x5,
    } COMPARE_OPERATION;
    typedef enum tagWAIT_MODE {
        WAIT_MODE_SIGNAL_MODE = 0x0,
        WAIT_MODE_POLLING_MODE = 0x1,
    } WAIT_MODE;
    typedef enum tagREGISTER_POLL_MODE {
        REGISTER_POLL_MODE_MEMORY_POLL = 0x0,
        REGISTER_POLL_MODE_REGISTER_POLL = 0x1,
    } REGISTER_POLL_MODE;
    typedef enum tagMEMORY_TYPE {
        MEMORY_TYPE_PER_PROCESS_GRAPHICS_ADDRESS = 0x0,
        MEMORY_TYPE_GLOBAL_GRAPHICS_ADDRESS = 0x1,
    } MEMORY_TYPE;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_SEMAPHORE_WAIT = 0x1c,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.CompareOperation = COMPARE_OPERATION_SAD_GREATER_THAN_SDD;
        TheStructure.Common.WaitMode = WAIT_MODE_SIGNAL_MODE;
        TheStructure.Common.RegisterPollMode = REGISTER_POLL_MODE_MEMORY_POLL; // patched
        TheStructure.Common.MemoryType = MEMORY_TYPE_PER_PROCESS_GRAPHICS_ADDRESS;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_SEMAPHORE_WAIT;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_SEMAPHORE_WAIT sInit() {
        MI_SEMAPHORE_WAIT state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 5);
        return TheStructure.RawData[index];
    }
    inline void setCompareOperation(const COMPARE_OPERATION value) {
        TheStructure.Common.CompareOperation = value;
    }
    inline COMPARE_OPERATION getCompareOperation() const {
        return static_cast<COMPARE_OPERATION>(TheStructure.Common.CompareOperation);
    }
    inline void setWaitMode(const WAIT_MODE value) {
        TheStructure.Common.WaitMode = value;
    }
    inline WAIT_MODE getWaitMode() const {
        return static_cast<WAIT_MODE>(TheStructure.Common.WaitMode);
    }
    inline void setRegisterPollMode(const REGISTER_POLL_MODE value) {
        TheStructure.Common.RegisterPollMode = value;
    }
    inline REGISTER_POLL_MODE getRegisterPollMode() const {
        return static_cast<REGISTER_POLL_MODE>(TheStructure.Common.RegisterPollMode);
    }
    inline void setIndirectSemaphoreDataDword(const bool value) {
        TheStructure.Common.IndirectSemaphoreDataDword = value;
    }
    inline bool getIndirectSemaphoreDataDword() const {
        return TheStructure.Common.IndirectSemaphoreDataDword;
    }
    inline void setWorkloadPartitionIdOffsetEnable(const bool value) {
        TheStructure.Common.WorkloadPartitionIdOffsetEnable = value;
    }
    inline bool getWorkloadPartitionIdOffsetEnable() const {
        return TheStructure.Common.WorkloadPartitionIdOffsetEnable;
    }
    inline void setMemoryType(const MEMORY_TYPE value) {
        TheStructure.Common.MemoryType = value;
    }
    inline MEMORY_TYPE getMemoryType() const {
        return static_cast<MEMORY_TYPE>(TheStructure.Common.MemoryType);
    }
    inline void setSemaphoreDataDword(const uint32_t value) {
        TheStructure.Common.SemaphoreDataDword = value;
    }
    inline uint32_t getSemaphoreDataDword() const {
        return TheStructure.Common.SemaphoreDataDword;
    }
    typedef enum tagSEMAPHOREADDRESS {
        SEMAPHOREADDRESS_BIT_SHIFT = 0x2,
        SEMAPHOREADDRESS_ALIGN_SIZE = 0x4,
    } SEMAPHOREADDRESS;
    inline void setSemaphoreGraphicsAddress(const uint64_t value) {
        UNRECOVERABLE_IF((value >> SEMAPHOREADDRESS_BIT_SHIFT) > 0xffffffffffffffffL);
        TheStructure.Common.SemaphoreAddress = value >> SEMAPHOREADDRESS_BIT_SHIFT;
    }
    inline uint64_t getSemaphoreGraphicsAddress() const {
        return TheStructure.Common.SemaphoreAddress << SEMAPHOREADDRESS_BIT_SHIFT;
    }
    inline void setWaitTokenNumber(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xff);
        TheStructure.Common.WaitTokenNumber = value;
    }
    inline uint32_t getWaitTokenNumber() const {
        return TheStructure.Common.WaitTokenNumber;
    }
} MI_SEMAPHORE_WAIT;
STATIC_ASSERT(20 == sizeof(MI_SEMAPHORE_WAIT));

typedef struct tagMI_STORE_DATA_IMM {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 9);
            uint32_t ForceWriteCompletionCheck : BITFIELD_RANGE(10, 10);
            uint32_t WorkloadPartitionIdOffsetEnable : BITFIELD_RANGE(11, 11);
            uint32_t Reserved_12 : BITFIELD_RANGE(12, 20);
            uint32_t StoreQword : BITFIELD_RANGE(21, 21);
            uint32_t UseGlobalGtt : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint64_t CoreModeEnable : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_33 : BITFIELD_RANGE(1, 1);
            uint64_t Address : BITFIELD_RANGE(2, 63);
            // DWORD 3
            uint32_t DataDword0;
            // DWORD 4
            uint32_t DataDword1;
        } Common;
        uint32_t RawData[5];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_STORE_DWORD = 0x2,
        DWORD_LENGTH_STORE_QWORD = 0x3,
    } DWORD_LENGTH;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_STORE_DATA_IMM = 0x20,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_STORE_DWORD;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_STORE_DATA_IMM;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_STORE_DATA_IMM sInit() {
        MI_STORE_DATA_IMM state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 5);
        return TheStructure.RawData[index];
    }
    inline void setDwordLength(const DWORD_LENGTH value) {
        TheStructure.Common.DwordLength = value;
    }
    inline DWORD_LENGTH getDwordLength() const {
        return static_cast<DWORD_LENGTH>(TheStructure.Common.DwordLength);
    }
    inline void setForceWriteCompletionCheck(const bool value) {
        TheStructure.Common.ForceWriteCompletionCheck = value;
    }
    inline bool getForceWriteCompletionCheck() const {
        return TheStructure.Common.ForceWriteCompletionCheck;
    }
    inline void setWorkloadPartitionIdOffsetEnable(const bool value) {
        TheStructure.Common.WorkloadPartitionIdOffsetEnable = value;
    }
    inline bool getWorkloadPartitionIdOffsetEnable() const {
        return TheStructure.Common.WorkloadPartitionIdOffsetEnable;
    }
    inline void setStoreQword(const bool value) {
        TheStructure.Common.StoreQword = value;
    }
    inline bool getStoreQword() const {
        return TheStructure.Common.StoreQword;
    }
    inline void setUseGlobalGtt(const bool value) {
        TheStructure.Common.UseGlobalGtt = value;
    }
    inline bool getUseGlobalGtt() const {
        return TheStructure.Common.UseGlobalGtt;
    }
    inline void setCoreModeEnable(const bool value) {
        TheStructure.Common.CoreModeEnable = value;
    }
    inline bool getCoreModeEnable() const {
        return TheStructure.Common.CoreModeEnable;
    }
    typedef enum tagADDRESS {
        ADDRESS_BIT_SHIFT = 0x2,
        ADDRESS_ALIGN_SIZE = 0x4,
    } ADDRESS;
    inline void setAddress(const uint64_t value) {
        UNRECOVERABLE_IF((value >> ADDRESS_BIT_SHIFT) > 0xffffffffffffffffL);
        TheStructure.Common.Address = value >> ADDRESS_BIT_SHIFT;
    }
    inline uint64_t getAddress() const {
        return TheStructure.Common.Address << ADDRESS_BIT_SHIFT;
    }
    inline void setDataDword0(const uint32_t value) {
        TheStructure.Common.DataDword0 = value;
    }
    inline uint32_t getDataDword0() const {
        return TheStructure.Common.DataDword0;
    }
    inline void setDataDword1(const uint32_t value) {
        TheStructure.Common.DataDword1 = value;
    }
    inline uint32_t getDataDword1() const {
        return TheStructure.Common.DataDword1;
    }
} MI_STORE_DATA_IMM;
STATIC_ASSERT(20 == sizeof(MI_STORE_DATA_IMM));

typedef struct tagMI_STORE_REGISTER_MEM {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 15);
            uint32_t WorkloadPartitionIdOffsetEnable : BITFIELD_RANGE(16, 16);
            uint32_t MmioRemapEnable : BITFIELD_RANGE(17, 17);
            uint32_t Reserved_18 : BITFIELD_RANGE(18, 18);
            uint32_t AddCsMmioStartOffset : BITFIELD_RANGE(19, 19);
            uint32_t Reserved_20 : BITFIELD_RANGE(20, 20);
            uint32_t PredicateEnable : BITFIELD_RANGE(21, 21);
            uint32_t UseGlobalGtt : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint32_t Reserved_32 : BITFIELD_RANGE(0, 1);
            uint32_t RegisterAddress : BITFIELD_RANGE(2, 22);
            uint32_t Reserved_55 : BITFIELD_RANGE(23, 31);
            // DWORD 2
            uint64_t Reserved_64 : BITFIELD_RANGE(0, 1);
            uint64_t MemoryAddress : BITFIELD_RANGE(2, 63);
        } Common;
        uint32_t RawData[4];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x2,
    } DWORD_LENGTH;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_STORE_REGISTER_MEM = 0x24,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_STORE_REGISTER_MEM;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_STORE_REGISTER_MEM sInit() {
        MI_STORE_REGISTER_MEM state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 4);
        return TheStructure.RawData[index];
    }
    inline void setWorkloadPartitionIdOffsetEnable(const bool value) {
        TheStructure.Common.WorkloadPartitionIdOffsetEnable = value;
    }
    inline bool getWorkloadPartitionIdOffsetEnable() const {
        return TheStructure.Common.WorkloadPartitionIdOffsetEnable;
    }
    inline void setMmioRemapEnable(const bool value) {
        TheStructure.Common.MmioRemapEnable = value;
    }
    inline bool getMmioRemapEnable() const {
        return TheStructure.Common.MmioRemapEnable;
    }
    inline void setAddCsMmioStartOffset(const bool value) {
        TheStructure.Common.AddCsMmioStartOffset = value;
    }
    inline bool getAddCsMmioStartOffset() const {
        return TheStructure.Common.AddCsMmioStartOffset;
    }
    inline void setPredicateEnable(const bool value) {
        TheStructure.Common.PredicateEnable = value;
    }
    inline bool getPredicateEnable() const {
        return TheStructure.Common.PredicateEnable;
    }
    inline void setUseGlobalGtt(const bool value) {
        TheStructure.Common.UseGlobalGtt = value;
    }
    inline bool getUseGlobalGtt() const {
        return TheStructure.Common.UseGlobalGtt;
    }
    typedef enum tagREGISTERADDRESS {
        REGISTERADDRESS_BIT_SHIFT = 0x2,
        REGISTERADDRESS_ALIGN_SIZE = 0x4,
    } REGISTERADDRESS;
    inline void setRegisterAddress(const uint32_t value) {
        UNRECOVERABLE_IF((value >> REGISTERADDRESS_BIT_SHIFT) > 0x7fffff);
        TheStructure.Common.RegisterAddress = value >> REGISTERADDRESS_BIT_SHIFT;
    }
    inline uint32_t getRegisterAddress() const {
        return TheStructure.Common.RegisterAddress << REGISTERADDRESS_BIT_SHIFT;
    }
    typedef enum tagMEMORYADDRESS {
        MEMORYADDRESS_BIT_SHIFT = 0x2,
        MEMORYADDRESS_ALIGN_SIZE = 0x4,
    } MEMORYADDRESS;
    inline void setMemoryAddress(const uint64_t value) {
        UNRECOVERABLE_IF((value >> MEMORYADDRESS_BIT_SHIFT) > 0xffffffffffffffffL);
        TheStructure.Common.MemoryAddress = value >> MEMORYADDRESS_BIT_SHIFT;
    }
    inline uint64_t getMemoryAddress() const {
        return TheStructure.Common.MemoryAddress << MEMORYADDRESS_BIT_SHIFT;
    }
} MI_STORE_REGISTER_MEM;
STATIC_ASSERT(16 == sizeof(MI_STORE_REGISTER_MEM));

typedef struct tagPIPELINE_SELECT {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t PipelineSelection : BITFIELD_RANGE(0, 1);
            uint32_t RenderSliceCommonPowerGateEnable : BITFIELD_RANGE(2, 2);
            uint32_t RenderSamplerPowerGateEnable : BITFIELD_RANGE(3, 3);
            uint32_t Reserved_4 : BITFIELD_RANGE(4, 4);
            uint32_t EnableComputeTo3DPerformanceMode : BITFIELD_RANGE(5, 5);
            uint32_t Reserved_6 : BITFIELD_RANGE(6, 6);
            uint32_t Reserved_7 : BITFIELD_RANGE(7, 7);
            uint32_t MaskBits : BITFIELD_RANGE(8, 15);
            uint32_t _3DCommandSubOpcode : BITFIELD_RANGE(16, 23);
            uint32_t _3DCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t CommandSubtype : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
        } Common;
        uint32_t RawData[1];
    } TheStructure;
    typedef enum tagPIPELINE_SELECTION {
        PIPELINE_SELECTION_3D = 0x0,
        PIPELINE_SELECTION_GPGPU = 0x2,
    } PIPELINE_SELECTION;
    typedef enum tag_3D_COMMAND_SUB_OPCODE {
        _3D_COMMAND_SUB_OPCODE_PIPELINE_SELECT = 0x4,
    } _3D_COMMAND_SUB_OPCODE;
    typedef enum tag_3D_COMMAND_OPCODE {
        _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED = 0x1,
    } _3D_COMMAND_OPCODE;
    typedef enum tagCOMMAND_SUBTYPE {
        COMMAND_SUBTYPE_GFXPIPE_SINGLE_DW = 0x1,
    } COMMAND_SUBTYPE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_GFXPIPE = 0x3,
    } COMMAND_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.PipelineSelection = PIPELINE_SELECTION_3D;
        TheStructure.Common._3DCommandSubOpcode = _3D_COMMAND_SUB_OPCODE_PIPELINE_SELECT;
        TheStructure.Common._3DCommandOpcode = _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED;
        TheStructure.Common.CommandSubtype = COMMAND_SUBTYPE_GFXPIPE_SINGLE_DW;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
    }
    static tagPIPELINE_SELECT sInit() {
        PIPELINE_SELECT state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 1);
        return TheStructure.RawData[index];
    }
    inline void setPipelineSelection(const PIPELINE_SELECTION value) {
        TheStructure.Common.PipelineSelection = value;
    }
    inline PIPELINE_SELECTION getPipelineSelection() const {
        return static_cast<PIPELINE_SELECTION>(TheStructure.Common.PipelineSelection);
    }
    inline void setRenderSliceCommonPowerGateEnable(const bool value) {
        TheStructure.Common.RenderSliceCommonPowerGateEnable = value;
    }
    inline bool getRenderSliceCommonPowerGateEnable() const {
        return TheStructure.Common.RenderSliceCommonPowerGateEnable;
    }
    inline void setRenderSamplerPowerGateEnable(const bool value) {
        TheStructure.Common.RenderSamplerPowerGateEnable = value;
    }
    inline bool getRenderSamplerPowerGateEnable() const {
        return TheStructure.Common.RenderSamplerPowerGateEnable;
    }
    inline void setEnableComputeTo3DPerformanceMode(const bool value) {
        TheStructure.Common.EnableComputeTo3DPerformanceMode = value;
    }
    inline bool getEnableComputeTo3DPerformanceMode() const {
        return TheStructure.Common.EnableComputeTo3DPerformanceMode;
    }
    inline void setMaskBits(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xff);
        TheStructure.Common.MaskBits = value;
    }
    inline uint32_t getMaskBits() const {
        return TheStructure.Common.MaskBits;
    }
} PIPELINE_SELECT;
STATIC_ASSERT(4 == sizeof(PIPELINE_SELECT));

typedef struct tagSTATE_COMPUTE_MODE {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 15);
            uint32_t _3DCommandSubOpcode : BITFIELD_RANGE(16, 23);
            uint32_t _3DCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t CommandSubtype : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint32_t ZPassAsyncComputeThreadLimit : BITFIELD_RANGE(0, 2);
            uint32_t NpZAsyncThrottleSettings : BITFIELD_RANGE(3, 4);
            uint32_t Reserved_37 : BITFIELD_RANGE(5, 6);
            uint32_t AsyncComputeThreadLimit : BITFIELD_RANGE(7, 9);
            uint32_t EnableVariableRegisterSizeAllocation_Vrt : BITFIELD_RANGE(10, 10);
            uint32_t Reserved_43 : BITFIELD_RANGE(11, 11);
            uint32_t EnablePipelinedEuThreadArbitration : BITFIELD_RANGE(12, 12);
            uint32_t EuThreadSchedulingMode : BITFIELD_RANGE(13, 14);
            uint32_t LargeGrfMode : BITFIELD_RANGE(15, 15);
            uint32_t Mask1 : BITFIELD_RANGE(16, 31);
            // DWORD 2
            uint32_t MidthreadPreemptionDelayTimer : BITFIELD_RANGE(0, 2);
            uint32_t MidthreadPreemptionOverdispatchThreadGroupCount : BITFIELD_RANGE(3, 4);
            uint32_t MidthreadPreemptionOverdispatchTestMode : BITFIELD_RANGE(5, 5);
            uint32_t UavCoherencyMode : BITFIELD_RANGE(6, 6);
            uint32_t Reserved_71 : BITFIELD_RANGE(7, 15);
            uint32_t Mask2 : BITFIELD_RANGE(16, 31);
        } Common;
        uint32_t RawData[3];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x1,
    } DWORD_LENGTH;
    typedef enum tag_3D_COMMAND_SUB_OPCODE {
        _3D_COMMAND_SUB_OPCODE_STATE_COMPUTE_MODE = 0x5,
    } _3D_COMMAND_SUB_OPCODE;
    typedef enum tag_3D_COMMAND_OPCODE {
        _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED = 0x1,
    } _3D_COMMAND_OPCODE;
    typedef enum tagCOMMAND_SUBTYPE {
        COMMAND_SUBTYPE_GFXPIPE_COMMON = 0x0,
    } COMMAND_SUBTYPE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_GFXPIPE = 0x3,
    } COMMAND_TYPE;
    typedef enum tagZ_PASS_ASYNC_COMPUTE_THREAD_LIMIT {
        Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60 = 0x0,
        Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_64 = 0x1,
        Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_56 = 0x2,
        Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_48 = 0x3,
        Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_40 = 0x4,
        Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_32 = 0x5,
    } Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT;
    typedef enum tagNP_Z_ASYNC_THROTTLE_SETTINGS {
        NP_Z_ASYNC_THROTTLE_SETTINGS_MAX_32 = 0x1,
        NP_Z_ASYNC_THROTTLE_SETTINGS_MAX_40 = 0x2,
        NP_Z_ASYNC_THROTTLE_SETTINGS_MAX_48 = 0x3,
    } NP_Z_ASYNC_THROTTLE_SETTINGS;
    typedef enum tagASYNC_COMPUTE_THREAD_LIMIT {
        ASYNC_COMPUTE_THREAD_LIMIT_DISABLED = 0x0,
        ASYNC_COMPUTE_THREAD_LIMIT_MAX_2 = 0x1,
        ASYNC_COMPUTE_THREAD_LIMIT_MAX_8 = 0x2,
        ASYNC_COMPUTE_THREAD_LIMIT_MAX_16 = 0x3,
        ASYNC_COMPUTE_THREAD_LIMIT_MAX_24 = 0x4,
        ASYNC_COMPUTE_THREAD_LIMIT_MAX_32 = 0x5,
        ASYNC_COMPUTE_THREAD_LIMIT_MAX_40 = 0x6,
        ASYNC_COMPUTE_THREAD_LIMIT_MAX_48 = 0x7,
    } ASYNC_COMPUTE_THREAD_LIMIT;
    typedef enum tagEU_THREAD_SCHEDULING_MODE {
        EU_THREAD_SCHEDULING_MODE_HW_DEFAULT = 0x0,
        EU_THREAD_SCHEDULING_MODE_OLDEST_FIRST = 0x1,
        EU_THREAD_SCHEDULING_MODE_ROUND_ROBIN = 0x2,
        EU_THREAD_SCHEDULING_MODE_STALL_BASED_ROUND_ROBIN = 0x3,
    } EU_THREAD_SCHEDULING_MODE;
    typedef enum tagMIDTHREAD_PREEMPTION_DELAY_TIMER {
        MIDTHREAD_PREEMPTION_DELAY_TIMER_MTP_TIMER_VAL_0 = 0x0,
        MIDTHREAD_PREEMPTION_DELAY_TIMER_MTP_TIMER_VAL_50 = 0x1,
        MIDTHREAD_PREEMPTION_DELAY_TIMER_MTP_TIMER_VAL_100 = 0x2,
        MIDTHREAD_PREEMPTION_DELAY_TIMER_MTP_TIMER_VAL_150 = 0x3,
    } MIDTHREAD_PREEMPTION_DELAY_TIMER;
    typedef enum tagMIDTHREAD_PREEMPTION_OVERDISPATCH_THREAD_GROUP_COUNT {
        MIDTHREAD_PREEMPTION_OVERDISPATCH_THREAD_GROUP_COUNT_OD_TG_M2 = 0x0,
        MIDTHREAD_PREEMPTION_OVERDISPATCH_THREAD_GROUP_COUNT_OD_TG_M4 = 0x1,
        MIDTHREAD_PREEMPTION_OVERDISPATCH_THREAD_GROUP_COUNT_OD_TG_M8 = 0x2,
        MIDTHREAD_PREEMPTION_OVERDISPATCH_THREAD_GROUP_COUNT_OD_TG_M16 = 0x3,
    } MIDTHREAD_PREEMPTION_OVERDISPATCH_THREAD_GROUP_COUNT;
    typedef enum tagMIDTHREAD_PREEMPTION_OVERDISPATCH_TEST_MODE {
        MIDTHREAD_PREEMPTION_OVERDISPATCH_TEST_MODE_REGULAR = 0x0,
        MIDTHREAD_PREEMPTION_OVERDISPATCH_TEST_MODE_TEST_MODE = 0x1,
    } MIDTHREAD_PREEMPTION_OVERDISPATCH_TEST_MODE;
    typedef enum tagUAV_COHERENCY_MODE {
        UAV_COHERENCY_MODE_DRAIN_DATAPORT_MODE = 0x0,
        UAV_COHERENCY_MODE_FLUSH_DATAPORT_L1 = 0x1,
    } UAV_COHERENCY_MODE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common._3DCommandSubOpcode = _3D_COMMAND_SUB_OPCODE_STATE_COMPUTE_MODE;
        TheStructure.Common._3DCommandOpcode = _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED;
        TheStructure.Common.CommandSubtype = COMMAND_SUBTYPE_GFXPIPE_COMMON;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
        TheStructure.Common.ZPassAsyncComputeThreadLimit = Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60;
        TheStructure.Common.AsyncComputeThreadLimit = ASYNC_COMPUTE_THREAD_LIMIT_DISABLED;
        TheStructure.Common.EuThreadSchedulingMode = EU_THREAD_SCHEDULING_MODE_HW_DEFAULT;
        TheStructure.Common.MidthreadPreemptionDelayTimer = MIDTHREAD_PREEMPTION_DELAY_TIMER_MTP_TIMER_VAL_0;
        TheStructure.Common.MidthreadPreemptionOverdispatchThreadGroupCount = MIDTHREAD_PREEMPTION_OVERDISPATCH_THREAD_GROUP_COUNT_OD_TG_M2;
        TheStructure.Common.MidthreadPreemptionOverdispatchTestMode = MIDTHREAD_PREEMPTION_OVERDISPATCH_TEST_MODE_REGULAR;
        TheStructure.Common.UavCoherencyMode = UAV_COHERENCY_MODE_DRAIN_DATAPORT_MODE;
    }
    static tagSTATE_COMPUTE_MODE sInit() {
        STATE_COMPUTE_MODE state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 3);
        return TheStructure.RawData[index];
    }
    inline void setZPassAsyncComputeThreadLimit(const Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT value) {
        TheStructure.Common.ZPassAsyncComputeThreadLimit = value;
    }
    inline Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT getZPassAsyncComputeThreadLimit() const {
        return static_cast<Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT>(TheStructure.Common.ZPassAsyncComputeThreadLimit);
    }
    inline void setNpZAsyncThrottleSettings(const NP_Z_ASYNC_THROTTLE_SETTINGS value) {
        TheStructure.Common.NpZAsyncThrottleSettings = value;
    }
    inline NP_Z_ASYNC_THROTTLE_SETTINGS getNpZAsyncThrottleSettings() const {
        return static_cast<NP_Z_ASYNC_THROTTLE_SETTINGS>(TheStructure.Common.NpZAsyncThrottleSettings);
    }
    inline void setAsyncComputeThreadLimit(const ASYNC_COMPUTE_THREAD_LIMIT value) {
        TheStructure.Common.AsyncComputeThreadLimit = value;
    }
    inline ASYNC_COMPUTE_THREAD_LIMIT getAsyncComputeThreadLimit() const {
        return static_cast<ASYNC_COMPUTE_THREAD_LIMIT>(TheStructure.Common.AsyncComputeThreadLimit);
    }
    inline void setEnableVariableRegisterSizeAllocationVrt(const bool value) {
        TheStructure.Common.EnableVariableRegisterSizeAllocation_Vrt = value;
    }
    inline bool getEnableVariableRegisterSizeAllocationVrt() const {
        return TheStructure.Common.EnableVariableRegisterSizeAllocation_Vrt;
    }
    inline void setEnablePipelinedEuThreadArbitration(const bool value) {
        TheStructure.Common.EnablePipelinedEuThreadArbitration = value;
    }
    inline bool getEnablePipelinedEuThreadArbitration() const {
        return TheStructure.Common.EnablePipelinedEuThreadArbitration;
    }
    inline void setEuThreadSchedulingMode(const EU_THREAD_SCHEDULING_MODE value) {
        TheStructure.Common.EuThreadSchedulingMode = value;
    }
    inline EU_THREAD_SCHEDULING_MODE getEuThreadSchedulingMode() const {
        return static_cast<EU_THREAD_SCHEDULING_MODE>(TheStructure.Common.EuThreadSchedulingMode);
    }
    inline void setLargeGrfMode(const bool value) {
        TheStructure.Common.LargeGrfMode = value;
    }
    inline bool getLargeGrfMode() const {
        return TheStructure.Common.LargeGrfMode;
    }
    inline void setMask1(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffff);
        TheStructure.Common.Mask1 = value;
    }
    inline uint32_t getMask1() const {
        return TheStructure.Common.Mask1;
    }
    inline void setMidthreadPreemptionDelayTimer(const MIDTHREAD_PREEMPTION_DELAY_TIMER value) {
        TheStructure.Common.MidthreadPreemptionDelayTimer = value;
    }
    inline MIDTHREAD_PREEMPTION_DELAY_TIMER getMidthreadPreemptionDelayTimer() const {
        return static_cast<MIDTHREAD_PREEMPTION_DELAY_TIMER>(TheStructure.Common.MidthreadPreemptionDelayTimer);
    }
    inline void setMidthreadPreemptionOverdispatchThreadGroupCount(const MIDTHREAD_PREEMPTION_OVERDISPATCH_THREAD_GROUP_COUNT value) {
        TheStructure.Common.MidthreadPreemptionOverdispatchThreadGroupCount = value;
    }
    inline MIDTHREAD_PREEMPTION_OVERDISPATCH_THREAD_GROUP_COUNT getMidthreadPreemptionOverdispatchThreadGroupCount() const {
        return static_cast<MIDTHREAD_PREEMPTION_OVERDISPATCH_THREAD_GROUP_COUNT>(TheStructure.Common.MidthreadPreemptionOverdispatchThreadGroupCount);
    }
    inline void setMidthreadPreemptionOverdispatchTestMode(const MIDTHREAD_PREEMPTION_OVERDISPATCH_TEST_MODE value) {
        TheStructure.Common.MidthreadPreemptionOverdispatchTestMode = value;
    }
    inline MIDTHREAD_PREEMPTION_OVERDISPATCH_TEST_MODE getMidthreadPreemptionOverdispatchTestMode() const {
        return static_cast<MIDTHREAD_PREEMPTION_OVERDISPATCH_TEST_MODE>(TheStructure.Common.MidthreadPreemptionOverdispatchTestMode);
    }
    inline void setUavCoherencyMode(const UAV_COHERENCY_MODE value) {
        TheStructure.Common.UavCoherencyMode = value;
    }
    inline UAV_COHERENCY_MODE getUavCoherencyMode() const {
        return static_cast<UAV_COHERENCY_MODE>(TheStructure.Common.UavCoherencyMode);
    }
    inline void setMask2(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xffff);
        TheStructure.Common.Mask2 = value;
    }
    inline uint32_t getMask2() const {
        return TheStructure.Common.Mask2;
    }
} STATE_COMPUTE_MODE;
STATIC_ASSERT(12 == sizeof(STATE_COMPUTE_MODE));

typedef struct tag_3DSTATE_BINDING_TABLE_POOL_ALLOC {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 15);
            uint32_t _3DCommandSubOpcode : BITFIELD_RANGE(16, 23);
            uint32_t _3DCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t CommandSubtype : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint64_t SurfaceObjectControlStateEncryptedData : BITFIELD_RANGE(0, 0);
            uint64_t SurfaceObjectControlStateIndexToMocsTables : BITFIELD_RANGE(1, 6);
            uint64_t Reserved_39 : BITFIELD_RANGE(7, 9);
            uint64_t Reserved_42 : BITFIELD_RANGE(10, 10);
            uint64_t Reserved_43 : BITFIELD_RANGE(11, 11);
            uint64_t BindingTablePoolBaseAddress : BITFIELD_RANGE(12, 47);
            uint64_t BindingTablePoolBaseAddressReserved_80 : BITFIELD_RANGE(48, 63);
            uint32_t Reserved_96 : BITFIELD_RANGE(0, 11);
            uint32_t BindingTablePoolBufferSize : BITFIELD_RANGE(12, 31);
        } Common;
        uint32_t RawData[4];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_DWORD_COUNT_N = 0x2,
    } DWORD_LENGTH;
    typedef enum tag_3D_COMMAND_SUB_OPCODE {
        _3D_COMMAND_SUB_OPCODE_3DSTATE_BINDING_TABLE_POOL_ALLOC = 0x19,
    } _3D_COMMAND_SUB_OPCODE;
    typedef enum tag_3D_COMMAND_OPCODE {
        _3D_COMMAND_OPCODE_3DSTATE_NONPIPELINED = 0x1,
    } _3D_COMMAND_OPCODE;
    typedef enum tagCOMMAND_SUBTYPE {
        COMMAND_SUBTYPE_GFXPIPE_3D = 0x3,
    } COMMAND_SUBTYPE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_GFXPIPE = 0x3,
    } COMMAND_TYPE;

    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_DWORD_COUNT_N;
        TheStructure.Common._3DCommandSubOpcode = _3D_COMMAND_SUB_OPCODE_3DSTATE_BINDING_TABLE_POOL_ALLOC;
        TheStructure.Common._3DCommandOpcode = _3D_COMMAND_OPCODE_3DSTATE_NONPIPELINED;
        TheStructure.Common.CommandSubtype = COMMAND_SUBTYPE_GFXPIPE_3D;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
        TheStructure.Common.BindingTablePoolBufferSize = 0;
    }
    static tag_3DSTATE_BINDING_TABLE_POOL_ALLOC sInit() {
        _3DSTATE_BINDING_TABLE_POOL_ALLOC state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 4);
        return TheStructure.RawData[index];
    }
    inline void setSurfaceObjectControlStateEncryptedData(const bool value) {
        TheStructure.Common.SurfaceObjectControlStateEncryptedData = value;
    }
    inline bool getSurfaceObjectControlStateEncryptedData() const {
        return TheStructure.Common.SurfaceObjectControlStateEncryptedData;
    }
    inline void setSurfaceObjectControlStateIndexToMocsTables(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0x7eL);
        TheStructure.Common.SurfaceObjectControlStateIndexToMocsTables = value >> 1;
    }
    inline uint64_t getSurfaceObjectControlStateIndexToMocsTables() const {
        return (TheStructure.Common.SurfaceObjectControlStateIndexToMocsTables << 1);
    }
    typedef enum tagBINDINGTABLEPOOLBASEADDRESS {
        BINDINGTABLEPOOLBASEADDRESS_BIT_SHIFT = 0xc,
        BINDINGTABLEPOOLBASEADDRESS_ALIGN_SIZE = 0x1000,
    } BINDINGTABLEPOOLBASEADDRESS;
    inline void setBindingTablePoolBaseAddress(const uint64_t value) {
        TheStructure.Common.BindingTablePoolBaseAddress = value >> BINDINGTABLEPOOLBASEADDRESS_BIT_SHIFT;
    }
    inline uint64_t getBindingTablePoolBaseAddress() const {
        return TheStructure.Common.BindingTablePoolBaseAddress << BINDINGTABLEPOOLBASEADDRESS_BIT_SHIFT;
    }
    inline void setBindingTablePoolBufferSize(const uint32_t value) { TheStructure.Common.BindingTablePoolBufferSize = value; }
    inline uint32_t getBindingTablePoolBufferSize() const { return TheStructure.Common.BindingTablePoolBufferSize; }
} _3DSTATE_BINDING_TABLE_POOL_ALLOC;
STATIC_ASSERT(16 == sizeof(_3DSTATE_BINDING_TABLE_POOL_ALLOC));

typedef struct tagMI_MEM_FENCE {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t FenceType : BITFIELD_RANGE(0, 1);
            uint32_t Reserved_2 : BITFIELD_RANGE(2, 16);
            uint32_t MiCommandSubOpcode : BITFIELD_RANGE(17, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
        } Common;
        uint32_t RawData[1];
    } TheStructure;
    typedef enum tagFENCE_TYPE {
        FENCE_TYPE_RELEASE_FENCE = 0x0,
        FENCE_TYPE_ACQUIRE_FENCE = 0x1,
        FENCE_TYPE_MI_WRITE_FENCE = 0x3,
    } FENCE_TYPE;
    typedef enum tagMI_COMMAND_SUB_OPCODE {
        MI_COMMAND_SUB_OPCODE_MI_MEM_FENCE = 0x0,
    } MI_COMMAND_SUB_OPCODE;
    typedef enum tagMI_COMMAND_OPCODE {
        MI_COMMAND_OPCODE_MI_EXTENDED = 0x9,
    } MI_COMMAND_OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_MI_COMMAND = 0x0,
    } COMMAND_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.FenceType = FENCE_TYPE_RELEASE_FENCE;
        TheStructure.Common.MiCommandSubOpcode = MI_COMMAND_SUB_OPCODE_MI_MEM_FENCE;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_EXTENDED;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_MEM_FENCE sInit() {
        MI_MEM_FENCE state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 1);
        return TheStructure.RawData[index];
    }
    inline void setFenceType(const FENCE_TYPE value) {
        TheStructure.Common.FenceType = value;
    }
    inline FENCE_TYPE getFenceType() const {
        return static_cast<FENCE_TYPE>(TheStructure.Common.FenceType);
    }
    inline void setMiCommandSubOpcode(const MI_COMMAND_SUB_OPCODE value) {
        TheStructure.Common.MiCommandSubOpcode = value;
    }
    inline MI_COMMAND_SUB_OPCODE getMiCommandSubOpcode() const {
        return static_cast<MI_COMMAND_SUB_OPCODE>(TheStructure.Common.MiCommandSubOpcode);
    }
} MI_MEM_FENCE;
STATIC_ASSERT(4 == sizeof(MI_MEM_FENCE));

typedef struct tagSTATE_SYSTEM_MEM_FENCE_ADDRESS {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 14);
            uint32_t ContextRestoreInvalid : BITFIELD_RANGE(15, 15);
            uint32_t _3DCommandSubOpcode : BITFIELD_RANGE(16, 23);
            uint32_t _3DCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t CommandSubtype : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint64_t Reserved_32 : BITFIELD_RANGE(0, 11);
            uint64_t SystemMemoryFenceAddress : BITFIELD_RANGE(12, 63);
        } Common;
        uint32_t RawData[3];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_LENGTH_1 = 0x1,
    } DWORD_LENGTH;
    typedef enum tag_3D_COMMAND_SUB_OPCODE {
        _3D_COMMAND_SUB_OPCODE_STATE_SYSTEM_MEM_FENCE_ADDRESS = 0x9,
    } _3D_COMMAND_SUB_OPCODE;
    typedef enum tag_3D_COMMAND_OPCODE {
        _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED = 0x1,
    } _3D_COMMAND_OPCODE;
    typedef enum tagCOMMAND_SUBTYPE {
        COMMAND_SUBTYPE_GFXPIPE_COMMON_ = 0x0,
    } COMMAND_SUBTYPE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_GFXPIPE = 0x3,
    } COMMAND_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_LENGTH_1;
        TheStructure.Common._3DCommandSubOpcode = _3D_COMMAND_SUB_OPCODE_STATE_SYSTEM_MEM_FENCE_ADDRESS;
        TheStructure.Common._3DCommandOpcode = _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED;
        TheStructure.Common.CommandSubtype = COMMAND_SUBTYPE_GFXPIPE_COMMON_;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
    }
    static tagSTATE_SYSTEM_MEM_FENCE_ADDRESS sInit() {
        STATE_SYSTEM_MEM_FENCE_ADDRESS state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 3);
        return TheStructure.RawData[index];
    }
    inline void setContextRestoreInvalid(const bool value) {
        TheStructure.Common.ContextRestoreInvalid = value;
    }
    inline bool getContextRestoreInvalid() const {
        return TheStructure.Common.ContextRestoreInvalid;
    }
    typedef enum tagSYSTEMMEMORYFENCEADDRESS {
        SYSTEMMEMORYFENCEADDRESS_BIT_SHIFT = 0xc,
        SYSTEMMEMORYFENCEADDRESS_ALIGN_SIZE = 0x1000,
    } SYSTEMMEMORYFENCEADDRESS;
    inline void setSystemMemoryFenceAddress(const uint64_t value) {
        UNRECOVERABLE_IF((value >> SYSTEMMEMORYFENCEADDRESS_BIT_SHIFT) > 0xffffffffffffffffL);
        TheStructure.Common.SystemMemoryFenceAddress = value >> SYSTEMMEMORYFENCEADDRESS_BIT_SHIFT;
    }
    inline uint64_t getSystemMemoryFenceAddress() const {
        return TheStructure.Common.SystemMemoryFenceAddress << SYSTEMMEMORYFENCEADDRESS_BIT_SHIFT;
    }
} STATE_SYSTEM_MEM_FENCE_ADDRESS;
STATIC_ASSERT(12 == sizeof(STATE_SYSTEM_MEM_FENCE_ADDRESS));

typedef struct tagSTATE_PREFETCH {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 15);
            uint32_t _3DCommandSubOpcode : BITFIELD_RANGE(16, 23);
            uint32_t _3DCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t CommandSubtype : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint32_t PrefetchSize : BITFIELD_RANGE(0, 9);
            uint32_t Reserved_42 : BITFIELD_RANGE(10, 15);
            uint32_t KernelInstructionPrefetch : BITFIELD_RANGE(16, 16);
            uint32_t Reserved_49 : BITFIELD_RANGE(17, 19);
            uint32_t ParserStall : BITFIELD_RANGE(20, 20);
            uint32_t Reserved_53 : BITFIELD_RANGE(21, 23);
            uint32_t MemoryObjectControlStateEncryptedData : BITFIELD_RANGE(24, 24);
            uint32_t MemoryObjectControlStateIndexToMocsTables : BITFIELD_RANGE(25, 30);
            uint32_t Reserved_63 : BITFIELD_RANGE(31, 31);
            // DWORD 2
            uint64_t Reserved_64 : BITFIELD_RANGE(0, 5);
            uint64_t Address : BITFIELD_RANGE(6, 63);
        } Common;
        uint32_t RawData[4];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_DWORD_COUNT_N = 0x2,
    } DWORD_LENGTH;
    typedef enum tag_3D_COMMAND_SUB_OPCODE {
        _3D_COMMAND_SUB_OPCODE_STATE_PREFETCH = 0x3,
    } _3D_COMMAND_SUB_OPCODE;
    typedef enum tag_3D_COMMAND_OPCODE {
        _3D_COMMAND_OPCODE_GFXPIPE_PIPELINED = 0x0,
    } _3D_COMMAND_OPCODE;
    typedef enum tagCOMMAND_SUBTYPE {
        COMMAND_SUBTYPE_GFXPIPE_COMMON = 0x0,
    } COMMAND_SUBTYPE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_GFXPIPE = 0x3,
    } COMMAND_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_DWORD_COUNT_N;
        TheStructure.Common._3DCommandSubOpcode = _3D_COMMAND_SUB_OPCODE_STATE_PREFETCH;
        TheStructure.Common._3DCommandOpcode = _3D_COMMAND_OPCODE_GFXPIPE_PIPELINED;
        TheStructure.Common.CommandSubtype = COMMAND_SUBTYPE_GFXPIPE_COMMON;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
    }
    static tagSTATE_PREFETCH sInit() {
        STATE_PREFETCH state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 4);
        return TheStructure.RawData[index];
    }
    inline void setPrefetchSize(const uint32_t value) {
        UNRECOVERABLE_IF(value - 1 > 0x3ff);
        TheStructure.Common.PrefetchSize = value - 1;
    }
    inline uint32_t getPrefetchSize() const {
        return TheStructure.Common.PrefetchSize + 1;
    }
    inline void setKernelInstructionPrefetch(const bool value) {
        TheStructure.Common.KernelInstructionPrefetch = value;
    }
    inline bool getKernelInstructionPrefetch() const {
        return TheStructure.Common.KernelInstructionPrefetch;
    }
    inline void setParserStall(const bool value) {
        TheStructure.Common.ParserStall = value;
    }
    inline bool getParserStall() const {
        return TheStructure.Common.ParserStall;
    }
    inline void setMemoryObjectControlStateEncryptedData(const bool value) {
        TheStructure.Common.MemoryObjectControlStateEncryptedData = value;
    }
    inline bool getMemoryObjectControlStateEncryptedData() const {
        return TheStructure.Common.MemoryObjectControlStateEncryptedData;
    }
    inline void setMemoryObjectControlState(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0x7f);
        setMemoryObjectControlStateEncryptedData(value & 1);
        TheStructure.Common.MemoryObjectControlStateIndexToMocsTables = value >> 1;
    }
    inline uint32_t getMemoryObjectControlState() const {
        uint32_t mocs = getMemoryObjectControlStateEncryptedData();
        return mocs | (TheStructure.Common.MemoryObjectControlStateIndexToMocsTables << 1);
    }
    typedef enum tagADDRESS {
        ADDRESS_BIT_SHIFT = 0x6,
        ADDRESS_ALIGN_SIZE = 0x40,
    } ADDRESS;
    inline void setAddress(const uint64_t value) {
        UNRECOVERABLE_IF((value >> ADDRESS_BIT_SHIFT) > 0xffffffffffffffffL);
        TheStructure.Common.Address = value >> ADDRESS_BIT_SHIFT;
    }
    inline uint64_t getAddress() const {
        return TheStructure.Common.Address << ADDRESS_BIT_SHIFT;
    }
} STATE_PREFETCH;
STATIC_ASSERT(16 == sizeof(STATE_PREFETCH));

typedef struct tagMEM_SET {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 8);
            uint32_t CompressionFormat30 : BITFIELD_RANGE(9, 12);
            uint32_t Reserved_13 : BITFIELD_RANGE(13, 16);
            uint32_t FillType : BITFIELD_RANGE(17, 18);
            uint32_t Mode : BITFIELD_RANGE(19, 20);
            uint32_t Reserved_21 : BITFIELD_RANGE(21, 21);
            uint32_t InstructionTarget_Opcode : BITFIELD_RANGE(22, 28);
            uint32_t Client : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint32_t FillWidth : BITFIELD_RANGE(0, 23);
            uint32_t Reserved_56 : BITFIELD_RANGE(24, 31);
            // DWORD 2
            uint32_t FillHeight : BITFIELD_RANGE(0, 17);
            uint32_t Reserved_82 : BITFIELD_RANGE(18, 31);
            // DWORD 3
            uint32_t DestinationPitch : BITFIELD_RANGE(0, 17);
            uint32_t Reserved_114 : BITFIELD_RANGE(18, 31);
            // DWORD 4
            uint32_t DestinationStartAddressLow;
            // DWORD 5
            uint32_t DestinationStartAddressHigh;
            // DWORD 6
            uint32_t DestinationEncryptEn : BITFIELD_RANGE(0, 0);
            uint32_t Reserved_193 : BITFIELD_RANGE(1, 2);
            uint32_t DestinationMocs : BITFIELD_RANGE(3, 6);
            uint32_t Reserved_199 : BITFIELD_RANGE(7, 23);
            uint32_t FillData : BITFIELD_RANGE(24, 31);
        } Common;
        uint32_t RawData[7];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x5,
    } DWORD_LENGTH;
    typedef enum tagCOMPRESSION_FORMAT30 {
        COMPRESSION_FORMAT30_CMF_R8 = 0x0,
        COMPRESSION_FORMAT30_CMF_R8_G8 = 0x1,
        COMPRESSION_FORMAT30_CMF_R8_G8_B8_A8 = 0x2,
        COMPRESSION_FORMAT30_CMF_R10_G10_B10_A2 = 0x3,
        COMPRESSION_FORMAT30_CMF_R11_G11_B10 = 0x4,
        COMPRESSION_FORMAT30_CMF_R16 = 0x5,
        COMPRESSION_FORMAT30_CMF_R16_G16 = 0x6,
        COMPRESSION_FORMAT30_CMF_R16_G16_B16_A16 = 0x7,
        COMPRESSION_FORMAT30_CMF_R32 = 0x8,
        COMPRESSION_FORMAT30_CMF_R32_G32 = 0x9,
        COMPRESSION_FORMAT30_CMF_R32_G32_B32_A32 = 0xa,
        COMPRESSION_FORMAT30_CMF_Y16_U16_Y16_V16 = 0xb,
        COMPRESSION_FORMAT30_CMF_ML8 = 0xf,
    } COMPRESSION_FORMAT30;
    typedef enum tagFILL_TYPE {
        FILL_TYPE_LINEAR_FILL = 0x0,
        FILL_TYPE_MATRIX_FILL = 0x1,
        FILL_TYPE_FAST_CLEAR_LINEAR_FILL = 0x2,
        FILL_TYPE_FAST_CLEAR_MATRIX_FILL = 0x3,
    } FILL_TYPE;
    typedef enum tagMODE {
        MODE_FILL_MODE = 0x0,
        MODE_EVICT_MODE = 0x1,
    } MODE;
    typedef enum tagINSTRUCTION_TARGETOPCODE {
        INSTRUCTION_TARGETOPCODE_MEM_SET = 0x5b,
    } INSTRUCTION_TARGETOPCODE;
    typedef enum tagCLIENT {
        CLIENT_2D_PROCESSOR = 0x2,
    } CLIENT;
    typedef enum tagDESTINATION_ENCRYPT_EN {
        DESTINATION_ENCRYPT_EN_DISABLE = 0x0,
        DESTINATION_ENCRYPT_EN_ENABLE = 0x1,
    } DESTINATION_ENCRYPT_EN;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.CompressionFormat30 = COMPRESSION_FORMAT30_CMF_R8;
        TheStructure.Common.FillType = FILL_TYPE_LINEAR_FILL;
        TheStructure.Common.Mode = MODE_FILL_MODE;
        TheStructure.Common.InstructionTarget_Opcode = INSTRUCTION_TARGETOPCODE_MEM_SET;
        TheStructure.Common.Client = CLIENT_2D_PROCESSOR;
        TheStructure.Common.DestinationEncryptEn = DESTINATION_ENCRYPT_EN_DISABLE;
    }
    static tagMEM_SET sInit() {
        MEM_SET state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 7);
        return TheStructure.RawData[index];
    }
    inline void setCompressionFormat(const uint32_t value) {
        TheStructure.Common.CompressionFormat30 = value;
    }
    inline uint32_t getCompressionFormat() const {
        return TheStructure.Common.CompressionFormat30;
    }
    inline void setFillType(const FILL_TYPE value) {
        TheStructure.Common.FillType = value;
    }
    inline FILL_TYPE getFillType() const {
        return static_cast<FILL_TYPE>(TheStructure.Common.FillType);
    }
    inline void setMode(const MODE value) {
        TheStructure.Common.Mode = value;
    }
    inline MODE getMode() const {
        return static_cast<MODE>(TheStructure.Common.Mode);
    }
    inline void setInstructionTargetOpcode(const INSTRUCTION_TARGETOPCODE value) {
        TheStructure.Common.InstructionTarget_Opcode = value;
    }
    inline INSTRUCTION_TARGETOPCODE getInstructionTargetOpcode() const {
        return static_cast<INSTRUCTION_TARGETOPCODE>(TheStructure.Common.InstructionTarget_Opcode);
    }
    inline void setClient(const CLIENT value) {
        TheStructure.Common.Client = value;
    }
    inline CLIENT getClient() const {
        return static_cast<CLIENT>(TheStructure.Common.Client);
    }
    inline void setFillWidth(const uint32_t value) {
        UNRECOVERABLE_IF(value - 1 > 0xffffff);
        TheStructure.Common.FillWidth = value - 1;
    }
    inline uint32_t getFillWidth() const {
        return TheStructure.Common.FillWidth + 1;
    }
    inline void setFillHeight(const uint32_t value) {
        UNRECOVERABLE_IF(value - 1 > 0x3ffff);
        TheStructure.Common.FillHeight = value - 1;
    }
    inline uint32_t getFillHeight() const {
        return TheStructure.Common.FillHeight + 1;
    }
    inline void setDestinationPitch(const uint32_t value) {
        UNRECOVERABLE_IF(value - 1 > 0x3ffff);
        TheStructure.Common.DestinationPitch = value - 1;
    }
    inline uint32_t getDestinationPitch() const {
        return TheStructure.Common.DestinationPitch + 1;
    }
    inline void setDestinationStartAddress(const uint64_t value) { // patched
        TheStructure.Common.DestinationStartAddressLow = value & 0xffffffff;
        TheStructure.Common.DestinationStartAddressHigh = (value >> 32) & 0xffffffff;
    }
    inline uint64_t getDestinationStartAddress() const { // patched
        return (static_cast<uint64_t>(TheStructure.Common.DestinationStartAddressHigh) << 32) | (TheStructure.Common.DestinationStartAddressLow);
    }
    inline void setDestinationEncryptEn(const DESTINATION_ENCRYPT_EN value) {
        TheStructure.Common.DestinationEncryptEn = value;
    }
    inline DESTINATION_ENCRYPT_EN getDestinationEncryptEn() const {
        return static_cast<DESTINATION_ENCRYPT_EN>(TheStructure.Common.DestinationEncryptEn);
    }
    inline void setDestinationMOCS(const uint32_t value) { // patched
        setDestinationEncryptEn(static_cast<DESTINATION_ENCRYPT_EN>(value & 1));
        UNRECOVERABLE_IF((value >> 1) > 0xf);
        TheStructure.Common.DestinationMocs = value >> 1;
    }
    inline uint32_t getDestinationMOCS() const { // patched
        return (TheStructure.Common.DestinationMocs << 1) | static_cast<uint32_t>(getDestinationEncryptEn());
    }
    inline void setFillData(const uint32_t value) {
        UNRECOVERABLE_IF(value > 0xff);
        TheStructure.Common.FillData = value;
    }
    inline uint32_t getFillData() const {
        return TheStructure.Common.FillData;
    }
} MEM_SET;
STATIC_ASSERT(28 == sizeof(MEM_SET));

typedef struct tagSTATE_SIP {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 15);
            uint32_t _3DCommandSubOpcode : BITFIELD_RANGE(16, 23);
            uint32_t _3DCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t CommandSubtype : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint64_t Reserved_32 : BITFIELD_RANGE(0, 3);
            uint64_t SystemInstructionPointer : BITFIELD_RANGE(4, 63);
        } Common;
        uint32_t RawData[3];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_DWORD_COUNT_N = 0x1,
    } DWORD_LENGTH;
    typedef enum tag_3D_COMMAND_SUB_OPCODE {
        _3D_COMMAND_SUB_OPCODE_STATE_SIP = 0x2,
    } _3D_COMMAND_SUB_OPCODE;
    typedef enum tag_3D_COMMAND_OPCODE {
        _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED = 0x1,
    } _3D_COMMAND_OPCODE;
    typedef enum tagCOMMAND_SUBTYPE {
        COMMAND_SUBTYPE_GFXPIPE_COMMON = 0x0,
    } COMMAND_SUBTYPE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_GFXPIPE = 0x3,
    } COMMAND_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_DWORD_COUNT_N;
        TheStructure.Common._3DCommandSubOpcode = _3D_COMMAND_SUB_OPCODE_STATE_SIP;
        TheStructure.Common._3DCommandOpcode = _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED;
        TheStructure.Common.CommandSubtype = COMMAND_SUBTYPE_GFXPIPE_COMMON;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
    }
    static tagSTATE_SIP sInit() {
        STATE_SIP state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 3);
        return TheStructure.RawData[index];
    }
    typedef enum tagSYSTEMINSTRUCTIONPOINTER {
        SYSTEMINSTRUCTIONPOINTER_BIT_SHIFT = 0x4,
        SYSTEMINSTRUCTIONPOINTER_ALIGN_SIZE = 0x10,
    } SYSTEMINSTRUCTIONPOINTER;
    inline void setSystemInstructionPointer(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0xffffffffffffffffL);
        TheStructure.Common.SystemInstructionPointer = value >> SYSTEMINSTRUCTIONPOINTER_BIT_SHIFT;
    }
    inline uint64_t getSystemInstructionPointer() const {
        return TheStructure.Common.SystemInstructionPointer << SYSTEMINSTRUCTIONPOINTER_BIT_SHIFT;
    }
} STATE_SIP;
STATIC_ASSERT(12 == sizeof(STATE_SIP));

typedef struct tagSAMPLER_BORDER_COLOR_STATE {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            float BorderColorRed;
            // DWORD 1
            float BorderColorGreen;
            // DWORD 2
            float BorderColorBlue;
            // DWORD 3
            float BorderColorAlpha;
        } Common;
        uint32_t RawData[4];
    } TheStructure;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.BorderColorRed = 0.0;
        TheStructure.Common.BorderColorGreen = 0.0;
        TheStructure.Common.BorderColorBlue = 0.0;
        TheStructure.Common.BorderColorAlpha = 0.0;
    }
    static tagSAMPLER_BORDER_COLOR_STATE sInit() {
        SAMPLER_BORDER_COLOR_STATE state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 4);
        return TheStructure.RawData[index];
    }
    inline void setBorderColorRed(const float value) {
        TheStructure.Common.BorderColorRed = value;
    }
    inline float getBorderColorRed() const {
        return TheStructure.Common.BorderColorRed;
    }
    inline void setBorderColorGreen(const float value) {
        TheStructure.Common.BorderColorGreen = value;
    }
    inline float getBorderColorGreen() const {
        return TheStructure.Common.BorderColorGreen;
    }
    inline void setBorderColorBlue(const float value) {
        TheStructure.Common.BorderColorBlue = value;
    }
    inline float getBorderColorBlue() const {
        return TheStructure.Common.BorderColorBlue;
    }
    inline void setBorderColorAlpha(const float value) {
        TheStructure.Common.BorderColorAlpha = value;
    }
    inline float getBorderColorAlpha() const {
        return TheStructure.Common.BorderColorAlpha;
    }
} SAMPLER_BORDER_COLOR_STATE;
STATIC_ASSERT(16 == sizeof(SAMPLER_BORDER_COLOR_STATE));

typedef struct tagSTATE_CONTEXT_DATA_BASE_ADDRESS {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 15);
            uint32_t _3DCommandSubOpcode : BITFIELD_RANGE(16, 23);
            uint32_t _3DCommandOpcode : BITFIELD_RANGE(24, 26);
            uint32_t CommandSubtype : BITFIELD_RANGE(27, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint64_t Reserved_32 : BITFIELD_RANGE(0, 11);
            uint64_t ContextDataBaseAddress : BITFIELD_RANGE(12, 63);
        } Common;
        uint32_t RawData[3];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_LENGTH_1 = 0x1,
    } DWORD_LENGTH;
    typedef enum tag_3D_COMMAND_SUB_OPCODE {
        _3D_COMMAND_SUB_OPCODE_STATE_CONTEXT_DATA_BASE_ADDRESS = 0xb,
    } _3D_COMMAND_SUB_OPCODE;
    typedef enum tag_3D_COMMAND_OPCODE {
        _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED = 0x1,
    } _3D_COMMAND_OPCODE;
    typedef enum tagCOMMAND_SUBTYPE {
        COMMAND_SUBTYPE_GFXPIPE_COMMON_ = 0x0,
    } COMMAND_SUBTYPE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_GFXPIPE = 0x3,
    } COMMAND_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_LENGTH_1;
        TheStructure.Common._3DCommandSubOpcode = _3D_COMMAND_SUB_OPCODE_STATE_CONTEXT_DATA_BASE_ADDRESS;
        TheStructure.Common._3DCommandOpcode = _3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED;
        TheStructure.Common.CommandSubtype = COMMAND_SUBTYPE_GFXPIPE_COMMON_;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFXPIPE;
    }
    static tagSTATE_CONTEXT_DATA_BASE_ADDRESS sInit() {
        STATE_CONTEXT_DATA_BASE_ADDRESS state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 3);
        return TheStructure.RawData[index];
    }
    typedef enum tagCONTEXTDATABASEADDRESS {
        CONTEXTDATABASEADDRESS_BIT_SHIFT = 0xc,
        CONTEXTDATABASEADDRESS_ALIGN_SIZE = 0x1000,
    } CONTEXTDATABASEADDRESS;
    inline void setContextDataBaseAddress(const uint64_t value) {
        UNRECOVERABLE_IF(value > 0xffffffffffffffffL);
        TheStructure.Common.ContextDataBaseAddress = value >> CONTEXTDATABASEADDRESS_BIT_SHIFT;
    }
    inline uint64_t getContextDataBaseAddress() const {
        return TheStructure.Common.ContextDataBaseAddress << CONTEXTDATABASEADDRESS_BIT_SHIFT;
    }
} STATE_CONTEXT_DATA_BASE_ADDRESS;
STATIC_ASSERT(12 == sizeof(STATE_CONTEXT_DATA_BASE_ADDRESS));

typedef struct tagRESOURCE_BARRIER {
    union tagTheStructure {
        struct tagCommon {
            // DWORD 0
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 23);
            uint32_t PredicateEnable : BITFIELD_RANGE(24, 24);
            uint32_t Reserved_25 : BITFIELD_RANGE(25, 25);
            uint32_t Opcode : BITFIELD_RANGE(26, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            // DWORD 1
            uint32_t WaitStage : BITFIELD_RANGE(0, 11);
            uint32_t SignalStage : BITFIELD_RANGE(12, 23);
            uint32_t Reserved_56 : BITFIELD_RANGE(24, 29);
            uint32_t BarrierType : BITFIELD_RANGE(30, 31);
            // DWORD 2
            uint32_t Reserved_64 : BITFIELD_RANGE(0, 20);
            uint32_t L1DataportCacheInvalidate : BITFIELD_RANGE(21, 21);
            uint32_t DepthCache : BITFIELD_RANGE(22, 22);
            uint32_t ColorCache : BITFIELD_RANGE(23, 23);
            uint32_t L1DataportUavFlush : BITFIELD_RANGE(24, 24);
            uint32_t Texture_Ro : BITFIELD_RANGE(25, 25);
            uint32_t State_Ro : BITFIELD_RANGE(26, 26);
            uint32_t Vf_Ro : BITFIELD_RANGE(27, 27);
            uint32_t Amfs : BITFIELD_RANGE(28, 28);
            uint32_t ConstantCache : BITFIELD_RANGE(29, 29);
            uint32_t Reserved_94 : BITFIELD_RANGE(30, 31);
            // DWORD 3
            uint64_t Reserved_96 : BITFIELD_RANGE(0, 2);
            uint64_t BarrierIdAddress : BITFIELD_RANGE(3, 63);
        } Common;
        uint32_t RawData[5];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_DWORD_COUNT_N = 0x3,
    } DWORD_LENGTH;
    typedef enum tagOPCODE {
        OPCODE_RESOURCE_BARRIER = 0x3,
    } OPCODE;
    typedef enum tagCOMMAND_TYPE {
        COMMAND_TYPE_GFX_EXEC = 0x5,
    } COMMAND_TYPE;
    typedef enum tagWAIT_STAGE {
        WAIT_STAGE_NONE = 0x0,
        WAIT_STAGE_TOP = 0x1,
        WAIT_STAGE_COLOR = 0x2,
        WAIT_STAGE_GPGPU = 0x4,
        WAIT_STAGE_COLOR_AND_COMPUTE = 0x6,
        WAIT_STAGE_GEOM = 0x10,
        WAIT_STAGE_GEOMETRY_AND_COMPUTE = 0x14,
        WAIT_STAGE_RASTER = 0x20,
        WAIT_STAGE_DEPTH = 0x40,
        WAIT_STAGE_PIXEL = 0x80,
    } WAIT_STAGE;
    typedef enum tagSIGNAL_STAGE {
        SIGNAL_STAGE_NONE = 0x0,
        SIGNAL_STAGE_TOP = 0x1,
        SIGNAL_STAGE_COLOR = 0x2,
        SIGNAL_STAGE_GPGPU = 0x4,
        SIGNAL_STAGE_COLOR_AND_COMPUTE = 0x6,
        SIGNAL_STAGE_GEOM = 0x10,
        SIGNAL_STAGE_GEOMETRY_AND_COMPUTE = 0x14,
        SIGNAL_STAGE_RASTER = 0x20,
        SIGNAL_STAGE_DEPTH = 0x40,
        SIGNAL_STAGE_PIXEL = 0x80,
    } SIGNAL_STAGE;
    typedef enum tagBARRIER_TYPE {
        BARRIER_TYPE_IMMEDIATE = 0x0,
        BARRIER_TYPE_SIGNAL = 0x1,
        BARRIER_TYPE_WAIT = 0x2,
        BARRIER_TYPE_UAV = 0x3,
    } BARRIER_TYPE;
    inline void init() {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_DWORD_COUNT_N;
        TheStructure.Common.Opcode = OPCODE_RESOURCE_BARRIER;
        TheStructure.Common.CommandType = COMMAND_TYPE_GFX_EXEC;
        TheStructure.Common.WaitStage = WAIT_STAGE_NONE;
        TheStructure.Common.SignalStage = SIGNAL_STAGE_NONE;
        TheStructure.Common.BarrierType = BARRIER_TYPE_IMMEDIATE;
    }
    static tagRESOURCE_BARRIER sInit() {
        RESOURCE_BARRIER state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        UNRECOVERABLE_IF(index >= 5);
        return TheStructure.RawData[index];
    }
    inline void setPredicateEnable(const bool value) {
        TheStructure.Common.PredicateEnable = value;
    }
    inline bool getPredicateEnable() const {
        return TheStructure.Common.PredicateEnable;
    }
    inline void setOpcode(const OPCODE value) {
        TheStructure.Common.Opcode = value;
    }
    inline OPCODE getOpcode() const {
        return static_cast<OPCODE>(TheStructure.Common.Opcode);
    }
    inline void setWaitStage(const WAIT_STAGE value) {
        TheStructure.Common.WaitStage = value;
    }
    inline WAIT_STAGE getWaitStage() const {
        return static_cast<WAIT_STAGE>(TheStructure.Common.WaitStage);
    }
    inline void setSignalStage(const SIGNAL_STAGE value) {
        TheStructure.Common.SignalStage = value;
    }
    inline SIGNAL_STAGE getSignalStage() const {
        return static_cast<SIGNAL_STAGE>(TheStructure.Common.SignalStage);
    }
    inline void setBarrierType(const BARRIER_TYPE value) {
        TheStructure.Common.BarrierType = value;
    }
    inline BARRIER_TYPE getBarrierType() const {
        return static_cast<BARRIER_TYPE>(TheStructure.Common.BarrierType);
    }
    inline void setL1DataportCacheInvalidate(const bool value) {
        TheStructure.Common.L1DataportCacheInvalidate = value;
    }
    inline bool getL1DataportCacheInvalidate() const {
        return TheStructure.Common.L1DataportCacheInvalidate;
    }
    inline void setDepthCache(const bool value) {
        TheStructure.Common.DepthCache = value;
    }
    inline bool getDepthCache() const {
        return TheStructure.Common.DepthCache;
    }
    inline void setColorCache(const bool value) {
        TheStructure.Common.ColorCache = value;
    }
    inline bool getColorCache() const {
        return TheStructure.Common.ColorCache;
    }
    inline void setL1DataportUavFlush(const bool value) {
        TheStructure.Common.L1DataportUavFlush = value;
    }
    inline bool getL1DataportUavFlush() const {
        return TheStructure.Common.L1DataportUavFlush;
    }
    inline void setTextureRo(const bool value) {
        TheStructure.Common.Texture_Ro = value;
    }
    inline bool getTextureRo() const {
        return TheStructure.Common.Texture_Ro;
    }
    inline void setStateRo(const bool value) {
        TheStructure.Common.State_Ro = value;
    }
    inline bool getStateRo() const {
        return TheStructure.Common.State_Ro;
    }
    inline void setVfRo(const bool value) {
        TheStructure.Common.Vf_Ro = value;
    }
    inline bool getVfRo() const {
        return TheStructure.Common.Vf_Ro;
    }
    inline void setAmfs(const bool value) {
        TheStructure.Common.Amfs = value;
    }
    inline bool getAmfs() const {
        return TheStructure.Common.Amfs;
    }
    inline void setConstantCache(const bool value) {
        TheStructure.Common.ConstantCache = value;
    }
    inline bool getConstantCache() const {
        return TheStructure.Common.ConstantCache;
    }
    typedef enum tagBARRIERIDADDRESS {
        BARRIERIDADDRESS_BIT_SHIFT = 0x3,
        BARRIERIDADDRESS_ALIGN_SIZE = 0x8,
    } BARRIERIDADDRESS;
    inline void setBarrierIdAddress(const uint64_t value) {
        TheStructure.Common.BarrierIdAddress = value >> BARRIERIDADDRESS_BIT_SHIFT;
    }
    inline uint64_t getBarrierIdAddress() const {
        return TheStructure.Common.BarrierIdAddress << BARRIERIDADDRESS_BIT_SHIFT;
    }
} RESOURCE_BARRIER;
STATIC_ASSERT(20 == sizeof(RESOURCE_BARRIER));

#pragma pack()
