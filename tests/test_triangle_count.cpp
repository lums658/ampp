// Copyright 2024 The Trustees of Indiana University.
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
//  Authors: Andrew Lumsdaine

// Distributed Triangle Counting using AM++
//
// Based on: "Distributed, Shared-Memory Parallel Triangle Counting"
// Kanewala, Zalewski, Lumsdaine - PASC 2018
//
// This implements the PSP (Predecessor, Successor's Predecessor) algorithm
// with degree-based neighbor partitioning for reduced communication.

#include <config.h>

// Token pasting macros (replacement for BOOST_JOIN)
#define AMPP_JOIN2(a, b) a##b
#define AMPP_JOIN(a, b) AMPP_JOIN2(a, b)

#define IS_MPI_TRANSPORT_mpi 1
#define IS_MPI_TRANSPORT_gasnet 0
#define IS_MPI_TRANSPORT_shm 0
#define IS_MPI_TRANSPORT AMPP_JOIN(IS_MPI_TRANSPORT_, TRANSPORT)
#define IS_SHM_TRANSPORT_mpi 0
#define IS_SHM_TRANSPORT_gasnet 0
#define IS_SHM_TRANSPORT_shm 1
#define IS_SHM_TRANSPORT AMPP_JOIN(IS_SHM_TRANSPORT_, TRANSPORT)

#if IS_SHM_TRANSPORT
#include <omp.h>
#endif

#include "am++/am++.hpp"
#define TRANSPORT_HEADER <am++/AMPP_JOIN(TRANSPORT, _transport).hpp>
#include TRANSPORT_HEADER
#include "am++/basic_coalesced_message_type.hpp"
#include "am++/reductions.hpp"
#include "am++/message_type_generators.hpp"
#if IS_MPI_TRANSPORT
#include <am++/mpi_sinha_kale_ramkumar_termination_detector.hpp>
#include <mpi.h>
#include <am++/make_mpi_datatype.hpp>
#endif

#include "rmat_graph_generator_faster.hpp"
#include <boost/graph/compressed_sparse_row_graph.hpp>
#include <boost/graph/iteration_macros.hpp>

#include <cstdio>
#include <string>
#include <algorithm>
#include <memory>
#include <mutex>
#include <barrier>
#include <cassert>
#include <utility>
#include <functional>
#include <iostream>
#include <sstream>
#include <random>
#include <vector>
#include <atomic>
#include <set>

typedef amplusplus::transport::rank_type rank_type;

// Graph type - undirected for triangle counting
typedef boost::compressed_sparse_row_graph<boost::directedS, boost::no_property,
        boost::no_property, boost::no_property, uint32_t, uint32_t> GraphT;
typedef boost::graph_traits<GraphT>::vertex_descriptor Vertex;

//=============================================================================
// Message Types
//=============================================================================

// Message for wedge checking
// For vertex j with i < j < k, send (i, k) to owner of k to check if edge (i, k) exists
struct wedge_message {
  Vertex i;  // Lower vertex (i < j)
  Vertex k;  // Higher vertex (k > j), message is sent to owner of k

  wedge_message() : i(0), k(0) {}
  wedge_message(Vertex i_, Vertex k_) : i(i_), k(k_) {}
};

// Get the target vertex for routing
struct get_wedge_target {
  typedef Vertex result_type;
  Vertex operator()(const wedge_message& msg) const { return msg.k; }
};

inline std::ostream& operator<<(std::ostream& o, const wedge_message& w) {
  o << "(i=" << w.i << " k=" << w.k << ")";
  return o;
}

//=============================================================================
// Owner Map - determines which rank owns a vertex (1D cyclic distribution)
//=============================================================================

struct owner_map_type {
  size_t num_ranks;
  explicit owner_map_type(size_t ranks = 1) : num_ranks(ranks) {}

  // 1D cyclic distribution for better load balance
  rank_type operator()(Vertex v) const {
    return static_cast<rank_type>(v % num_ranks);
  }

  // Wedge messages are sent to owner of k
  friend rank_type get(const owner_map_type& o, const wedge_message& msg) {
    return o(msg.k);
  }
};

//=============================================================================
// MPI Datatype Registration
//=============================================================================

#if IS_MPI_TRANSPORT
namespace amplusplus {
  template <>
  struct make_mpi_datatype<wedge_message> : make_mpi_datatype_base {
    make_mpi_datatype<Vertex> dt_vertex;
    scoped_mpi_datatype dt;

    make_mpi_datatype() : dt_vertex() {
      int blocklengths[2] = {1, 1};
      MPI_Aint displacements[2];
      wedge_message test_object;
      MPI_Aint test_object_ptr;
      MPI_Get_address(&test_object, &test_object_ptr);
      MPI_Get_address(&test_object.i, &displacements[0]);
      MPI_Get_address(&test_object.k, &displacements[1]);
      displacements[0] -= test_object_ptr;
      displacements[1] -= test_object_ptr;
      MPI_Datatype types[2] = {dt_vertex.get(), dt_vertex.get()};
      MPI_Type_create_struct(2, blocklengths, displacements, types, dt.get_ptr());
      MPI_Type_commit(dt.get_ptr());
    }
    MPI_Datatype get() const { return dt; }
  };
}
#endif


//=============================================================================
// Wedge Handler - Checks for triangle completion
// For triangle (i, j, k) with i < j < k:
// - j sends wedge (i, k) to owner of k
// - k checks if i is in N_low(k), i.e., if edge (i, k) exists
//=============================================================================

struct wedge_handler {
  const std::vector<std::vector<Vertex>>* neighbors_low;  // N_low(v) = {u : u < v and (u,v) edge}
  std::atomic<unsigned long>* triangle_count;
  size_t num_ranks;

  wedge_handler()
    : neighbors_low(nullptr), triangle_count(nullptr), num_ranks(1) {}

  wedge_handler(const std::vector<std::vector<Vertex>>& n_low,
                std::atomic<unsigned long>& count,
                size_t ranks)
    : neighbors_low(&n_low), triangle_count(&count), num_ranks(ranks) {}

  void operator()(const wedge_message& msg) const {
    // Check if i is in N_low(k), meaning edge (i, k) exists with i < k
    Vertex local_k = msg.k / num_ranks;
    const auto& n_low_k = (*neighbors_low)[local_k];

    // Binary search since neighbors_low is sorted
    if (std::binary_search(n_low_k.begin(), n_low_k.end(), msg.i)) {
      triangle_count->fetch_add(1, std::memory_order_relaxed);
    }
  }
};

//=============================================================================
// Triangle Counting Algorithm
// Uses vertex ID ordering: for triangle (i, j, k) with i < j < k,
// count from vertex j by checking if edge (i, k) exists
//=============================================================================

template <typename AMTransport>
void count_triangles(const GraphT& graph, AMTransport& transport,
                     Vertex num_local_vertices, Vertex my_start,
                     Vertex graph_size) {
  rank_type rank = transport.rank();
  rank_type size = transport.size();

  owner_map_type owner(size);

  // Partition neighbors by vertex ID
  // N_low(v) = {u in N(v) : u < v}
  // N_high(v) = {u in N(v) : u > v}
  std::vector<std::vector<Vertex>> neighbors_low(num_local_vertices);
  std::vector<std::vector<Vertex>> neighbors_high(num_local_vertices);

  if (rank == 0) fprintf(stderr, "Partitioning neighbors by vertex ID...\n");

  // Build N_low and N_high from graph edges
  BGL_FORALL_VERTICES_T(v, graph, GraphT) {
    Vertex global_v = my_start + v * size;  // Cyclic: local v maps to global

    BGL_FORALL_ADJ_T(v, neighbor, graph, GraphT) {
      // neighbor is a global vertex ID
      if (neighbor < global_v) {
        neighbors_low[v].push_back(neighbor);
      } else if (neighbor > global_v) {
        neighbors_high[v].push_back(neighbor);
      }
      // Skip self-loops (neighbor == global_v)
    }
  }

  // Sort for binary search
  for (Vertex v = 0; v < num_local_vertices; ++v) {
    std::sort(neighbors_low[v].begin(), neighbors_low[v].end());
    std::sort(neighbors_high[v].begin(), neighbors_high[v].end());
  }

  // Count total edges in low/high sets
  size_t total_low = 0, total_high = 0;
  for (Vertex v = 0; v < num_local_vertices; ++v) {
    total_low += neighbors_low[v].size();
    total_high += neighbors_high[v].size();
  }

  size_t global_low = 0, global_high = 0;
  {
    amplusplus::scoped_epoch_value epoch(transport, total_low, global_low);
  }
  {
    amplusplus::scoped_epoch_value epoch(transport, total_high, global_high);
  }

  if (rank == 0) {
    fprintf(stderr, "Total N_low: %zu, N_high: %zu\n", global_low, global_high);
  }

  // Triangle counter (use unsigned long for scoped_epoch_value compatibility)
  std::atomic<unsigned long> local_triangles{0};

  //===========================================================================
  // Triangle Counting: For each vertex j, for each i in N_low(j), for each
  // k in N_high(j), check if edge (i, k) exists (i.e., i in N_low(k))
  // This counts triangle (i, j, k) with i < j < k exactly once from j.
  //===========================================================================
  if (rank == 0) fprintf(stderr, "Counting triangles...\n");

  double start_time = amplusplus::get_time();

  {
    typedef amplusplus::simple_generator<amplusplus::basic_coalesced_message_type_gen> Gen;
    Gen gen(amplusplus::basic_coalesced_message_type_gen(1 << 12));

    typename Gen::template call_result<wedge_message, wedge_handler,
                                        owner_map_type, amplusplus::no_reduction_t>::type
      wedge_msg(gen, transport, owner, amplusplus::no_reduction);

    wedge_msg.set_handler(wedge_handler(neighbors_low, local_triangles, size));

    {
      amplusplus::scoped_epoch epoch(transport);

      for (Vertex v = 0; v < num_local_vertices; ++v) {
        const auto& n_low = neighbors_low[v];
        const auto& n_high = neighbors_high[v];

        if (n_low.empty() || n_high.empty()) continue;

        // For each i < j and k > j, send wedge (i, k) to owner of k
        // Owner of k checks if i is in N_low(k)
        for (Vertex k : n_high) {
          for (Vertex i : n_low) {
            wedge_msg.send(wedge_message(i, k));
          }
        }
      }
    }
  }

  double end_time = amplusplus::get_time();

  //===========================================================================
  // Reduce triangle count
  //===========================================================================
  unsigned long local_tri_count = local_triangles.load();
  unsigned long global_triangles = 0;
  {
    amplusplus::scoped_epoch_value epoch(transport,
                                         local_tri_count,
                                         global_triangles);
  }

  if (rank == 0) {
    fprintf(stdout, "Triangle count: %lu\n", (unsigned long)global_triangles);
    fprintf(stdout, "Time: %.4f seconds on %zu ranks\n", end_time - start_time, (size_t)size);
    fprintf(stdout, "Rate: %.2f M triangles/sec\n",
            global_triangles / (end_time - start_time) / 1e6);
  }
}

//=============================================================================
// Main
//=============================================================================

void do_one_thread(amplusplus::environment& env) {
  amplusplus::transport trans = env.create_transport();
  rank_type rank = trans.rank();
  rank_type size = trans.size();

#if IS_MPI_TRANSPORT
  amplusplus::register_mpi_datatype<wedge_message>();
#endif

  // RMAT Graph500 parameters for scale-free graphs
  // a=0.57, b=0.19, c=0.19, d=0.05 produces power-law degree distribution
  const double rmat_a = 0.57;
  const double rmat_b = 0.19;
  const double rmat_c = 0.19;
  const double rmat_d = 0.05;

  // Graph parameters - RMAT requires power-of-2 vertex count
  const int SCALE = 18;
  const Vertex graph_size = (1u << SCALE);  // 2^SCALE vertices
  const size_t edge_factor = 16;  // Graph500 uses 16
  const size_t target_edges = edge_factor * graph_size;

  if (rank == 0) {
    fprintf(stderr, "=== Distributed Triangle Counting (RMAT) ===\n");
    fprintf(stderr, "Ranks: %zu, SCALE: %d\n", (size_t)size, SCALE);
    fprintf(stderr, "Vertices: %zu, Target edges: %zu\n",
            (size_t)graph_size, target_edges);
    fprintf(stderr, "RMAT params: a=%.2f, b=%.2f, c=%.2f, d=%.2f\n",
            rmat_a, rmat_b, rmat_c, rmat_d);
  }

  // Distribution for cyclic assignment
  struct cyclic_distrib {
    size_t num_ranks;
    cyclic_distrib() : num_ranks(1) {}
    explicit cyclic_distrib(size_t ranks) : num_ranks(ranks) {}
    rank_type operator()(Vertex v) const { return v % num_ranks; }
  };

  cyclic_distrib distrib(size);

  // Generate RMAT graph following Graph500 specification:
  // 1. Generate target_edges raw edges (with duplicates possible)
  // 2. Remove self-loops (u == v)
  // 3. Remove duplicate edges
  // 4. Make undirected (add both directions)
  // All ranks use the SAME seed to generate the SAME global graph
  std::mt19937 rng(42);  // Same seed on all ranks for consistent global graph

  if (rank == 0) fprintf(stderr, "Generating RMAT edges (Graph500 style)...\n");

  // Generate raw edges using basic rmat_iterator
  boost::rmat_iterator_faster<std::mt19937, GraphT>
    edges_b(rng, graph_size, target_edges, rmat_a, rmat_b, rmat_c, rmat_d,
            true),  // permute_vertices - randomize vertex IDs
    edges_e;

  // Collect edges, normalizing for undirected (min, max)
  std::set<std::pair<Vertex, Vertex>> edge_set;
  for (; edges_b != edges_e; ++edges_b) {
    std::pair<Vertex, Vertex> e = *edges_b;
    // Skip self-loops
    if (e.first == e.second) continue;
    // Normalize to (min, max) for undirected
    if (e.first > e.second) std::swap(e.first, e.second);
    edge_set.insert(e);
  }

  if (rank == 0) {
    fprintf(stderr, "After dedup: %zu unique undirected edges\n", edge_set.size());
  }

  // Expand to bidirectional and filter to local edges
  std::vector<std::pair<uint32_t, uint32_t>> edges;
  for (const auto& e : edge_set) {
    // Add both directions for undirected graph
    // Keep edge if source vertex belongs to this rank
    if (distrib(e.first) == rank) {
      edges.push_back(std::make_pair(e.first / size, e.second));
    }
    if (distrib(e.second) == rank) {
      edges.push_back(std::make_pair(e.second / size, e.first));
    }
  }

  // Count local vertices (those owned by this rank)
  Vertex num_local_vertices = (graph_size + size - 1) / size;
  if (rank == (rank_type)(size - 1)) {
    // Last rank may have fewer vertices
    num_local_vertices = graph_size - (size - 1) * ((graph_size + size - 1) / size);
  }

  // Sort edges for CSR construction
  std::sort(edges.begin(), edges.end());

  if (rank == 0) fprintf(stderr, "Building graph...\n");

  // Build CSR graph - source vertices are local indices
  GraphT my_graph(boost::edges_are_sorted, edges.begin(), edges.end(),
                  num_local_vertices);

  size_t local_edge_count = num_edges(my_graph);
  size_t total_edges = 0;
  {
    amplusplus::scoped_epoch_value epoch(trans, local_edge_count, total_edges);
  }

  if (rank == 0) {
    fprintf(stderr, "Graph built: %zu vertices, %zu edges total\n",
            (size_t)graph_size, total_edges);
  }

  // Run triangle counting
  Vertex my_start = rank;  // With cyclic, first vertex on rank r is r
  count_triangles(my_graph, trans, num_local_vertices, my_start, graph_size);
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;

#if IS_MPI_TRANSPORT
  amplusplus::environment env = amplusplus::mpi_environment(argc, argv);
  do_one_thread(env);
#elif IS_SHM_TRANSPORT
  std::unique_ptr<amplusplus::shm_environment_common> common;

#pragma omp parallel
  {
#pragma omp single
    {
      common.reset(new amplusplus::shm_environment_common(omp_get_num_threads()));
    }
    amplusplus::environment env = amplusplus::shm_environment(*common, omp_get_thread_num());
    do_one_thread(env);
  }
#endif
  return 0;
}
