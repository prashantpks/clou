#pragma once

#include <vector>
#include <utility>
#include <string>
#include <sstream>

#include <z3++.h>

#include "config.h"
#include "progress.h"

namespace aeg {


extern unsigned constraint_counter; // TODO: this is sloppy



struct Constraints {
    std::vector<std::pair<z3::expr, std::string>> exprs;
    
    Constraints() {}
    explicit Constraints(const z3::expr& expr, const std::string& name): exprs({{expr, name}}) {}
    
    template <class Solver>
    void add_to(Solver& solver) const {
        for (const auto& p : exprs) {
            add_to(solver, p);
        }
    }
    
    template <class Solver>
    void add_to_progress(Solver& solver) const {
        Progress progress(exprs.size());
        for (const auto& p : exprs) {
            ++progress;
            add_to(solver, p);
        }
    }
    
    void operator()(const z3::expr& clause, const std::string& name);
    
    void simplify();
    
    z3::expr get(z3::context& ctx) const;
    
private:
    template <class Solver>
    void add_to(Solver& solver, const std::pair<z3::expr, std::string>& p) const {
        std::stringstream ss;
        ss << p.second << ":" << constraint_counter++;
        if constexpr (should_name_constraints) {
            solver.add(p.first, ss.str().c_str());
        } else {
            solver.add(p.first);
        }
    }
};

std::ostream& operator<<(std::ostream& os, const Constraints& c);


}