#include "diff_expr.hpp"
#include "parser.hpp"
#include "expr.hpp"
#include "test_common.hpp"
// This test file assumes parser, expr works
// it is quite high-level; the goal is to assure
// the derivative is correct

using namespace nivalis;
namespace {
    Parser parse;
    Environment env;
    std::default_random_engine reng;

    // Automatically test if derivative equals expected derivative
    bool test_derivative_random(
            const std::string& str,
            const std::string& derivative_str,
            uint32_t var_id,
            double xmin = -100, double xmax = 100) {
        Expr expr = parse(str, env);
        Expr expect = parse(derivative_str, env);
        Expr diff = expr.diff(var_id, env);
        int cnt = 0;
        static const int N_ITER = 1000;
        std::uniform_real_distribution<double> unif(xmin, xmax);
        for (int i = 0; i < N_ITER; ++i) {
            double x = unif(reng);
            env.vars[var_id] = x;
            double dfx = diff(env);
            double dfx_expect = expect(env);
            if (std::isnan(dfx) && std::isnan(dfx)) ++cnt;
            else cnt += (std::fabs(dfx - dfx_expect) < FLOAT_EPS);
        }
        if (cnt != N_ITER) {
            std::cerr << "Derivative equiv test fail\ndiff   " << diff <<
                "\nexpect " << expect << "\n";
            return false;
        }
        return true;
    }
}  // namespace

int main() {
    BEGIN_TEST(test_diff_expr);

    env.addr_of("x", false); env.set("a", 3.0);
    ASSERT(test_derivative_random("nan", "nan", 0));
    ASSERT(test_derivative_random("e", "0", 0));
    ASSERT(test_derivative_random("a*x", "a", 0));
    ASSERT(test_derivative_random("a^2*x^2", "2*a^2*x", 0));
    ASSERT(test_derivative_random("x^3", "3*x^2", 0));
    ASSERT(test_derivative_random("x^(-1)", "-1/x^2", 0));
    ASSERT(test_derivative_random("1/x", "-x^(-2)", 0));
    ASSERT(test_derivative_random("x/(1+x)", "1/(1+x)^2", 0));
    ASSERT(test_derivative_random("log(x)", "1/x", 0));
    ASSERT(test_derivative_random("log2(x)", "1/(x*ln(2))", 0));
    ASSERT(test_derivative_random("log(x,a)", "1/(x*ln(a))", 0));
    ASSERT(test_derivative_random("sin(x)", "cos(x)", 0));
    ASSERT(test_derivative_random("cos(x)", "-sin(x)", 0));
    ASSERT(test_derivative_random("tan(x)", "1/cos(x)^2", 0));
    ASSERT(test_derivative_random("asin(x)", "1/sqrt(1-x^2)", 0));
    ASSERT(test_derivative_random("acos(x)", "-1/sqrt(1-x^2)", 0));
    ASSERT(test_derivative_random("atan(x)", "1/(1+x^2)", 0));
    ASSERT(test_derivative_random("sinh(x)", "cosh(x)", 0));
    ASSERT(test_derivative_random("cosh(x)", "sinh(x)", 0));
    ASSERT(test_derivative_random("tanh(x)", "1-tanh(x)^2", 0));
    ASSERT(test_derivative_random("sin(x)*cos(x)", "cos(x)^2-sin(x)^2", 0));
    ASSERT(test_derivative_random("sin(x)^2", "2*sin(x)*cos(x)", 0));
    ASSERT(test_derivative_random("sin(cos(x))", "-cos(cos(x))*sin(x)", 0));
    ASSERT(test_derivative_random("exp(2*x)", "2*exp(2*x)", 0));
    ASSERT(test_derivative_random("exp(abs(x))", "exp(abs(x))*sgn(x)", 0));
    ASSERT(test_derivative_random("exp2(-x)", "log(2)*-1*exp2(-x)", 0));
    ASSERT(test_derivative_random("3^(-x)", "ln(3)*-1*3^(-x)", 0,
            -10.0, 10.0));
    env.set("a", 0.5);
    ASSERT(test_derivative_random("a^(-x)", "ln(a)*-1*a^(-x)", 0,
                -10.0, 10.0));
    ASSERT(test_derivative_random("abs(x)", "sgn(x)", 0));
    ASSERT(test_derivative_random("abs(x^2)", "2*x*(x!=0)", 0));
    ASSERT(test_derivative_random("sgn(x^2)", "0", 0));
    ASSERT(test_derivative_random("floor(x)", "0", 0));
    ASSERT(test_derivative_random("choose(x,x/2)", "0", 0));
    ASSERT(test_derivative_random("gamma(x)", "gamma(x)*digamma(x)", 0));
    ASSERT(test_derivative_random("fact(x)", "gamma(x+1)*digamma(x+1)", 0));
    ASSERT(test_derivative_random("N(x)", "-1/(sqrt(2*pi))*exp(-0.5*x^2)*x", 0,
                -2.0, 2.0));
    ASSERT(test_derivative_random("beta(x,x^2)",
                "beta(x,x^2)*(digamma(x) - digamma(x+x^2)) + "
                "2*x*beta(x,x^2)*(digamma(x^2) - digamma(x+x^2))", 0));

    END_TEST;
}
