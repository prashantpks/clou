#pragma once

#include "aeg.h"
#include "taint.h"

class Taint_BV: public Taint {
public:
    static constexpr unsigned taint_bits = 2;
    Taint_BV(AEG& aeg): aeg(aeg), ctx(aeg.context.context) {}
    
    virtual z3::expr flag(NodeRef ref) override {
        const Node& node = aeg.lookup(ref);
        assert(node.is_memory_op());
        return get_value(ref, node.get_memory_address_pair().first);
    }
    
protected:
    using Node = AEG::Node;
    using Edge = AEG::Edge;
    AEG& aeg;
    z3::context& ctx;
    std::unordered_map<const llvm::Argument *, z3::expr> args;
    
    virtual z3::expr get_value(NodeRef ref, const llvm::Value *V) const;
};

class Taint_Array: public Taint_BV {
public:
    z3::expr taint_mem;
    Taint_Array(AEG& aeg): Taint_BV(aeg), taint_mem(aeg.context) {}
    
    virtual void run() override;
    
private:
    void handle_entry(NodeRef ref, Node& node);
    void handle_exit(NodeRef ref, Node& node);
    void handle_inst(NodeRef ref, Node& node);
};