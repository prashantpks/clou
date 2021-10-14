#pragma once

#include <string>
#include <cassert>
#include <unordered_set>
#include <sstream>
#include <set>

#include <llvm/IR/Instruction.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <z3++.h>

#include "util/z3.h"
#include "graph.h"
#include "util.h"
#include "uhb.h"
#include "noderef.h"
#include "progress.h"
#include "taint.h"

class Taint_Array;
class CFG_Expanded;

class AEG {
public:
    const CFG_Expanded& po; /*!<  The input CFG. The AEG constructs nodes in a 1:1 correspondence and heavily uses the preds/succs relations of this CFG. */
    UHBContext context; /*!<  The context for AEG construction. */

    using Node = UHBNode;
    using Edge = UHBEdge;
    using graph_type = Graph<NodeRef, Edge, std::hash<NodeRef>, Edge::Hash>;
    
    NodeRef entry; /*!< The unique entry node of the AEG (node has type Inst::Kind::ENTRY) */
    NodeRefSet exits; /*!< The set of exit nodes of the AEG (nodes have type Inst::Kind::EXIT) */
    
    graph_type graph;
    
    /** Construct the AEG.
     * \param AA alias analysis results to use
     * \param rob_size reorder buffer size (used to limit comx)
     */
    void construct(llvm::AliasAnalysis& AA, unsigned rob_size);
    
    const Node& lookup(NodeRef ref) const { return nodes.at(static_cast<unsigned>(ref)); }
    Node& lookup(NodeRef ref) { return nodes.at(static_cast<unsigned>(ref)); }
    
    explicit AEG(const CFG_Expanded& po): po(po), context(), constraints() {}
    
    void dump_graph(std::ostream& os) const;
    void dump_graph(const std::string& path) const;
    
    void simplify();
    
    void test();
    
    std::size_t size() const { return nodes.size(); } /*!< Get the number of nodes in the graph */
    
    template <typename Function> void for_each_edge(Function f) { graph.for_each_edge(f); }
    template <typename Function> void for_each_edge(Function f) const { graph.for_each_edge(f); }
    template <typename Function> void for_each_edge(Edge::Kind kind, Function f);
    template <typename Function> void for_each_edge(Edge::Kind kind, Function f) const;
    
    template <typename Function> void for_each_node(Inst::Kind kind, Function f);
    template <typename Function> void for_each_node(Inst::Kind kind, Function f) const;
    
    const UHBContext& ctx() const { return context; }
   
private:
    UHBConstraints constraints; /*!< global constraints (i.e. can't be attributed to one particular node or edge) */
    std::vector<Node> nodes;
    unsigned nedges = 0;
    
    unsigned num_specs() const;
    
    void construct_nodes();
    void construct_exec();
    void construct_arch();
    void construct_trans();
    void construct_po();
    void construct_tfo();
    void construct_addr_defs();
    void construct_addr_refs();
    void construct_aliases(llvm::AliasAnalysis& AA);
    void construct_com();
    void construct_arch_order();
    void construct_exec_order();
    void construct_trans_group();
    void construct_xsaccess_order(const NodeRefSet& xsaccesses);
    void construct_mem();
    void construct_comx();
    void construct_addr();
    void construct_data();
    void construct_ctrl();
    void construct_taint();
    
    
    // TODO: unify this, so that it just returns a NodeRefMap for in, out.
    using DependencyMap = NodeRefMap;
    DependencyMap dependencies;
    void construct_dependencies();
    
    using DominatorMap = NodeRefMap;
    DominatorMap construct_dominators_shared(Direction dir) const;
    DominatorMap dominators, postdominators;
    void construct_dominators() { dominators = construct_dominators_shared(Direction::OUT); }
    void construct_postdominators() { postdominators = construct_dominators_shared(Direction::IN); }
    
    struct LeakageClause {
        z3::expr pred;
        NodeRefSet nodes;
        std::unordered_set<Edge::Kind> edges;
        std::string name;
    };
    
    template <typename OutputIt>
    void leakage_rfx(OutputIt out) const;
    template <typename OutputIt>
    void leakage_cox(OutputIt out) const;
    template <typename OutputIt>
    void leakage_frx(OutputIt out) const;
    unsigned leakage(z3::solver& solver, unsigned max);
    
    struct Leakage_SpectreV4 {
        NodeRef store0;
        NodeRef store1;
        NodeRef load2;
        NodeRef access3;
    };

    template <typename OutputIt>
    OutputIt leakage_spectre_v4(z3::solver& solver, OutputIt out);
    
    using Mems = std::unordered_map<NodeRef, z3::expr>;
    struct MemsPair {
        Mems arch;
        Mems trans;
    };
    
    template <typename OutputIt>
    void leakage_spectre_v4_load2(z3::solver& solver, const MemsPair& mems, NodeRef load2, NodeRef access3, OutputIt& out, unsigned traceback_depth = 0);
    
    template <typename OutputIt>
    void leakage_spectre_v4_store1(z3::solver& solver, const MemsPair& mems, NodeRef store1, NodeRef load2, NodeRef access3, OutputIt& out);
    
    template <typename OutputIt>
    void leakage_spectre_v4_store0(z3::solver& solver, const MemsPair& mem, NodeRef store0, NodeRef store1, NodeRef load2, NodeRef access3, OutputIt& out);

    z3::expr leakage_get_same_solution(const LeakageClause& clause, const z3::eval& eval);
    
    // TODO: this should be more general than hard-coding two com/comx edges, maybe?
    struct Leakage;
    
    template <typename OutputIt>
    OutputIt process_leakage(OutputIt out, const z3::eval& eval);
    
public:
    using NodeRange = util::RangeContainer<NodeRef>;
    NodeRange node_range() const {
        return NodeRange {NodeRef {entry}, NodeRef {static_cast<unsigned>(nodes.size())}};
    }
    
private:
    NodeRef add_node(Node&& node); /*!< Add node to the AEG and return its corresponding node ref. */
    
    void add_unidir_edge(NodeRef src, NodeRef dst, const UHBEdge& e); /*!< Add a directed edge from \p src to \p dst with edge \p e */
    void add_bidir_edge(NodeRef a, NodeRef b, const UHBEdge& e); /*!< Add a directed edge in each direction between nodes \p a and \p b with edge \p e */
    /** Add a directed edge from \p src to \p dst and make the edge \p e optional with an additional unconstrained boolean variable
     * @param name The base name of the unconstrained boolean variable
     */
    z3::expr add_optional_edge(NodeRef src, NodeRef dst, const UHBEdge& e, const std::string& name);
    
    template <typename OutputIt>
    OutputIt get_edges(Direction dir, NodeRef ref, OutputIt out, Edge::Kind kind);
    
    using EdgePtrVec = std::vector<Edge *>;
    EdgePtrVec get_edges(Direction dir, NodeRef ref, Edge::Kind kind);
    
    template <typename OutputIt>
    OutputIt get_nodes(Direction dir, NodeRef ref, OutputIt out, Edge::Kind kind) const;
    
    std::vector<std::pair<NodeRef, z3::expr>> get_nodes(Direction dir, NodeRef ref, Edge::Kind kind) const;
    
    using EdgeSet = std::set<std::tuple<NodeRef, NodeRef, Edge::Kind>>;
    void output_execution(std::ostream& os, const z3::eval& eval, const EdgeSet& flag_edges = EdgeSet());
    void output_execution(const std::string& path, const z3::eval& eval, const EdgeSet& flag_edges = EdgeSet());
    
    Edge *find_edge(NodeRef src, NodeRef dst, Edge::Kind kind);
    const Edge *find_edge(NodeRef src, NodeRef dst, Edge::Kind kind) const;
    
    bool is_pseudoedge(Edge::Kind kind) const {
        static const std::unordered_set<Edge::Kind> pseudo = {
            Edge::RF, Edge::CO, Edge::FR, Edge::RFX, Edge::COX, Edge::FRX,
        };
        return pseudo.find(kind) != pseudo.end();
    }
    
public:
    template <typename OutputIt, typename Pred>
    OutputIt get_nodes_if(OutputIt out, Pred pred) const {
        for (NodeRef ref : node_range()) {
            if (pred(ref, lookup(ref))) {
                *out++ = ref;
            }
        }
        return out;
    }
    
    template <typename Container, typename OutputIt>
    OutputIt get_nodes(OutputIt out, const Container& container) const {
        return get_nodes_if(out, [&] (NodeRef, const Node& node) -> bool {
            return container.find(node.inst->kind()) != container.end();
        });
    }
    
    template <typename OutputIt>
    OutputIt get_nodes(OutputIt out, Inst::Kind kind) {
        return get_nodes(out, std::unordered_set<Inst::Kind> {kind});
    }
    
    z3::expr exists(Edge::Kind kind, NodeRef src, NodeRef dst);
    z3::expr exists_src(Edge::Kind kind, NodeRef src) const;
    z3::expr exists_dst(Edge::Kind kind, NodeRef dst) const;
    z3::expr co_exists(NodeRef src, NodeRef dst);
    z3::expr rf_exists(NodeRef src, NodeRef dst);
    z3::expr fr_exists(NodeRef src, NodeRef dst);
    z3::expr rfx_exists(NodeRef src, NodeRef dst) const;
    z3::expr cox_exists(NodeRef src, NodeRef dst) const;
    z3::expr frx_exists(NodeRef src, NodeRef dst) const;
    
private:
    z3::expr com_exists_precond(NodeRef src, NodeRef dst, Access src_kind, Access dst_kind) const;
    z3::expr comx_exists_precond(NodeRef src, NodeRef dst, XSAccessType src_kind, XSAccessType dst_kind) const;

    using ValueLoc = std::pair<CFG::ID, const llvm::Value *>;
    using ValueLocRel = std::unordered_map<std::pair<ValueLoc, ValueLoc>, llvm::AliasResult>;
    ValueLocRel alias_rel;
    
    llvm::AliasResult check_alias(NodeRef ref1, NodeRef ref2) const;
    void add_alias_result(const ValueLoc& vl1, const ValueLoc& vl2, llvm::AliasResult res);
    
    // SPECULATION QUERIES
    // TODO: remove
#if 0
    bool can_introduce_trans(NodeRef ref) const {
        return po.po.fwd.at(ref).size() > 1;
    }
    
    std::optional<NodeRef> can_trans(NodeRef ref) const {
        const auto& preds = po.po.rev.at(ref);
        if (preds.size() == 1) {
            return *preds.begin();
        } else {
            return std::nullopt;
        }
    }
#endif

    friend class Taint;
    friend class Taint_Array;
    std::unique_ptr<Taint> tainter;
    
    template <typename OutputIt>
    OutputIt get_path(const z3::eval& eval, OutputIt out) const;
};


struct AEG::Leakage {
    Edge::Kind com_kind;
    using Pair = std::pair<NodeRef, NodeRef>;
    Pair com;
    Edge::Kind comx_kind;
    Pair comx;
    std::string desc;
    z3::expr pred;
    
    auto to_tuple() const { return std::make_tuple(com_kind, com, comx_kind, comx, desc); }

    bool operator<(const Leakage& other) const {
        return to_tuple() < other.to_tuple();
    }
};



template <typename Function>
void AEG::for_each_edge(Edge::Kind kind, Function f) {
    graph.for_each_edge([&] (NodeRef src, NodeRef dst, Edge& edge) {
        if (edge.kind == kind) {
            f(src, dst, edge);
        }
    });
}

template <typename Function>
void AEG::for_each_edge(Edge::Kind kind, Function f) const {
    graph.for_each_edge([&] (NodeRef src, NodeRef dst, const Edge& edge) {
        if (edge.kind == kind) {
            f(src, dst, edge);
        }
    });
}

template <typename OutputIt>
OutputIt AEG::get_nodes(Direction dir, NodeRef ref, OutputIt out, Edge::Kind kind) const {
    const auto& map = graph(dir);
    for (const auto& p : map.at(ref)) {
        for (const auto& edge : p.second) {
            if (edge->kind == kind) {
                *out++ = std::make_pair(p.first, edge->exists);
                break;
            }
        }
    }
    return out;
}

template <typename Function> void AEG::for_each_node(Inst::Kind kind, Function f) {
    for (NodeRef ref : node_range()) {
        Node& node = lookup(ref);
        if (node.inst->kind() == kind) {
            f(ref, node);
        }
    }
}

template <typename Function> void AEG::for_each_node(Inst::Kind kind, Function f) const {
    for (NodeRef ref : node_range()) {
        const Node& node = lookup(ref);
        if (node.inst->kind() == kind) {
            f(ref, node);
        }
    }
}

