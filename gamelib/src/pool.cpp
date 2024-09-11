#include "pool.h"

#include "testable.h"
REGISTER_TESTABLE_FILE(8)

DEFINE_TESTABLE(pool_compact)
{
  lsResult result = lsR_Success;

  {
    pool<int32_t> p;

    TESTABLE_ASSERT_SUCCESS(pool_insertAt(&p, 1, 1));
    TESTABLE_ASSERT_SUCCESS(pool_insertAt(&p, 2, 2));
    TESTABLE_ASSERT_SUCCESS(pool_insertAt(&p, 5, 6));
    TESTABLE_ASSERT_SUCCESS(pool_insertAt(&p, 4, 24));
    TESTABLE_ASSERT_SUCCESS(pool_insertAt(&p, 3, 25));
    TESTABLE_ASSERT_SUCCESS(pool_insertAt(&p, 0, 100));

    std::function<void(int &, size_t, size_t)> nullfunc;
    pool_compact(p, nullfunc);

    for (const auto &&e : p)
    {
      const int32_t val = *e.pItem;
      const size_t idx = e.index;
      TESTABLE_ASSERT_EQUAL(idx, val);
    }
  }

epilogue:
  return result;
}

DEFINE_TESTABLE(pool_remove_iterator)
{
  lsResult result = lsR_Success;

  {
    pool<size_t> p;

    for (size_t i = 0; i < 128; i++)
      TESTABLE_ASSERT_SUCCESS(pool_insertAt(&p, i, i));

    size_t it = (size_t)-1;

    for (auto &&e : p)
    {
      ++it;
      const size_t val = *e.pItem;
      const size_t idx = e.index;
      TESTABLE_ASSERT_EQUAL(it, idx);
      TESTABLE_ASSERT_EQUAL(idx, val);
      TESTABLE_ASSERT_EQUAL(128 - idx, p.count);
      TESTABLE_ASSERT_EQUAL(val, pool_remove(e));
      TESTABLE_ASSERT_EQUAL(128 - idx - 1, p.count);
    }

    TESTABLE_ASSERT_EQUAL(it, 127);
  }

epilogue:
  return result;
}
