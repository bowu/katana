/** worklists building blocks -*- C++ -*-
 * @file
 * @section License
 *
 * Galois, a framework to exploit amorphous data-parallelism in irregular
 * programs.
 *
 * Copyright (C) 2012, The University of Texas at Austin. All rights reserved.
 * UNIVERSITY EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES CONCERNING THIS
 * SOFTWARE AND DOCUMENTATION, INCLUDING ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR ANY PARTICULAR PURPOSE, NON-INFRINGEMENT AND WARRANTIES OF
 * PERFORMANCE, AND ANY WARRANTY THAT MIGHT OTHERWISE ARISE FROM COURSE OF
 * DEALING OR USAGE OF TRADE.  NO WARRANTY IS EITHER EXPRESS OR IMPLIED WITH
 * RESPECT TO THE USE OF THE SOFTWARE OR DOCUMENTATION. Under no circumstances
 * shall University be liable for incidental, special, indirect, direct or
 * consequential damages or loss of profits, interruption of business, or
 * related expenses which may arise from use of Software or Documentation,
 * including but not limited to those resulting from defects in Software and/or
 * Documentation, or loss or inaccuracy of data of any kind.
 *
 * @author Andrew Lenharth <andrewl@lenharth.org>
 */
#ifndef GALOIS_RUNTIME_WORKLISTHELPERS_H
#define GALOIS_RUNTIME_WORKLISTHELPERS_H

#ifndef WLCOMPILECHECK
#define WLCOMPILECHECK(name) //
#endif

#include "ll/PtrLock.h"

#include "Galois/Runtime/ll/PaddedLock.h"

#include <boost/optional.hpp>

namespace GaloisRuntime {
namespace WorkList {

template<typename T, unsigned __chunksize = 64, bool concurrent = true>
class FixedSizeRing :private boost::noncopyable, private LL::PaddedLock<concurrent> {
  using LL::PaddedLock<concurrent>::lock;
  using LL::PaddedLock<concurrent>::unlock;
  unsigned start;
  unsigned end;
  //FIXME: This is the last place requiring default constructors in the worklists
  //T data[__chunksize + 1];

  char datac[sizeof(T[__chunksize + 1])] __attribute__ ((aligned (__alignof__(T))));

  T* at(unsigned i) {
    assert(i < (__chunksize + 1));
    T* s = reinterpret_cast<T*>(&datac[0]);
    return &s[i];
  }

  inline unsigned chunksize() const { return __chunksize + 1; }

  inline bool _i_empty() const {
    return start == end;
  }

  inline bool _i_full() const {
    return (end + 1) % chunksize() == start;
  }

  inline void assertSE() const {
    assert(start <= chunksize());
    assert(end <= chunksize());
  }

public:
  
  template<bool newconcurrent>
  struct rethread {
    typedef FixedSizeRing<T, __chunksize, newconcurrent> WL;
  };

  typedef T value_type;

  FixedSizeRing() :start(0), end(0) { assertSE(); }

  int size() const {
    unsigned s = start;
    unsigned e = end;
    int retval = 0;
    while (s != e) {
      ++retval;
      ++s;
      s %= chunksize();
    }
    return retval;
  }

  bool empty() const {
    lock();
    assertSE();
    bool retval = _i_empty();
    assertSE();
    unlock();
    return retval;
  }

  bool full() const {
    lock();
    assertSE();
    bool retval = _i_full();
    assertSE();
    unlock();
    return retval;
  }

  bool push_front(value_type val) {
    lock();
    assertSE();
    if (_i_full()) {
      unlock();
      return false;
    }
    start += chunksize() - 1;
    start %= chunksize();
    new (at(start)) T(val);
    assertSE();
    unlock();
    return true;
  }

  bool push_back(value_type val) {
    lock();
    assertSE();
    if (_i_full()) {
      unlock();
      return false;
    }
    new (at(end)) T(val);
    end += 1;
    end %= chunksize();
    assertSE();
    unlock();
    return true;
  }

  template<typename Iter>
  Iter push_back(Iter b, Iter e) {
    lock();
    assertSE();
    while (!_i_full() && b != e) {
      new (at(end)) T(*b++);
      ++end;
      end %= chunksize();
    }
    assertSE();
    unlock();
    return b;
  }

  boost::optional<value_type> pop_front() {
    lock();
    assertSE();
    if (_i_empty()) {
      unlock();
      return boost::optional<value_type>();
    }
    value_type retval = *at(start);
    at(start)->~T();
    ++start;
    start %= chunksize();
    assertSE();
    unlock();
    return boost::optional<value_type>(retval);
  }

  boost::optional<value_type> pop_back() {
    lock();
    assertSE();
    if (_i_empty()) {
      unlock();
      return boost::optional<value_type>();
    }
    end += chunksize() - 1;
    end %= chunksize();
    value_type retval = *at(end);
    at(end)->~T();
    assertSE();
    unlock();
    return boost::optional<value_type>(retval);
  }
};

template<typename T, bool concurrent>
class ConExtLinkedStack {
  LL::PtrLock<T*, concurrent> head;

public:
  
  class ListNode {
    T* NextPtr;
  public:
    ListNode() :NextPtr(0) {}
    T*& getNextPtr() { return NextPtr; }
  };

  bool empty() const {
    return !head.getValue();
  }

  void push(T* C) {
    T* oldhead(0);
    do {
      oldhead = head.getValue();
      C->getNextPtr() = oldhead;
    } while (!head.CAS(oldhead, C));
  }

  T* pop() {
    //lock free Fast path (empty)
    if (empty()) return 0;
    
    //Disable CAS
    head.lock();
    T* C = head.getValue();
    if (!C) {
      head.unlock();
      return 0;
    }
    head.unlock_and_set(C->getNextPtr());
    C->getNextPtr() = 0;
    return C;
  }
};


template<typename T, bool concurrent>
class ConExtLinkedQueue {
  
  LL::PtrLock<T*,concurrent> head;
  T* tail;
  
public:
  class ListNode {
    T* NextPtr;
  public:
    ListNode() :NextPtr(0) {}
    T*& getNextPtr() { return NextPtr; }
  };
  
  ConExtLinkedQueue() :tail(0) { }

  bool empty() const {
    return !tail;
  }

  void push(T* C) {
    head.lock();
    //std::cerr << "in(" << C << ") ";
    C->getNextPtr() = 0;
    if (tail) {
      tail->getNextPtr() = C;
      tail = C;
      head.unlock();
    } else {
      assert(!head.getValue());
      tail = C;
      head.unlock_and_set(C);
    }
  }

  T* pop() {
    //lock free Fast path empty case
    if (empty()) return 0;

    head.lock();
    T* C = head.getValue();
    if (!C) {
      head.unlock();
      return 0;
    }
    if (tail == C) {
      tail = 0;
      assert(!C->getNextPtr());
      head.unlock_and_clear();
    } else {
      head.unlock_and_set(C->getNextPtr());
      C->getNextPtr() = 0;
    }
    return C;
  }
};

template<typename T>
struct DummyIndexer: public std::unary_function<const T&,unsigned> {
  unsigned operator()(const T& x) { return 0; }
};

}
}


#endif

