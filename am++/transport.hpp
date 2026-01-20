// Copyright 2010-2013 The Trustees of Indiana University.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met: 

// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer. 
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution. 

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//  Authors: Jeremiah Willcock
//           Andrew Lumsdaine

#ifndef AMPLUSPLUS_TRANSPORT_HPP
#define AMPLUSPLUS_TRANSPORT_HPP

#include <memory>
#include <any>
#include <cassert>
#include <vector>
#include <functional>
#include <map>
#include <list>
#include <sstream>
#include <iostream>
#include <typeinfo>
#include <algorithm>
#include <utility>
#include <cstdio>
#include <am++/traits.hpp>
#include <am++/message_queue.hpp>
#include <am++/detail/signal.hpp>
#include <am++/detail/thread_support.hpp>
#include <am++/termination_detector.hpp>
#include <am++/detail/term_detect_level_manager.hpp>
#include <am++/detail/type_info_map.hpp>
// include of performance_counters.hpp below

namespace amplusplus {

double get_time();

class environment_base;
class transport;

class environment {
  std::shared_ptr<environment_base> env;

  public:
  explicit environment(std::shared_ptr<environment_base> env);
  environment(const environment& e);
  scheduler& get_scheduler() const;

  template <typename Subclass>
  std::shared_ptr<Subclass> downcast_to_impl() const {
    std::shared_ptr<Subclass> p = std::dynamic_pointer_cast<Subclass>(env);
    assert (p.get());
    return p;
  }

  transport create_transport();
};

class environment_base {
  public:
  environment_base(): sched(new scheduler) {}
  virtual ~environment_base() {}
  virtual transport create_transport(environment& e) = 0;
  scheduler& get_scheduler() const {return *sched;}

  protected:
  friend class environment;

  private:
  std::unique_ptr<scheduler> sched;
};

inline scheduler& environment::get_scheduler() const {
  assert (env);
  return env->get_scheduler();
}

class message_type_base;
template <class> class message_type;
class transport_base;

typedef size_t rank_type;

class transport_base {
  public:
  transport_base(): handler_calls_pending(0), handler_calls_pending_or_active(0) {}
  virtual ~transport_base() {}

  typedef amplusplus::rank_type rank_type;

  virtual const rank_type& rank() const = 0;
  virtual const rank_type& size() const = 0;
  virtual bool is_valid_rank(rank_type r) const = 0;

  virtual bool begin_epoch() = 0; // Returns true in first thread

  virtual void set_termination_detector(const termination_detector& td) = 0;
  virtual termination_detector get_termination_detector() const = 0;

  virtual void increase_activity_count(unsigned long long) = 0;
  virtual void decrease_activity_count(unsigned long long) = 0;

  virtual receive_only<termination_message> get_termination_queue() = 0; // Wraps to allow only receives

  virtual void set_nthreads(size_t n = 1) {assert (n == 1); (void)n;}
  virtual size_t get_nthreads() const {return 1;}

  virtual std::shared_ptr<void> alloc_memory(size_t nbytes) const = 0;

  void message_being_built(rank_type dest, int message_type) {this->get_termination_detector()->message_being_built(dest, message_type);}
  void add_flush_object(const std::function<bool ()>& f);
  virtual scheduler::task_result flush();

  private:
  virtual void setup_end_epoch() = 0;
  virtual void setup_end_epoch_with_value(uintmax_t val) = 0;
  virtual void finish_end_epoch() = 0;

  virtual message_type_base* create_message_type(const std::type_info& ti, size_t size, transport& trans) = 0;

  amplusplus::detail::mutex flush_lock;
  std::vector<std::function<bool ()> > flushes_needed;

  protected:
  amplusplus::detail::atomic<unsigned int> handler_calls_pending;
  amplusplus::detail::atomic<unsigned int> handler_calls_pending_or_active;

  protected:
  friend class transport;
  template <typename T> friend class message_type;
};

class valid_rank_set_base {
  public:
  virtual ~valid_rank_set_base() {}
  virtual bool is_valid(transport_base::rank_type r) const = 0;
  virtual transport_base::rank_type count() const = 0;
  virtual transport_base::rank_type rank_from_index(transport_base::rank_type idx) const = 0;
};
typedef std::shared_ptr<valid_rank_set_base> valid_rank_set;

}

// This uses names defined above
#include <am++/performance_counters.hpp>

namespace amplusplus {

// Do not override anything in this class; nothing is virtual
class transport {
  std::shared_ptr<transport_base> trans_base;
  environment& env;
  transport_base::rank_type rank_;
  transport_base::rank_type size_;
  size_t cached_nthreads;

  public:
  transport(environment& env): trans_base(), env(env) {}
  explicit transport(std::shared_ptr<transport_base> t, environment& env): trans_base(t), env(env), rank_(0), size_(0), cached_nthreads(1) {
    rank_ = t->rank();
    size_ = t->size();
  }

  typedef transport_base::rank_type rank_type;

  template <typename Subclass>
  std::shared_ptr<Subclass> downcast_to_impl() const {
    std::shared_ptr<Subclass> p = std::dynamic_pointer_cast<Subclass>(trans_base);
    assert (p.get());
    return p;
  }

  transport clone() {
    return env.create_transport();
  }

  scheduler& get_scheduler() const {
    return env.get_scheduler();
  }

  void begin_epoch() {
    assert (trans_base.get());
    trans_base->handler_calls_pending.store(0u);
    trans_base->handler_calls_pending_or_active.store(0u);
    bool true_in_one_thread = trans_base->begin_epoch();
    if (true_in_one_thread) amplusplus::performance_counters::hook_begin_epoch(*this);
    return;
  }

  const rank_type& rank() const {return rank_;}
  const rank_type& size() const {return size_;}
  bool is_valid_rank(rank_type r) const {assert (trans_base.get()); return trans_base->is_valid_rank(r);}

  private: struct dummy_function {void operator()() const {}};

  public:
  class end_epoch_request {
    std::shared_ptr<transport> trans;
    bool active;
    uintmax_t combined_val;
    std::shared_ptr<bool> alive;

    explicit end_epoch_request(transport trans, const std::shared_ptr<bool>& alive)
      : trans(std::make_shared<transport>(trans)), active(true), combined_val(0), alive(alive)
    {
      this->trans->get_termination_queue().
        receive(
          [this, alive](termination_message val) { this->mark_finished(alive, val); });
    }

    void mark_finished(std::shared_ptr<bool> alive, termination_message val) {
      assert (*alive);
      /*fprintf(stderr, "Terminating %p\n", this);*/
      combined_val = val.get_combined_value();
      active = false;
      *alive = false;
      if (val.is_last_thread()) amplusplus::performance_counters::hook_epoch_finished(*trans);
    }

    public:
    end_epoch_request(): trans(), active(false), combined_val(0), alive() {}
    ~end_epoch_request() {assert (!active);} // Also makes sure termination queue receive is done or was never started

#if 0
    end_epoch_request& operator=(const end_epoch_request& r) {
      assert (!this->active);
      trans = r.trans;
      active = r.active;
      combined_val = r.combined_val;
      if (active) {
        this->trans->get_termination_queue().
          receive(
            [this](termination_message val) { this->mark_finished(this->alive, val); });
      }
      return *this;
    }
#endif

    bool test() { // Returns true if done
      if (active) {
        trans->get_scheduler().run_one();
      } 
      return (!active);
    }

    end_epoch_request& wait() {
      while (!test()) {}
      return *this;
    }

    uintmax_t get_value() const {
      assert (!active);
      return combined_val;
    }

    friend class transport;
  };
  
  private:
  struct do_flush_all {
    std::weak_ptr<transport> trans;
    std::shared_ptr<bool> alive;
    do_flush_all(std::shared_ptr<transport> sp, std::shared_ptr<bool> alive): trans(sp), alive(alive) {}
    scheduler::task_result operator()(scheduler&) const {
      assert (alive.get());
      if (!*alive) return scheduler::tr_remove_from_queue;
      std::shared_ptr<transport> trans_sp = trans.lock();
      if (!trans_sp) return scheduler::tr_remove_from_queue;
      if (!trans_sp->idle()) return scheduler::tr_idle;
      return trans_sp->flush();
    }
  };

  public:
  end_epoch_request i_end_epoch() {
    // fprintf(stderr, "i_end_epoch\n");
    assert (trans_base.get());
    this->flush();
    trans_base->setup_end_epoch();
    std::shared_ptr<bool> alive(std::make_shared<bool>(true));
    end_epoch_request req(*this, alive);
    this->get_scheduler().add_idle_task(do_flush_all(req.trans, alive));
    return req;
  }

  end_epoch_request i_end_epoch_with_value(uintmax_t val) {
    // fprintf(stderr, "i_end_epoch_with_value\n");
    assert (trans_base.get());
    this->flush();
    trans_base->setup_end_epoch_with_value(val);
    std::shared_ptr<bool> alive(std::make_shared<bool>(true));
    end_epoch_request req(*this, alive);
    this->get_scheduler().add_idle_task(do_flush_all(req.trans, alive));
    return req;
  }

  void end_epoch() {this->i_end_epoch().wait();}
  uintmax_t end_epoch_with_value(uintmax_t val) {return this->i_end_epoch_with_value(val).wait().get_value();}

  void increase_activity_count(unsigned long long v) {assert (trans_base.get()); trans_base->increase_activity_count(v);}
  void decrease_activity_count(unsigned long long v) {assert (trans_base.get()); trans_base->decrease_activity_count(v);}

  bool operator==(const transport&) const;
  bool operator!=(const transport&) const;

  void set_termination_detector(const termination_detector& td) {assert (trans_base.get()); trans_base->set_termination_detector(td);}
  termination_detector get_termination_detector() const {assert (trans_base.get()); return trans_base->get_termination_detector();}

  void set_nthreads(size_t n = 1) {assert (trans_base.get()); trans_base->set_nthreads(n); cached_nthreads = n;}
  size_t get_nthreads() const {return cached_nthreads;}

  std::shared_ptr<void> alloc_memory(size_t nbytes) const {assert (trans_base.get()); return trans_base->alloc_memory(nbytes);}

  template <typename T>
  message_type<T> create_message_type(int priority = 0);

  void message_being_built(rank_type dest, int message_type) {assert (trans_base.get()); trans_base->message_being_built(dest, message_type);}

  void add_flush_object(const std::function<bool ()>& f) const {assert (trans_base.get()); trans_base->add_flush_object(f);}
  scheduler::task_result flush() {assert (trans_base.get()); return trans_base->flush();}

  receive_only<termination_message> get_termination_queue() {
    assert (trans_base.get());
    return trans_base->get_termination_queue();
  }

  bool idle() const {assert(trans_base.get()); return trans_base->handler_calls_pending_or_active.load() == 0 && trans_base->get_termination_detector()->really_ending_epoch();}

  // This function is only an approximation and may return incorrect results (due to relaxed memory order).
#ifdef AMPLUSPLUS_BUILTIN_ATOMICS
  bool handlers_pending() const { assert(trans_base.get()); return trans_base->handler_calls_pending.load(std::memory_order_relaxed); }
#endif // AMPLUSPLUS_BUILTIN_ATOMICS

  template <typename T> friend class message_type;
};

class message_type_base {
public:
  message_type_base(const transport& trans): trans(trans) {}
  virtual ~message_type_base() {}

  transport get_transport() const {return trans;}

  virtual void set_max_count(size_t max_count) = 0;
  virtual size_t get_max_count() const = 0;

  virtual void set_possible_sources(valid_rank_set p) = 0;
  virtual valid_rank_set get_possible_sources() const = 0;
  virtual void set_possible_dests(valid_rank_set p) = 0;
  virtual valid_rank_set get_possible_dests() const = 0;

  virtual void message_being_built(transport::rank_type dest) = 0;
  virtual void handler_done(transport::rank_type src) = 0;
  virtual void send_untyped(const void* buf, size_t count, transport::rank_type dest, std::function<void ()> buf_deleter) = 0;

  typedef std::function<void (transport::rank_type src, std::shared_ptr<const void> data, size_t count)> handler_type;

protected:
  virtual void set_handler_internal(handler_type h) = 0;

  template<typename T> friend class message_type;
  
private:
  transport trans;
};

template <typename T>
class message_type {
  std::shared_ptr<message_type_base> mt;
  scheduler& sched;
  int msgPriority;	

  template <typename Handler, int priority>
  struct wrapper_handler_gen {
    struct wrapper_handler {
      const Handler& h;
      transport trans;
      std::shared_ptr<message_type_base> mt;
      transport::rank_type src;
      std::shared_ptr<const void> buf;
      size_t count;

      wrapper_handler(const Handler& h, const transport& trans, const std::shared_ptr<message_type_base>& mt, transport::rank_type src, const std::shared_ptr<const void>& buf, size_t count): h(AMPLUSPLUS_MOVE(h)), trans(trans), mt(mt), src(src), buf(buf), count(count) {}
      scheduler::task_result operator()(scheduler& sched) const {
        if (!sched.should_run_handlers()) return scheduler::tr_idle;
        --trans.trans_base->handler_calls_pending;
        h(src, (T*)buf.get(), count);
        assert (mt);
        mt->handler_done(src);
        --trans.trans_base->handler_calls_pending_or_active;
        return scheduler::tr_busy_and_finished;
      }
    };
    const Handler h;
    transport trans;
    std::weak_ptr<message_type_base> mt;
    wrapper_handler_gen(const Handler& h, const transport& trans, std::shared_ptr<message_type_base> mt): h(h), trans(trans),  mt(AMPLUSPLUS_MOVE(mt)) {}
    void operator()(transport::rank_type src, const std::shared_ptr<const void>& buf, size_t count) {
      std::shared_ptr<message_type_base> mt_ = mt.lock();
      if (!mt_) return; // Message type has been deleted
      trans.env.get_scheduler().template add_runnable<wrapper_handler, priority>(wrapper_handler(h, trans, mt_, src, buf, count));
    }
  };

  public:
  explicit message_type(std::shared_ptr<message_type_base> mt, scheduler& sched, int priority =0): mt(mt), sched(sched), msgPriority(priority) {}
  message_type(const message_type& m, int priority = 0): mt(m.mt), sched(m.sched),  msgPriority(priority) {}
  message_type(message_type&& m): mt(std::move(m.mt)), sched(m.sched) {}

  typedef T arg_type;
  typedef typename message_type_base::handler_type handler_type;

  message_type* operator->() {return this;}
  const message_type* operator->() const {return this;}
  message_type& get() {return *this;}
  const message_type& get() const {return *this;}

  transport get_transport() const {assert (mt.get()); return mt->get_transport();}

  template <typename H>
  void set_handler(const H& h) {
    assert (mt.get());
    if(msgPriority == 0)
      mt->set_handler_internal(wrapper_handler_gen<H, 0>(h, mt->get_transport(), mt));
    else
      mt->set_handler_internal(wrapper_handler_gen<H, 1>(h, mt->get_transport(), mt));
  }

  scheduler::task_result flush() {
    // std::cerr << "message_type::flush_all() " << this << std::endl;
    // return mt->get_transport().flush();
    return scheduler::tr_idle;
  }

  void message_being_built(transport::rank_type dest) {
    assert (mt.get()); 
    assert (dest < mt->get_transport().size());
    mt->message_being_built(dest);
  }

  void send(const T* buf, size_t count, transport::rank_type dest, const std::function<void ()>& buf_deleter) {
    assert (mt.get()); 
    assert (dest < mt->get_transport().size());
    mt->send_untyped((const void*)buf, count, dest, buf_deleter);
  }

  void set_max_count(size_t max_count) {assert (mt.get()); mt->set_max_count(max_count);}
  size_t get_max_count() const {assert (mt.get()); return mt->get_max_count();}

  void set_possible_sources(valid_rank_set p) {assert (mt.get()); mt->set_possible_sources(p);}
  valid_rank_set get_possible_sources() const {assert (mt.get()); return mt->get_possible_sources();}
  void set_possible_dests(valid_rank_set p) {assert (mt.get()); mt->set_possible_dests(p);}
  valid_rank_set get_possible_dests() const {assert (mt.get()); return mt->get_possible_dests();}
};

namespace detail {

class all_ranks: public valid_rank_set_base {
  transport_base::rank_type size;

  public:
  all_ranks(transport_base::rank_type size): size(size) {}
  bool is_valid(transport_base::rank_type r) const {return r < size;}
  transport_base::rank_type count() const {return size;}
  transport_base::rank_type rank_from_index(transport_base::rank_type idx) const {return idx;}
};

} // end detail

template <typename T>
message_type<T> transport::create_message_type(int priority) {
  assert (trans_base.get()); 
  message_type<T> msg(
           std::shared_ptr<message_type_base> (
           trans_base->create_message_type(detail::get_type_info<T>(), sizeof(T), *this)),
           this->get_scheduler(), priority);
  msg.set_possible_sources(std::make_shared<detail::all_ranks>(this->size()));
  msg.set_possible_dests(msg.get_possible_sources());
  return msg;
}

} // end amplusplus

#endif // AMPLUSPLUS_TRANSPORT_HPP
