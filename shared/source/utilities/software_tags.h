/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/string.h"

#include <cstdint>
#include <string>

namespace NEO {
namespace SWTags {

enum class OpCode : uint32_t {
    unknown,
    kernelName,
    pipeControlReason,
    callNameBegin,
    callNameEnd,
    arbitraryString,
};

enum class Component : uint32_t {
    common = 1
};

inline constexpr uint32_t reservedDefault = 0u;
inline constexpr uint32_t driverDebugDefault = 1u;

struct BXMLHeapInfo {
    const uint32_t magicNumber = 0xDEB06D0C;
    const uint32_t heapSize;
    const uint32_t component = static_cast<uint32_t>(Component::common);

    BXMLHeapInfo(size_t size) : heapSize(static_cast<uint32_t>(size)) {}

    static void bxml(std::ostream &os);
};

struct SWTagHeapInfo {
    const uint32_t magicNumber = 0xDEB06DD1;
    const uint32_t heapSize;
    const uint32_t component = static_cast<uint32_t>(Component::common);

    SWTagHeapInfo(size_t size) : heapSize(static_cast<uint32_t>(size)) {}

    static void bxml(std::ostream &os);
};

struct BaseTag {
  public:
    BaseTag(OpCode code, size_t size) : opcode(static_cast<uint32_t>(code)),
                                        reserved(reservedDefault),
                                        component(static_cast<uint32_t>(Component::common)),
                                        driverDebug(driverDebugDefault),
                                        dwordCount(static_cast<uint32_t>(size / sizeof(uint32_t) - 2)) {}

    OpCode getOpCode() const { return static_cast<OpCode>(opcode); }
    static uint32_t getMarkerNoopID(OpCode opcode);
    static uint32_t getOffsetNoopID(uint32_t offset);
    static void bxml(std::ostream &os, OpCode opcode, size_t size, const char *name);

  protected:
    union MarkerNoopID {
        struct {
            uint32_t opCode : 20;
            uint32_t reserved : 12;
        };
        uint32_t value;
    };
    union OffsetNoopID {
        struct {
            uint32_t offset : 20;
            uint32_t subTag : 1;
            uint32_t magicBit : 1;
            uint32_t reserved : 10;
        };
        uint32_t value;
    };

    const uint32_t opcode : 20;
    const uint32_t reserved : 4;
    const uint32_t component : 7;
    const uint32_t driverDebug : 1; // always 0x1
    const uint32_t dwordCount;
};

struct KernelNameTag : public BaseTag {
  public:
    KernelNameTag(const char *name, uint32_t callId)
        : BaseTag(OpCode::kernelName, sizeof(KernelNameTag)) {
        strcpy_s(kernelName, kenelNameStrLength, name);
    }

    static void bxml(std::ostream &os);

  private:
    static constexpr unsigned int kenelNameStrLength = sizeof(uint32_t) * 16; // Dword aligned
    char kernelName[kenelNameStrLength] = {};
};

struct ArbitraryStringTag : public BaseTag {
  public:
    ArbitraryStringTag(const char *name)
        : BaseTag(OpCode::arbitraryString, sizeof(ArbitraryStringTag)) {
        strcpy_s(arbitraryString, tagStringLength, name);
    }

    static void bxml(std::ostream &os);

  protected:
    static constexpr unsigned int tagStringLength = sizeof(uint32_t) * 16; // Dword aligned
    char arbitraryString[tagStringLength] = {};
};

struct PipeControlReasonTag : public BaseTag {
  public:
    PipeControlReasonTag(const char *reason, uint32_t callId)
        : BaseTag(OpCode::pipeControlReason, sizeof(PipeControlReasonTag)) {
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
        : BaseTag(OpCode::callNameBegin, sizeof(CallNameBeginTag)) {
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
        : BaseTag(OpCode::callNameEnd, sizeof(CallNameEndTag)) {
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
