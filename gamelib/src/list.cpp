#include "list.h"

#include "testable.h"
REGISTER_TESTABLE_FILE(6);

DEFINE_TESTABLE(list_remove_at)
{
  lsResult result = lsR_Success;

  for (size_t j = 0; j < 12; j++)
  {
    list<size_t> l;

    for (size_t i = 0; i < 12; i++)
      TESTABLE_ASSERT_SUCCESS(list_add(&l, i));

    size_t v;
    list_remove_at(l, j, &v);
    TESTABLE_ASSERT_EQUAL(v, j);
    TESTABLE_ASSERT_EQUAL(l.count, 11);

    for (size_t i = 0; i < 11; i++)
      TESTABLE_ASSERT_EQUAL(l[i], i >= j ? i + 1 : i);
  }

epilogue:
  return result;
}

DEFINE_TESTABLE(list_remove_sorted_indexes_single)
{
  lsResult result = lsR_Success;

  for (size_t j = 0; j < 12; j++)
  {
    list<size_t> l;

    for (size_t i = 0; i < 12; i++)
      TESTABLE_ASSERT_SUCCESS(list_add(&l, i));

    size_t v;
    list_remove_sorted_indexes(l, &j, 1, &v);
    TESTABLE_ASSERT_EQUAL(v, j);
    TESTABLE_ASSERT_EQUAL(l.count, 11);

    for (size_t i = 0; i < 11; i++)
      TESTABLE_ASSERT_EQUAL(l[i], i >= j ? i + 1 : i);
  }

epilogue:
  return result;
}

DEFINE_TESTABLE(list_remove_sorted_indexes_multiple)
{
  lsResult result = lsR_Success;

  {
    constexpr size_t total = 13;
    list<size_t> l;

    for (size_t i = 0; i < total; i++)
      TESTABLE_ASSERT_SUCCESS(list_add(&l, i));

    const uint8_t idx[] = { 0, 2, 4, 6, 8, 10, 12 };
    size_t v[LS_ARRAYSIZE(idx)];
    list_remove_sorted_indexes(l, idx, LS_ARRAYSIZE(idx), v);

    for (size_t i = 0; i < LS_ARRAYSIZE(idx); i++)
      TESTABLE_ASSERT_EQUAL(v[i], i * 2);

    constexpr size_t count = total - LS_ARRAYSIZE(idx);
    TESTABLE_ASSERT_EQUAL(l.count, count);

    for (size_t i = 0; i < count; i++)
      TESTABLE_ASSERT_EQUAL(l[i], i * 2 + 1);
  }

  {
    constexpr size_t total = 12;
    list<size_t> l;

    for (size_t i = 0; i < total; i++)
      TESTABLE_ASSERT_SUCCESS(list_add(&l, i));

    const uint8_t idx[] = { 0, 2, 4, 6, 8, 10 };
    size_t v[LS_ARRAYSIZE(idx)];
    list_remove_sorted_indexes(l, idx, LS_ARRAYSIZE(idx), v);

    for (size_t i = 0; i < LS_ARRAYSIZE(idx); i++)
      TESTABLE_ASSERT_EQUAL(v[i], i * 2);

    constexpr size_t count = total - LS_ARRAYSIZE(idx);
    TESTABLE_ASSERT_EQUAL(l.count, count);

    for (size_t i = 0; i < count; i++)
      TESTABLE_ASSERT_EQUAL(l[i], i * 2 + 1);
  }

  {
    constexpr size_t total = 13;
    list<size_t> l;

    for (size_t i = 0; i < total; i++)
      TESTABLE_ASSERT_SUCCESS(list_add(&l, i));

    const uint8_t idx[] = { 1, 3, 5, 7, 9, 11 };
    size_t v[LS_ARRAYSIZE(idx)];
    list_remove_sorted_indexes(l, idx, LS_ARRAYSIZE(idx), v);

    for (size_t i = 0; i < LS_ARRAYSIZE(idx); i++)
      TESTABLE_ASSERT_EQUAL(v[i], i * 2 + 1);

    constexpr size_t count = total - LS_ARRAYSIZE(idx);
    TESTABLE_ASSERT_EQUAL(l.count, count);

    for (size_t i = 0; i < count; i++)
      TESTABLE_ASSERT_EQUAL(l[i], i * 2);
  }

  {
    constexpr size_t total = 12;
    list<size_t> l;

    for (size_t i = 0; i < total; i++)
      TESTABLE_ASSERT_SUCCESS(list_add(&l, i));

    const uint8_t idx[] = { 1, 3, 5, 7, 9, 11 };
    size_t v[LS_ARRAYSIZE(idx)];
    list_remove_sorted_indexes(l, idx, LS_ARRAYSIZE(idx), v);

    for (size_t i = 0; i < LS_ARRAYSIZE(idx); i++)
      TESTABLE_ASSERT_EQUAL(v[i], i * 2 + 1);

    constexpr size_t count = total - LS_ARRAYSIZE(idx);
    TESTABLE_ASSERT_EQUAL(l.count, count);

    for (size_t i = 0; i < count; i++)
      TESTABLE_ASSERT_EQUAL(l[i], i * 2);
  }

  {
    list<size_t> l;

    const size_t data[] = { 7, 6, 5, 4, 3, 2, 1 };
    TESTABLE_ASSERT_SUCCESS(list_add_range(&l, data, LS_ARRAYSIZE(data)));

    const uint8_t idx[] = { 1, 2, 3 };
    size_t v[LS_ARRAYSIZE(idx)];
    list_remove_sorted_indexes(l, idx, LS_ARRAYSIZE(idx), v);

    for (size_t i = 0; i < LS_ARRAYSIZE(v); i++)
      TESTABLE_ASSERT_EQUAL(v[i], data[idx[i]]);

    const size_t expected[] = { 7, 3, 2, 1 };
    static_assert(LS_ARRAYSIZE(data) - LS_ARRAYSIZE(idx) == LS_ARRAYSIZE(expected));
    TESTABLE_ASSERT_EQUAL(l.count, LS_ARRAYSIZE(expected));

    for (size_t i = 0; i < LS_ARRAYSIZE(expected); i++)
      TESTABLE_ASSERT_EQUAL(l[i], expected[i]);
  }

  {
    list<size_t> l;

    const size_t data[] = { 69, 68, 67, 62, 45, 43, 42, 41, 33 };
    TESTABLE_ASSERT_SUCCESS(list_add_range(&l, data, LS_ARRAYSIZE(data)));

    const uint8_t idx[] = { 0, 1, 2, 7 };
    size_t v[LS_ARRAYSIZE(idx)];
    list_remove_sorted_indexes(l, idx, LS_ARRAYSIZE(idx), v);

    for (size_t i = 0; i < LS_ARRAYSIZE(v); i++)
      TESTABLE_ASSERT_EQUAL(v[i], data[idx[i]]);

    const size_t expected[] = { 62, 45, 43, 42, 33 };
    static_assert(LS_ARRAYSIZE(data) - LS_ARRAYSIZE(idx) == LS_ARRAYSIZE(expected));
    TESTABLE_ASSERT_EQUAL(l.count, LS_ARRAYSIZE(expected));

    for (size_t i = 0; i < LS_ARRAYSIZE(expected); i++)
      TESTABLE_ASSERT_EQUAL(l[i], expected[i]);
  }

epilogue:
  return result;
}

DEFINE_TESTABLE(list_insert_sorted_indexes)
{
  lsResult result = lsR_Success;

  {
    constexpr size_t total = 6;
    list<size_t> l;

    for (size_t i = 0; i < total; i++)
      TESTABLE_ASSERT_SUCCESS(list_add(&l, (i + 1) * 3));

    const uint8_t idx[] = { 0, 0, 0, 6, 6, 6 };
    const size_t v[LS_ARRAYSIZE(idx)] = { 0, 1, 2, 19, 20, 21 };
    TESTABLE_ASSERT_SUCCESS(list_insert_sorted_indexes(l, idx, LS_ARRAYSIZE(idx), v));

    const size_t expected[] = { 0, 1, 2, 3, 6, 9, 12, 15, 18, 19, 20, 21 };
    static_assert(LS_ARRAYSIZE(expected) == LS_ARRAYSIZE(idx) + total);

    TESTABLE_ASSERT_EQUAL(l.count, LS_ARRAYSIZE(idx) + total);

    for (size_t i = 0; i < LS_ARRAYSIZE(expected); i++)
      TESTABLE_ASSERT_EQUAL(l[i], expected[i]);
  }

  {
    constexpr size_t total = 6;
    list<size_t> l;

    for (size_t i = 0; i < total; i++)
      TESTABLE_ASSERT_SUCCESS(list_add(&l, (i + 1) * 3));

    const uint8_t idx[] = { 1, 2, 3, 4, 5 };
    const size_t v[LS_ARRAYSIZE(idx)] = { 4, 7, 10, 13, 16 };
    TESTABLE_ASSERT_SUCCESS(list_insert_sorted_indexes(l, idx, LS_ARRAYSIZE(idx), v));

    const size_t expected[] = { 3, 4, 6, 7, 9, 10, 12, 13, 15, 16, 18 };
    static_assert(LS_ARRAYSIZE(expected) == LS_ARRAYSIZE(idx) + total);

    TESTABLE_ASSERT_EQUAL(l.count, LS_ARRAYSIZE(idx) + total);

    for (size_t i = 0; i < LS_ARRAYSIZE(expected); i++)
      TESTABLE_ASSERT_EQUAL(l[i], expected[i]);
  }

  {
    constexpr size_t total = 6;
    list<size_t> l;

    for (size_t i = 0; i < total; i++)
      TESTABLE_ASSERT_SUCCESS(list_add(&l, (i + 1) * 3));

    const uint8_t idx[] = { 0, 1, 2, 3, 4, 5, 6 };
    const size_t v[LS_ARRAYSIZE(idx)] = { 0, 4, 7, 10, 13, 16, 19 };
    TESTABLE_ASSERT_SUCCESS(list_insert_sorted_indexes(l, idx, LS_ARRAYSIZE(idx), v));

    const size_t expected[] = { 0, 3, 4, 6, 7, 9, 10, 12, 13, 15, 16, 18, 19 };
    static_assert(LS_ARRAYSIZE(expected) == LS_ARRAYSIZE(idx) + total);

    TESTABLE_ASSERT_EQUAL(l.count, LS_ARRAYSIZE(idx) + total);

    for (size_t i = 0; i < LS_ARRAYSIZE(expected); i++)
      TESTABLE_ASSERT_EQUAL(l[i], expected[i]);
  }

  {
    constexpr size_t total = 6;
    list<size_t> l;

    for (size_t i = 0; i < total; i++)
      TESTABLE_ASSERT_SUCCESS(list_add(&l, (i + 1) * 3));

    const uint8_t idx[] = { 0, 1, 2, 3, 3, 4, 5, 6 };
    const size_t v[LS_ARRAYSIZE(idx)] = { 0, 4, 7, 10, 11, 13, 16, 19 };
    TESTABLE_ASSERT_SUCCESS(list_insert_sorted_indexes(l, idx, LS_ARRAYSIZE(idx), v));

    const size_t expected[] = { 0, 3, 4, 6, 7, 9, 10, 11, 12, 13, 15, 16, 18, 19 };
    static_assert(LS_ARRAYSIZE(expected) == LS_ARRAYSIZE(idx) + total);

    TESTABLE_ASSERT_EQUAL(l.count, LS_ARRAYSIZE(idx) + total);

    for (size_t i = 0; i < LS_ARRAYSIZE(expected); i++)
      TESTABLE_ASSERT_EQUAL(l[i], expected[i]);
  }

epilogue:
  return result;
}

DEFINE_TESTABLE(list_sorted_find)
{
  lsResult result = lsR_Success;

  {
    list<size_t> l;

    const size_t v[] = { 1, 3, 4, 6, 7, 9, 10, 11, 12, 13, 15, 16, 18, 19 };
    TESTABLE_ASSERT_SUCCESS(list_add_range(&l, v, LS_ARRAYSIZE(v)));

    TESTABLE_ASSERT_EQUAL((size_t)-1, sorted_list_find(l, 0));
    TESTABLE_ASSERT_EQUAL((size_t)-1, sorted_list_find(l, 2));
    TESTABLE_ASSERT_EQUAL((size_t)-1, sorted_list_find(l, 20));

    for (size_t i = 0; i < LS_ARRAYSIZE(v); i++)
      TESTABLE_ASSERT_EQUAL(i, sorted_list_find(l, v[i]));
  }

  {
    list<size_t> l;

    const size_t v[] = { 20, 19, 15, 14, 13, 12, 10, 7, 6, 4, 3, 1 };
    TESTABLE_ASSERT_SUCCESS(list_add_range(&l, v, LS_ARRAYSIZE(v)));

    TESTABLE_ASSERT_EQUAL((size_t)-1, sorted_list_find(l, 0));
    TESTABLE_ASSERT_EQUAL((size_t)-1, sorted_list_find(l, 18));
    TESTABLE_ASSERT_EQUAL((size_t)-1, sorted_list_find(l, 21));

    for (size_t i = 0; i < LS_ARRAYSIZE(v); i++)
      TESTABLE_ASSERT_EQUAL(i, descending_sorted_list_find(l, v[i]));
  }

epilogue:
  return result;
}
