/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/tbx/tbx_sockets_imp.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/string.h"

#ifdef _WIN32
#ifndef _WIN32_LEAN_AND_MEAN
#define _WIN32_LEAN_AND_MEAN
#endif
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
typedef struct sockaddr SOCKADDR;
#define INVALID_SOCKET -1
#endif
#include "tbx_proto.h"

#include <cstdint>

namespace NEO {

TbxSocketsImp::TbxSocketsImp(std::ostream &err)
    : cerrStream(err) {
}

void TbxSocketsImp::close() {
    if (0 != socket) {
#ifdef WIN32
        ::shutdown(socket, 0x02 /*SD_BOTH*/);

        ::closesocket(socket);
        ::WSACleanup();
#else
        ::shutdown(socket, SHUT_RDWR);
        ::close(socket);
#endif
        socket = 0;
    }
}

void TbxSocketsImp::logErrorInfo(const char *tag) {
#ifdef WIN32
    cerrStream << tag << "TbxSocketsImp Error: <" << WSAGetLastError() << ">" << std::endl;
#else
    cerrStream << tag << strerror(errno) << std::endl;
#endif
    DEBUG_BREAK_IF(!false);
}

bool TbxSocketsImp::init(const std::string &hostNameOrIp, uint16_t port) {
    do {
#ifdef WIN32
        WSADATA wsaData;
        auto iResult = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (iResult != NO_ERROR) {
            cerrStream << "Error at WSAStartup()" << std::endl;
            break;
        }
#endif

        socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socket == INVALID_SOCKET) {
            logErrorInfo("Error at socket(): ");
            break;
        }

        if (!connectToServer(hostNameOrIp, port)) {
            break;
        }

        HasMsg cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.hdr.msgType = HAS_CONTROL_REQ_TYPE;
        cmd.hdr.size = sizeof(HasControlReq);
        cmd.hdr.transID = transID++;

        cmd.u.controlReq.timeAdvMask = 1;
        cmd.u.controlReq.timeAdv = 0;

        cmd.u.controlReq.asyncMsgMask = 1;
        cmd.u.controlReq.asyncMsg = 0;

        cmd.u.controlReq.hasMask = 1;
        cmd.u.controlReq.has = 1;

        sendWriteData(&cmd, sizeof(HasHdr) + cmd.hdr.size);
    } while (false);

    return socket != INVALID_SOCKET;
}

bool TbxSocketsImp::connectToServer(const std::string &hostNameOrIp, uint16_t port) {
    do {
        sockaddr_in clientService;
        if (::isalpha(hostNameOrIp.at(0))) {
            auto hostData = ::gethostbyname(hostNameOrIp.c_str());
            if (hostData == nullptr) {
                cerrStream << "Host name look up failed for " << hostNameOrIp.c_str() << std::endl;
                break;
            }

            memcpy_s(&clientService.sin_addr, sizeof(clientService.sin_addr), hostData->h_addr, hostData->h_length);
        } else {
            clientService.sin_addr.s_addr = inet_addr(hostNameOrIp.c_str());
        }

        clientService.sin_family = AF_INET;
        clientService.sin_port = htons(port);

        if (::connect(socket, (SOCKADDR *)&clientService, sizeof(clientService)) == static_cast<int>(INVALID_SOCKET)) {
            logErrorInfo("Failed to connect: ");
            cerrStream << "Is TBX server process running on host system [ " << hostNameOrIp.c_str()
                       << ", port " << port << "]?" << std::endl;
            break;
        }
    } while (false);

    return !!socket;
}

bool TbxSocketsImp::readMMIO(uint32_t offset, uint32_t *data) {
    bool success;
    do {
        HasMsg cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.hdr.msgType = HAS_MMIO_REQ_TYPE;
        cmd.hdr.size = sizeof(HasMmioReq);
        cmd.hdr.transID = transID++;
        cmd.u.mmioReq.offset = offset;
        cmd.u.mmioReq.data = 0;
        cmd.u.mmioReq.write = 0;
        cmd.u.mmioReq.delay = 0;
        cmd.u.mmioReq.msgType = MSG_TYPE_MMIO;
        cmd.u.mmioReq.size = sizeof(uint32_t);

        success = sendWriteData(&cmd, sizeof(HasHdr) + cmd.hdr.size);
        if (!success) {
            break;
        }

        HasMsg resp;
        success = getResponseData((char *)(&resp), sizeof(HasHdr) + sizeof(HasMmioRes));
        if (!success) {
            break;
        }

        if (resp.hdr.msgType != HAS_MMIO_RES_TYPE || cmd.hdr.transID != resp.hdr.transID) {
            *data = 0xdeadbeef;
            success = false;
            break;
        }

        *data = resp.u.mmioRes.data;
        success = true;
    } while (false);

    DEBUG_BREAK_IF(!success);
    return success;
}

bool TbxSocketsImp::writeMMIO(uint32_t offset, uint32_t value) {
    HasMsg cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.hdr.msgType = HAS_MMIO_REQ_TYPE;
    cmd.hdr.size = sizeof(HasMmioReq);
    cmd.hdr.transID = transID++;
    cmd.u.mmioReq.msgType = MSG_TYPE_MMIO;
    cmd.u.mmioReq.offset = offset;
    cmd.u.mmioReq.data = value;
    cmd.u.mmioReq.write = 1;
    cmd.u.mmioReq.size = sizeof(uint32_t);

    return sendWriteData(&cmd, sizeof(HasHdr) + cmd.hdr.size);
}

bool TbxSocketsImp::readMemory(uint64_t addrOffset, void *data, size_t size) {
    HasMsg cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.hdr.msgType = HAS_READ_DATA_REQ_TYPE;
    cmd.hdr.transID = transID++;
    cmd.hdr.size = sizeof(HasReadDataReq);
    cmd.u.readReq.address = static_cast<uint32_t>(addrOffset);
    cmd.u.readReq.addressH = static_cast<uint32_t>(addrOffset >> 32);
    cmd.u.readReq.addrType = 0;
    cmd.u.readReq.size = static_cast<uint32_t>(size);
    cmd.u.readReq.ownershipReq = 0;
    cmd.u.readReq.frontdoor = 0;
    cmd.u.readReq.cachelineDisable = cmd.u.readReq.frontdoor;

    bool success;
    do {
        success = sendWriteData(&cmd, sizeof(HasHdr) + sizeof(HasReadDataReq));
        if (!success) {
            break;
        }

        HasMsg resp;
        success = getResponseData(&resp, sizeof(HasHdr) + sizeof(HasReadDataRes));
        if (!success) {
            break;
        }

        if (resp.hdr.msgType != HAS_READ_DATA_RES_TYPE || resp.hdr.transID != cmd.hdr.transID) {
            cerrStream << "Out of sequence read data packet?" << std::endl;
            success = false;
            break;
        }

        success = getResponseData(data, size);
    } while (false);

    DEBUG_BREAK_IF(!success);
    return success;
}

bool TbxSocketsImp::writeMemory(uint64_t physAddr, const void *data, size_t size, uint32_t type) {
    HasMsg cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.hdr.msgType = HAS_WRITE_DATA_REQ_TYPE;
    cmd.hdr.transID = transID++;
    cmd.hdr.size = sizeof(HasWriteDataReq);

    cmd.u.writeReq.address = static_cast<uint32_t>(physAddr);
    cmd.u.writeReq.addressH = static_cast<uint32_t>(physAddr >> 32);
    cmd.u.writeReq.addrType = 0;
    cmd.u.writeReq.size = static_cast<uint32_t>(size);
    cmd.u.writeReq.takeOwnership = 0;
    cmd.u.writeReq.frontdoor = 0;
    cmd.u.writeReq.cachelineDisable = cmd.u.writeReq.frontdoor;
    cmd.u.writeReq.memoryType = type;

    bool success;
    do {
        success = sendWriteData(&cmd, sizeof(HasHdr) + sizeof(HasWriteDataReq));
        if (!success) {
            break;
        }

        success = sendWriteData(data, size);
        if (!success) {
            cerrStream << "Problem sending write data?" << std::endl;
            break;
        }
    } while (false);

    DEBUG_BREAK_IF(!success);
    return success;
}

bool TbxSocketsImp::writeGTT(uint32_t offset, uint64_t entry) {
    HasMsg cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.hdr.msgType = HAS_GTT_REQ_TYPE;
    cmd.hdr.size = sizeof(HasGtt64Req);
    cmd.hdr.transID = transID++;
    cmd.u.gtt64Req.write = 1;
    cmd.u.gtt64Req.offset = offset / sizeof(uint64_t); // the TBX server expects GTT index here, not offset
    cmd.u.gtt64Req.data = static_cast<uint32_t>(entry & 0xffffffff);
    cmd.u.gtt64Req.dataH = static_cast<uint32_t>(entry >> 32);

    return sendWriteData(&cmd, sizeof(HasHdr) + cmd.hdr.size);
}

bool TbxSocketsImp::sendWriteData(const void *buffer, size_t sizeInBytes) {
    size_t totalSent = 0;
    auto dataBuffer = reinterpret_cast<const char *>(buffer);

    do {
        auto bytesSent = ::send(socket, &dataBuffer[totalSent], static_cast<int>(sizeInBytes - totalSent), 0);
        if (bytesSent == 0 || bytesSent == static_cast<int>(INVALID_SOCKET)) {
            logErrorInfo("Connection Closed.");
            return false;
        }

        totalSent += bytesSent;
    } while (totalSent < sizeInBytes);

    return true;
}

bool TbxSocketsImp::getResponseData(void *buffer, size_t sizeInBytes) {
    size_t totalRecv = 0;
    auto dataBuffer = static_cast<char *>(buffer);

    do {
        auto bytesRecv = ::recv(socket, &dataBuffer[totalRecv], static_cast<int>(sizeInBytes - totalRecv), 0);
        if (bytesRecv == 0 || bytesRecv == static_cast<int>(INVALID_SOCKET)) {
            logErrorInfo("Connection Closed.");
            return false;
        }

        totalRecv += bytesRecv;
    } while (totalRecv < sizeInBytes);

    return true;
}

} // namespace NEO
