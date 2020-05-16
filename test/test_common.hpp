#pragma once
#ifndef _TEST_COMMON_H_795032FD_6CA7_4D79_BB80_D62CA09CFE65
#define _TEST_COMMON_H_795032FD_6CA7_4D79_BB80_D62CA09CFE65

#include <random>
#include <iomanip>
#include <iostream>
#include <vector>
#include <chrono>
#include <utility>
#include "expr.hpp"
#define BEGIN_TEST(tname) int _fail = 0; const char* _tname = #tname;
#define ASSERT(x) do { \
    auto tx = x; \
    if (!tx) { \
    std::cout << "Assertion FAILED: \"" << #x << "\" (" << tx << \
        ")\n  at " << _tname << " line " << __LINE__ <<"\n"; \
    ++_fail;\
}} while(0)
#define ASSERT_EQ(x, y) do {\
    auto tx = x; auto ty = y; \
    if (tx != ty) { \
    std::cout << "Assertion FAILED: " << #x << " != " << #y << " (" << tx << " != " << ty << \
        ")\n  at " << _tname << " line " << __LINE__ <<"\n"; \
    ++_fail;\
}} while(0)
#define FLOAT_EPS 1e-6
#define ASSERT_FLOAT_EQ(x, y) do {\
    double tx = x, ty = y; \
    if (absrelerr(tx, ty) > FLOAT_EPS) { \
    std::cout << "Assertion FAILED: (float compare) " << \
        std::setprecision(10) <<#x << " != " << #y << " (" << tx << " != " << ty << \
        ")\n  at " << _tname << " line " << __LINE__ <<"\n"; \
    ++_fail;\
}} while(0)
#define END_TEST do{if(_fail == 0) { \
    std::cout << _tname<< ": all passed\n"; \
    return 0; \
} else { \
    std::cout << _tname << ": " << _fail << " asserts FAILED\n"; \
    return 1; \
} \
} while(0);

#define BEGIN_PROFILE auto start = std::chrono::high_resolution_clock::now()
#define PROFILE(x) do{printf("%s: %f ns\n", #x, std::chrono::duration<double, std::nano>(std::chrono::high_resolution_clock::now() - start).count()); start = std::chrono::high_resolution_clock::now(); }while(false)
#define PROFILE_STEPS(x,stp) do{printf("%s: %f ns / step\n", #x, std::chrono::duration<double, std::nano>(std::chrono::high_resolution_clock::now() - start).count()/(stp)); start = std::chrono::high_resolution_clock::now(); }while(false)

#define WAT(x) cerr<<(#x)<<"="<<(x)<<endl
#define WAT2(x,y) cerr<<(#x)<<"="<<(x)<<" "<<(#y)<<"="<<(y)<<endl
#define WAT3(x,y,z) cerr<<(#x)<<"="<<(x)<<" ",WAT2(y,z)
#define PELN cerr<<"\n";
#define PE1(x) cerr<<(x)<<endl
#define PE2(x,y) cerr<<(x)<<" "<<(y)<<endl

#define PARR(a,s,e) for(size_t _c=size_t(s);_c<size_t(e);++_c)cout<<(a[_c])<<(_c==size_t(e-1)?"":" ");

template <class T1, class T2> std::ostream& operator<<(std::ostream& os, const std::pair<T1, T2> & p){
    return os << p.ff << "," << p.ss;
}
template <class T> std::ostream& operator<<(std::ostream& os, const std::vector<T> & v){
    for(int i=0; i<(int)v.size();++i){ if(i) os << " "; os << v[i];} return os; 
}
template <class Float> Float absrelerr(Float x, Float y) {
    return std::min(std::fabs(x - y), std::fabs(x - y) / std::fabs(x));
}

namespace nivalis {
namespace test {

namespace {
std::default_random_engine reng{std::random_device{}()};
using AST = Expr::AST;
using ASTNode = nivalis::Expr::ASTNode;

struct ThunkManager {
    ThunkManager (AST& ast) : ast(ast) {}
    AST& ast;
    std::vector<size_t> thunks;
    void begin() {
        thunks.push_back(ast.size());
        ast.push_back(OpCode::thunk_ret);
    }
    void end() {
        ast.emplace_back(OpCode::thunk_jmp,
                ast.size() - thunks.back());
        thunks.pop_back();
    }
};
// AST node shorthands
ASTNode Ref(uint32_t addr) { return ASTNode(OpCode::ref, addr); }
ASTNode SumOver(uint32_t addr) { return ASTNode(OpCode::sums, addr); }
ASTNode ProdOver(uint32_t addr) { return ASTNode(OpCode::prods, addr); }
}  // namespace

}  // namespace test
}  // namespace nivalis
#endif // ifndef _TEST_COMMON_H_795032FD_6CA7_4D79_BB80_D62CA09CFE65
