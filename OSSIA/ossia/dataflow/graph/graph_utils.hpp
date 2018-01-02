#pragma once
#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/dataflow.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/breadth_first_search.hpp>
#include <ossia/dataflow/graph/graph_ordering.hpp>
#include <ossia/editor/scenario/time_value.hpp>
#include <ossia/detail/ptr_set.hpp>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/transitive_closure.hpp>

class DataflowTest;

namespace ossia
{
using graph_t = boost::
    adjacency_list<boost::vecS, boost::vecS, boost::directedS, node_ptr, std::shared_ptr<graph_edge>>;

using graph_vertex_t = graph_t::vertex_descriptor;
using graph_edge_t = graph_t::edge_descriptor;

using node_map = ossia::shared_ptr_map<ossia::graph_node, graph_vertex_t>;
using edge_map = ossia::shared_ptr_map<ossia::graph_edge, graph_edge_t>;

using node_flat_set = boost::container::flat_set<graph_node*>;
enum class node_ordering
{
  topological, temporal, hierarchical
};

template<typename Graph_T, typename IO>
void print_graph(Graph_T& g, IO& stream)
{
  std::stringstream s;
  boost::write_graphviz(s, g, [&] (auto& out, auto v) {
    if(g[v] && !g[v]->label().empty())
      out << "[label=\"" << g[v]->label() << "\"]";
    else
      out << "[]";
  },
  [] (auto&&...) {});

  stream << s.str() << "\n";
}


struct OSSIA_EXPORT graph_util
{
  static void copy_from_local(const data_type& out, inlet& in)
  {
    if (out.which() == in.data.which() && out && in.data)
    {
      eggs::variants::apply(copy_data{}, out, in.data);
    }
  }

  static void copy(const delay_line_type& out, std::size_t pos, inlet& in)
  {
    if (out.which() == in.data.which() && out && in.data)
    {
      eggs::variants::apply(copy_data_pos{pos}, out, in.data);
    }
  }

  static void copy(const outlet& out, inlet& in)
  {
    copy_from_local(out.data, in);
  }

  static void copy_to_local(
      const data_type& out, const destination& d, execution_state& in)
  {
    in.insert(destination_t{&d.address()}, out);
  }

  static void copy_to_global(
      const data_type& out, const destination& d, execution_state& in)
  {
    // TODO
  }

  static void pull_from_parameter(inlet& in, execution_state& e)
  {
    apply_to_destination(in.address, e.allDevices, [&] (ossia::net::parameter_base* addr) {
      if (in.scope & port::scope_t::local)
      {
        e.find_and_copy(*addr, in);
      }
      else
      {
        e.copy_from_global(*addr, in);
      }
    });
  }

  static void init_node(graph_node& n, execution_state& e)
  {
    // Clear the outputs of the node
    for (const outlet_ptr& out : n.outputs())
    {
      if (out->data)
        eggs::variants::apply(clear_data{}, out->data);
    }

    // Copy from environment and previous ports to inputs
    for (const inlet_ptr& in : n.inputs())
    {
      if (!in->sources.empty())
      {
        for (auto edge : in->sources)
        {
          ossia::apply(init_node_visitor<graph_util>{*in, *edge, e}, edge->con);
        }
      }
      else
      {
        pull_from_parameter(*in, e);
      }
    }
  }

  static void teardown_node(const graph_node& n, execution_state& e)
  {
    for (const inlet_ptr& in : n.inputs())
    {
      if (in->data)
        eggs::variants::apply(clear_data{}, in->data);
    }

    // Copy from output ports to environment
    for (const outlet_ptr& out : n.outputs())
    {
      out->write(e);
      for (auto tgt : out->targets)
      {
        ossia::apply(env_writer{*out, *tgt, e}, tgt->con);
      }
    }
  }

  static void disable_strict_nodes(const node_flat_set& enabled_nodes, node_flat_set& ret)
  {
    for (graph_node* node : enabled_nodes)
    {
      for (const auto& in : node->inputs())
      {
        for (const auto& edge : in->sources)
        {
          assert(edge->out_node);

          if (immediate_strict_connection* sc = edge->con.target<immediate_strict_connection>())
          {
            if ((sc->required_sides
                 & immediate_strict_connection::required_sides_t::outbound)
                && node->enabled() && !edge->out_node->enabled())
            {
              ret.insert(node);
            }
          }
          else if (delayed_strict_connection* delay = edge->con.target<delayed_strict_connection>())
          {
            const auto n = ossia::apply(data_size{}, delay->buffer);
            if (n == 0 || delay->pos >= n)
              ret.insert(node);
          }
        }
      }

      for (const auto& out : node->outputs())
      {
        for (const auto& edge : out->targets)
        {
          assert(edge->in_node);

          if (auto sc = edge->con.target<immediate_strict_connection>())
          {
            if ((sc->required_sides
                 & immediate_strict_connection::required_sides_t::inbound)
                && node->enabled() && !edge->in_node->enabled())
            {
              ret.insert(node);
            }
          }
        }
      }
    }
  }

  static void disable_strict_nodes_rec(node_flat_set& cur_enabled_node, node_flat_set& disabled_cache)
  {
    do
    {
      disabled_cache.clear();
      disable_strict_nodes(cur_enabled_node, disabled_cache);
      for (ossia::graph_node* n : disabled_cache)
      {
        if(!n->requested_tokens.empty())
          n->set_prev_date(n->requested_tokens.back().date);
        n->disable();

        cur_enabled_node.erase(n);
      }

    } while (!disabled_cache.empty());
  }

  static void exec_node(graph_node& first_node,
                 execution_state& e)
  {
    init_node(first_node, e);
    if(first_node.start_discontinuous()) {
      first_node.requested_tokens.front().start_discontinuous = true;
      first_node.set_start_discontinuous(false);
    }
    if(first_node.end_discontinuous()) {
      first_node.requested_tokens.front().end_discontinuous = true;
      first_node.set_end_discontinuous(false);
    }

    for(const auto& request : first_node.requested_tokens) {
      first_node.run(request, e);
      first_node.set_prev_date(request.date);
    }

    first_node.set_executed(true);
    teardown_node(first_node, e);
  }

  // These methods are only accessed by ossia::graph
  static bool can_execute(graph_node& node, const execution_state&)
  {
    return ossia::all_of(node.inputs(), [](const auto& inlet) {
      // A port can execute if : - it has source ports and all its source ports
      // have executed

      // if there was a strict connection, this node would have been disabled
      // so we can just do the following check.
      bool previous_nodes_executed
          = ossia::all_of(inlet->sources, [&](graph_edge* edge) {
          return edge->out_node->executed()
          || (!edge->out_node->enabled() /* && bool(inlet->address) */
              /* TODO check that it's in scope */);
    });

      // it does not have source ports ; we have to check for variables :
      // find if address available in local / global scope
      return !inlet->sources.empty() ? previous_nodes_executed : true // TODO
                                       ;
    });
  }

  static void finish_nodes(const node_map& nodes)
  {
    for (auto& node : nodes)
    {
      ossia::graph_node& n = *node.first;
      n.set_executed(false);
      n.disable();
      for (const outlet_ptr& out : node.first->outputs())
      {
        if (out->data)
          eggs::variants::apply(clear_data{}, out->data);
      }
    }
  }

  static bool find_path(int source, int sink, graph_t& graph)
  {
    bool ok = false;
    struct bfs_time_visitor : boost::default_bfs_visitor
    {
      int node_to_find{};
      bool& ok;
      bool discover_vertex(int u, const graph_t&) const
      {
        if(u == node_to_find)
          ok = true;
        return ok;
      }
    } to_find{{}, sink, ok};

    ossia::bfs::breadth_first_search_simple(graph, source, to_find);
    return ok;
  }
};
}