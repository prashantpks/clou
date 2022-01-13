#pragma once

#include <string>
#include <ostream>
#include <optional>
#include <mutex>

#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Instructions.h>

namespace llvm {
template <typename T>
std::string to_string(const T& x) {
    std::string s;
    llvm::raw_string_ostream ss {s};
    ss << x;
    return s;
}

bool getelementptr_can_zero(const llvm::GetElementPtrInst *GEP);
std::optional<int> getelementptr_const_offset(const llvm::GetElementPtrInst *GEP);
std::optional<int> getelementptr_min_offset(const llvm::GetElementPtrInst *GEP);
std::optional<int> getelementptr_max_offset(const llvm::GetElementPtrInst *GEP);

bool contains_struct(const llvm::Type *T);

bool pointer_is_read_only(const llvm::Value *P);

  extern std::mutex errs_mutex;
struct locked_raw_ostream {
    std::unique_lock<std::mutex> lock;
    llvm::raw_ostream& os;
    
    locked_raw_ostream(llvm::raw_ostream& os, std::mutex& mutex): lock(mutex), os(os) {}
    
    operator llvm::raw_ostream&() { return os; }
};

locked_raw_ostream cerr();

}

template <typename T>
std::ostream& llvm_to_cxx_os(std::ostream& os, const T& x) {
    return os << llvm::to_string(x);
}

inline std::ostream& operator<<(std::ostream& os, const llvm::Instruction& I) {
   return llvm_to_cxx_os(os, I);
}

