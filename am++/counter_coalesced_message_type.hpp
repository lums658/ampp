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

#ifndef AMPLUSPLUS_COUNTER_COALESCED_MESSAGE_TYPE_HPP
#define AMPLUSPLUS_COUNTER_COALESCED_MESSAGE_TYPE_HPP

#include <am++/traits.hpp>
#include <cassert>
#include <utility>
#include <vector>
#include <memory>
#include <boost/thread.hpp>
#include <boost/noncopyable.hpp>
#include <am++/detail/typed_in_place_factory_owning.hpp>
#include <am++/performance_counters.hpp>
#include <stdint.h>
#include <am++/detail/signal.hpp>
#include <am++/detail/buffer_cache.hpp>
#include <am++/detail/thread_support.hpp>
#include <am++/detail/factory_wrapper.hpp>
#include <am++/dummy_buffer_sorter.hpp>

// This code needs to match promela/counter_coalesced_message_type2.pr

namespace amplusplus {

template <typename CoalescingMsg>
class default_coalescing_heuristic;

template <typename CoalescingMsg>
class relative_velocity_heuristic;

class default_coalescing_heuristic_gen {
public:
  template <typename CoalescingMsg>
  using Heuristic = default_coalescing_heuristic<CoalescingMsg>;
};

template <typename CoalescingMsg>
class default_coalescing_heuristic {
public:
  explicit default_coalescing_heuristic(const default_coalescing_heuristic_gen& dh_gen) {}
  bool execute () {}
};

class  relative_velocity_heuristic_gen {
public:
  int msg_count_thres;
  explicit relative_velocity_heuristic_gen (int msg_count_thres = 20)
    : msg_count_thres(msg_count_thres) {}
  //relative_velocity_heuristic_gen (relative_velocity_heuristic_gen& o) :
  //msg_count_thres(o.msg_count_thres){}

    template <typename CoalescingMsg>
    using Heuristic = relative_velocity_heuristic<CoalescingMsg>;
};

  template <typename Arg, typename Handler, typename BufferSorter = amplusplus::DummyBufferSorter<Arg>, typename CoalescingHeuristicGen = amplusplus::default_coalescing_heuristic_gen>
class counter_coalesced_message_type;

/*Heuristic based on relative velocity. If the velocity decreases from previous run, we would want to flush without waiting.*/
template <typename CoalescingMsg>
class  relative_velocity_heuristic {
  // int msg_count_thres = 20;
  double start, stop;
  double velocity;
  bool enable_flush;
  relative_velocity_heuristic_gen rh_gen;
public:
  relative_velocity_heuristic(const relative_velocity_heuristic_gen& rh_gen):
    rh_gen(rh_gen) {
    start = amplusplus::get_time();
    stop = 0.0;
    velocity = 1.0;
    enable_flush = false;
    std::cout << "threshold " << rh_gen.msg_count_thres <<std::endl;
  }

  bool execute() {
    // std::cout << "In relative vel heuristic execute" << std::endl;
    static_cast<CoalescingMsg*>(this)->message_cnt.fetch_add(1); // TODO: move it to coalescing_message_type
    enable_flush = false;
    if (static_cast<CoalescingMsg*>(this)->message_cnt.load()
	== rh_gen.msg_count_thres) {
      stop = amplusplus::get_time();
      double vel = (static_cast<CoalescingMsg*>(this)->message_cnt.load()) / (stop - start) ;
      // printf("Current velocity %f\n", vel);
      if (velocity > vel) {
	enable_flush = true;
      }
      velocity = vel;
      start =  amplusplus::get_time();
      stop = 0.0;
      static_cast<CoalescingMsg*>(this)->message_cnt.store(0);
    }
    return enable_flush;
  }
};


template <typename CoalescingHeuristicGen = default_coalescing_heuristic_gen>
// TODO: What would be needed to provide  a default template parameter?
struct counter_coalesced_message_type_gen {
  typedef transport::rank_type rank_type;
  size_t coalescing_size;
  unsigned int priority;
  CoalescingHeuristicGen ch_gen; //TODO: spell out full name

  public:
  template <typename Arg, typename Handler, typename BufferSorter = amplusplus::DummyBufferSorter<Arg> >
  struct inner { // TODO: convert to c++11 'using' convention
    typedef counter_coalesced_message_type<Arg, Handler, BufferSorter, CoalescingHeuristicGen> type; // TODO: rename type to counter_coalesced_msg_type
  };

  explicit counter_coalesced_message_type_gen(
			      size_t coalescing_size, int p=0,
			      const CoalescingHeuristicGen& ch_gen
			      = CoalescingHeuristicGen{})
    : coalescing_size(coalescing_size), priority(p),
      ch_gen(ch_gen) {}
};



// Thread-safe functions:
//   Handler calls (from progress on underlying transport)
//   send
//   flush (both versions)
  template <typename Arg, typename Handler, typename BufferSorter, typename CoalescingHeuristicGen>
  class counter_coalesced_message_type :
    CoalescingHeuristicGen::template Heuristic<counter_coalesced_message_type<Arg, Handler, BufferSorter, CoalescingHeuristicGen>> {
  private:
  struct sp_deleter {std::shared_ptr<void> p; sp_deleter(const std::shared_ptr<void>& p): p(p) {} void operator()() {p.reset();}};

  struct message_buffer {
    amplusplus::detail::atomic<unsigned int> count_allocated, count_written;
    static const unsigned int sender_active = UINT_MAX - UINT_MAX / 2;
    static const unsigned int count_mask = sender_active - 1;
    unsigned int max_count;
    amplusplus::detail::atomic<bool> registered_with_td;
    std::shared_ptr<void> data_owner;
    Arg* data;
    struct {
      struct size_test {amplusplus::detail::atomic<unsigned int> a, b; unsigned int c; amplusplus::detail::atomic<bool> c2; std::shared_ptr<void> d; Arg* e;};
      char padding[128 - sizeof(size_test)];
    } false_sharing_padding;

    explicit message_buffer(size_t max_count)
        : count_allocated(0),
          count_written(0),
          max_count(max_count),
          registered_with_td(false),
          data_owner(),
          data(0) {assert (max_count != 0);}

    message_buffer()
      : count_allocated(0), count_written(0), max_count(0), registered_with_td(false), data_owner(), data(0)
    {}

    message_buffer(const message_buffer& mb)
      : count_allocated(0),
        count_written(0),
        max_count(mb.max_count),
        registered_with_td(false),
        data_owner(mb.data_owner),
        data(mb.data)
    {
      // Only empty buffers should be copied
      assert (mb.count_allocated.load() == 0);
      assert (mb.count_written.load() == 0);
      assert (mb.registered_with_td.load() == false);
    }

    message_buffer& operator=(const message_buffer& mb) {
      // Only empty buffers should be copied, and only onto empty buffers
      assert (mb.count_allocated.load() == 0);
      assert (mb.count_written.load() == 0);
      assert (mb.registered_with_td.load() == false);
      assert (count_allocated.load() == 0);
      assert (count_written.load() == 0);
      assert (registered_with_td.load() == false);
      max_count = mb.max_count;
      data = mb.data;
      data_owner = mb.data_owner;
      return *this;
    }

    ~message_buffer() {
      assert (count_allocated.load() == 0);
      assert (count_written.load() == 0);
      assert (registered_with_td.load() == false);
    }

    bool empty() const {
      return count_allocated.load() == 0;
    }

    void clear(const std::shared_ptr<void>& new_data_owner) {
      data_owner = new_data_owner;
      data = static_cast<Arg*>(data_owner.get());
      registered_with_td.store(false);
      count_written.store(0u);
      count_allocated.store(0u); // This must be last since it releases other threads that may be waiting to add things to the buffer, and it must be at least a release to pick up the other writes
    }
  };

  public:
  struct traits {
    typedef Arg arg_type;
    typedef Handler handler_type;
  };
  typedef typename traits::arg_type arg_type;
  typedef typename traits::handler_type handler_type;

  counter_coalesced_message_type(counter_coalesced_message_type_gen<CoalescingHeuristicGen> gen,
                                 transport trans,
                                 valid_rank_set possible_dests_ = valid_rank_set(),
                                 valid_rank_set possible_sources_ = valid_rank_set(),
                                 const BufferSorter& buf_sorter = DummyBufferSorter<Arg>())
    : trans(trans),
      mt(trans.create_message_type<Arg>(gen.priority)),
      buf_cache(new detail::buffer_cache(trans, gen.coalescing_size * sizeof(Arg))),
      handler(handler_type()),
      outgoing_buffers(trans.size()),
      last_active(trans.size()),
      coalescing_size(gen.coalescing_size),
      buffer_sorter(buf_sorter), base_type(gen.ch_gen),
      alive(std::make_shared<bool>(true))
  {
    assert (coalescing_size != 0);
    message_cnt.store(0);
    if (!possible_dests_) possible_dests_ = std::make_shared<detail::all_ranks>(trans.size());
    if (!possible_sources_) possible_sources_ = std::make_shared<detail::all_ranks>(trans.size());
    this->mt.set_max_count(gen.coalescing_size);
    this->mt.set_handler(raw_message_handler(*this));
    this->mt.set_possible_dests(possible_dests_);
    this->mt.set_possible_sources(possible_sources_);
    typedef transport::rank_type rank_type;
    trans.add_flush_object(boost::bind(&counter_coalesced_message_type::flush, this, alive));
    for (size_t i = 0; i < possible_dests_->count(); ++i) {
      rank_type r = possible_dests_->rank_from_index(i);
      assert (r < trans.size());
      outgoing_buffers[r] = message_buffer(gen.coalescing_size);
      outgoing_buffers[r].clear(buf_cache->allocate());
      last_active[r] = 0;
    }
  }

#ifndef BOOST_NO_DEFAULTED_FUNCTIONS
  counter_coalesced_message_type(counter_coalesced_message_type&& o)
    : trans(o.trans), mt(std::move(o.mt)), buf_cache(),
      handler(std::move(o.handler)), outgoing_buffers(std::move(o.outgoing_buffers)),
      last_active(std::move(o.last_active)),
      coalescing_size(o.coalescing_size), buffer_sorter(o.get_buffer_sorter()) {
    buf_cache.swap(o.buf_cache);
  }
#endif

  ~counter_coalesced_message_type() {
    // These must be done in this order
    *alive = false;
    this->outgoing_buffers.clear();
    this->buf_cache.reset();
  }

  private:
  bool send_buffer(message_buffer& buf, unsigned int my_id, transport::rank_type dest) {
    // fprintf(stderr, "%zu: send_buffer %08x to %zu, current state is %08x\n", trans.rank(), my_id, dest, buf.count_allocated.load());
    assert (buf.count_allocated.load() & message_buffer::sender_active);
    const unsigned int count = my_id & message_buffer::count_mask;
    if ((my_id & message_buffer::sender_active) != 0) return false;
    assert (!!buf.data_owner);
    assert (count <= buf.max_count);
    if (count != 0) {
      while (buf.count_written.load() != count) {amplusplus::detail::do_pause();} // Make sure all elements are written, must be an acquire so we can read data in the buffer
      assert (buf.registered_with_td.load() == true); // This may not be true before the previous line's while loop is done
      Arg* send_data = buf.data;
      std::shared_ptr<void> send_data_owner = buf.data_owner;
      buf.clear(buf_cache->allocate());
      // fprintf(stderr, "%zu: actual send %u to %zu, current state is %08x\n", trans.rank(), count, dest, buf.count_allocated.load());
      this->mt.send(send_data, count, dest, sp_deleter(send_data_owner));
      return true;
    }
    return false;
  }


  struct raw_message_handler {
    counter_coalesced_message_type& mt;
    raw_message_handler(counter_coalesced_message_type& mt): mt(mt) {}
    void operator()(transport::rank_type src, Arg* buf, size_t count) const {
      amplusplus::performance_counters::hook_message_received(src, count, sizeof(Arg));
      handler_type& h = mt.handler;
      BufferSorter buf_sorter = mt.get_buffer_sorter();
      buf_sorter.sort(buf, buf + count);

      for (size_t i = 0; i < count; ++i) {
        h(src, buf[i]);
      }
    }
  };


  public:
  void set_handler(const handler_type& handler_) {
    handler = handler_;
  }

#ifndef BOOST_NO_RVALUE_REFERENCES
  void set_handler(handler_type&& handler_) {handler = std::move(handler_);}
#endif

  handler_type& get_handler() {
    return handler;
  }

  BufferSorter& get_buffer_sorter() {
    return buffer_sorter;
  }


  void send(const arg_type& arg, transport::rank_type dest) {
    assert (trans.is_valid_rank(dest));
    message_buffer& buf = outgoing_buffers[dest];
    const unsigned int max_count = buf.max_count;
    while (true) {
      while (true) {
        assert (max_count > 0);
        unsigned int x = buf.count_allocated.load();
        // fprintf(stderr, "send_spin %08x of %x\n", x, max_count);
        if ((x & message_buffer::count_mask) < max_count && (x & message_buffer::sender_active) == 0) {
          break;
        }
        amplusplus::detail::do_pause();
      }
      unsigned int my_id = buf.count_allocated.fetch_add(1);
      // fprintf(stderr, "send %08x\n", my_id);
      if (my_id & message_buffer::sender_active) continue;
      if ((my_id & message_buffer::count_mask) >= max_count) continue;
      assert ((my_id & message_buffer::count_mask) < max_count);
      assert (buf.data_owner);
      assert (buf.data);
      buf.data[my_id & message_buffer::count_mask] = arg;
      if ((my_id & message_buffer::count_mask) == max_count - 1) {
        buf.count_allocated.store(message_buffer::sender_active);
        if (!buf.registered_with_td.exchange(true)) {
          this->mt.message_being_built(dest);
        }
        ++buf.count_written;
        amplusplus::performance_counters::hook_full_buffer_send(dest, max_count, sizeof(Arg));
        // this->mt.get_transport().flush();
	send_buffer(buf, max_count, dest);
      } else if ((my_id & message_buffer::count_mask) == 0) {
        if (!buf.registered_with_td.exchange(true)) {
          this->mt.message_being_built(dest);
        }
        ++buf.count_written;
      } else {
        ++buf.count_written;
      }
      bool enable_flush = base_type::execute();
      if (enable_flush) {
      	flush(alive);
      }
      break;
    }
  }

  void send_with_tid(const arg_type& arg, transport::rank_type dest, int /*tid*/) {
    this->send(arg, dest);
  }

  void message_being_built(transport::rank_type dest) {
    assert (trans.is_valid_rank(dest));
    message_buffer& buf = this->outgoing_buffers[dest];
    bool was_registered = buf.registered_with_td.exchange(true);
    if (!was_registered) {
      this->mt.message_being_built(dest);
    }
  }

  bool flush(std::shared_ptr<bool> alive) {
    assert (alive.get());
    if(!*alive) return false;
    // fprintf(stderr, "%zu: counter_coalesced_message_type::flush(%zu)\n", trans.rank(), dest);
    for (size_t i = 0; i < this->mt.get_possible_dests()->count(); ++i) {
      rank_type r = this->mt.get_possible_dests()->rank_from_index(i);
      assert (trans.is_valid_rank(r));
      message_buffer& buf = this->outgoing_buffers[r];
      const unsigned int max_count = buf.max_count;
      unsigned int my_id = buf.count_allocated.load();
      if(my_id != last_active[r].load()) {
	last_active[r].store(my_id);
	continue;
      }
      while (true) {
	if (my_id > 0 && my_id < max_count) {
	  if (buf.count_allocated.compare_exchange_weak(my_id, message_buffer::sender_active)) break;
	  amplusplus::detail::do_pause();
	} else {
	  break;
	}
      }
      if (my_id > 0 && my_id < max_count) {
	amplusplus::performance_counters::hook_flushed_message_size(r, my_id, sizeof(Arg));
	send_buffer(buf, my_id, r);
      }
    }
    return true;
  }

  transport get_transport() const {return trans;}

  private:
  transport trans;
  message_type<Arg> mt;
  std::unique_ptr<detail::buffer_cache> buf_cache;
  handler_type handler;
  std::vector<message_buffer> outgoing_buffers;
  std::vector<amplusplus::detail::atomic<unsigned int> > last_active;
  size_t coalescing_size;
  BufferSorter buffer_sorter;
  std::shared_ptr<bool> alive;
  amplusplus::detail::atomic<unsigned int>  message_cnt;
  typedef typename CoalescingHeuristicGen::template Heuristic<counter_coalesced_message_type> base_type;
  CoalescingHeuristicGen heuristic;
  friend base_type;
};

}

#endif // AMPLUSPLUS_COUNTER_COALESCED_MESSAGE_TYPE_HPP
