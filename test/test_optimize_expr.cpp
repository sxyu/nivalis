#include "optimize_expr.hpp"
#include "parser.hpp"
#include "expr.hpp"
#include "test_common.hpp"
// This test file assumes parser, expr works
// it is quite high-level; the goal is to assure
// the optimizer does not make the expression incorrect

using namespace nivalis;
namespace {
    Parser parse;
    std::default_random_engine reng;

    // Automatically test if optimization leaves expression intact
    bool test_optim_equiv_random(const std::string& str,
            uint32_t var_id,
            double xmin = -100, double xmax = 100) {
        Environment env;
        env.addr_of("x", false);
        env.set("a", 10.);
        Expr expr = parse(str, env);
        Expr orig = expr;
        expr.optimize();
        int cnt = 0;
        static const int N_ITER = 1000;
        std::uniform_real_distribution<double> unif(xmin, xmax);
        for (int i = 0; i < N_ITER; ++i) {
            double x = unif(reng);
            env.vars[var_id] = x;
            double fx = expr(env);
            double ofx = orig(env);
            if (std::isnan(fx) && std::isnan(ofx)) ++cnt;
            else cnt += (absrelerr(fx, ofx) < FLOAT_EPS);
            if (absrelerr(fx, ofx) > FLOAT_EPS) {
                std::cerr << x << ":"<<fx << "," << ofx << "\n";
            }
        }
        if (cnt != N_ITER) {
            std::cerr << "Optimization equiv test fail\nopti " << expr <<
                "\norig " << orig << "\n";
            return false;
        }
        return true;
    }
}  // namespace

int main() {
    BEGIN_TEST(test_optimize_expr);

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
    ASSERT(test_optim_equiv_random("9*log(x^2,2)*ln(x)", 0, 0.0, 200.0));
    ASSERT(test_optim_equiv_random("3%2%3%2%9%2%x", 0));
    ASSERT(test_optim_equiv_random("-fact(x+1)^1", 0));
    ASSERT(test_optim_equiv_random("{x<32.5: 9, 4}", 0));
    ASSERT(test_optim_equiv_random("{x<32.5: x, x}", 0));
    ASSERT(test_optim_equiv_random("{x<0: {x<=3: 5}, 4}", 0));
    ASSERT(test_optim_equiv_random("{x<0: abs(x)}", 0));
    ASSERT(test_optim_equiv_random("max(x,min(x^2,x))", 0));
    ASSERT(test_optim_equiv_random("sum(x:0,a)[a*x^2]", 0));
    ASSERT(test_optim_equiv_random("prod(x:1,3)[a*x^2]", 0));
    ASSERT(test_optim_equiv_random("prod(x:1,3)[{x<2:a*x^2,3}]", 0));

    ASSERT(test_optim_equiv_random("10*(sin(x)^2 + cos(x)^2)", 0));

    END_TEST;
}
