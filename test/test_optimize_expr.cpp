#include "expr.hpp"
#include "parser.hpp"
#include "test_common.hpp"
#include "util.hpp"
// This test file assumes parser, expr works
// it is quite high-level; the goal is to assure
// the optimizer does not make the expression incorrect

using namespace nivalis;
using namespace nivalis::test;
namespace {
    Environment env;

    // Automatically test if optimization leaves expression intact
    bool test_optim_equiv_random(const std::string& str,
            uint32_t var_id,
            double xmin = -100, double xmax = 100) {
        env.set("a", 10.);
        std::string err;
        Expr expr = parse(str, env, false, true, 0, &err);
        util::trim(err);
        if (err.size()) {
            std::cout << "Provided expression failed to parse: " << err << "\n";
            return false;
        }
        Expr orig = expr;
        expr.optimize();
        int cnt = 0;
        static const int N_ITER = 1000;
        std::uniform_real_distribution<double> unif(xmin, xmax);
        for (int i = 0; i < N_ITER; ++i) {
            double x = unif(test::reng);
            env.vars[var_id] = x;
            double fx = expr(env);
            double ofx = orig(env);
            if (std::isnan(fx) && std::isnan(ofx)) ++cnt;
            else cnt += (absrelerr(fx, ofx) < FLOAT_EPS);
        }
        if (cnt != N_ITER) {
            std::cerr << "Optimization equiv test fail\nopti " << expr <<
                "\norig " << orig << "\n";
            return false;
        }
        return true;
    }
    Expr optim(const std::string& expr_str) {
        Expr expr = parse(expr_str, env, false, true);
        expr.optimize();
        return expr;
    }
}  // namespace

int main() {
    BEGIN_TEST(test_optimize_expr);

    auto x = env.addr_of("x", false);

    // Correctness
    ASSERT(test_optim_equiv_random("nan", 0));

    ASSERT(test_optim_equiv_random("2*-x*x", 0));
    ASSERT(test_optim_equiv_random("2*(x*x)", 0));
    ASSERT(test_optim_equiv_random("-(x*x)", 0));
    ASSERT(test_optim_equiv_random("0*x*1", 0));
    ASSERT(test_optim_equiv_random("+-1*(--+--x)", 0));
    ASSERT(test_optim_equiv_random("0+1*x/1", 0));
    ASSERT(test_optim_equiv_random("x+x", 0));
    ASSERT(test_optim_equiv_random("x-x", 0));
    ASSERT(test_optim_equiv_random("-x+2*x-1*x-0*x", 0));
    ASSERT(test_optim_equiv_random("x+2*x", 0));
    ASSERT(test_optim_equiv_random("9.5*-2*2*x/2*3*101", 0));
    ASSERT(test_optim_equiv_random("-2*x+3*x", 0));
    ASSERT(test_optim_equiv_random("3.2/(x+3)*x-3*x", 0));

    ASSERT(test_optim_equiv_random("-29*a+3*a-5*a*x", 1));
    ASSERT(test_optim_equiv_random("-29*a+3*a-5*a", 1));
    ASSERT(test_optim_equiv_random("-a^2+a^1-a^0", 1));
    ASSERT(test_optim_equiv_random("31*x^1+3*x-5e2*x", 0));
    ASSERT(test_optim_equiv_random("-29*pi/2*x^0/2+3.5", 0));
    ASSERT(test_optim_equiv_random("-x^2*N(x)", 0));
    ASSERT(test_optim_equiv_random("abs(abs(abs(x)^2))", 0));
    ASSERT(test_optim_equiv_random("e*x/x+x-x*x*x-x/2", 0));
    ASSERT(test_optim_equiv_random("3^x-3^(x*x)", 0, -10.0, 10.0));
    ASSERT(test_optim_equiv_random("e^x-e^x^0.5", 0, -10.0, 10.0));
    ASSERT(test_optim_equiv_random("2^x-2^(x*x)", 0, -5.0, 10.0));
    ASSERT(test_optim_equiv_random("2*exp2(x^2)-exp2(x*x)", 0, -5.0, 8.0));
    ASSERT(test_optim_equiv_random("exp(2*x)+exp(x)", 0, -10.0, 100.0));
    ASSERT(test_optim_equiv_random("3*(exp(2*x)-exp(3*x))*2",
                0, -10.0, 100.0));
    ASSERT(test_optim_equiv_random("9*log(x^2,2)*ln(x)", 0, 1e-10, 200.0));
    ASSERT(test_optim_equiv_random("3%2%3%2%9%2%x", 0));
    ASSERT(test_optim_equiv_random("-fact(x+1)^1", 0));
    ASSERT(test_optim_equiv_random("{x<32.5: 9, 4}", 0));
    ASSERT(test_optim_equiv_random("{x<32.5: x, x}", 0));
    ASSERT(test_optim_equiv_random("{1: x, x^2}", 0));
    ASSERT(test_optim_equiv_random("{0: x, 2*x^2}", 0));
    ASSERT(test_optim_equiv_random("{x<3: {x<4:3}, {x<4:3}}", 0));
    ASSERT(test_optim_equiv_random("{x<0: {x<=3: 5}, 4}", 0));
    ASSERT(test_optim_equiv_random("{x<0: abs(x)}", 0));
    ASSERT(test_optim_equiv_random("max(x,min(x^2,x))", 0));
    ASSERT(test_optim_equiv_random("sum(x=0,a)[a*x^2]", 0));
    ASSERT(test_optim_equiv_random("prod(x=1,3)[a*x^2]", 0));
    ASSERT(test_optim_equiv_random("prod(x=1,3)[{x<2:a*x^2,3}]", 0));

    ASSERT(test_optim_equiv_random("10*(sin(x)^2 + cos(x)^2)", 0));
    ASSERT(test_optim_equiv_random("1/(2/x)", 0));
    ASSERT(test_optim_equiv_random("3+1/(1/x)", 0));
    ASSERT(test_optim_equiv_random("exp(log(x))", 0, 1e-10, 10));
    ASSERT(test_optim_equiv_random("2*pi^(ln(x))", 0, 1e-10, 10));
    ASSERT(test_optim_equiv_random("log2(exp2(x))", 0, -10, 10));
    ASSERT(test_optim_equiv_random("log2(x^a)", 0, -10, 10));
    ASSERT(test_optim_equiv_random("exp(ln(abs(x)))", 0, -10, 10));
    ASSERT(test_optim_equiv_random("3^(log(x,3))", 0, -10., 10));
    ASSERT(test_optim_equiv_random("exp(log(2^x))", 0, -10., 10));
    ASSERT(test_optim_equiv_random("2*2^(1+x)", 0, -10., 10));
    ASSERT(test_optim_equiv_random("2*exp2(1+2*x)", 0, -10., 10));
    ASSERT(test_optim_equiv_random("2^(diff(x)x^2)", 0, -10., 10));

    using namespace OpCode;
    AST ast_zero = {0.}, ast_one = {1.};
    // Basic competence
    ASSERT_EQ(optim("x+x").ast, AST({mul, 2., Ref(x)}));
    ASSERT_EQ(optim("exp(x)+3*exp(x)").ast, AST({mul, 4., expb, Ref(x)}));
    ASSERT_EQ(optim("2*x-x").ast, AST({Ref(x)}));
    ASSERT_EQ(optim("3*x^2-2*x^2-x^2").ast, ast_zero);
    ASSERT_EQ(optim("x*-3*x*x*2*x").ast, AST({mul, -6., power, Ref(x), 4.}));
    ASSERT_EQ(optim("sin(x)*sin(x) + cos(x)*cos(x)").ast, ast_one);
    ASSERT_EQ(optim("---x").ast, AST({unaryminus, Ref(x)}));
    ASSERT_EQ(optim("----x^2").ast, AST({sqrb, Ref(x)}));
    ASSERT_EQ(optim("3*3/4*x/3*4").ast, AST({mul, 3., Ref(x)}));
    ASSERT_EQ(optim("x^2/x").ast, AST({Ref(x)}));
    ASSERT_EQ(optim("x^1.5*x^0.2").ast, AST({power, Ref(x), 1.7}));
    ASSERT_EQ(optim("2^x").ast, AST({exp2b, Ref(x)}));
    ASSERT_EQ(optim("log(x,10)").ast, AST({log10b, Ref(x)}));
    ASSERT_EQ(optim("exp(x)*exp(2*x)").ast, AST({expb, mul, 3., Ref(x)}));
    ASSERT_EQ(optim("exp(x)/exp(2*x)").ast, AST({expb, unaryminus, Ref(x)}));
    ASSERT_EQ(optim("exp(log(2^x))").ast, AST({exp2b, Ref(x)}));
    ASSERT_EQ(optim("log(exp(sgn(x)))").ast, AST({sgn, Ref(x)}));

    END_TEST;
}
