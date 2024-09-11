#include "data_blob.h"

//////////////////////////////////////////////////////////////////////////

void data_blob_createFromForeign(data_blob *pBlob, const void *pData, const size_t bytes)
{
  if (pBlob == nullptr)
    return;

  pBlob->pData = reinterpret_cast<uint8_t *>(const_cast<void *>(pData));
  pBlob->isForeign = true;
  pBlob->size = bytes;
  pBlob->readPosition = 0;
}

lsResult data_blob_reserve(data_blob *pBlob, const size_t minimumSize)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pBlob == nullptr, lsR_ArgumentNull);
  LS_ERROR_IF(pBlob->isForeign, lsR_ResourceStateInvalid);

  if (pBlob->capacity < minimumSize)
  {
    LS_ERROR_CHECK(lsRealloc(&pBlob->pData, minimumSize));
    pBlob->capacity = minimumSize;
  }

epilogue:
  return result;
}

void data_blob_destroy(data_blob *pBlob)
{
  if (!pBlob->isForeign)
    lsFreePtr(&pBlob->pData);

  pBlob->size = 0;
  pBlob->capacity = 0;
}

void data_blob_reset(data_blob *pBlob)
{
  if (pBlob == nullptr)
    return;

  pBlob->readPosition = 0;

  if (!pBlob->isForeign)
    pBlob->size = 0;
}

data_blob::~data_blob()
{
  data_blob_destroy(this);
}
