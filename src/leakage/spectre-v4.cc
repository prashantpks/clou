#include "spectre-v4.h"
#include "cfg/expanded.h"
#include "util/algorithm.h"

namespace lkg {

Detector::DepVec SpectreV4_Detector::deps() const {
    return DepVec {aeg::Edge::ADDR, aeg::Edge::ADDR};
}


void SpectreV4_Detector::run_() {
    for_each_transmitter(aeg::Edge::ADDR, [&] (NodeRef transmitter, CheckMode mode) {
        run_transmitter(transmitter, mode);
    });
}

void SpectreV4_Detector::run_transmitter(NodeRef transmitter, CheckMode mode) {
    traceback_deps(transmitter, [&] (const NodeRefVec& vec, CheckMode mode) {
        
        const NodeRef load = vec.back();
        assert(aeg.lookup(load).may_read());
        
        /* add <ENTRY> -RFX-> load */
        z3_cond_scope;
        if (!spectre_v4_mode.concrete_sourced_stores) {
            if (mode == CheckMode::SLOW) {
                solver.add(aeg.rfx_exists(aeg.entry, load), "entry -RFX-> load");
            }
        }
        
        run_bypassed_store(load, vec, mode);
        
    }, mode);
}

void SpectreV4_Detector::run_bypassed_store(NodeRef load, const NodeRefVec& vec, CheckMode mode) {
    if (mode == CheckMode::SLOW) {
        // check if sat
        if (solver_check() == z3::unsat) {
            logv(1, __FUNCTION__ << ":" << __LINE__ << ": backtrack: unsat\n");
            return;
        }
    }
    
    /*
     * TODO: in fast mode, Don't even need to trace back RF… just need to find ONE store that can be sourced,
     * Basically, we can just OR all stores together.
     */
    // TODO: give this its own variable?
    if (!spectre_v4_mode.concrete_sourced_stores) {
        run_bypassed_store_fast(load, vec, mode);
        return;
    }
    
    traceback_rf(load, [&] (NodeRef bypassed_store, CheckMode mode) {
        // store can't be bypassed if older than stb_size
        if (bypassed_store == aeg.entry) { return; }
        
        if (!aeg.may_source_stb(load, bypassed_store)) {
            return;
        }
        
        const auto& node = aeg.lookup(bypassed_store);
        
        if (mode == CheckMode::SLOW) {
            solver.add(node.arch, "bypassed_store.arch");
            
            // check if sat
            if (solver_check() == z3::unsat) {
                logv(1, __FUNCTION__ << ":" << __LINE__ << ": backtrack: unsat\n");
                return;
            }
        }

        if (spectre_v4_mode.concrete_sourced_stores) {
            run_sourced_store(load, bypassed_store, vec, mode);
        } else {
            check_solution(load, bypassed_store, aeg.entry, vec, mode);
        }
        
    }, mode);
}

void SpectreV4_Detector::run_bypassed_store_fast(NodeRef load, const NodeRefVec& vec, CheckMode mode) {
    NodeRefVec todo = {load};
    NodeRefSet seen;
    z3::expr_vector exprs(ctx());
    
    while (!todo.empty()) {
        NodeRef bypassed_store = todo.back();
        todo.pop_back();
        if (!seen.insert(bypassed_store).second) { continue; }
        
        const auto& node = aeg.lookup(bypassed_store);
        
        // if it is a write, then check whether it can be bypassed
        if (node.may_write() && aeg.may_source_stb(load, bypassed_store)) {
            if (mode == CheckMode::SLOW) {
                exprs.push_back(node.arch && node.write && node.same_addr(aeg.lookup(load)));
            }
            if (mode == CheckMode::FAST) {
                throw lookahead_found();
            }
        }
                
        util::copy(aeg.po.po.rev.at(bypassed_store), std::back_inserter(todo));
    }

    if (mode == CheckMode::SLOW) {
        // TODO: make it so that bypassed store is optoinal
        solver.add(z3::mk_or(exprs), "bypassed_store");
        check_solution(load, aeg.entry, aeg.entry, vec, mode);
    }
}

void SpectreV4_Detector::check_solution(NodeRef load, NodeRef bypassed_store, NodeRef sourced_store, const NodeRefVec& vec, CheckMode mode) {
    if (mode == CheckMode::SLOW) {
        switch (solver.check()) {
            case z3::sat: {
                const auto edge = push_edge(EdgeRef {
                    .src = sourced_store,
                    .dst = load,
                    .kind = aeg::Edge::RFX,
                });
                const NodeRef universl_transmitter = vec.front();
                const NodeRefVec vec2 = {sourced_store, bypassed_store, load, universl_transmitter};
                output_execution(Leakage {
                    .vec = vec2,
                    .transmitter = universl_transmitter,
                });
                break;
            }
                
            case z3::unsat: {
                logv(0, __FUNCTION__ << ": backtrack: unsat\n");
                break;
            }
                
            case z3::unknown: {
                std::cerr << "Z3 ERROR: unknown: " << solver.reason_unknown() << "\n";
                std::abort();
            }
                
            default: std::abort();
        }
    } else {
        
        throw lookahead_found();
        
    }
}

void SpectreV4_Detector::run_sourced_store(NodeRef load, NodeRef bypassed_store, const NodeRefVec& vec, CheckMode mode) {
    const auto bypassed_store_idx = aeg.po.postorder_idx(bypassed_store);
    assert(load_idx < bypassed_store_idx);
    
    NodeRefSet sourced_store_candidates = exec_window;
    
    for (NodeRef sourced_store : sourced_store_candidates) {
        const auto sourced_store_idx = aeg.po.postorder_idx(sourced_store);
        const aeg::Node& sourced_store_node = aeg.lookup(sourced_store);
        
        // require sourced store to come before in postorder
        if (!(sourced_store_idx > bypassed_store_idx)) {
            continue;
        }
        
        // require that it may be store
        if (!sourced_store_node.may_write()) {
            continue;
        }
        
#if 0
        // check if would be outside of store buffer
        if (sourced_store != aeg.entry && !aeg.may_source_stb(load, sourced_store)) {
            continue;
        }
#endif
        
#if 0
        // Another approximation of is_ancestor()
        if (aeg.lookup(sourced_store).stores_in > aeg.lookup(leak.bypassed_store).stores_in) {
            continue;
        }
#endif
        
        z3_cond_scope;
        
        if (mode == CheckMode::SLOW) {
            const z3::expr same_addr = aeg::Node::same_addr(sourced_store_node, aeg.lookup(load));
            solver.add(same_addr, "load.addr == sourced_store.addr");
            solver.add(aeg.rfx_exists(sourced_store, load), "load -RFX-> sourced_store");
        }
        
        const auto action = util::push(actions, util::to_string("sourced ", sourced_store));
        
        check_solution(load, bypassed_store, sourced_store, vec, mode);
        
    }
}

void SpectreV4_Detector::run_postdeps(const NodeRefVec& vec, CheckMode mode) {
    const NodeRef load = vec.back();
    assert(aeg.lookup(load).may_read());
    run_bypassed_store(load, vec, mode);
}


std::optional<float> SpectreV4_Detector::get_timeout() const {
    // return std::nullopt;
    if (unsats.empty()) {
        return 5.f;
    } else {
        return util::average(unsats) * 5;
    }
}

void SpectreV4_Detector::set_timeout(z3::check_result check_res, float secs) {
    switch (check_res) {
        case z3::sat:
            sats.push_back(secs);
            break;
            
        case z3::unsat:
            unsats.push_back(secs);
            break;
            
        case z3::unknown:
            unknowns.push_back(secs);
            break;
            
        default: std::abort();
    }
}


}
