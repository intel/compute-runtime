/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/tbx/tbx_sockets_imp.h"

#include "core/helpers/string.h"
#include "runtime/helpers/debug_helpers.h"

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
typedef struct sockaddr SOCKADDR;
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
#define WSAECONNRESET -1
#endif
#include "tbx_proto.h"

#include <cstdint>

namespace NEO {

TbxSocketsImp::TbxSocketsImp(std::ostream &err)
    : cerrStream(err) {
}

void TbxSocketsImp::close() {
    if (0 != m_socket) {
#ifdef WIN32
        ::shutdown(m_socket, 0x02 /*SD_BOTH*/);

        ::closesocket(m_socket);
        ::WSACleanup();
#else
        ::shutdown(m_socket, SHUT_RDWR);
        ::close(m_socket);
#endif
        m_socket = 0;
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

        m_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_socket == INVALID_SOCKET) {
            logErrorInfo("Error at socket(): ");
            break;
        }

        if (!connectToServer(hostNameOrIp, port)) {
            break;
        }

        HAS_MSG cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.hdr.msg_type = HAS_CONTROL_REQ_TYPE;
        cmd.hdr.size = sizeof(HAS_CONTROL_REQ);
        cmd.hdr.trans_id = transID++;

        cmd.u.control_req.time_adv_mask = 1;
        cmd.u.control_req.time_adv = 0;

        cmd.u.control_req.async_msg_mask = 1;
        cmd.u.control_req.async_msg = 0;

        cmd.u.control_req.has_mask = 1;
        cmd.u.control_req.has = 1;

        sendWriteData(&cmd, sizeof(HAS_HDR) + cmd.hdr.size);
    } while (false);

    return m_socket != INVALID_SOCKET;
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

        if (::connect(m_socket, (SOCKADDR *)&clientService, sizeof(clientService)) == SOCKET_ERROR) {
            logErrorInfo("Failed to connect: ");
            cerrStream << "Is TBX server process running on host system [ " << hostNameOrIp.c_str()
                       << ", port " << port << "]?" << std::endl;
            break;
        }
    } while (false);

    return !!m_socket;
}

bool TbxSocketsImp::readMMIO(uint32_t offset, uint32_t *data) {
    bool success;
    do {
        HAS_MSG cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.hdr.msg_type = HAS_MMIO_REQ_TYPE;
        cmd.hdr.size = sizeof(HAS_MMIO_REQ);
        cmd.hdr.trans_id = transID++;
        cmd.u.mmio_req.offset = offset;
        cmd.u.mmio_req.data = 0;
        cmd.u.mmio_req.write = 0;
        cmd.u.mmio_req.delay = 0;
        cmd.u.mmio_req.msg_type = MSG_TYPE_MMIO;
        cmd.u.mmio_req.size = sizeof(uint32_t);

        success = sendWriteData(&cmd, sizeof(HAS_HDR) + cmd.hdr.size);
        if (!success) {
            break;
        }

        HAS_MSG resp;
        success = getResponseData((char *)(&resp), sizeof(HAS_HDR) + sizeof(HAS_MMIO_RES));
        if (!success) {
            break;
        }

        if (resp.hdr.msg_type != HAS_MMIO_RES_TYPE || cmd.hdr.trans_id != resp.hdr.trans_id) {
            *data = 0xdeadbeef;
            success = false;
            break;
        }

        *data = resp.u.mmio_res.data;
        success = true;
    } while (false);

    DEBUG_BREAK_IF(!success);
    return success;
}

bool TbxSocketsImp::writeMMIO(uint32_t offset, uint32_t value) {
    HAS_MSG cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.hdr.msg_type = HAS_MMIO_REQ_TYPE;
    cmd.hdr.size = sizeof(HAS_MMIO_REQ);
    cmd.hdr.trans_id = transID++;
    cmd.u.mmio_req.msg_type = MSG_TYPE_MMIO;
    cmd.u.mmio_req.offset = offset;
    cmd.u.mmio_req.data = value;
    cmd.u.mmio_req.write = 1;
    cmd.u.mmio_req.size = sizeof(uint32_t);

    return sendWriteData(&cmd, sizeof(HAS_HDR) + cmd.hdr.size);
}

bool TbxSocketsImp::readMemory(uint64_t addrOffset, void *data, size_t size) {
    HAS_MSG cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.hdr.msg_type = HAS_READ_DATA_REQ_TYPE;
    cmd.hdr.trans_id = transID++;
    cmd.hdr.size = sizeof(HAS_READ_DATA_REQ);
    cmd.u.read_req.address = static_cast<uint32_t>(addrOffset);
    cmd.u.read_req.address_h = static_cast<uint32_t>(addrOffset >> 32);
    cmd.u.read_req.addr_type = 0;
    cmd.u.read_req.size = static_cast<uint32_t>(size);
    cmd.u.read_req.ownership_req = 0;
    cmd.u.read_req.frontdoor = 0;
    cmd.u.read_req.cacheline_disable = cmd.u.read_req.frontdoor;

    bool success;
    do {
        success = sendWriteData(&cmd, sizeof(HAS_HDR) + sizeof(HAS_READ_DATA_REQ));
        if (!success) {
            break;
        }

        HAS_MSG resp;
        success = getResponseData(&resp, sizeof(HAS_HDR) + sizeof(HAS_READ_DATA_RES));
        if (!success) {
            break;
        }

        if (resp.hdr.msg_type != HAS_READ_DATA_RES_TYPE || resp.hdr.trans_id != cmd.hdr.trans_id) {
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
    HAS_MSG cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.hdr.msg_type = HAS_WRITE_DATA_REQ_TYPE;
    cmd.hdr.trans_id = transID++;
    cmd.hdr.size = sizeof(HAS_WRITE_DATA_REQ);

    cmd.u.write_req.address = static_cast<uint32_t>(physAddr);
    cmd.u.write_req.address_h = static_cast<uint32_t>(physAddr >> 32);
    cmd.u.write_req.addr_type = 0;
    cmd.u.write_req.size = static_cast<uint32_t>(size);
    cmd.u.write_req.take_ownership = 0;
    cmd.u.write_req.frontdoor = 0;
    cmd.u.write_req.cacheline_disable = cmd.u.write_req.frontdoor;
    cmd.u.write_req.memory_type = type;

    bool success;
    do {
        success = sendWriteData(&cmd, sizeof(HAS_HDR) + sizeof(HAS_WRITE_DATA_REQ));
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
    HAS_MSG cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.hdr.msg_type = HAS_GTT_REQ_TYPE;
    cmd.hdr.size = sizeof(HAS_GTT64_REQ);
    cmd.hdr.trans_id = transID++;
    cmd.u.gtt64_req.write = 1;
    cmd.u.gtt64_req.offset = offset / sizeof(uint64_t); // the TBX server expects GTT index here, not offset
    cmd.u.gtt64_req.data = static_cast<uint32_t>(entry & 0xffffffff);
    cmd.u.gtt64_req.data_h = static_cast<uint32_t>(entry >> 32);

    return sendWriteData(&cmd, sizeof(HAS_HDR) + cmd.hdr.size);
}

bool TbxSocketsImp::sendWriteData(const void *buffer, size_t sizeInBytes) {
    size_t totalSent = 0;
    auto dataBuffer = reinterpret_cast<const char *>(buffer);

    do {
        auto bytesSent = ::send(m_socket, &dataBuffer[totalSent], static_cast<int>(sizeInBytes - totalSent), 0);
        if (bytesSent == 0 || bytesSent == WSAECONNRESET) {
            logErrorInfo("Connection Closed.");
            return false;
        }

        if (bytesSent == SOCKET_ERROR) {
            logErrorInfo("Error on send()");
            return false;
        }
        totalSent += bytesSent;
    } while (totalSent < sizeInBytes);

    return true;
}

bool TbxSocketsImp::getResponseData(void *buffer, size_t sizeInBytes) {
    size_t totalRecv = 0;
    auto dataBuffer = reinterpret_cast<char *>(buffer);

    do {
        auto bytesRecv = ::recv(m_socket, &dataBuffer[totalRecv], static_cast<int>(sizeInBytes - totalRecv), 0);
        if (bytesRecv == 0 || bytesRecv == WSAECONNRESET) {
            logErrorInfo("Connection Closed.");
            return false;
        }

        if (bytesRecv == SOCKET_ERROR) {
            logErrorInfo("Error on recv()");
            return false;
        }

        totalRecv += bytesRecv;
    } while (totalRecv < sizeInBytes);

    return true;
}

} // namespace NEO
