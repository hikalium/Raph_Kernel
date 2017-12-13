/*
 *
 * Copyright (c) 2017 Raphine Project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Author: Liva
 * 
 */

#pragma once

#include <raph.h>
#include <_queue3.h>
#include <oqueue.h>
#include <_buf.h>
#include <ptr.h>
#include <function.h>
#include <cpu.h>
#include <timer.h>
#include <setjmp.h>

class ThreadCtrl;

class Thread final : public IntQueue<Thread>::ContainerInterface, public CustomPtrObjInterface {
public:
  enum class State {
    kRunning,
    kWaitingInQueue,
    kStopping,
    kWaitingToDelete,
    kDeleting,
    kOutOfQueue,
    kIdle,
  };
  enum class StackState {
    kIndependent,
    kShared,
  };
  Thread() = delete;
  Thread(ThreadCtrl *ctrl) : _op_obj(this), _wq_ele(this), _ctrl(ctrl), _container(this), _stack_state(StackState::kShared) {
  }
  virtual ~Thread() {
    kassert(_state == State::kOutOfQueue);
  }
  void Join();

  class OperatorObj;
  class Operator final {
  public:
    Operator() = delete;
    Operator(const Operator &op);
    Operator &operator=(const Operator &op) = delete;
    ~Operator();
    Operator *operator&() = delete;
    void Schedule();
    void Schedule(int us);
    void SetFunc(uptr<GenericFunction<>> func);
    State GetState() {
      return _obj._thread->GetState();
    }
    State Stop() {
      return _obj._thread->Stop();
    }
  private:
    friend class Thread;
    explicit Operator(Thread::OperatorObj &obj);
  
    OperatorObj& _obj;
  };
  Operator CreateOperator();
private:
  friend class PtrObj<Thread, true>;
  virtual void Delete() override;
  void Wait();
  State Stop();
  State GetState() {
    return _state;
  }
  void Schedule();
  void Schedule(int us);

  //  friend class Operator;
  class OperatorObj {
  public:
    OperatorObj() = delete;
    OperatorObj(Thread *thread) : _thread(thread) {
    }
    int _cnt = 0;
    Thread *_thread;
  } _op_obj;

  friend class ThreadCtrl;
  //
  // functions for ThreadCtrl
  //
  void Init(StackState sstate);
  void Execute();
  class WaitQueueElement final : public OrderedQueue<WaitQueueElement, Time>::ContainerInterface {
  public:
    WaitQueueElement() = delete;
    WaitQueueElement(Thread *thread) : _container(this), _thread(thread) {
    }
    virtual OrderedQueue<WaitQueueElement, Time>::Container *GetContainer() override {
      return &_container;
    }
    Thread *GetThread() {
      return _thread;
    }
  private:
    OrderedQueue<WaitQueueElement, Time>::Container _container;
    Thread *_thread;
  };
  WaitQueueElement &GetWqElement() {
    return _wq_ele;
  }
  virtual IntQueue<Thread>::Container *GetContainer() override {
    return &_container;
  }
  State SetState(State state) {
    return __sync_lock_test_and_set(&_state, state);
  }
  bool CompareAndSetState(State old_state, State new_state) {
    return __sync_bool_compare_and_swap(&_state, old_state, new_state);
  }
  
  uptr<GenericFunction<>> _func;
  State _state = State::kDeleting;
  WaitQueueElement _wq_ele;
  ThreadCtrl * const _ctrl;
  IntQueue<Thread>::Container _container;
  Thread *_waiting_thread = nullptr;

  //
  // stack switching implementation
  //

  // switch to this thread
  int SwitchTo();
  void AllocReturnBuf() {
    assert(__sync_lock_test_and_set(&_using_return_buf, 1) == 0);
  }
  void ReleaseReturnBuf() {
    assert(_using_return_buf == 1);
    _using_return_buf = 0;
  }
  static void InitBufferSub(Thread *t) {
    t->InitBuffer();
  }
  void InitBuffer();
  void Release();
  StackState _stack_state;
  virt_addr _stack;
  jmp_buf _buf;
  jmp_buf _return_buf;
  int _using_return_buf = 0;
};

class ThreadCtrl {
public:
  static void Init();
  static ThreadCtrl &GetCurrentCtrl() {
    return GetCtrl(cpu_ctrl->GetCpuId());
  }
  static ThreadCtrl &GetCtrl(CpuId id) {
    return _thread_ctrl[id.GetRawId()];
  }
  void Run();
  enum class QueueState {
    kNotStarted,
    kRunning,
    kSleeping,
  };
  QueueState GetState() {
    return _state;
  }
  uptr<Thread> AllocNewThread(Thread::StackState sstate);
  static Thread::Operator GetCurrentThreadOperator() {
    Thread *thread = GetCtrl(cpu_ctrl->GetCpuId())._current_thread;
    kassert(thread != nullptr);
    return thread->CreateOperator();
  }
  static void WaitCurrentThread() {
    Thread *thread = GetCtrl(cpu_ctrl->GetCpuId())._current_thread;
    kassert(thread != nullptr);
    thread->Wait();
  }
  static bool IsInitialized() {
    return _is_initialized;
  }
private:
  friend class Thread;
  //
  // functions for Thread
  //
  bool ScheduleRunQueue(Thread *thread);
  bool ScheduleWaitQueue(Thread *thread, int us);
  void PushToIdle(Thread *thread) {
    thread->Release();
    _idle_threads.Push(thread);
  }
  static Thread *GetCurrentThread() {
    return GetCtrl(cpu_ctrl->GetCpuId())._current_thread;
  }
  
  ThreadCtrl() {
  }
  ~ThreadCtrl();
  void InitSub();

  static ThreadCtrl *_thread_ctrl;
  static bool _is_initialized;

  static const int kMaxThreadNum = 100;
  static const int kExecutionInterval = 1000; // us
  
  Thread **_threads;
  QueueState _state = QueueState::kNotStarted;
  IntQueue<Thread> _run_queue;
  OrderedQueue<Thread::WaitQueueElement, Time> _wait_queue;
  class IdleThreads {
  public:
    void Init() {
      _buf = new RingBuffer2<Thread *, kMaxThreadNum>;
    }
    void Push(Thread *t);
    void Pop(Thread *&t);
  private:
    RingBuffer2<Thread *, kMaxThreadNum> *_buf;
  } _idle_threads;
  Thread *_current_thread = nullptr;
};

inline Thread::Operator Thread::CreateOperator() {
  Thread::Operator op(_op_obj);
  return op;
}

inline void Thread::Schedule() {
  kassert(!_func.IsNull());
  if (_ctrl->ScheduleRunQueue(this)) {
    __sync_fetch_and_add(&_op_obj._cnt, 1);
  }
}

inline void Thread::Schedule(int us) {
  kassert(!_func.IsNull());
  bool queued;
  if (us == 0) {
    queued = _ctrl->ScheduleRunQueue(this);
  } else {
    queued = _ctrl->ScheduleWaitQueue(this, us);
  }
  if (queued) {
    __sync_fetch_and_add(&_op_obj._cnt, 1);
  }
}

inline Thread::Operator::Operator(const Thread::Operator &op) : _obj(op._obj) {
  __sync_fetch_and_add(&_obj._cnt, 1);
}

inline Thread::Operator::~Operator() {
  __sync_fetch_and_sub(&_obj._cnt, 1);
}

inline Thread::Operator::Operator(Thread::OperatorObj &obj) : _obj(obj) {
  __sync_fetch_and_add(&_obj._cnt, 1);
}

inline void Thread::Operator::Schedule() { 
  auto that = _obj._thread;
  that->Schedule();
}

inline void Thread::Operator::Schedule(int us) {
  auto that = _obj._thread;
  that->Schedule(us);
}

inline void Thread::Operator::SetFunc(uptr<GenericFunction<>> func) {
  _obj._thread->_func = func;
}
