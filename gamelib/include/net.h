#pragma once

#include "core.h"

struct ipAddress
{
  uint8_t version; // 4 or 6.

#pragma warning(push)
#pragma warning(disable: 4201)
  union
  {
    struct
    {
      uint8_t v4[4];
    };

    struct
    {
      uint8_t v6[16];
    };
  };
#pragma warning(pop)
};

inline lsResult ipAddress_toString(const ipAddress *pIP, _Out_ char *string, const size_t capacity)
{
  if (pIP->version == 4)
  {
    snprintf(string, capacity, "%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8, pIP->v4[0], pIP->v4[1], pIP->v4[2], pIP->v4[3]);
  }
  else if (pIP->version == 6)
  {
    snprintf(string, capacity, "%" PRIx8 "%" PRIx8 ":%" PRIx8 "%" PRIx8 ":%" PRIx8 "%" PRIx8 ":%" PRIx8 "%" PRIx8 ":%" PRIx8 "%" PRIx8 ":%" PRIx8 "%" PRIx8 ":%" PRIx8 "%" PRIx8 ":%" PRIx8 "%" PRIx8, pIP->v6[0], pIP->v6[1], pIP->v6[2], pIP->v6[3], pIP->v6[4], pIP->v6[5], pIP->v6[6], pIP->v6[7], pIP->v6[8], pIP->v6[9], pIP->v6[10], pIP->v6[11], pIP->v6[12], pIP->v6[13], pIP->v6[14], pIP->v6[15]);
  }
  else
  {
    return lsR_InvalidParameter;
  }

  return lsR_Success;
}

struct tcpServer
{
  uint64_t socketHandle = 0;
};

struct tcpClient
{
  uint64_t socketHandle = 0;
};

lsResult tcpServer_create(_Out_ tcpServer *pServer, const uint16_t port);
void tcpServer_destroy(tcpServer *pServer);

lsResult tcpServer_listen(tcpServer *pServer, _Out_ tcpClient *pClient, const size_t timeoutMs = (size_t)-1);

lsResult tcpServer_getLocalEndPointInfo(tcpServer *pServer, _Out_ ipAddress *pAddress, _Out_ uint16_t *pPort);

lsResult tcpClient_create(_Out_ tcpClient *pClient, const ipAddress *pIP, const uint16_t port);
void tcpClient_destroy(tcpClient *pClient);

lsResult tcpClient_send(tcpClient *pClient, const void *pData, const size_t bytes, _Out_opt_ size_t *pBytesSent = nullptr);
lsResult tcpClient_receive(tcpClient *pClient, void *pData, const size_t maxLength, _Out_opt_ size_t *pBytesReceived = nullptr);
lsResult tcpClient_getReadableBytes(tcpClient *pClient, _Out_ size_t *pBytes, const size_t timeoutMs = 0);
lsResult tcpClient_getWriteableBytes(tcpClient *pClient, _Out_ size_t *pBytes, const size_t timeoutMs = 0);

lsResult tcpClient_getRemoteEndPointInfo(tcpClient *pClient, _Out_ ipAddress *pAddress, _Out_ uint16_t *pPort);
lsResult tcpClient_getLocalEndPointInfo(tcpClient *pClient, _Out_ ipAddress *pAddress, _Out_ uint16_t *pPort);

lsResult tcpClient_disableSendDelay(tcpClient *pClient);
lsResult tcpClient_setSendTimeout(tcpClient *pClient, const size_t timeMs);
lsResult tcpClient_setReceiveTimeout(tcpClient *pClient, const size_t timeMs);
