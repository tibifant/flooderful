#pragma once

#include "core.h"

struct raw_string
{
  char *text = nullptr;
  size_t bytes = 0;
  size_t capacity = 0;

  inline ~raw_string();
};

lsResult string_reserve(raw_string &str, const size_t bytes);
lsResult string_append(raw_string &str, const char *text, const size_t bytes);

template <typename T>
inline lsResult string_append(raw_string &str, T text)
{
  return string_append(str, text, lsStringLength(text));
}

void string_destroy(raw_string &str);

inline raw_string::~raw_string()
{
  string_destroy(*this);
}
