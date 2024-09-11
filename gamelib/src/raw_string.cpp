#include "raw_string.h"

lsResult string_reserve(raw_string &str, const size_t bytes)
{
  lsResult result = lsR_Success;

  if (bytes > str.capacity)
  {
    const size_t newCapacity = bytes;
    LS_ERROR_CHECK(lsRealloc(&str.text, newCapacity));
    str.capacity = newCapacity;
  }

epilogue:
  return result;
}

lsResult string_append(raw_string &str, const char *text, const size_t bytes)
{
  lsResult result = lsR_Success;

  if (bytes == 0)
    goto epilogue;

  {
    size_t len = bytes;

    if (text[len - 1] == '\0')
      len--;

    const size_t combinedLength = lsMax(str.bytes, 1) + len;

    // Grow if we need to grow.
    if (str.capacity <= combinedLength)
    {
      const size_t newCapacity = lsMax(str.capacity * 2 + 64, combinedLength + 63) & ~(size_t)(64 - 1);
      LS_ERROR_CHECK(lsRealloc(&str.text, newCapacity));
      str.capacity = newCapacity;
    }

    // Append.
    const size_t offset = str.bytes == 0 ? 0 : str.bytes - 1;
    lsMemcpy(str.text + offset, text, bytes);
    str.text[offset + bytes] = '\0';
    str.bytes = combinedLength;
  }

epilogue:
  return result;
}

void string_destroy(raw_string &str)
{
  str.bytes = 0;
  str.capacity = 0;
  lsFreePtr(&str.text);
}
