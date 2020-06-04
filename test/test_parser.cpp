#include "parser.hpp"
#include "opcodes.hpp"
#include "util.hpp"
#include "test_common.hpp"

using namespace nivalis;
using namespace nivalis::test;

namespace {
void push_ref(AST& ast, uint64_t refid) {
    ast.emplace_back(OpCode::ref, refid);
}
void push_op(AST& ast, uint32_t op) {
    ast.push_back(op);
}
}  // namespace

int main() {
    BEGIN_TEST(test_parser);
    using namespace OpCode;
    std::string err;
    Environment dummy_env;
    ASSERT_EQ(parse("nan", dummy_env, true, false, 0, &err).ast, AST(1));
        // AST(1) has only null
    ASSERT(err.empty());
    // Syntax error
    ASSERT_EQ(parse("()()", dummy_env, true, true, 0, &err).ast, AST(1));
    ASSERT_EQ(err.substr(0,6), "Syntax");
    err.clear();
    // Numeric parsing error
    ASSERT_EQ(parse("2x+(3)", dummy_env, true, true, 0, &err).ast, AST(1));
    ASSERT_EQ(err.substr(0,7), "Numeric");
    err.clear();
    // Undefined variable
    ASSERT_EQ(parse("2*x", dummy_env, true, true, 0, &err).ast, AST(1));
    ASSERT_EQ(err.substr(0,21), "\"x\" is not a variable");
    err.clear();
    {
        Environment env_tmp;
        ASSERT_EQ(parse("(3)*x", dummy_env, false, false, 0, &err).ast,
                AST({ mul, 3., Ref(0) }));
        ASSERT(err.empty());
    }
    ASSERT_EQ(parse("2+++e", dummy_env).ast,
            AST({ add, 2., M_E }));
    ASSERT_EQ(parse("2--+-+e", dummy_env).ast,
            AST({ sub, 2., unaryminus, unaryminus, M_E }));

    ASSERT_EQ(parse("pi*-2", dummy_env).ast,
            AST({ mul, M_PI, unaryminus, 2. }));
    ASSERT(err.empty());

    ASSERT_EQ(parse("1+2-3", dummy_env).ast,
        AST({ sub, add, 1., 2., 3. }));
    {
        AST ast = { add, 1., sub, 4., 3. };
        ASSERT_EQ(parse("1+(4-3)", dummy_env).ast, ast);
        ASSERT_EQ(parse("1+[4-3]", dummy_env).ast, ast);
    }
    ASSERT_EQ(parse("1/(6%4)*3", dummy_env).ast,
            AST({ mul, divi, 1., mod, 6., 4., 3. }));
    ASSERT_EQ(parse("3.4*33.^1.3e4^.14", dummy_env).ast,
            AST({ mul, 3.4, power, 33., power, 13000., 0.14 }));
    {
        Environment env; env.addr_of("x", false);
        AST ast = { mul, mul, 2., sinb, Ref(env.addr_of("x")),
                    cosb, Ref(env.addr_of("x")) };
        ASSERT_EQ(parse("2*sin  (x)*cos (x)", env).ast, ast);
    }
    {
        Environment env; env.addr_of("yy", false);
        ASSERT_EQ(parse("fact(yy)^(-2)", env).ast,
                AST({ power, tgammab, add, 1., Ref(0), unaryminus, 2. }));
        ASSERT(err.empty());
    }
    {
        Environment env; env.addr_of("yy", false);
        ASSERT_EQ(parse("yy!^(-2)", env).ast,
                AST({ power, tgammab, add, 1., Ref(0), unaryminus, 2. }));
        ASSERT(err.empty());
    }

    {
        Environment env; env.addr_of("yy", false);
        env.addr_of("x", false);

        AST ast = { bnz, gt, Ref(1), 0. };
        ThunkManager thunk(ast);
        thunk.begin();
            ast.insert(ast.end(), { bnz, le, Ref(0),
                    unaryminus, 1.5});
            thunk.begin();
                ast.insert(ast.end(), { absb, Ref(1) });
            thunk.end();
            thunk.begin(); ast.push_back(null); thunk.end();
        thunk.end();
        thunk.begin();
        ast.push_back(M_PI);
        thunk.end();

        ASSERT_EQ(parse("{x>0: {yy<=-1.5: abs(x)}, pi}", env).ast, ast);
        ASSERT(err.empty());
    }

    {
        Environment env; env.addr_of("a", false);
        env.addr_of("x", false);

        AST ast = { SumOver(1), unaryminus, 10., Ref(0) };
        ThunkManager thunk(ast); thunk.begin();
        ast.insert(ast.end(), { mul, Ref(0), log2b, Ref(1)});
        thunk.end();
        ASSERT_EQ(parse("sum(x=-10.,a)[a*log2(x)]", env).ast, ast);
        ASSERT(err.empty());
    }

    {
        Environment env; env.addr_of("a", false);
        env.addr_of("x", false);

        AST ast = { sub, add, 3., SumOver(1), 5.5, Ref(0) };
        ThunkManager thunk(ast); thunk.begin();
        ast.insert(ast.end(), { mul, mul, Ref(1), 3., power, Ref(1), 2.});
        thunk.end();
        ast.insert(ast.end(), {9.});
        ASSERT_EQ(parse("3+sum(x=5.5,a)x*3*x^2-9", env).ast, ast);
        ASSERT(err.empty());
    }

    {
        Environment env; env.addr_of("a", false);
        env.addr_of("x", false);

        AST ast = { ProdOver(1), add, 19., unaryminus, 5.5, Ref(0) };
        ThunkManager thunk(ast); thunk.begin();
        ast.insert(ast.end(),  {mul, Ref(0), sqrtb,
            Ref(1)});
        thunk.end();
        ASSERT_EQ(parse("prod(x=19.+-+5.5,a)[a*sqrt(x)]", env).ast, ast);
        ASSERT(err.empty());
    }

    {
        Environment env; env.addr_of("a", false);
        env.addr_of("x", false);

        AST ast = { mul, divi, 1., mul, log(2), Ref(1), Ref(0) };

        ASSERT_EQ(parse("diff(x)[a*log(x, 2)]", env).ast, ast);
        ASSERT(err.empty());
    }

    {
        Environment env; env.addr_of("a", false);
        env.addr_of("x", false);

        AST ast = { add, add, unaryminus, 1., mul, 2., mul, 2., Ref(1), 1. };

        ASSERT_EQ(parse("-1+2*diff(x)x^2+1", env).ast, ast);
        ASSERT(err.empty());
    }

    {
        Environment env; env.addr_of("a", false);
        env.addr_of("x", false);

        AST ast = { add, sub, 0.9, sinb, mul, Ref(1), Ref(1), 23.4 };

        ASSERT_EQ(parse(".9-sin x*x+23.4", env).ast, ast);
        ASSERT(err.empty());
    }
    {
        Environment env; env.addr_of("a", false);
        env.addr_of("x", false);

        AST ast = { mul, 2., tanb, power, Ref(1), power, Ref(0), 2. };

        ASSERT_EQ(parse("2*tan x^a^2", env).ast, ast);
        ASSERT(err.empty());
    }

    {
        Environment env; env.addr_of("a", false);
        env.addr_of("x", false);
        AST ast = { unaryminus, power, 3., unaryminus, 2. };
        ASSERT_EQ(parse("-3^-2", env).ast, ast);
        ASSERT(err.empty());
    }

    {
        Environment env; env.addr_of("a", false);
        env.addr_of("x", false);
        AST ast = { lor, land, unaryminus, 1., 0., eq, 1., 1. };
        ASSERT_EQ(parse("-1&0|1==1", env).ast, ast);
        ASSERT(err.empty());
    }

    {
        AST ast = { ne, gcd, 1., 2., lcm, 3., 4. };
        ASSERT_EQ(parse("gcd(1,2)!= lcm(3,4)", dummy_env).ast, ast);
        ASSERT(err.empty());
    }

    {
        AST ast = { lxor, choose, 9., 2., floorb, 9.5 };
        ASSERT_EQ(parse("xor(choose(9.,2.), floor(9.5))", dummy_env).ast, ast);
        ASSERT(err.empty());
    }

    END_TEST;
}
