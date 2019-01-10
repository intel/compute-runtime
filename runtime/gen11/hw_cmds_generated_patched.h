/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#pragma pack(1)
typedef struct tagMI_BATCH_BUFFER_START {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t AddressSpaceIndicator : BITFIELD_RANGE(8, 8);
            uint32_t Reserved_9 : BITFIELD_RANGE(9, 9);
            uint32_t ResourceStreamerEnable : BITFIELD_RANGE(10, 10);
            uint32_t Reserved_11 : BITFIELD_RANGE(11, 14);
            uint32_t PredicationEnable : BITFIELD_RANGE(15, 15);
            uint32_t Reserved_16 : BITFIELD_RANGE(16, 19);
            uint32_t PoshEnable : BITFIELD_RANGE(20, 20);
            uint32_t PoshStart : BITFIELD_RANGE(21, 21);
            uint32_t SecondLevelBatchBuffer : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint64_t Reserved_32 : BITFIELD_RANGE(0, 1);
            uint64_t BatchBufferStartAddress_Graphicsaddress39_2 : BITFIELD_RANGE(2, 47);
            uint64_t BatchBufferStartAddress_Reserved : BITFIELD_RANGE(48, 63);
        } Common;
        uint32_t RawData[3];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x1,
    } DWORD_LENGTH;
    typedef enum tagADDRESS_SPACE_INDICATOR {
        ADDRESS_SPACE_INDICATOR_GGTT = 0x0,
        ADDRESS_SPACE_INDICATOR_PPGTT = 0x1,
    } ADDRESS_SPACE_INDICATOR;
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
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.AddressSpaceIndicator = ADDRESS_SPACE_INDICATOR_GGTT;
        TheStructure.Common.SecondLevelBatchBuffer = SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_BATCH_BUFFER_START;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_BATCH_BUFFER_START sInit(void) {
        MI_BATCH_BUFFER_START state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 3);
        return TheStructure.RawData[index];
    }
    inline void setAddressSpaceIndicator(const ADDRESS_SPACE_INDICATOR value) {
        TheStructure.Common.AddressSpaceIndicator = value;
    }
    inline ADDRESS_SPACE_INDICATOR getAddressSpaceIndicator(void) const {
        return static_cast<ADDRESS_SPACE_INDICATOR>(TheStructure.Common.AddressSpaceIndicator);
    }
    inline void setResourceStreamerEnable(const bool value) {
        TheStructure.Common.ResourceStreamerEnable = value;
    }
    inline bool getResourceStreamerEnable(void) const {
        return (TheStructure.Common.ResourceStreamerEnable);
    }
    inline void setPredicationEnable(const uint32_t value) {
        TheStructure.Common.PredicationEnable = value;
    }
    inline uint32_t getPredicationEnable(void) const {
        return (TheStructure.Common.PredicationEnable);
    }
    inline void setPoshEnable(const uint32_t value) {
        TheStructure.Common.PoshEnable = value;
    }
    inline uint32_t getPoshEnable(void) const {
        return (TheStructure.Common.PoshEnable);
    }
    inline void setPoshStart(const uint32_t value) {
        TheStructure.Common.PoshStart = value;
    }
    inline uint32_t getPoshStart(void) const {
        return (TheStructure.Common.PoshStart);
    }
    inline void setSecondLevelBatchBuffer(const SECOND_LEVEL_BATCH_BUFFER value) {
        TheStructure.Common.SecondLevelBatchBuffer = value;
    }
    inline SECOND_LEVEL_BATCH_BUFFER getSecondLevelBatchBuffer(void) const {
        return static_cast<SECOND_LEVEL_BATCH_BUFFER>(TheStructure.Common.SecondLevelBatchBuffer);
    }
    typedef enum tagBATCHBUFFERSTARTADDRESS_GRAPHICSADDRESS39_2 {
        BATCHBUFFERSTARTADDRESS_GRAPHICSADDRESS39_2_BIT_SHIFT = 0x2,
        BATCHBUFFERSTARTADDRESS_GRAPHICSADDRESS39_2_ALIGN_SIZE = 0x4,
    } BATCHBUFFERSTARTADDRESS_GRAPHICSADDRESS39_2;
    inline void setBatchBufferStartAddressGraphicsaddress392(const uint64_t value) {
        TheStructure.Common.BatchBufferStartAddress_Graphicsaddress39_2 = value >> BATCHBUFFERSTARTADDRESS_GRAPHICSADDRESS39_2_BIT_SHIFT;
    }
    inline uint64_t getBatchBufferStartAddressGraphicsaddress392(void) const {
        return (TheStructure.Common.BatchBufferStartAddress_Graphicsaddress39_2 << BATCHBUFFERSTARTADDRESS_GRAPHICSADDRESS39_2_BIT_SHIFT);
    }
    inline void setBatchBufferStartAddressGraphicsaddress472(const uint64_t value) {
        setBatchBufferStartAddressGraphicsaddress392(value);
    }
    inline uint64_t getBatchBufferStartAddressGraphicsaddress472(void) const {
        return getBatchBufferStartAddressGraphicsaddress392();
    }
    typedef enum tagBATCHBUFFERSTARTADDRESS_RESERVED {
        BATCHBUFFERSTARTADDRESS_RESERVED_BIT_SHIFT = 0x2,
        BATCHBUFFERSTARTADDRESS_RESERVED_ALIGN_SIZE = 0x4,
    } BATCHBUFFERSTARTADDRESS_RESERVED;
    inline void setBatchBufferStartAddressReserved(const uint64_t value) {
        TheStructure.Common.BatchBufferStartAddress_Reserved = value >> BATCHBUFFERSTARTADDRESS_RESERVED_BIT_SHIFT;
    }
    inline uint64_t getBatchBufferStartAddressReserved(void) const {
        return (TheStructure.Common.BatchBufferStartAddress_Reserved << BATCHBUFFERSTARTADDRESS_RESERVED_BIT_SHIFT);
    }
} MI_BATCH_BUFFER_START;
STATIC_ASSERT(12 == sizeof(MI_BATCH_BUFFER_START));
typedef struct tagMEDIA_SURFACE_STATE {
    union tagTheStructure {
        struct tagCommon {
            uint32_t Reserved_0 : BITFIELD_RANGE(0, 29);
            uint32_t Rotation : BITFIELD_RANGE(30, 31);
            uint32_t Cr_VCb_UPixelOffsetVDirection : BITFIELD_RANGE(0, 1);
            uint32_t PictureStructure : BITFIELD_RANGE(2, 3);
            uint32_t Width : BITFIELD_RANGE(4, 17);
            uint32_t Height : BITFIELD_RANGE(18, 31);
            uint32_t TileMode : BITFIELD_RANGE(0, 1);
            uint32_t HalfPitchForChroma : BITFIELD_RANGE(2, 2);
            uint32_t SurfacePitch : BITFIELD_RANGE(3, 20);
            uint32_t AddressControl : BITFIELD_RANGE(21, 21);
            uint32_t MemoryCompressionEnable : BITFIELD_RANGE(22, 22);
            uint32_t MemoryCompressionMode : BITFIELD_RANGE(23, 23);
            uint32_t Cr_VCb_UPixelOffsetVDirectionMsb : BITFIELD_RANGE(24, 24);
            uint32_t Cr_VCb_UPixelOffsetUDirection : BITFIELD_RANGE(25, 25);
            uint32_t InterleaveChroma : BITFIELD_RANGE(26, 26);
            uint32_t SurfaceFormat : BITFIELD_RANGE(27, 31);
            uint32_t YOffsetForU_Cb : BITFIELD_RANGE(0, 13);
            uint32_t Reserved_110 : BITFIELD_RANGE(14, 15);
            uint32_t XOffsetForU_Cb : BITFIELD_RANGE(16, 29);
            uint32_t Reserved_126 : BITFIELD_RANGE(30, 31);
            uint32_t Reserved_128;
            uint32_t SurfaceMemoryObjectControlState_Reserved : BITFIELD_RANGE(0, 0);
            uint32_t SurfaceMemoryObjectControlState_IndexToMocsTables : BITFIELD_RANGE(1, 6);
            uint32_t Reserved_167 : BITFIELD_RANGE(7, 17);
            uint32_t TiledResourceMode : BITFIELD_RANGE(18, 19);
            uint32_t Depth : BITFIELD_RANGE(20, 23);
            uint32_t Reserved_184 : BITFIELD_RANGE(24, 29);
            uint32_t VerticalLineStrideOffset : BITFIELD_RANGE(30, 30);
            uint32_t VerticalLineStride : BITFIELD_RANGE(31, 31);
            uint32_t SurfaceBaseAddressLow;
            uint32_t SurfaceBaseAddressHigh : BITFIELD_RANGE(0, 15);
            uint32_t Reserved_240 : BITFIELD_RANGE(16, 31);
        } Common;
        struct tagSurfaceFormatIsNotOneOfPlanarFormats {
            uint32_t Reserved_0;
            uint32_t Reserved_32;
            uint32_t Reserved_64;
            uint32_t Reserved_96;
            uint32_t Reserved_128;
            uint32_t Reserved_160;
            uint32_t Reserved_192;
            uint32_t Reserved_224;
        } SurfaceFormatIsNotOneOfPlanarFormats;
        struct tagSurfaceFormatIsOneOfPlanarFormats {
            uint32_t Reserved_0 : BITFIELD_RANGE(0, 15);
            uint32_t YOffset : BITFIELD_RANGE(16, 19);
            uint32_t XOffset : BITFIELD_RANGE(20, 26);
            uint32_t Reserved_27 : BITFIELD_RANGE(27, 31);
            uint32_t Reserved_32;
            uint32_t Reserved_64;
            uint32_t Reserved_96;
            uint32_t Reserved_128;
            uint32_t Reserved_160;
            uint32_t Reserved_192;
            uint32_t Reserved_224;
        } SurfaceFormatIsOneOfPlanarFormats;
        struct tag_SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0 {
            uint32_t Reserved_0;
            uint32_t Reserved_32;
            uint32_t Reserved_64;
            uint32_t Reserved_96;
            uint32_t YOffsetForV_Cr : BITFIELD_RANGE(0, 14);
            uint32_t Reserved_143 : BITFIELD_RANGE(15, 15);
            uint32_t XOffsetForV_Cr : BITFIELD_RANGE(16, 29);
            uint32_t Reserved_158 : BITFIELD_RANGE(30, 31);
            uint32_t Reserved_160;
            uint32_t Reserved_192;
            uint32_t Reserved_224;
        } _SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0;
        uint32_t RawData[8];
    } TheStructure;
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
        TILE_MODE_TILEMODE_XMAJOR = 0x2,
        TILE_MODE_TILEMODE_YMAJOR = 0x3,
    } TILE_MODE;
    typedef enum tagADDRESS_CONTROL {
        ADDRESS_CONTROL_CLAMP = 0x0,
        ADDRESS_CONTROL_MIRROR = 0x1,
    } ADDRESS_CONTROL;
    typedef enum tagMEMORY_COMPRESSION_MODE {
        MEMORY_COMPRESSION_MODE_HORIZONTAL_COMPRESSION_MODE = 0x0,
        MEMORY_COMPRESSION_MODE_VERTICAL_COMPRESSION_MODE = 0x1,
    } MEMORY_COMPRESSION_MODE;
    typedef enum tagSURFACE_FORMAT {
        SURFACE_FORMAT_YCRCB_NORMAL = 0x0,
        SURFACE_FORMAT_YCRCB_SWAPUVY = 0x1,
        SURFACE_FORMAT_YCRCB_SWAPUV = 0x2,
        SURFACE_FORMAT_YCRCB_SWAPY = 0x3,
        SURFACE_FORMAT_PLANAR_420_8 = 0x4,
        SURFACE_FORMAT_Y8_UNORM_VA = 0x5,
        SURFACE_FORMAT_Y16_SNORM = 0x6,
        SURFACE_FORMAT_Y16_UNORM_VA = 0x7,
        SURFACE_FORMAT_R10G10B10A2_UNORM = 0x8,
        SURFACE_FORMAT_R8G8B8A8_UNORM = 0x9,
        SURFACE_FORMAT_R8B8_UNORM_CRCB = 0xa,
        SURFACE_FORMAT_R8_UNORM_CR_CB = 0xb,
        SURFACE_FORMAT_Y8_UNORM = 0xc,
        SURFACE_FORMAT_A8Y8U8V8_UNORM = 0xd,
        SURFACE_FORMAT_B8G8R8A8_UNORM = 0xe,
        SURFACE_FORMAT_R16G16B16A16 = 0xf,
        SURFACE_FORMAT_Y1_UNORM = 0x10,
        SURFACE_FORMAT_Y32_UNORM = 0x11,
        SURFACE_FORMAT_PLANAR_422_8 = 0x12,
        SURFACE_FORMAT_FM_STRBUF_Y1 = 0x13,
        SURFACE_FORMAT_FM_STRBUF_Y8 = 0x14,
        SURFACE_FORMAT_FM_STRBUF_Y16 = 0x15,
        SURFACE_FORMAT_FM_STRBUF_Y32 = 0x16,
        SURFACE_FORMAT_PLANAR_420_16 = 0x17,
        SURFACE_FORMAT_R16B16_UNORM_CRCB = 0x18,
        SURFACE_FORMAT_R16_UNORM_CR_CB = 0x19,
        SURFACE_FORMAT_Y16_UNORM = 0x1a,
    } SURFACE_FORMAT;
    typedef enum tagSURFACE_MEMORY_OBJECT_CONTROL_STATE {
        SURFACE_MEMORY_OBJECT_CONTROL_STATE_DEFAULTVAUEDESC = 0x0,
    } SURFACE_MEMORY_OBJECT_CONTROL_STATE;
    typedef enum tagTILED_RESOURCE_MODE {
        TILED_RESOURCE_MODE_TRMODE_NONE = 0x0,
        TILED_RESOURCE_MODE_TRMODE_TILEYF = 0x1,
        TILED_RESOURCE_MODE_TRMODE_TILEYS = 0x2,
    } TILED_RESOURCE_MODE;
    typedef enum tagPATCH_CONSTANTS {
        SURFACEBASEADDRESS_BYTEOFFSET = 0x18,
        SURFACEBASEADDRESS_INDEX = 0x6,
        SURFACEBASEADDRESSHIGH_BYTEOFFSET = 0x1c,
        SURFACEBASEADDRESSHIGH_INDEX = 0x7,
    } PATCH_CONSTANTS;
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.Rotation = ROTATION_NO_ROTATION_OR_0_DEGREE;
        TheStructure.Common.PictureStructure = PICTURE_STRUCTURE_FRAME_PICTURE;
        TheStructure.Common.TileMode = TILE_MODE_TILEMODE_LINEAR;
        TheStructure.Common.AddressControl = ADDRESS_CONTROL_CLAMP;
        TheStructure.Common.MemoryCompressionMode = MEMORY_COMPRESSION_MODE_HORIZONTAL_COMPRESSION_MODE;
        TheStructure.Common.SurfaceFormat = SURFACE_FORMAT_YCRCB_NORMAL;
        TheStructure.Common.TiledResourceMode = TILED_RESOURCE_MODE_TRMODE_NONE;
    }
    static tagMEDIA_SURFACE_STATE sInit(void) {
        MEDIA_SURFACE_STATE state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 8);
        return TheStructure.RawData[index];
    }
    inline void setRotation(const ROTATION value) {
        TheStructure.Common.Rotation = value;
    }
    inline ROTATION getRotation(void) const {
        return static_cast<ROTATION>(TheStructure.Common.Rotation);
    }
    inline void setCrVCbUPixelOffsetVDirection(const uint32_t value) {
        TheStructure.Common.Cr_VCb_UPixelOffsetVDirection = value;
    }
    inline uint32_t getCrVCbUPixelOffsetVDirection(void) const {
        return (TheStructure.Common.Cr_VCb_UPixelOffsetVDirection);
    }
    inline void setPictureStructure(const PICTURE_STRUCTURE value) {
        TheStructure.Common.PictureStructure = value;
    }
    inline PICTURE_STRUCTURE getPictureStructure(void) const {
        return static_cast<PICTURE_STRUCTURE>(TheStructure.Common.PictureStructure);
    }
    inline void setWidth(const uint32_t value) {
        TheStructure.Common.Width = value - 1;
    }
    inline uint32_t getWidth(void) const {
        return (TheStructure.Common.Width + 1);
    }
    inline void setHeight(const uint32_t value) {
        TheStructure.Common.Height = value - 1;
    }
    inline uint32_t getHeight(void) const {
        return (TheStructure.Common.Height + 1);
    }
    inline void setTileMode(const TILE_MODE value) {
        TheStructure.Common.TileMode = value;
    }
    inline TILE_MODE getTileMode(void) const {
        return static_cast<TILE_MODE>(TheStructure.Common.TileMode);
    }
    inline void setHalfPitchForChroma(const bool value) {
        TheStructure.Common.HalfPitchForChroma = value;
    }
    inline bool getHalfPitchForChroma(void) const {
        return (TheStructure.Common.HalfPitchForChroma);
    }
    inline void setSurfacePitch(const uint32_t value) {
        TheStructure.Common.SurfacePitch = value - 1;
    }
    inline uint32_t getSurfacePitch(void) const {
        return (TheStructure.Common.SurfacePitch + 1);
    }
    inline void setAddressControl(const ADDRESS_CONTROL value) {
        TheStructure.Common.AddressControl = value;
    }
    inline ADDRESS_CONTROL getAddressControl(void) const {
        return static_cast<ADDRESS_CONTROL>(TheStructure.Common.AddressControl);
    }
    inline void setMemoryCompressionEnable(const bool value) {
        TheStructure.Common.MemoryCompressionEnable = value;
    }
    inline bool getMemoryCompressionEnable(void) const {
        return (TheStructure.Common.MemoryCompressionEnable);
    }
    inline void setMemoryCompressionMode(const MEMORY_COMPRESSION_MODE value) {
        TheStructure.Common.MemoryCompressionMode = value;
    }
    inline MEMORY_COMPRESSION_MODE getMemoryCompressionMode(void) const {
        return static_cast<MEMORY_COMPRESSION_MODE>(TheStructure.Common.MemoryCompressionMode);
    }
    inline void setCrVCbUPixelOffsetVDirectionMsb(const uint32_t value) {
        TheStructure.Common.Cr_VCb_UPixelOffsetVDirectionMsb = value;
    }
    inline uint32_t getCrVCbUPixelOffsetVDirectionMsb(void) const {
        return (TheStructure.Common.Cr_VCb_UPixelOffsetVDirectionMsb);
    }
    inline void setCrVCbUPixelOffsetUDirection(const uint32_t value) {
        TheStructure.Common.Cr_VCb_UPixelOffsetUDirection = value;
    }
    inline uint32_t getCrVCbUPixelOffsetUDirection(void) const {
        return (TheStructure.Common.Cr_VCb_UPixelOffsetUDirection);
    }
    inline void setInterleaveChroma(const bool value) {
        TheStructure.Common.InterleaveChroma = value;
    }
    inline bool getInterleaveChroma(void) const {
        return (TheStructure.Common.InterleaveChroma);
    }
    inline void setSurfaceFormat(const SURFACE_FORMAT value) {
        TheStructure.Common.SurfaceFormat = value;
    }
    inline SURFACE_FORMAT getSurfaceFormat(void) const {
        return static_cast<SURFACE_FORMAT>(TheStructure.Common.SurfaceFormat);
    }
    inline void setYOffsetForUCb(const uint32_t value) {
        TheStructure.Common.YOffsetForU_Cb = value;
    }
    inline uint32_t getYOffsetForUCb(void) const {
        return (TheStructure.Common.YOffsetForU_Cb);
    }
    inline void setXOffsetForUCb(const uint32_t value) {
        TheStructure.Common.XOffsetForU_Cb = value;
    }
    inline uint32_t getXOffsetForUCb(void) const {
        return (TheStructure.Common.XOffsetForU_Cb);
    }
    inline void setSurfaceMemoryObjectControlStateReserved(const uint32_t value) {
        TheStructure.Common.SurfaceMemoryObjectControlState_Reserved = value;
    }
    inline uint32_t getSurfaceMemoryObjectControlStateReserved(void) const {
        return (TheStructure.Common.SurfaceMemoryObjectControlState_Reserved);
    }
    inline void setSurfaceMemoryObjectControlStateIndexToMocsTables(const uint32_t value) {
        TheStructure.Common.SurfaceMemoryObjectControlState_IndexToMocsTables = value >> 1;
    }
    inline uint32_t getSurfaceMemoryObjectControlStateIndexToMocsTables(void) const {
        return (TheStructure.Common.SurfaceMemoryObjectControlState_IndexToMocsTables << 1);
    }
    inline void setTiledResourceMode(const TILED_RESOURCE_MODE value) {
        TheStructure.Common.TiledResourceMode = value;
    }
    inline TILED_RESOURCE_MODE getTiledResourceMode(void) const {
        return static_cast<TILED_RESOURCE_MODE>(TheStructure.Common.TiledResourceMode);
    }
    inline void setDepth(const uint32_t value) {
        TheStructure.Common.Depth = value;
    }
    inline uint32_t getDepth(void) const {
        return (TheStructure.Common.Depth);
    }
    inline void setVerticalLineStrideOffset(const uint32_t value) {
        TheStructure.Common.VerticalLineStrideOffset = value;
    }
    inline uint32_t getVerticalLineStrideOffset(void) const {
        return (TheStructure.Common.VerticalLineStrideOffset);
    }
    inline void setVerticalLineStride(const uint32_t value) {
        TheStructure.Common.VerticalLineStride = value;
    }
    inline uint32_t getVerticalLineStride(void) const {
        return (TheStructure.Common.VerticalLineStride);
    }
    inline void setSurfaceBaseAddress(const uint64_t value) {
        TheStructure.Common.SurfaceBaseAddressLow = static_cast<uint32_t>(value & 0xffffffff);
        TheStructure.Common.SurfaceBaseAddressHigh = (value >> 32) & 0xffffffff;
    }
    inline uint64_t getSurfaceBaseAddress(void) const {
        return (TheStructure.Common.SurfaceBaseAddressLow |
                static_cast<uint64_t>(TheStructure.Common.SurfaceBaseAddressHigh) << 32);
    }
    inline void setSurfaceBaseAddressHigh(const uint32_t value) {
        TheStructure.Common.SurfaceBaseAddressHigh = value;
    }
    inline uint32_t getSurfaceBaseAddressHigh(void) const {
        return (TheStructure.Common.SurfaceBaseAddressHigh);
    }
    typedef enum tagYOFFSET {
        YOFFSET_BIT_SHIFT = 0x2,
        YOFFSET_ALIGN_SIZE = 0x4,
    } YOFFSET;
    inline void setYOffset(const uint32_t value) {
        TheStructure.SurfaceFormatIsOneOfPlanarFormats.YOffset = value >> YOFFSET_BIT_SHIFT;
    }
    inline uint32_t getYOffset(void) const {
        return (TheStructure.SurfaceFormatIsOneOfPlanarFormats.YOffset << YOFFSET_BIT_SHIFT);
    }
    typedef enum tagXOFFSET {
        XOFFSET_BIT_SHIFT = 0x2,
        XOFFSET_ALIGN_SIZE = 0x4,
    } XOFFSET;
    inline void setXOffset(const uint32_t value) {
        TheStructure.SurfaceFormatIsOneOfPlanarFormats.XOffset = value >> XOFFSET_BIT_SHIFT;
    }
    inline uint32_t getXOffset(void) const {
        return (TheStructure.SurfaceFormatIsOneOfPlanarFormats.XOffset << XOFFSET_BIT_SHIFT);
    }
    inline void setYOffsetForVCr(const uint32_t value) {
        TheStructure._SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0.YOffsetForV_Cr = value;
    }
    inline uint32_t getYOffsetForVCr(void) const {
        return (TheStructure._SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0.YOffsetForV_Cr);
    }
    inline void setXOffsetForVCr(const uint32_t value) {
        TheStructure._SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0.XOffsetForV_Cr = value;
    }
    inline uint32_t getXOffsetForVCr(void) const {
        return (TheStructure._SurfaceFormatIsOneOfPlanarAnd_InterleaveChromaIs0.XOffsetForV_Cr);
    }
} MEDIA_SURFACE_STATE;
STATIC_ASSERT(32 == sizeof(MEDIA_SURFACE_STATE));
typedef struct tagMI_MATH {
    union _DW0 {
        struct _BitField {
            uint32_t DwordLength : BITFIELD_RANGE(0, 5);
            uint32_t Reserved : BITFIELD_RANGE(6, 22);
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
typedef struct tagMI_SEMAPHORE_WAIT {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 7);
            uint32_t Reserved_8 : BITFIELD_RANGE(8, 11);
            uint32_t CompareOperation : BITFIELD_RANGE(12, 14);
            uint32_t WaitMode : BITFIELD_RANGE(15, 15);
            uint32_t RegisterPollMode : BITFIELD_RANGE(16, 16);
            uint32_t Reserved_17 : BITFIELD_RANGE(17, 21);
            uint32_t MemoryType : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint32_t SemaphoreDataDword;
            uint64_t Reserved_64 : BITFIELD_RANGE(0, 1);
            uint64_t SemaphoreAddress_Graphicsaddress : BITFIELD_RANGE(2, 63);
        } Common;
        uint32_t RawData[4];
    } TheStructure;
    typedef enum tagDWORD_LENGTH {
        DWORD_LENGTH_EXCLUDES_DWORD_0_1 = 0x2,
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
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_EXCLUDES_DWORD_0_1;
        TheStructure.Common.CompareOperation = COMPARE_OPERATION_SAD_GREATER_THAN_SDD;
        TheStructure.Common.WaitMode = WAIT_MODE_SIGNAL_MODE;
        TheStructure.Common.RegisterPollMode = REGISTER_POLL_MODE_MEMORY_POLL;
        TheStructure.Common.MemoryType = MEMORY_TYPE_PER_PROCESS_GRAPHICS_ADDRESS;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_SEMAPHORE_WAIT;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_SEMAPHORE_WAIT sInit(void) {
        MI_SEMAPHORE_WAIT state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 4);
        return TheStructure.RawData[index];
    }
    inline void setCompareOperation(const COMPARE_OPERATION value) {
        TheStructure.Common.CompareOperation = value;
    }
    inline COMPARE_OPERATION getCompareOperation(void) const {
        return static_cast<COMPARE_OPERATION>(TheStructure.Common.CompareOperation);
    }
    inline void setWaitMode(const WAIT_MODE value) {
        TheStructure.Common.WaitMode = value;
    }
    inline WAIT_MODE getWaitMode(void) const {
        return static_cast<WAIT_MODE>(TheStructure.Common.WaitMode);
    }
    inline void setRegisterPollMode(const REGISTER_POLL_MODE value) {
        TheStructure.Common.RegisterPollMode = value;
    }
    inline REGISTER_POLL_MODE getRegisterPollMode(void) const {
        return static_cast<REGISTER_POLL_MODE>(TheStructure.Common.RegisterPollMode);
    }
    inline void setMemoryType(const MEMORY_TYPE value) {
        TheStructure.Common.MemoryType = value;
    }
    inline MEMORY_TYPE getMemoryType(void) const {
        return static_cast<MEMORY_TYPE>(TheStructure.Common.MemoryType);
    }
    inline void setSemaphoreDataDword(const uint32_t value) {
        TheStructure.Common.SemaphoreDataDword = value;
    }
    inline uint32_t getSemaphoreDataDword(void) const {
        return (TheStructure.Common.SemaphoreDataDword);
    }
    typedef enum tagSEMAPHOREADDRESS_GRAPHICSADDRESS {
        SEMAPHOREADDRESS_GRAPHICSADDRESS_BIT_SHIFT = 0x2,
        SEMAPHOREADDRESS_GRAPHICSADDRESS_ALIGN_SIZE = 0x4,
    } SEMAPHOREADDRESS_GRAPHICSADDRESS39_2;
    inline void setSemaphoreGraphicsAddress(const uint64_t value) {
        TheStructure.Common.SemaphoreAddress_Graphicsaddress = value >> SEMAPHOREADDRESS_GRAPHICSADDRESS_BIT_SHIFT;
    }
    inline uint64_t getSemaphoreGraphicsAddress(void) const {
        return (TheStructure.Common.SemaphoreAddress_Graphicsaddress << SEMAPHOREADDRESS_GRAPHICSADDRESS_BIT_SHIFT);
    }
    typedef enum tagSEMAPHOREADDRESS_RESERVED {
        SEMAPHOREADDRESS_RESERVED_BIT_SHIFT = 0x2,
        SEMAPHOREADDRESS_RESERVED_ALIGN_SIZE = 0x4,
    } SEMAPHOREADDRESS_RESERVED;
} MI_SEMAPHORE_WAIT;
STATIC_ASSERT(16 == sizeof(MI_SEMAPHORE_WAIT));

typedef struct tagMI_STORE_DATA_IMM {
    union tagTheStructure {
        struct tagCommon {
            uint32_t DwordLength : BITFIELD_RANGE(0, 9);
            uint32_t Reserved_10 : BITFIELD_RANGE(10, 20);
            uint32_t StoreQword : BITFIELD_RANGE(21, 21);
            uint32_t UseGlobalGtt : BITFIELD_RANGE(22, 22);
            uint32_t MiCommandOpcode : BITFIELD_RANGE(23, 28);
            uint32_t CommandType : BITFIELD_RANGE(29, 31);
            uint64_t CoreModeEnable : BITFIELD_RANGE(0, 0);
            uint64_t Reserved_33 : BITFIELD_RANGE(1, 1);
            uint64_t Address_Graphicsaddress39_2 : BITFIELD_RANGE(2, 39);
            uint64_t Address_Reserved : BITFIELD_RANGE(40, 63);
            uint32_t DataDword0;
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
    inline void init(void) {
        memset(&TheStructure, 0, sizeof(TheStructure));
        TheStructure.Common.DwordLength = DWORD_LENGTH_STORE_DWORD;
        TheStructure.Common.MiCommandOpcode = MI_COMMAND_OPCODE_MI_STORE_DATA_IMM;
        TheStructure.Common.CommandType = COMMAND_TYPE_MI_COMMAND;
    }
    static tagMI_STORE_DATA_IMM sInit(void) {
        MI_STORE_DATA_IMM state;
        state.init();
        return state;
    }
    inline uint32_t &getRawData(const uint32_t index) {
        DEBUG_BREAK_IF(index >= 5);
        return TheStructure.RawData[index];
    }
    inline void setDwordLength(const DWORD_LENGTH value) {
        TheStructure.Common.DwordLength = value;
    }
    inline DWORD_LENGTH getDwordLength(void) const {
        return static_cast<DWORD_LENGTH>(TheStructure.Common.DwordLength);
    }
    inline void setStoreQword(const bool value) {
        TheStructure.Common.StoreQword = value;
    }
    inline bool getStoreQword(void) const {
        return (TheStructure.Common.StoreQword);
    }
    inline void setUseGlobalGtt(const bool value) {
        TheStructure.Common.UseGlobalGtt = value;
    }
    inline bool getUseGlobalGtt(void) const {
        return (TheStructure.Common.UseGlobalGtt);
    }
    inline void setCoreModeEnable(const uint64_t value) {
        TheStructure.Common.CoreModeEnable = value;
    }
    inline uint64_t getCoreModeEnable(void) const {
        return (TheStructure.Common.CoreModeEnable);
    }
    typedef enum tagADDRESS_GRAPHICSADDRESS39_2 {
        ADDRESS_GRAPHICSADDRESS39_2_BIT_SHIFT = 0x2,
        ADDRESS_GRAPHICSADDRESS39_2_ALIGN_SIZE = 0x4,
    } ADDRESS_GRAPHICSADDRESS39_2;
    inline void setAddress(const uint64_t value) {
        TheStructure.Common.Address_Graphicsaddress39_2 = value >> ADDRESS_GRAPHICSADDRESS39_2_BIT_SHIFT;
    }
    inline uint64_t getAddress(void) const {
        return (TheStructure.Common.Address_Graphicsaddress39_2 << ADDRESS_GRAPHICSADDRESS39_2_BIT_SHIFT);
    }
    typedef enum tagADDRESS_RESERVED {
        ADDRESS_RESERVED_BIT_SHIFT = 0x2,
        ADDRESS_RESERVED_ALIGN_SIZE = 0x4,
    } ADDRESS_RESERVED;
    inline void setAddressReserved(const uint64_t value) {
        TheStructure.Common.Address_Reserved = value >> ADDRESS_RESERVED_BIT_SHIFT;
    }
    inline uint64_t getAddressReserved(void) const {
        return (TheStructure.Common.Address_Reserved << ADDRESS_RESERVED_BIT_SHIFT);
    }
    inline void setDataDword0(const uint32_t value) {
        TheStructure.Common.DataDword0 = value;
    }
    inline uint32_t getDataDword0(void) const {
        return (TheStructure.Common.DataDword0);
    }
    inline void setDataDword1(const uint32_t value) {
        TheStructure.Common.DataDword1 = value;
    }
    inline uint32_t getDataDword1(void) const {
        return (TheStructure.Common.DataDword1);
    }
} MI_STORE_DATA_IMM;
STATIC_ASSERT(20 == sizeof(MI_STORE_DATA_IMM));

typedef struct tagPWR_CLK_STATE_REGISTER {
    typedef enum tagADDRESS {
        REG_ADDRESS = 0x20C8
    } ADDRESS;
    union tagTheStructure {
        struct tagCommon {
            uint32_t EUmin : BITFIELD_RANGE(0, 3);
            uint32_t EUmax : BITFIELD_RANGE(4, 7);
            uint32_t SScount : BITFIELD_RANGE(8, 10);
            uint32_t SSCountEn : BITFIELD_RANGE(11, 11);
            uint32_t SliceCountRequest : BITFIELD_RANGE(12, 17);
            uint32_t EnableSliceCountRequest : BITFIELD_RANGE(18, 18);
            uint32_t Reserved : BITFIELD_RANGE(19, 30);
            uint32_t EnablePwrClockGating : BITFIELD_RANGE(31, 31);
        } Common;
        uint32_t RawData[1];
    } TheStructure;
    inline void init(void) {
        TheStructure.RawData[0] = 0;
        TheStructure.Common.EnablePwrClockGating = 1;
        TheStructure.Common.EUmin = 8;
        TheStructure.Common.EUmax = 8;
    }
    static tagPWR_CLK_STATE_REGISTER sInit(void) {
        PWR_CLK_STATE_REGISTER state;
        state.init();
        return state;
    }
} PWR_CLK_STATE_REGISTER;
STATIC_ASSERT(4 == sizeof(PWR_CLK_STATE_REGISTER));

typedef struct tagASYNC_SLICE_COUNT_SELECT_REGISTER {
    typedef enum tagADDRESS {
        REG_ADDRESS = 0xA204
    } ADDRESS;
    union tagTheStructure {
        struct tagCommon {
            uint32_t AsyncSliceCount : BITFIELD_RANGE(0, 3);
            uint32_t AsyncEuCount : BITFIELD_RANGE(4, 7);
            uint32_t AsyncSubSliceCount : BITFIELD_RANGE(8, 10);
            uint32_t Reserved : BITFIELD_RANGE(11, 31);
        } Common;
        uint32_t RawData[1];
    } TheStructure;
    inline void init(void) {
        TheStructure.RawData[0] = 0;
        TheStructure.Common.AsyncSliceCount = 2;
        TheStructure.Common.AsyncSubSliceCount = 4;
        TheStructure.Common.AsyncEuCount = 8;
    }
    static tagASYNC_SLICE_COUNT_SELECT_REGISTER sInit(void) {
        ASYNC_SLICE_COUNT_SELECT_REGISTER state;
        state.init();
        return state;
    }
} ASYNC_SLICE_COUNT_SELECT_REGISTER;
STATIC_ASSERT(4 == sizeof(ASYNC_SLICE_COUNT_SELECT_REGISTER));
#pragma pack()
