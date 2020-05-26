#include "expr.hpp"
#include "env.hpp"
#include "opcodes.hpp"
#include "util.hpp"
#include "test_common.hpp"

#include <cmath>

using namespace nivalis;
using namespace nivalis::detail;
using namespace nivalis::test;

int main() {
    BEGIN_TEST(test_eval_expr);

    using namespace OpCode;
    std::uniform_real_distribution<double> unif(-100.0, 100.0);
    Environment env; env.addr_of("x",false);
    Environment dummy_env;
    ASSERT(std::isnan(eval_ast(dummy_env, { null })));
    ASSERT_FLOAT_EQ(eval_ast(dummy_env, { add, 2., 1001. }), 1003.);
    ASSERT_FLOAT_EQ(eval_ast(dummy_env,
                { sub, add, -13., -100., divi, -52., -10. }), -118.2);
    {
        AST ast = { mul, 2., Ref(0) };
        Environment env; env.set("x", -1.5);
        ASSERT_EQ(env.addr_of("x"), 0);
        ASSERT_FLOAT_EQ(eval_ast(env, ast), -3.);
    }
    ASSERT_FLOAT_EQ(eval_ast(dummy_env,
                { power , sub, -13., -16., 9.5 }), pow(3, 9.5));
    ASSERT_FLOAT_EQ(eval_ast(dummy_env,
                { roundb, mul, absb, -19.5, 2.9 }), 57.);
    ASSERT_FLOAT_EQ(eval_ast(dummy_env, { absb, mul, 13., 14. }), 182.);
    ASSERT_FLOAT_EQ(eval_ast(dummy_env,
                { eq, sqrb, -5.5, power, -5.5, 2. }), 1.);
    ASSERT_FLOAT_EQ(eval_ast(dummy_env,
                { lt, sqrb, -5.5, mul, -5.5, -5.5 }), 0.);
    {
        Environment env; env.set("_", -1.5); env.set("x", 2.65);
        AST ast = { OpCode::max, Ref(env.addr_of("x")),
                    tgammab, 3.29 };
        ASSERT_FLOAT_EQ(eval_ast(env, ast), tgamma(3.29));
        env.set("x", M_PI);
        ASSERT_FLOAT_EQ(eval_ast(env, ast), M_PI);
    }
    {
        Environment env; env.set("y", 1.5); env.set("x", 2.65);

        AST ast = { mod, Ref(env.addr_of("y")),
                    digammab, -1.5 };
        ASSERT_FLOAT_EQ(eval_ast(env, ast), 0.09368671870951362);
        env.set("x", 9.0);
        ASSERT_FLOAT_EQ(eval_ast(env, ast), 0.09368671870951362);
        env.set("y", 10.0);
        ASSERT_FLOAT_EQ(eval_ast(env, ast), 0.15580703096659532);
    }
    {
        AST ast = { polygammab, 2., 5. };
        ASSERT_FLOAT_EQ(eval_ast(dummy_env, ast), -0.0487897322451144967254);
    }

    {
        AST ast = { land, lxor, 0.0, 1.0, 0.0 };
        ASSERT_FLOAT_EQ(eval_ast(dummy_env, ast), 0.0);
        ast.pop_back(); ast.pop_back(); ast.pop_back();
        ast.push_back(1.0);
    }

    {
        AST ast; ast.push_back(bnz);
        ThunkManager thunk(ast);
        ast.push_back(1.0);
        thunk.begin(); ast.push_back(2.0); thunk.end();
        thunk.begin(); ast.push_back(3.0); thunk.end();
        ASSERT_FLOAT_EQ(eval_ast(dummy_env, ast), 2.0);
        ast.clear(); ast.push_back(bnz);
        ast.push_back(0.0);
        thunk.begin(); ast.push_back(2.0); thunk.end();
        thunk.begin(); ast.push_back(3.0); thunk.end();
        ASSERT_FLOAT_EQ(eval_ast(dummy_env, ast), 3.0);
    }

    {
        Environment env; env.set("y", 1.5); env.set("x", 0.0);
        AST ast = { bnz, ge, Ref(0), 0.0 };
        ThunkManager thunk(ast);

        thunk.begin();
            ast.insert(ast.end(), {bnz, le, Ref(1), 0.});
            thunk.begin(); ast.push_back(-1.); thunk.end();
            thunk.begin(); ast.insert(ast.end(), {absb, -9.}); thunk.end();
        thunk.end();
        thunk.begin(); ast.emplace_back(ref, 0); thunk.end();

        ASSERT_FLOAT_EQ(eval_ast(env, ast), -1.0);
        env.set("x", 1.5);
        ASSERT_FLOAT_EQ(eval_ast(env, ast), 9.0);
        env.set("y", -9999.312);
        ASSERT_FLOAT_EQ(eval_ast(env, ast), -9999.312);
    }

    {
        Environment env; env.set("i", 0);
        AST ast = { SumOver(0), -1., 100.};
        ThunkManager thunk(ast);
        thunk.begin();
        ast.insert(ast.end(), {mul, Ref(0), -2.});
        thunk.end();

        ASSERT_FLOAT_EQ(eval_ast(env, ast), -10098.);
    }

    {
        Environment env; env.addr_of("j", false);
        AST ast = { ProdOver(0), 3., 7. };
        ThunkManager thunk(ast);
        thunk.begin();
        ast.insert(ast.end(), {unaryminus, Ref(0)});
        thunk.end();

        ASSERT_FLOAT_EQ(eval_ast(env, ast), -2520.);
    }

    {
        Environment env; env.set("j", 2.0); env.addr_of("k", false);
        env.set("lb", 2.0);
        AST ast = { ProdOver(1), 5., Ref(2) };
        ThunkManager thunk(ast);
        thunk.begin();
            ast.insert(ast.end(), {SumOver(0), 2., 6.} );
            thunk.begin();
                ast.insert(ast.end(), { add, add, Ref(0), Ref(1), Ref(2) } );
            thunk.end();
        thunk.end();
        ASSERT_FLOAT_EQ(eval_ast(env, ast), 4950000.);
    }
    END_TEST;
}
