#pragma once

#include <functional>
#include <vector>
#include <iostream>
#include <unordered_map>

#include <z3++.h>

#include "config.h"
#include "util/output.h"
#include "cfg/block.h"
#include "util/z3.h"
#include "util/scope.h"
#include "util/timer.h"
#include "util/algorithm.h"
#include "noderef.h"

namespace lkg {

struct Leakage {
    NodeRefVec vec;
    NodeRef transmitter;
    
    void print_short(std::ostream& os) const;
    void print_long(std::ostream& os, const aeg::AEG& aeg) const;
};


class Detector {
protected:
    enum class CheckMode { FAST, SLOW };
    using DepVec = std::vector<std::pair<aeg::Edge::Kind, aeg::ExecMode>>;

    struct next_transmitter {};
    struct lookahead_found {};
    
    struct EdgeRef {
        NodeRef src;
        NodeRef dst;
        aeg::Edge::Kind kind;
    };
    using EdgeVec = std::vector<EdgeRef>;
    using Mems = std::unordered_map<NodeRef, z3::expr>;
    
    using Actions = std::vector<std::string>;
    using PushAction = util::push_scope<Actions>;
    
    using Deps = std::vector<aeg::Edge::Kind>;
    using DepIt = DepVec::const_reverse_iterator;
    
    struct Child {
        NodeRef ref;
        int fd;
    };

public:
    struct CheckStats {
        unsigned sat = 0;
        unsigned unsat = 0;
        unsigned unknown = 0;
        unsigned total() const { return sat + unsat + unknown; }
    };
    
    void run();
    virtual ~Detector();
    
    const auto& get_transmitters() const { return transmitters; }
    
protected:
    CheckStats check_stats;
    std::unordered_set<const llvm::Instruction *> transmitters;
    aeg::AEG& aeg;
    z3::solver solver;
    z3::solver alias_solver;
    Actions actions;
    z3::expr init_mem;
    Mems mems;
    NodeRefSet exec_window;
    NodeRefSet exec_notwindow;
    NodeRefSet trans_window;
    NodeRefSet trans_notwindow;
    EdgeVec flag_edges;
    
    Detector(aeg::AEG& aeg, z3::solver& solver);
    
    /* SUBCLASS INTERFACE */
    virtual void run_() = 0;
    virtual std::string name() const = 0;
    virtual std::optional<float> get_timeout() const { return std::nullopt; } // no timeout by default
    virtual void set_timeout(z3::check_result check_res, float secs) {} // no timeout by default, so nothing to update
    virtual void run_transmitter(NodeRef transmitter, CheckMode mode) = 0;
    virtual void run_postdeps(const NodeRefVec& vec, CheckMode mode) = 0;
    virtual DepVec deps() const = 0;
    
    /* UTILITIES FOR SUBCLASSES */
    bool lookahead(std::function<void ()> thunk);
    z3::check_result solver_check(bool allow_unknown = true);
    void assert_edge(NodeRef src, NodeRef dst, const z3::expr& edge, aeg::Edge::Kind kind);
    bool check_edge(NodeRef src, NodeRef dst) const {
        return exec_window.contains(src) && exec_window.contains(dst);
    }
    z3::context& ctx();

    void traceback(NodeRef load, aeg::ExecMode exec_mode, std::function<void (NodeRef, CheckMode)> func, CheckMode mode);
    void traceback_rf(NodeRef load, aeg::ExecMode exec_mode, std::function<void (NodeRef, CheckMode)> func, CheckMode mode);
    void traceback_edge(aeg::Edge::Kind kind, NodeRef ref, std::function<void (NodeRef, CheckMode)> func, CheckMode mode);
    
    void traceback_deps(NodeRef from_ref, std::function<void (const NodeRefVec&, CheckMode)> func,
                        CheckMode mode);
    
    void for_each_transmitter(std::function<void (NodeRef, CheckMode)> func);
    void for_each_transmitter_parallel_private(NodeRefSet& candidate_transmitters, std::function<void (NodeRef, CheckMode)> func);
    void for_each_transmitter_parallel_shared(NodeRefSet& candidate_transmitters, std::function<void (NodeRef, CheckMode)> func);

    template <class OutputIt>
    OutputIt for_new_transmitter(NodeRef transmitter, std::function<void (NodeRef, CheckMode)> func, OutputIt out);
    void for_one_transmitter(NodeRef transmitter, std::function<void (NodeRef, CheckMode)> func, bool priv);
    
    auto push_edge(const EdgeRef& edge) {
        return util::push(flag_edges, edge);
    }
    
    NodeRefSet reachable_r(const NodeRefSet& window, NodeRef ref) const;
    std::unordered_map<NodeRef, z3::expr> precompute_rf_one(NodeRef load, const NodeRefSet& window);
    void precompute_rf(NodeRef load);
    
    using Sources = std::unordered_map<NodeRef, z3::expr>;
    using RF = std::unordered_map<NodeRef, Sources>;
    const Sources& rf_sources(NodeRef load);
    void rf_sources(NodeRef load, Sources&& sources);
        
    // TODO: this is protected for now
    unsigned traceback_depth = 0;
    bool lookahead_tmp = true;
    
    void output_execution(const Leakage& leak);
    
private:
    CFGOrder partial_order;
    std::vector<std::pair<Leakage, std::string>> leaks;
    RF rf; // TODO: remove?
    
    Mems get_mems(const NodeRefSet& set); /*!< this uses topological order */
    
    void traceback_deps_rec(DepIt it, DepIt end, NodeRefVec& vec, NodeRef from_ref,
                            std::function<void (const NodeRefVec&, CheckMode)> func, CheckMode mode);
};




namespace dbg {

template <typename Solver>
void append_core(const Solver& solver, const std::string& label) {
    if (const char *corepath = std::getenv("CORES")) {
        std::ofstream ofs {corepath, std::ios::app};
        ofs << __FILE__ << ":" << __LINE__ << ": " << label << ": " << solver.unsat_core() << "\n";
    }
}

}

}


#define z3_cond_scope \
const std::optional<z3::scope> scope = (mode == CheckMode::SLOW) ? std::make_optional(z3::scope(solver)) : std::optional<z3::scope>()
