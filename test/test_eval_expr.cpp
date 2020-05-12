#include "eval_expr.hpp"
#include "env.hpp"
#include "opcodes.hpp"
#include "util.hpp"
#include "test_common.hpp"

#include <cmath>

using namespace nivalis;
using namespace nivalis::detail;

namespace {
using AST = std::vector<uint32_t>;
double test_eval_ast(Environment& env, AST& ast) {
    const uint32_t* ast_ptr = &ast[0];
    return eval_ast(env, &ast_ptr);
}
}  // namespace

int main() {
    BEGIN_TEST(test_eval_expr);
    using namespace OpCode;
    Environment dummy_env;
    {
        AST ast; ast.push_back(OpCode::null);
        ASSERT(std::isnan(test_eval_ast(dummy_env, ast)));
    }
    {
        AST ast; ast.push_back(add);
        util::push_dbl(ast, 2.); util::push_dbl(ast, 1001.);
        ASSERT_FLOAT_EQ(test_eval_ast(dummy_env, ast), 1003.);
    }
    {
        AST ast; ast.push_back(sub); ast.push_back(add);
        util::push_dbl(ast, -13.); util::push_dbl(ast, -100.);
        ast.push_back(OpCode::div);
        util::push_dbl(ast, -52.);
        util::push_dbl(ast, 10.);
        ASSERT_FLOAT_EQ(test_eval_ast(dummy_env, ast), -107.8);
    }
    {
        AST ast; ast.push_back(mul);
        util::push_dbl(ast, 2.);
        ast.push_back(ref); ast.push_back(0);

        Environment env; env.set("x", -1.5);
        ASSERT_EQ(env.addr_of("x"), 0);
        ASSERT_EQ(ast.size(), 6);
        ASSERT_FLOAT_EQ(test_eval_ast(env, ast), -3.);
    }
    {
        AST ast;
        ast.push_back(power); ast.push_back(sub);
        util::push_dbl(ast, -13.); util::push_dbl(ast, -16.);
        util::push_dbl(ast, 9.5);
        ASSERT_FLOAT_EQ(test_eval_ast(dummy_env, ast), pow(3, 9.5));
    }
    {
        AST ast;
        ast.push_back(roundb);
        ast.push_back(mul); ast.push_back(absb);
        util::push_dbl(ast, -19.5);
        util::push_dbl(ast, 2.9);
        ASSERT_FLOAT_EQ(test_eval_ast(dummy_env, ast), 57.);
    }
    {
        AST ast; ast.push_back(absb);
        ast.push_back(mul);
        util::push_dbl(ast, 13.);
        util::push_dbl(ast, 14.);
        ASSERT_FLOAT_EQ(test_eval_ast(dummy_env, ast), 182.);
    }
    {
        AST ast; ast.push_back(OpCode::eq); ast.push_back(sqrb);
        util::push_dbl(ast, -5.5);
        ast.push_back(power);
        util::push_dbl(ast, -5.5);
        util::push_dbl(ast, 2.);
        ASSERT_FLOAT_EQ(test_eval_ast(dummy_env, ast), 1.);
    }
    {
        AST ast; ast.push_back(OpCode::lt); ast.push_back(sqrb);
        util::push_dbl(ast, -5.5);
        ast.push_back(mul);
        util::push_dbl(ast, -5.5);
        util::push_dbl(ast, -5.5);
        ASSERT_FLOAT_EQ(test_eval_ast(dummy_env, ast), 0.);
    }
    {
        Environment env; env.set("_", -1.5); env.set("x", 2.65);

        AST ast; ast.push_back(OpCode::max);
        ast.push_back(ref); ast.push_back(env.addr_of("x"));
        ast.push_back(tgammab);
        util::push_dbl(ast, 3.29);
        ASSERT_FLOAT_EQ(test_eval_ast(env, ast), tgamma(3.29));
        env.set("x", M_PI);
        ASSERT_FLOAT_EQ(test_eval_ast(env, ast), M_PI);
    }
    {
        Environment env; env.set("y", 1.5); env.set("x", 2.65);

        AST ast; ast.push_back(OpCode::mod);
        ast.push_back(ref); ast.push_back(env.addr_of("y"));
        ast.push_back(digammab);
        util::push_dbl(ast, -1.5);
        ASSERT_FLOAT_EQ(test_eval_ast(env, ast), 0.09368671870951362);
        env.set("x", 9.0);
        ASSERT_FLOAT_EQ(test_eval_ast(env, ast), 0.09368671870951362);
        env.set("y", 10.0);
        ASSERT_FLOAT_EQ(test_eval_ast(env, ast), 0.15580703096659532);
    }

    {
        AST ast;
        ast.push_back(OpCode::land);
        ast.push_back(OpCode::lxor);
        util::push_dbl(ast, 0.0); util::push_dbl(ast, 1.0);
        util::push_dbl(ast, 0.0);
        ASSERT_FLOAT_EQ(test_eval_ast(dummy_env, ast), 0.0);
        ast.pop_back(); ast.pop_back(); ast.pop_back();
        util::push_dbl(ast, 1.0);
        ASSERT_FLOAT_EQ(test_eval_ast(dummy_env, ast), 1.0);
    }

    {
        AST ast; ast.push_back(OpCode::bnz);
        util::push_dbl(ast, 1.0); util::push_dbl(ast, 2.0);
        util::push_dbl(ast, 3.0);
        ASSERT_FLOAT_EQ(test_eval_ast(dummy_env, ast), 2.0);
        ast.clear(); ast.push_back(OpCode::bnz);
        util::push_dbl(ast, 0.0); util::push_dbl(ast, 2.0);
        util::push_dbl(ast, 3.0);
        ASSERT_FLOAT_EQ(test_eval_ast(dummy_env, ast), 3.0);
    }

    {
        Environment env; env.set("y", 1.5); env.set("x", 0.0);
        AST ast; ast.push_back(OpCode::bnz);
        ast.push_back(OpCode::ge);
        ast.push_back(ref); ast.push_back(0);
        util::push_dbl(ast, 0.0);

        ast.push_back(OpCode::bnz);
        ast.push_back(OpCode::le);
        ast.push_back(ref); ast.push_back(1);
        util::push_dbl(ast, 0.0);

        util::push_dbl(ast, -1.0);
        ast.push_back(OpCode::absb);
        util::push_dbl(ast, -9.0);
        ast.push_back(ref); ast.push_back(0);

        ASSERT_FLOAT_EQ(test_eval_ast(env, ast), -1.0);
        env.set("x", 1.5);
        ASSERT_FLOAT_EQ(test_eval_ast(env, ast), 9.0);
        env.set("y", -9999.312);
        ASSERT_FLOAT_EQ(test_eval_ast(env, ast), -9999.312);
    }

    {
        Environment env; env.set("i", 0);
        AST ast; ast.push_back(OpCode::sums);
        ast.push_back(0); // ref
        util::push_dbl(ast, -1.0);
        util::push_dbl(ast, 100.0);
        ast.push_back(mul);
        ast.push_back(ref); ast.push_back(0);
        util::push_dbl(ast, -2.);

        ASSERT_FLOAT_EQ(test_eval_ast(env, ast), -10098.);
    }

    {
        Environment env; env.addr_of("j", false);
        AST ast; ast.push_back(OpCode::prods);
        ast.push_back(0); // ref
        util::push_dbl(ast, 3.0);
        util::push_dbl(ast, 7.0);
        ast.push_back(unaryminus);
        ast.push_back(ref); ast.push_back(0);

        ASSERT_FLOAT_EQ(test_eval_ast(env, ast), -2520.);
    }

    {
        Environment env; env.set("j", 2.0); env.addr_of("k", false);
        env.set("lb", 2.0);
        AST ast; ast.push_back(OpCode::prods);
        ast.push_back(1); // ref
        util::push_dbl(ast, 5.0);
        ast.push_back(ref); ast.push_back(2);

        ast.push_back(OpCode::sums);
        ast.push_back(0); // ref
        util::push_dbl(ast, 2.0);
        util::push_dbl(ast, 6.0);

        ast.push_back(add); ast.push_back(add);
        ast.push_back(ref); ast.push_back(0);
        ast.push_back(ref); ast.push_back(1);
        ast.push_back(ref); ast.push_back(2);

        ASSERT_FLOAT_EQ(test_eval_ast(env, ast), 4950000.);
    }
    END_TEST;
}
