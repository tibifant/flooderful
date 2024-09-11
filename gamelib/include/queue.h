#pragma once

#include "core.h"

//////////////////////////////////////////////////////////////////////////

template <typename T>
struct queue;

template <typename T>
struct _const_queue_iterator;

template <typename T>
struct _queue_iterator
{
  queue<T> *pQueue;
  T *pPos;

  _queue_iterator(queue<T> *pQueue);
  T &operator *();
  const T &operator *() const;
  bool operator != (const _queue_iterator<T> other) const;
  bool operator != (const _const_queue_iterator<T> other) const;
  _queue_iterator &operator++();
};

template <typename T>
struct _reverse_queue_iterator : public _queue_iterator<T>
{
  _reverse_queue_iterator(queue<T> *pQueue);
  _reverse_queue_iterator &operator++();
};

template <typename T>
struct _const_queue_iterator
{
  const queue<T> *pQueue;
  const T *pPos;

  _const_queue_iterator(const queue<T> *pQueue);
  const T &operator *() const;
  bool operator != (const _queue_iterator<T> other) const;
  bool operator != (const _const_queue_iterator<T> other) const;
  _const_queue_iterator &operator++();
};

template <typename T>
struct _const_reverse_queue_iterator : public _const_queue_iterator<T>
{
  _const_reverse_queue_iterator(const queue<T> *pQueue);
  _const_reverse_queue_iterator &operator++();
};

//////////////////////////////////////////////////////////////////////////

template <typename T>
struct queue
{
  T *pStart = nullptr; // allocated block *starts* here.
  T *pFront = nullptr; // first queue item.
  T *pBack = nullptr; // behind last queue item.
  T *pLast = nullptr; // allocated block *ends* here.
  size_t capacity = 0, count = 0;

  inline _queue_iterator<T> begin()
  {
    lsAssert(pFront);
    return _queue_iterator<T>(this);
  }

  inline _reverse_queue_iterator<T> end()
  {
    lsAssert(pFront);
    return _reverse_queue_iterator<T>(this);
  }

  inline _const_queue_iterator<T> begin() const
  {
    lsAssert(pFront);
    return _const_queue_iterator<T>(this);
  }

  inline _const_reverse_queue_iterator<T> end() const
  {
    lsAssert(pFront);
    return _const_reverse_queue_iterator<T>(this);
  }

  inline struct
  {
    queue<T> *pQueue;
    inline _reverse_queue_iterator<T> begin() { return _reverse_queue_iterator<T>(pQueue); };
    inline _queue_iterator<T> end() { return _queue_iterator<T>(pQueue); };
  } IterateReverse() { return { this }; };

  inline struct
  {
    queue<T> *pQueue;
    inline _const_reverse_queue_iterator<T> begin() { return _const_reverse_queue_iterator<T>(pQueue); };
    inline _const_queue_iterator<T> end() { return _const_queue_iterator<T>(pQueue); };
  } IterateReverse() const { return { this }; };

  T &operator [](const size_t index);
  const T &operator [](const size_t index) const;

  ~queue();

  inline queue() {}
  
  inline queue(const queue<T> &copy) = delete;

  inline queue(queue<T> &&move) :
    pStart(move.pStart),
    pFront(move.pFront),
    pBack(move.pBack),
    pLast(move.pLast),
    capacity(move.capacity),
    count(move.count)
  {
    move.pStart = move.pFront = move.pLast = move.pBack = nullptr;
    move.capacity = 0;
    move.count = 0;
  }

  inline queue<T> &operator = (const queue<T> &move) = delete;

  inline queue<T> &operator = (queue<T> &&move)
  {
    queue_destroy(this);

    pStart = move.pStart;
    pFront = move.pFront;
    pBack = move.pBack;
    pLast = move.pLast;
    capacity = move.capacity;
    count = move.count;

    move.pStart = move.pFront = move.pLast = move.pBack = nullptr;
    move.capacity = 0;
    move.count = 0;

    return *this;
  }
};

template <typename T>
lsResult queue_reserve(queue<T> *pQueue, const size_t newCapacity)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pQueue == nullptr, lsR_ArgumentNull);

  if (pQueue->capacity < newCapacity)
  {
    const size_t offsetStart = pQueue->pFront - pQueue->pStart;
    const size_t offsetEnd = pQueue->pBack - pQueue->pStart;
    LS_ERROR_CHECK(lsRealloc(&pQueue->pStart, newCapacity));

    pQueue->pFront = pQueue->pStart + offsetStart;
    pQueue->pBack = pQueue->pStart + offsetEnd;
    pQueue->pLast = pQueue->pStart + pQueue->capacity;

    if (offsetStart > 0 && offsetStart + pQueue->count > pQueue->capacity)
    {
      const size_t wrappedCount = pQueue->count - (pQueue->capacity - offsetStart);
      memmove(pQueue->pLast, pQueue->pStart, wrappedCount * sizeof(T));
      pQueue->pBack = pQueue->pLast + wrappedCount;
    }

    pQueue->capacity = newCapacity;
    pQueue->pLast = pQueue->pStart + pQueue->capacity;
  }

epilogue:
  return result;
}

template <typename T>
lsResult queue_popBack(queue<T> *pQueue, _Out_ T *pItem)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pQueue == nullptr || pItem == nullptr, lsR_ArgumentNull);
  LS_ERROR_IF(pQueue->count == 0, lsR_ResourceNotFound);

  if (pQueue->pBack == pQueue->pStart)
    pQueue->pBack = pQueue->pLast;

  pQueue->count--;
  pQueue->pBack--;
  *pItem = *pQueue->pBack;

epilogue:
  return result;
}

template <typename T>
T &&queue_popBack_unsafe(queue<T> &q)
{
  lsAssert(q.count != 0);

  if (q.pBack == q.pStart)
    q.pBack = q.pLast;

  q.count--;
  q.pBack--;
  return std::move(*q.pBack);
}

template <typename T>
lsResult queue_pushBack(queue<T> *pQueue, const T *pItem)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pQueue == nullptr || pItem == nullptr, lsR_ArgumentNull);

  if (pQueue->count + 1 >= pQueue->capacity)
    LS_ERROR_CHECK(queue_reserve(pQueue, lsMax(64ULL, pQueue->capacity * 2)));

  pQueue->count++;
  *pQueue->pBack = *pItem;
  pQueue->pBack++;

  if (pQueue->pBack == pQueue->pLast)
    pQueue->pBack = pQueue->pStart;

epilogue:
  return result;
}

template <typename T>
lsResult queue_pushBack(queue<T> *pQueue, const T &item)
{
  return queue_pushBack(pQueue, &item);
}

template <typename T>
lsResult queue_popFront(queue<T> *pQueue, _Out_ T *pItem)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pQueue == nullptr || pItem == nullptr, lsR_ArgumentNull);
  LS_ERROR_IF(pQueue->count == 0, lsR_ResourceNotFound);

  *pItem = *pQueue->pFront;

  pQueue->pFront++;
  pQueue->count--;

  if (pQueue->pFront == pQueue->pLast)
    pQueue->pFront = pQueue->pStart;

epilogue:
  return result;
}

template <typename T>
T && queue_popFront_unsafe(queue<T> &q)
{
  lsAssert(q.count != 0);

  T && ret = std::move(*q.pFront);

  q.pFront++;
  q.count--;

  if (q.pFront == q.pLast)
    q.pFront = q.pStart;

  return std::move(ret);
}

template <typename T>
lsResult queue_pushFront(queue<T> *pQueue, const T *pItem)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pQueue == nullptr || pItem == nullptr, lsR_ArgumentNull);

  if (pQueue->count + 1 >= pQueue->capacity)
    LS_ERROR_CHECK(queue_reserve(pQueue, lsMax(64ULL, pQueue->capacity * 2)));

  pQueue->pFront--;

  if (pQueue->pFront < pQueue->pStart)
    pQueue->pFront = pQueue->pLast - 1;

  *pQueue->pFront = *pItem;
  pQueue->count++;

epilogue:
  return result;
}

template <typename T>
lsResult queue_pushFront(queue<T> *pQueue, const T &item)
{
  return queue_pushFront(pQueue, &item);
}

template <typename T>
lsResult queue_get(const queue<T> *pQueue, const size_t index, _Out_ T *pItem)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pQueue == nullptr || pItem == nullptr, lsR_ArgumentNull);
  LS_ERROR_IF(index >= pQueue->count, lsR_ResourceNotFound);

  {
    const size_t offset = pQueue->pFront - pQueue->pStart;
    *pItem = pQueue->pStart[(offset + index) % pQueue->capacity];
  }

epilogue:
  return result;
}

template <typename T>
lsResult queue_getPtr(queue<T> *pQueue, const size_t index, _Out_ T **ppItem)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pQueue == nullptr || ppItem == nullptr, lsR_ArgumentNull);
  LS_ERROR_IF(index >= pQueue->count, lsR_ResourceNotFound);

  {
    const size_t offset = pQueue->pFront - pQueue->pStart;
    *ppItem = &pQueue->pStart[(offset + index) % pQueue->capacity];
  }

epilogue:
  return result;
}

template <typename T>
T & queue_get(queue<T> *pQueue, const size_t index)
{
  T *ret = nullptr;
  queue_getPtr(pQueue, index, &ret);
  return *ret;
}

template <typename T>
T & queue_getRef_unsafe(queue<T> &q, const size_t index)
{
  lsAssert(index < q.count);
  
  const size_t offset = q.pFront - q.pStart;
  return q.pStart[(offset + index) % q.capacity];
}

template <typename T>
const T & queue_getRef_unsafe(const queue<T> &q, const size_t index)
{
  lsAssert(index < q.count);
  
  const size_t offset = q.pFront - q.pStart;
  return q.pStart[(offset + index) % q.capacity];
}

template <typename T>
void queue_clear(queue<T> *pQueue)
{
  if (pQueue == nullptr)
    return;

  if (pQueue->count)
    for (auto &_i : *pQueue)
      _i.~T();

  pQueue->pFront = pQueue->pBack = pQueue->pStart;
  pQueue->count = 0;
}

template <typename T>
bool queue_any(queue<T> *pQueue)
{
  if (pQueue != nullptr)
    return pQueue->pBack != pQueue->pFront;

  return false;
}

template <typename T>
void queue_destroy(queue<T> *pQueue)
{
  if (pQueue->count)
    for (auto &_i : *pQueue)
      _i.~T();

  lsFreePtr(&pQueue->pStart);

  pQueue->pFront = nullptr;
  pQueue->pBack = nullptr;
  pQueue->pLast = nullptr;
  pQueue->capacity = 0;
  pQueue->count = 0;
}

template <typename T>
lsResult queue_clone(const queue<T> *pSource, queue<T> *pTarget)
{
  lsResult result = lsR_Success;

  LS_ERROR_IF(pSource == nullptr || pTarget == nullptr, lsR_ArgumentNull);

  queue_clear(pTarget);
  LS_ERROR_CHECK(queue_reserve(pTarget, pSource->count));

  const size_t offsetStart = pSource->pFront - pSource->pStart;
  
  if (offsetStart > 0 && offsetStart + pSource->count > pSource->capacity)
  {
    const size_t wrappedCount = pSource->count - (pSource->capacity - offsetStart);
    lsMemcpy(pTarget->pFront, pSource->pFront, pSource->count - wrappedCount);
    lsMemcpy(pTarget->pFront + pSource->count - wrappedCount, pSource->pStart, wrappedCount);
  }
  else
  {
    lsMemcpy(pTarget->pFront, pSource->pFront, pSource->count);
  }

  pTarget->count = pSource->count;
  pTarget->pBack += pTarget->count;

epilogue:
  return result;
}

template<typename T>
inline void queue_reverse(queue<T> &queue)
{
  const size_t last = queue.count - 1;
  const size_t half = queue.count / 2;

  for (size_t i = 0; i < half; i++)
    std::swap(queue[i], queue[last - i]);
}

template<typename T, typename TLessFunc, typename TGreaterFunc>
inline void queue_orderBy(queue<T> &q)
{
  struct _internal
  {
    static void dualPivotQuickSort_partition(queue<T> &q, const int64_t low, const int64_t high, int64_t *pRightPivot, int64_t *pLeftPivot)
    {
      TLessFunc _less = TLessFunc();
      TGreaterFunc _greater = TGreaterFunc();

      if (_greater(q[low], q[high]))
        std::swap(q[low], q[high]);

      int64_t j = low + 1;
      int64_t g = high - 1;
      int64_t k = low + 1;

      T *pP = &q[low];
      T *pQ = &q[high];

      while (k <= g)
      {
        if (_less(q[k], *pP))
        {
          std::swap(q[k], q[j]);
          j++;
        }

        else if (!_less(q[k], *pQ))
        {
          while (_greater(q[g], *pQ) && k < g)
            g--;

          std::swap(q[k], q[g]);
          g--;

          if (_less(q[k], *pP))
          {
            std::swap(q[k], q[j]);
            j++;
          }
        }

        k++;
      }

      j--;
      g++;

      std::swap(q[low], q[j]);
      std::swap(q[high], q[g]);

      *pLeftPivot = j;
      *pRightPivot = g;
    }

    static void quickSort(queue<T> &q, const int64_t start, const int64_t end)
    {
      if (start < end)
      {
        int64_t leftPivot, rightPivot;

        dualPivotQuickSort_partition(q, start, end, &rightPivot, &leftPivot);

        quickSort(q, start, leftPivot - 1);
        quickSort(q, leftPivot + 1, rightPivot - 1);
        quickSort(q, rightPivot + 1, end);
      }
    }
  };

  if (q.count)
    _internal::quickSort(q, 0, q.count - 1);
}

template<typename T, typename TLessFunc>
inline void queue_orderBy(queue<T> &q)
{
  struct _internal
  {
    static void dualPivotQuickSort_partition(queue<T> &q, const int64_t low, const int64_t high, int64_t *pRightPivot, int64_t *pLeftPivot)
    {
      TLessFunc _less = TLessFunc();

      if (!_less(q[low], q[high]))
        std::swap(q[low], q[high]);

      int64_t j = low + 1;
      int64_t g = high - 1;
      int64_t k = low + 1;

      T *pP = &q[low];
      T *pQ = &q[high];

      while (k <= g)
      {
        if (_less(q[k], *pP))
        {
          std::swap(q[k], q[j]);
          j++;
        }

        else if (!_less(q[k], *pQ))
        {
          while (!_less(q[g], *pQ) && k < g)
            g--;

          std::swap(q[k], q[g]);
          g--;

          if (_less(q[k], *pP))
          {
            std::swap(q[k], q[j]);
            j++;
          }
        }

        k++;
      }

      j--;
      g++;

      std::swap(q[low], q[j]);
      std::swap(q[high], q[g]);

      *pLeftPivot = j;
      *pRightPivot = g;
    }

    static void quickSort(queue<T> &q, const int64_t start, const int64_t end)
    {
      if (start < end)
      {
        int64_t leftPivot, rightPivot;

        dualPivotQuickSort_partition(q, start, end, &rightPivot, &leftPivot);

        quickSort(q, start, leftPivot - 1);
        quickSort(q, leftPivot + 1, rightPivot - 1);
        quickSort(q, rightPivot + 1, end);
      }
    }
  };

  if (q.count)
    _internal::quickSort(q, 0, q.count - 1);
}

//////////////////////////////////////////////////////////////////////////

template<typename T>
inline T & queue<T>::operator[](const size_t index)
{
  return queue_getRef_unsafe(*this, index);
}

template<typename T>
inline const T & queue<T>::operator[](const size_t index) const
{
  return queue_getRef_unsafe(*this, index);
}

template<typename T>
inline queue<T>::~queue()
{
  queue_destroy(this);
}

//////////////////////////////////////////////////////////////////////////

template<typename T>
inline _queue_iterator<T>::_queue_iterator(queue<T> *pQueue) :
  pQueue(pQueue),
  pPos(pQueue->pFront)
{ }

template<typename T>
inline T &_queue_iterator<T>::operator*()
{
  return *pPos;
}

template<typename T>
inline const T &_queue_iterator<T>::operator*() const
{
  return *pPos;
}

template<typename T>
inline bool _queue_iterator<T>::operator!=(const _queue_iterator other) const
{
  return pPos != other.pPos;
}

template<typename T>
inline bool _queue_iterator<T>::operator!=(const _const_queue_iterator<T> other) const
{
  return pPos != other.pPos;
}

template<typename T>
inline _queue_iterator<T> &_queue_iterator<T>::operator++()
{
  pPos++;

  if (pPos >= pQueue->pLast)
    pPos = pQueue->pStart;

  return *this;
}

template<typename T>
inline _reverse_queue_iterator<T>::_reverse_queue_iterator(queue<T> *pQueue) :
  _queue_iterator<T>(pQueue)
{
  this->pPos = pQueue->pBack;
}

template<typename T>
inline _reverse_queue_iterator<T> &_reverse_queue_iterator<T>::operator++()
{
  this->pPos--;

  if (this->pPos < this->pQueue->pStart)
    this->pPos = this->pQueue->pLast - 1;

  return *this;
}

template<typename T>
inline _const_queue_iterator<T>::_const_queue_iterator(const queue<T> *pQueue) :
  pQueue(pQueue),
  pPos(pQueue->pFront)
{ }

template<typename T>
inline const T &_const_queue_iterator<T>::operator*() const
{
  return *this->pPos;
}

template<typename T>
inline bool _const_queue_iterator<T>::operator!=(const _queue_iterator<T> other) const
{
  return this->pPos != other.pPos;
}

template<typename T>
inline bool _const_queue_iterator<T>::operator!=(const _const_queue_iterator<T> other) const
{
  return this->pPos != other.pPos;
}

template<typename T>
inline _const_queue_iterator<T> &_const_queue_iterator<T>::operator++()
{
  this->pPos++;

  if (this->pPos >= this->pQueue->pLast)
    this->pPos = this->pQueue->pStart;

  return *this;
}

template<typename T>
inline _const_reverse_queue_iterator<T>::_const_reverse_queue_iterator(const queue<T> *pQueue) :
  _const_queue_iterator(pQueue)
{
  this->pPos = this->pQueue->pBack;
}

template<typename T>
inline _const_reverse_queue_iterator<T> &_const_reverse_queue_iterator<T>::operator++()
{
  this->pPos--;

  if (this->pPos < this->pQueue->pStart)
    this->pPos = this->pQueue->pLast - 1;

  return *this;
}
