#pragma once

#include <functional>
#include <vector>
#include <iostream>

#include <z3++.h>

#include "config.h"
#include "util/output.h"
#include "cfg/block.h"
#include "util/z3.h"
#include "util/scope.h"
#include "aeg/aeg.h"

namespace lkg {

template <typename Derived>
struct Leakage {
    NodeRefVec vec() const { std::abort(); }
    void print_short(std::ostream& os) const;
    void print_long(std::ostream& os, const aeg::AEG& aeg) const;
    NodeRef get_transmitter() const { std::abort(); }
};


class Detector {
public:
    using Solver = aeg::AEG::Solver;
    
    virtual void run() = 0;
    virtual ~Detector() {
        std::cerr << rf_source_count << "\n";
    }
    
protected:
    std::unordered_set<const llvm::Instruction *> transmitters;
    struct next_transmitter {};
    struct lookahead_found {};
    
    bool lookahead(std::function<void ()> thunk);
    
    struct EdgeRef {
        NodeRef src;
        NodeRef dst;
        aeg::Edge::Kind kind;
    };
    using EdgeVec = std::vector<EdgeRef>;
    using Mems = std::unordered_map<NodeRef, z3::expr>;
    
    aeg::AEG& aeg;
    Solver& solver;

    using Actions = std::vector<std::string>;
    using PushAction = util::push_scope<Actions>;
    Actions actions;
    z3::expr init_mem;
    Mems mems;
    
    z3::expr mem(NodeRef ref) const;
    
    EdgeVec flag_edges;
    
    Detector(aeg::AEG& aeg, Solver& solver);
    
    z3::context& ctx();
    
    /* UTILITIES FOR SUBCLASSES */
    
    /** Trace back load */
    enum class CheckMode { FAST, SLOW };
    
    void traceback(NodeRef load, std::function<void (NodeRef, CheckMode)> func, CheckMode mode);
    void traceback_rf(NodeRef load, std::function<void (NodeRef, CheckMode)> func, CheckMode mode);
    void traceback_edge(aeg::Edge::Kind kind, NodeRef ref, std::function<void (NodeRef, CheckMode)> func, CheckMode mode);
    
    void for_each_transmitter(aeg::Edge::Kind kind, std::function<void (NodeRef, CheckMode)> func);
    
    auto push_edge(const EdgeRef& edge) {
        return util::push(flag_edges, edge);
    }
    
    void precompute_rf(NodeRef load);
    
    using Sources = std::unordered_map<NodeRef, z3::expr>;
    using RF = std::unordered_map<NodeRef, Sources>;
    const Sources& rf_sources(NodeRef load);
    void rf_sources(NodeRef load, Sources&& sources);
        
    // TODO: this is protected for now
    unsigned traceback_depth = 0;
    bool lookahead_tmp = true;
    
private:
    CFGOrder partial_order;
    
    Mems get_mems();
    Mems get_mems(const NodeRefSet& set); /*!< only consider nodes in \p set */
    Mems get_mems1(const NodeRefSet& set); /*!< this uses topological order */
    
    RF rf; // TODO: remove?
    
    // DEBUG members
    // TODO: remove
    std::unordered_map<NodeRef, unsigned> rf_source_count;
};

template <class Leakage>
class Detector_: public Detector {
public:
    using leakage_type = Leakage;
    
    virtual void run() override final;
    
protected:
    using LeakVec = std::vector<Leakage>;
    using OutputIt = std::back_insert_iterator<LeakVec>;
    
    virtual std::string name() const = 0;
    virtual void run_() = 0;

    void output_execution(const Leakage& leak);
    
    Detector_(aeg::AEG& aeg, Solver& solver): Detector(aeg, solver) {}
    
private:
    std::vector<std::pair<Leakage, std::string>> leaks;
};

/* IMPLEMENTATIONS */

template <typename Leakage>
void Detector_<Leakage>::run() {
    run_();
    
    const std::ios::openmode openmode = batch_mode ? (std::ios::out | std::ios::app) : (std::ios::out);
    
    {
        // open file
        const std::string path = util::to_string(output_dir, "/leakage.txt");
        std::ofstream ofs {
            path,
            openmode,
        };
        
        // dump leakage
        if (batch_mode) {
            ofs << "\n" << aeg.function_name() << ": \n";
        }
        for (const auto& leak : leaks) {
            leak.first.print_short(ofs);
            ofs << " : " << leak.second << " --";
            leak.first.print_long(ofs, aeg);
            ofs << "\n";
        }
    }
    
    {
        const std::string path = util::to_string(output_dir, "/transmitters.txt");
        std::ofstream ofs {
            path,
            openmode,
        };
        
        // print out set of transmitters
        std::unordered_set<const llvm::Instruction *> transmitters;
        for (const auto& leak : leaks) {
            transmitters.insert(aeg.lookup(leak.first.get_transmitter()).inst->get_inst());
        }
        llvm::errs() << "transmitters:\n";
        for (const auto transmitter : transmitters) {
            llvm::errs() << *transmitter << "\n";
            ofs << *transmitter << "\n";
        }
    }
}


template <typename Leakage>
void Detector_<Leakage>::output_execution(const Leakage& leak) {
    assert(lookahead_tmp);
    
    leaks.emplace_back(leak, std::accumulate(actions.rbegin(), actions.rend(), std::string(), [] (const std::string& a, const std::string& b) -> std::string {
        std::string res = a;
        if (!res.empty()) {
            res += "; ";
        }
        res += b;
        return res;
    }));
        
    std::stringstream ss;
    ss << output_dir << "/" << name();
    for (const NodeRef ref : leak.vec()) {
        ss << "-" << ref;
    }
    ss << ".dot";
    const std::string path = ss.str();
    assert(solver.check() == z3::sat);
    const z3::eval eval {solver.get_model()};
    const auto edge = push_edge(EdgeRef {
        .src = leak.get_transmitter(),
        .dst = aeg.exit_con(eval),
        .kind = aeg::Edge::RFX,
    });
    
    // TODO: shouldn't need to do this
    std::vector<std::tuple<NodeRef, NodeRef, aeg::Edge::Kind>> flag_edges_;
    std::transform(flag_edges.begin(), flag_edges.end(), std::back_inserter(flag_edges_), [] (const EdgeRef& e) {
        return std::make_tuple(e.src, e.dst, e.kind);
    });
    
    // add to detector's transmitters list
    {
        const aeg::Node& transmitter_node = aeg.lookup(leak.get_transmitter());
        transmitters.insert(transmitter_node.inst->get_inst());
    }
    
    if (witness_executions) {
        aeg.output_execution(path, eval, flag_edges_);
    }
    
    if (fast_mode) {
        throw next_transmitter {};
    }
}

template <typename Derived>
void Leakage<Derived>::print_long(std::ostream& os, const aeg::AEG& aeg) const {
    const auto v = static_cast<const Derived&>(*this).vec();
    for (auto it = v.begin(); it != v.end(); ++it) {
        if (it != v.begin()) {
            os << "; ";
        }
        os << *aeg.lookup(*it).inst;
    }
}

template <typename Derived>
void Leakage<Derived>::print_short(std::ostream& os) const {
    const auto v = static_cast<const Derived&>(*this).vec();
    for (auto it = v.begin(); it != v.end(); ++it) {
        if (it != v.begin()) {
            os << " ";
        }
        os << *it;
    }
}


namespace dbg {

template <typename Solver>
void append_core(const Solver& solver) {
    if (const char *corepath = std::getenv("CORES")) {
        std::ofstream ofs {corepath, std::ios::app};
        ofs << __FILE__ << ":" << __LINE__ << ": " << solver.unsat_core() << "\n";
    }
}

}

}


#define z3_cond_scope \
const std::optional<z3::scope<std::remove_reference<decltype(solver)>::type>> scope = (mode == CheckMode::SLOW) ? std::make_optional(z3::scope<std::remove_reference<decltype(solver)>::type>(solver)) : std::optional<z3::scope<std::remove_reference<decltype(solver)>::type>>()
