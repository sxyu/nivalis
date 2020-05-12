#include "parser.hpp"
#include "env.hpp"
#include "expr.hpp"
#include "opcodes.hpp"
#include "util.hpp"
#include "test_common.hpp"

using namespace nivalis;

namespace {
using AST = std::vector<uint32_t>;
using nivalis::util::push_dbl;
void push_ref(AST& ast, uint32_t refid) {
    ast.push_back(OpCode::ref);
    ast.push_back(refid);
}
void push_op(AST& ast, uint32_t op) {
    ast.push_back(op);
}
}  // namespace

int main() {
    BEGIN_TEST(test_parser);
    using namespace OpCode;
    Parser parser;
    Environment dummy_env;
    {
        AST ast = { null }; ASSERT_EQ(parser("nan", dummy_env).ast, ast);
        ASSERT(parser.error_msg.empty());
    }
    {
        AST ast = { null };
        // Syntax error
        ASSERT_EQ(parser("()()", dummy_env, true, true).ast, ast);
        ASSERT_EQ(parser.error_msg.substr(0,6), "Syntax");
    }
    {
        AST ast = { null };
        // Numeric parsing error
        ASSERT_EQ(parser("2x+(3)", dummy_env, true, true).ast, ast);
        ASSERT_EQ(parser.error_msg.substr(0,7), "Numeric");
    }
    {
        AST ast = { null };
        // Undefined variable
        ASSERT_EQ(parser("2*x", dummy_env, true, true).ast, ast);
        ASSERT_EQ(parser.error_msg.substr(0,9), "Undefined");
    }
    {
        Environment env_tmp;
        AST ast = { mul };
        push_dbl(ast, 3); push_ref(ast, 0);
        ASSERT_EQ(parser("(3)*x", dummy_env, false).ast, ast);
        ASSERT(parser.error_msg.empty());
    }
    {
        AST ast = { add };
        util::push_dbl(ast, 2);
        util::push_dbl(ast, M_E);
        ASSERT_EQ(parser("2+++e", dummy_env).ast, ast);
    }
    {
        AST ast = { sub };
        util::push_dbl(ast, 2);
        push_op(ast, unaryminus);
        push_op(ast, unaryminus);
        util::push_dbl(ast, M_E);
        ASSERT_EQ(parser("2--+-+e", dummy_env).ast, ast);
    }
    {
        AST ast = { mul };
        util::push_dbl(ast, M_PI);
        ast.push_back(unaryminus); util::push_dbl(ast, 2);
        ASSERT_EQ(parser("pi*-2", dummy_env).ast, ast);
        ASSERT(parser.error_msg.empty());
    }
    {
        AST ast = { sub, add };
        util::push_dbl(ast, 1); util::push_dbl(ast, 2); util::push_dbl(ast, 3);
        ASSERT_EQ(parser("1+2-3", dummy_env).ast, ast);
    }
    {
        AST ast = { add };
        util::push_dbl(ast, 1); 
        ast.push_back(sub);
        util::push_dbl(ast, 4); util::push_dbl(ast, 3);
        ASSERT_EQ(parser("1+(4-3)", dummy_env).ast, ast);
        ASSERT_EQ(parser("1+[4-3]", dummy_env).ast, ast);
    }
    {
        AST ast = { mul, OpCode::div };
        util::push_dbl(ast, 1);
        ast.push_back(mod);
        util::push_dbl(ast, 6); util::push_dbl(ast, 4);
        util::push_dbl(ast, 3);
        ASSERT_EQ(parser("1/(6%4)*3", dummy_env).ast, ast);
    }
    {
        AST ast = { mul };
        util::push_dbl(ast, 3.4);
        ast.push_back(power); util::push_dbl(ast, 33);
        ast.push_back(power); util::push_dbl(ast, 13000);
        util::push_dbl(ast, 0.14);
        ASSERT_EQ(parser("3.4*33.^1.3e4^.14", dummy_env).ast, ast);
    }
    {
        AST ast = { mul, mul };
        Environment env; env.addr_of("x", false);
        util::push_dbl(ast, 2);
        ast.push_back(sinb);
        push_ref(ast, env.addr_of("x"));
        ast.push_back(cosb);
        ast.push_back(ref); ast.push_back(env.addr_of("x"));
        ASSERT_EQ(parser("2*sin(x)*cos(x)", env).ast, ast);
    }
    {
        Environment env; env.addr_of("yy", false);

        AST ast = { power };
        ast.push_back(tgammab);
        ast.push_back(add);
        util::push_dbl(ast, 1.0);
        ast.push_back(ref); ast.push_back(0);
        ast.push_back(unaryminus);
        util::push_dbl(ast, 2.0);
        ASSERT_EQ(parser("fact(yy)^(-2)", env).ast, ast);
        ASSERT(parser.error_msg.empty());
    }

    {
        Environment env; env.addr_of("yy", false);
        env.addr_of("x", false);

        AST ast = { bnz, gt };
        push_ref(ast, 1); push_dbl(ast, 0);
        push_op(ast, bnz); push_op(ast, le);
        push_ref(ast, 0);
        push_op(ast, unaryminus); push_dbl(ast, 1.5);
        push_op(ast, absb); push_ref(ast, 1);
        push_op(ast, null); push_dbl(ast, M_PI);
        
        ASSERT_EQ(parser("{x>0: {yy<=-1.5: abs(x)}, pi}", env).ast, ast);
        ASSERT(parser.error_msg.empty());
    }

    {
        Environment env; env.addr_of("a", false);
        env.addr_of("x", false);

        AST ast = { sums };
        ast.push_back(1); // ref
        push_op(ast, unaryminus);
        push_dbl(ast, 10.); push_ref(ast, 0);
        push_op(ast, mul);
        push_ref(ast, 0);
        push_op(ast, log2b);
        push_ref(ast, 1);
        
        ASSERT_EQ(parser("sum(x:-10.,a)[a*log2(x)]", env).ast, ast);
        ASSERT(parser.error_msg.empty());
    }

    {
        Environment env; env.addr_of("a", false);
        env.addr_of("x", false);

        AST ast = { prods };
        ast.push_back(1); // ref
        push_op(ast, add);
        push_dbl(ast, 19.);
        push_op(ast, unaryminus);
        push_dbl(ast, 5.5);
        push_ref(ast, 0);
        push_op(ast, mul);
        push_ref(ast, 0);
        push_op(ast, sqrtb);
        push_ref(ast, 1);
        
        ASSERT_EQ(parser("prod(x:19.+-+5.5,a)[a*sqrt(x)]", env).ast, ast);
        ASSERT(parser.error_msg.empty());
    }

    {
        Environment env; env.addr_of("a", false);
        env.addr_of("x", false);

        AST ast = { mul, OpCode::div };
        push_dbl(ast, 1);
        push_op(ast, mul);
        push_dbl(ast, log(2));
        push_ref(ast, 1);
        push_ref(ast, 0);
        
        ASSERT_EQ(parser("diff(x)[a*log(x, 2)]", env).ast, ast);
        ASSERT(parser.error_msg.empty());
    }

    {
        Environment env; env.addr_of("a", false);
        env.addr_of("x", false);

        AST ast = { unaryminus, power };
        push_dbl(ast, 3);
        push_op(ast, unaryminus);
        push_dbl(ast, 2);
        
        ASSERT_EQ(parser("-3^-2", env).ast, ast);
        ASSERT(parser.error_msg.empty());
    }

    END_TEST;
}
