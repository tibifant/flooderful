#pragma once

#include "core.h"

struct data_blob
{
  uint8_t *pData = nullptr;
  size_t size = 0;
  size_t capacity = 0;
  size_t readPosition = 0;
  bool isForeign = false;

  ~data_blob();
};

//////////////////////////////////////////////////////////////////////////

void data_blob_createFromForeign(data_blob *pBlob, const void *pData, const size_t bytes);
lsResult data_blob_reserve(data_blob *pBlob, const size_t minimumSize);
void data_blob_destroy(data_blob *pBlob);
void data_blob_reset(data_blob *pBlob);

//////////////////////////////////////////////////////////////////////////

template <typename T>
lsResult data_blob_append(data_blob *pBlob, const T *pData, const size_t count = 1)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pBlob == nullptr || pData == nullptr, lsR_ArgumentNull);
  LS_ERROR_IF(pBlob->isForeign, lsR_ResourceStateInvalid);

  {
    const size_t requiredSize = sizeof(T) * count;

    if (pBlob->capacity < pBlob->size + requiredSize)
      LS_ERROR_CHECK(data_blob_reserve(pBlob, (lsMax(pBlob->size * 2, pBlob->size + requiredSize * 2) + 1023) & ~(size_t)(1023)));

    memcpy(pBlob->pData + pBlob->size, pData, requiredSize);
    pBlob->size += requiredSize;
  }

epilogue:
  return result;
}

template <typename T>
inline lsResult data_blob_appendValue(data_blob *pBlob, const T &data)
{
  return data_blob_append(pBlob, &data);
}

template <typename T>
lsResult data_blob_read(data_blob *pBlob, _Out_ T *pData, const size_t count = 1)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pBlob == nullptr || pData == nullptr, lsR_ArgumentNull);
  
  {
    const size_t requiredSize = sizeof(T) * count;

    LS_ERROR_IF(pBlob->readPosition + requiredSize > pBlob->size, lsR_ArgumentOutOfBounds);

    memcpy(pData, pBlob->pData + pBlob->readPosition, requiredSize);
    pBlob->readPosition += requiredSize;
  }

epilogue:
  return result;
}
