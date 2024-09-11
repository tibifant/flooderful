#pragma warning(push, 0)
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>
#pragma warning(pop)

#include "net.h"

//////////////////////////////////////////////////////////////////////////

static lsResult tcpSocket_getConnectionInfo(SOCKADDR_STORAGE_LH *pInfo, _Out_ ipAddress *pAddress, _Out_ uint16_t *pPort);

//////////////////////////////////////////////////////////////////////////

static bool net_initialized = false;
static WSADATA net_winsockInfo;

int32_t WSACleanupWrapper()
{
  return WSACleanup();
}

lsResult net_init()
{
  lsResult result = lsR_Success;

  if (net_initialized)
    goto epilogue;

  {
    const int32_t error = WSAStartup(MAKEWORD(2, 2), &net_winsockInfo);

    if (error != 0)
    {
      printf("Failed to initialize WinSock with error code 0x%" PRIX32 ".\n", error);
      LS_ERROR_SET(lsR_OperationNotSupported);
    }

    onexit(WSACleanupWrapper);
    net_initialized = true;
  }

epilogue:
  return result;
}

//////////////////////////////////////////////////////////////////////////

lsResult tcpServer_create(_Out_ tcpServer *pServer, const uint16_t port)
{
  lsResult result = lsR_Success;

  struct addrinfo *pResult = nullptr;
  struct addrinfo hints = { 0 };
  char portString[sizeof("65535")];
  int32_t error;

  LS_ERROR_IF(pServer == nullptr, lsR_ArgumentNull);
  LS_ERROR_CHECK(net_init());

  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_PASSIVE;

  snprintf(portString, LS_ARRAYSIZE(portString), "%" PRIu16, port);

  error = getaddrinfo(nullptr, portString, &hints, &pResult);
  LS_ERROR_IF(error != 0, lsR_ResourceAlreadyExists);

  pServer->socketHandle = socket(pResult->ai_family, pResult->ai_socktype, pResult->ai_protocol);

  if (pServer->socketHandle == INVALID_SOCKET)
  {
    error = WSAGetLastError();
    LS_ERROR_SET(lsR_InternalError);
  }

  error = bind(pServer->socketHandle, pResult->ai_addr, (int32_t)pResult->ai_addrlen);

  if (error == SOCKET_ERROR)
  {
    error = WSAGetLastError();

    switch (error)
    {
    case WSAEADDRINUSE:
      LS_ERROR_SET(lsR_ResourceAlreadyExists);

    default:
      LS_ERROR_SET(lsR_InternalError);
    }
  }

epilogue:
  if (pResult != nullptr)
    freeaddrinfo(pResult);

  return result;
}

lsResult tcpServer_listen(tcpServer *pServer, _Out_ tcpClient *pClient, const size_t timeoutMs /* = (size_t)-1 */)
{
  lsResult result = lsR_Success;

  int32_t error;

  LS_ERROR_IF(pServer == nullptr || pClient == nullptr, lsR_ArgumentNull);
  LS_ERROR_IF(timeoutMs != (size_t)-1 && timeoutMs > INT_MAX, lsR_ArgumentOutOfBounds);

  error = listen(pServer->socketHandle, SOMAXCONN);

  if (error == SOCKET_ERROR)
  {
    error = WSAGetLastError();
    LS_ERROR_SET(lsR_ResourceNotFound);
  }

  if (timeoutMs != (size_t)-1)
  {
    WSAPOLLFD connection;
    lsZeroMemory(&connection);

    connection.fd = pServer->socketHandle;
    connection.events = POLLRDNORM;
    connection.revents = POLLRDNORM;

    error = WSAPoll(&connection, 1, (INT)timeoutMs);

    if (error == 0)
      goto epilogue;

    LS_ERROR_IF(error < 0, lsR_InternalError);
  }

  pClient->socketHandle = INVALID_SOCKET;
  pClient->socketHandle = accept(pServer->socketHandle, (sockaddr *)nullptr, (int32_t *)nullptr);

  if (pClient->socketHandle == INVALID_SOCKET)
  {
    error = WSAGetLastError();
    LS_ERROR_SET(lsR_InternalError);
  }

epilogue:
  return result;
}

void tcpServer_destroy(tcpServer *pServer)
{
  if (pServer == nullptr)
    return;

  if (pServer->socketHandle != 0 && pServer->socketHandle != INVALID_SOCKET)
  {
    closesocket(pServer->socketHandle);
    pServer->socketHandle = INVALID_SOCKET;
  }
}

lsResult tcpServer_getLocalEndPointInfo(tcpServer *pServer, _Out_ ipAddress *pAddress, _Out_ uint16_t *pPort)
{
  lsResult result = lsR_Success;

  SOCKADDR_STORAGE_LH address;
  int32_t nameLength = sizeof(address);

  LS_ERROR_IF(pServer == nullptr || pAddress == nullptr, lsR_ArgumentNull);

  {
    const int32_t error = getsockname(pServer->socketHandle, reinterpret_cast<SOCKADDR *>(&address), &nameLength);
    LS_ERROR_IF(error != 0, lsR_InternalError);
  }

  LS_ERROR_CHECK(tcpSocket_getConnectionInfo(&address, pAddress, pPort));

epilogue:
  return result;
}

//////////////////////////////////////////////////////////////////////////

lsResult tcpClient_create(_Out_ tcpClient *pClient, const ipAddress *pIP, const uint16_t port)
{
  lsResult result = lsR_Success;

  struct addrinfo *pResult = nullptr;
  struct addrinfo hints = { 0 };
  char portString[sizeof("65535")];
  char address[sizeof("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")];
  int32_t error;

  LS_ERROR_IF(pClient == nullptr || pIP == nullptr, lsR_ArgumentNull);
  LS_ERROR_IF(port == 0, lsR_InvalidParameter);

  LS_ERROR_CHECK(net_init());

  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  snprintf(portString, LS_ARRAYSIZE(portString), "%" PRIu16, port);

  LS_ERROR_CHECK(ipAddress_toString(pIP, address, sizeof(address)));

  error = getaddrinfo(address, portString, &hints, &pResult);
  LS_ERROR_IF(error != 0, lsR_ResourceInvalid);

  pClient->socketHandle = socket(pResult->ai_family, pResult->ai_socktype, pResult->ai_protocol);

  if (pClient->socketHandle == INVALID_SOCKET)
  {
    error = WSAGetLastError();
    LS_ERROR_SET(lsR_InternalError);
  }

  // Connect.
  {
    error = connect(pClient->socketHandle, pResult->ai_addr, (int32_t)pResult->ai_addrlen);

    if (error == SOCKET_ERROR)
    {
      error = WSAGetLastError();
      LS_ERROR_SET(lsR_ResourceNotFound);
    }
  }

epilogue:
  if (pResult != nullptr)
    freeaddrinfo(pResult);

  return result;
}

void tcpClient_destroy(tcpClient *pClient)
{
  if (pClient == nullptr)
    return;

  if (pClient->socketHandle != 0 && pClient->socketHandle != INVALID_SOCKET)
  {
    shutdown(pClient->socketHandle, SD_SEND);
    closesocket(pClient->socketHandle);
    pClient->socketHandle = INVALID_SOCKET;
  }
}

lsResult tcpClient_send(tcpClient *pClient, const void *pData, const size_t bytes, _Out_opt_ size_t *pBytesSent /* = nullptr */)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pClient == nullptr || pData == nullptr, lsR_ArgumentNull);
  LS_ERROR_IF(bytes > INT32_MAX, lsR_ArgumentOutOfBounds);

  {
    const int32_t bytesSent = send(pClient->socketHandle, reinterpret_cast<const char *>(pData), (int32_t)bytes, 0);

    if (bytesSent == SOCKET_ERROR || bytesSent < 0)
    {
      const int32_t error = WSAGetLastError();
      (void)error;

      if (pBytesSent != nullptr)
        *pBytesSent = (size_t)bytesSent;

      LS_ERROR_SET(lsR_IOFailure);
    }

    if (pBytesSent != nullptr)
      *pBytesSent = (size_t)bytesSent;
    else
      LS_ERROR_IF(bytesSent != bytes, lsR_IOFailure);
  }

epilogue:
  return result;
}

lsResult tcpClient_receive(tcpClient *pClient, void *pData, const size_t maxLength, _Out_opt_ size_t *pBytesReceived /* = nullptr */)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pClient == nullptr || pData == nullptr, lsR_ArgumentNull);

  {
    const int32_t bytesReceived = recv(pClient->socketHandle, reinterpret_cast<char *>(pData), (int32_t)lsMin((size_t)INT32_MAX, maxLength), 0);

    if (bytesReceived < 0 || bytesReceived == SOCKET_ERROR)
    {
      const int32_t error = WSAGetLastError();
      (void)error;

      if (pBytesReceived != nullptr)
        *pBytesReceived = 0;

      LS_ERROR_SET(lsR_IOFailure);
    }
    else if (bytesReceived == 0)
    {
      if (pBytesReceived != nullptr)
        *pBytesReceived = 0;

      LS_ERROR_SET(lsR_EndOfStream);
    }
    else
    {
      if (pBytesReceived != nullptr)
        *pBytesReceived = (size_t)bytesReceived;
      else
        LS_ERROR_IF(bytesReceived != maxLength, lsR_IOFailure);
    }
  }

epilogue:
  return result;
}

lsResult tcpClient_getReadableBytes(tcpClient *pClient, _Out_ size_t *pBytes, const size_t timeoutMs /* = 0 */)
{
  lsResult result = lsR_Success;

  WSAPOLLFD pollInfo;

  LS_ERROR_IF(pClient == nullptr || pBytes == nullptr, lsR_ArgumentNull);

  pollInfo.fd = pClient->socketHandle;
  pollInfo.events = POLLRDNORM;

  {
    const int32_t status = WSAPoll(&pollInfo, 1, timeoutMs >= INT_MAX ? -1 : (INT)timeoutMs);

    if (status < 0 || pollInfo.revents < 0)
    {
      const int32_t error = WSAGetLastError();
      (void)error;

      *pBytes = 0;

      LS_ERROR_SET(lsR_IOFailure);
    }
  }

  *pBytes = (size_t)pollInfo.revents;

epilogue:
  return result;
}

lsResult tcpClient_getWriteableBytes(tcpClient *pClient, _Out_ size_t *pBytes, const size_t timeoutMs /* = 0 */)
{
  lsResult result = lsR_Success;

  WSAPOLLFD pollInfo;

  LS_ERROR_IF(pClient == nullptr || pBytes == nullptr, lsR_ArgumentNull);

  pollInfo.fd = pClient->socketHandle;
  pollInfo.events = POLLWRNORM;

  {
    const int32_t status = WSAPoll(&pollInfo, 1, timeoutMs >= INT_MAX ? -1 : (INT)timeoutMs);

    if (status < 0 || pollInfo.revents < 0)
    {
      const int32_t error = WSAGetLastError();
      (void)error;

      *pBytes = 0;

      LS_ERROR_SET(lsR_IOFailure);
    }
  }

  *pBytes = (size_t)pollInfo.revents;

epilogue:
  return result;
}

lsResult tcpClient_getRemoteEndPointInfo(tcpClient *pClient, _Out_ ipAddress *pAddress, _Out_ uint16_t *pPort)
{
  lsResult result = lsR_Success;

  SOCKADDR_STORAGE_LH address;
  int32_t nameLength = sizeof(address);

  LS_ERROR_IF(pClient == nullptr || pAddress == nullptr || pPort == nullptr, lsR_ArgumentNull);

  {
    const int32_t status = getpeername(pClient->socketHandle, reinterpret_cast<SOCKADDR *>(&address), &nameLength);
    LS_ERROR_IF(status != 0, lsR_InternalError);
  }

  LS_ERROR_CHECK(tcpSocket_getConnectionInfo(&address, pAddress, pPort));

epilogue:
  return result;
}

lsResult tcpClient_getLocalEndPointInfo(tcpClient *pClient, _Out_ ipAddress *pAddress, _Out_ uint16_t *pPort)
{
  lsResult result = lsR_Success;

  SOCKADDR_STORAGE_LH address;
  int32_t nameLength = sizeof(address);

  LS_ERROR_IF(pClient == nullptr || pAddress == nullptr || pPort == nullptr, lsR_ArgumentNull);

  {
    const int32_t status = getsockname(pClient->socketHandle, reinterpret_cast<SOCKADDR *>(&address), &nameLength);
    LS_ERROR_IF(status != 0, lsR_InternalError);
  }

  LS_ERROR_CHECK(tcpSocket_getConnectionInfo(&address, pAddress, pPort));

epilogue:
  return result;
}

lsResult tcpClient_disableSendDelay(tcpClient *pClient)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pClient == nullptr, lsR_ArgumentNull);

  {
    const BOOL option = TRUE;
    const int32_t status = setsockopt(pClient->socketHandle, SOL_SOCKET, SO_DONTLINGER, reinterpret_cast<const char *>(&option), (int32_t)sizeof(option));

    LS_ERROR_IF(status != 0, lsR_InternalError);
  }

epilogue:
  return result;
}

lsResult tcpClient_setSendTimeout(tcpClient *pClient, const size_t timeMs)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pClient == nullptr, lsR_ArgumentNull);
  LS_ERROR_IF(timeMs > MAXDWORD, lsR_ArgumentOutOfBounds);

  {
    const DWORD option = (DWORD)timeMs;
    const int32_t status = setsockopt(pClient->socketHandle, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&option), (int32_t)sizeof(option));

    LS_ERROR_IF(status != 0, lsR_InternalError);
  }

epilogue:
  return result;
}

lsResult tcpClient_setReceiveTimeout(tcpClient *pClient, const size_t timeMs)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pClient == nullptr, lsR_ArgumentNull);
  LS_ERROR_IF(timeMs > MAXDWORD, lsR_ArgumentOutOfBounds);

  {
    const DWORD option = (DWORD)timeMs;
    const int32_t status = setsockopt(pClient->socketHandle, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&option), (int32_t)sizeof(option));

    LS_ERROR_IF(status != 0, lsR_InternalError);
  }

epilogue:
  return result;
}

//////////////////////////////////////////////////////////////////////////

static lsResult tcpSocket_getConnectionInfo(SOCKADDR_STORAGE_LH *pInfo, _Out_ ipAddress *pAddress, _Out_ uint16_t *pPort)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pInfo == nullptr || pAddress == nullptr || pPort == nullptr, lsR_ArgumentNull);

  switch (pInfo->ss_family)
  {
  case AF_INET:
  {
    const SOCKADDR_IN *pIPv4 = reinterpret_cast<const SOCKADDR_IN *>(pInfo);

    pAddress->version = 4;
    *pPort = htons(pIPv4->sin_port);
    memcpy(pAddress->v4, &pIPv4->sin_addr, sizeof(pAddress->v4));

    break;
  }

  case AF_INET6:
  {
    const SOCKADDR_IN6 *pIPv6 = reinterpret_cast<const SOCKADDR_IN6 *>(pInfo);

    pAddress->version = 6;
    *pPort = htons(pIPv6->sin6_port);
    memcpy(pAddress->v6, &pIPv6->sin6_addr, sizeof(pAddress->v6));

    break;
  }

  default:
  {
    LS_ERROR_SET(lsR_NotSupported);
  }
  }

epilogue:
  return result;
}
