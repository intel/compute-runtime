/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/string.h"

#include <ostream>
#include <string>

namespace NEO {
namespace SWTags {

enum class OpCode : uint32_t {
    Unknown,
    KernelName,
    PipeControlReason,
    CallNameBegin,
    CallNameEnd
};

enum class Component : uint32_t {
    COMMON = 1
};

struct BXMLHeapInfo {
    const uint32_t magicNumber = 0xDEB06D0C;
    const uint32_t heapSize;
    const uint32_t component = static_cast<uint32_t>(Component::COMMON);

    BXMLHeapInfo(size_t size) : heapSize(static_cast<uint32_t>(size)) {}

    static void bxml(std::ostream &os);
};

struct SWTagHeapInfo {
    const uint32_t magicNumber = 0xDEB06DD1;
    const uint32_t heapSize;
    const uint32_t component = static_cast<uint32_t>(Component::COMMON);

    SWTagHeapInfo(size_t size) : heapSize(static_cast<uint32_t>(size)) {}

    static void bxml(std::ostream &os);
};

struct BaseTag {
  public:
    BaseTag(OpCode code, size_t size) : opcode(static_cast<uint32_t>(code)),
                                        reserved(0),
                                        component(static_cast<uint32_t>(Component::COMMON)),
                                        driverDebug(1),
                                        DWORDCount(static_cast<uint32_t>(size / sizeof(uint32_t) - 2)) {}

    OpCode getOpCode() const { return static_cast<OpCode>(opcode); }
    static uint32_t getMarkerNoopID(OpCode opcode);
    static uint32_t getOffsetNoopID(uint32_t offset);
    static void bxml(std::ostream &os, OpCode opcode, size_t size, const char *name);

  protected:
    union MarkerNoopID {
        struct {
            uint32_t OpCode : 20;
            uint32_t Reserved : 12;
        };
        uint32_t value;
    };
    union OffsetNoopID {
        struct {
            uint32_t Offset : 20;
            uint32_t SubTag : 1;
            uint32_t MagicBit : 1;
            uint32_t Reserved : 10;
        };
        uint32_t value;
    };

    const uint32_t opcode : 20;
    const uint32_t reserved : 4;
    const uint32_t component : 7;
    const uint32_t driverDebug : 1; // always 0x1
    const uint32_t DWORDCount;
};

struct KernelNameTag : public BaseTag {
  public:
    KernelNameTag(const char *name, uint32_t callId)
        : BaseTag(OpCode::KernelName, sizeof(KernelNameTag)) {
        strcpy_s(kernelName, kenelNameStrLength, name);
    }

    static void bxml(std::ostream &os);

  private:
    static constexpr unsigned int kenelNameStrLength = sizeof(uint32_t) * 16; // Dword aligned
    char kernelName[kenelNameStrLength] = {};
};

struct PipeControlReasonTag : public BaseTag {
  public:
    PipeControlReasonTag(const char *reason, uint32_t callId)
        : BaseTag(OpCode::PipeControlReason, sizeof(PipeControlReasonTag)) {
        strcpy_s(reasonString, reasonStrLength, reason);
    }

    static void bxml(std::ostream &os);

  private:
    static constexpr unsigned int reasonStrLength = sizeof(uint32_t) * 32; // Dword aligned
    char reasonString[reasonStrLength] = {};
};

struct CallNameBeginTag : public BaseTag {
  public:
    CallNameBeginTag(const char *name, uint32_t callId)
        : BaseTag(OpCode::CallNameBegin, sizeof(CallNameBeginTag)) {
        strcpy_s(zeCallName, zeCallNameStrLength, name);
        snprintf(zeCallId, sizeof(uint32_t), "%x", callId);
    }

    static void bxml(std::ostream &os);

  private:
    static constexpr unsigned int zeCallNameStrLength = sizeof(uint32_t) * 32; // Dword aligned
    char zeCallName[zeCallNameStrLength] = {};
    char zeCallId[zeCallNameStrLength] = {};
};

struct CallNameEndTag : public BaseTag {
  public:
    CallNameEndTag(const char *name, uint32_t callId)
        : BaseTag(OpCode::CallNameEnd, sizeof(CallNameEndTag)) {
        strcpy_s(zeCallName, zeCallNameStrLength, name);
        snprintf(zeCallId, sizeof(uint32_t), "%x", callId);
    }

    static void bxml(std::ostream &os);

  private:
    static constexpr unsigned int zeCallNameStrLength = sizeof(uint32_t) * 32; // Dword aligned
    char zeCallName[zeCallNameStrLength] = {};
    char zeCallId[zeCallNameStrLength] = {};
};

struct SWTagBXML {
    SWTagBXML();

    std::string str;
};

} // namespace SWTags
} // namespace NEO
