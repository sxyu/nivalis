#include "opcodes.hpp"

#include <cmath>

namespace nivalis {
namespace OpCode {

// Add the operator here if it is binary
size_t n_args(uint32_t opcode) {
    using namespace nivalis::OpCode;
    switch (opcode) {
        case OpCode::null:
        case val:
        case ref:
        case thunk_jmp:
        case arg:
            return 0;
        case bsel:
        case add:
        case sub:
        case mul:
        case divi:
        case power:
        case logbase:
        case max:
        case min:
        case land:
        case lor:
        case lxor:
        case lt:
        case le:
        case eq:
        case ne:
        case ge:
        case gt:
        case thunk_ret:
            return 2;
        case sums:
        case prods:
        case bnz:
            return 3;
            // case call: return -1;
    }
    return 1;
}

// Design format of operator expression here
const char* repr(uint32_t opcode) {
    using namespace OpCode;
    switch (opcode) {
        case OpCode::null:
            return "nan";
        case val:
            return "\v";
        case ref:
            return "\r";
        case sums:
            return "sum(\r, @, @)[@]";
        case prods:
            return "prod(\r, @, @)[@]";
        case bnz:
            return "{@: @, @}";
        case bsel:
            return "{0: @, @}";
        case add:
            return "(@ + @)";
        case sub:
            return "(@ - @)";
        case mul:
            return "(@ * @)";
        case divi:
            return "(@ / @)";
        case power:
            return "(@ ^ @)";
        case logbase:
            return "log(@, @)";
        case max:
            return "max(@, @)";
        case min:
            return "min(@, @)";
        case land:
            return "(@ & @)";
        case lor:
            return "(@ | @)";
        case lxor:
            return "xor(@, @)";
        case le:
            return "(@ <= @)";
        case lt:
            return "(@ < @)";
        case eq:
            return "(@ == @)";
        case ne:
            return "(@ != @)";
        case ge:
            return "(@ >= @)";
        case gt:
            return "(@ > @)";
        case unaryminus:
            return "(-@)";
        case lnot:
            return "not(@)";
        case absb:
            return "abs(@)";
        case sqrtb:
            return "sqrt(@)";
        case sqrb:
            return "@^2";
        case sgn:
            return "sgn(@)";
        case floorb:
            return "floor(@)";
        case ceilb:
            return "ceil(@)";
        case roundb:
            return "round(@)";
        case expb:
            return "exp(@)";
        case logb:
            return "ln(@)";
        case log10b:
            return "log10(@)";
        case sinb:
            return "sin(@)";
        case cosb:
            return "cos(@)";
        case tanb:
            return "tan(@)";
        case asinb:
            return "arcsin(@)";
        case acosb:
            return "arccos(@)";
        case atanb:
            return "arctan(@)";
        case sinhb:
            return "sinh(@)";
        case coshb:
            return "cosh(@)";
        case tanhb:
            return "tanh(@)";
        case thunk_jmp:
            return "";
        case thunk_ret:
            return "@@";
        case arg:
            return "$";
        case call:
            return "\t(@)";
        case conjb:
            return "conj(@)";
        case realb:
            return "re(@)";
        case imagb:
            return "im(@)";
        case argb:
            return "arg(@)";
        default:
            return "";
    };
}

const char* latex_repr(uint32_t opcode) {
    using namespace OpCode;
    switch (opcode) {
        case OpCode::null:
            return "nan";
        case val:
            return "\v";
        case ref:
            return "\r";
        case sums:
            return "\\sum_{\r=@}^{@}\\left(@\\right)";
        case prods:
            return "\\prod_{\r=@}^{@}\\left(@\\right)";
        case bnz:
            return "\\left\\{@:@,@\\right\\}";
        case bsel:
            return "\\left\\{0:@,@\\right\\}";
        case add:
            return "\\left(@+@\\right)";
        case sub:
            return "\\left(@-@\\right)";
        case mul:
            return "\\left(@\\cdot @\\right)";
        case divi:
            return "\\frac{@}{@}";
        case power:
            return "@^{@}";
        case logbase:
            return "\\log\\left(@, @\\right)";
        case max:
            return "\\max\\left(@, @\\right)";
        case min:
            return "\\min\\left(@, @\\right)";
        case land:
            return "\\left(@\\wedge @\\right)";
        case lor:
            return "\\left(@\\vee @\\right)";
        case lxor:
            return "xor\\left(@, @\\right)";
        case lt:
            return "\\left(@ < @\\right)";
        case le:
            return "\\left(@ <= @\\right)";
        case eq:
            return "\\left(@ = @\\right)";
        case ne:
            return "\\left(@ \\ne @\\right)";
        case gt:
            return "\\left(@ > @\\right)";
        case ge:
            return "\\left(@ >= @\\right)";
        case unaryminus:
            return "(-@)";
        case lnot:
            return "\\not\\left(@\\right)";
        case absb:
            return "\\left|@\\right|";
        case sqrtb:
            return "\\sqrt\\left{@\\right}";
        case sqrb:
            return "@^{2}";
        case sgn:
            return "\\sgn\\left(@\\right)";
        case floorb:
            return "\\floor\\left(@\\right)";
        case ceilb:
            return "\\ceil\\left(@\\right)";
        case roundb:
            return "\\round\\left(@\\right)";
        case expb:
            return "\\exp\\left(@\\right)";
        case logb:
            return "\\ln\\left(@\\right)";
        case log10b:
            return "\\log_{10}\\left(@\\right)";
        case sinb:
            return "\\sin\\left(@\\right)";
        case cosb:
            return "\\cos\\left(@\\right)";
        case tanb:
            return "\\tan\\left(@\\right)";
        case asinb:
            return "\\arcsin\\left(@\\right)";
        case acosb:
            return "\\arccos\\left(@\\right)";
        case atanb:
            return "\\arctan\\left(@\\right)";
        case sinhb:
            return "\\sinh\\left(@\\right)";
        case coshb:
            return "\\cosh\\left(@\\right)";
        case tanhb:
            return "\\tanh\\left(@\\right)";
        case conjb:
            return "\\conj\\left(@\\right)";
        case realb:
            return "\\re\\left(@\\right)";
        case imagb:
            return "\\im\\left(@\\right)";
        case argb:
            return "\\arg\\left(@\\right)";
        case thunk_jmp:
            return "";
        case thunk_ret:
            return "@@";
        case arg:
            return "$";
        case call:
            return "\t(@)";
        default:
            return "";
    };
}

// Used only for operator assignment (not important)
uint32_t from_char(char opchar) {
    switch (opchar) {
        case '+':
            return OpCode::add;
        case '-':
            return OpCode::sub;
        case '*':
            return OpCode::mul;
        case '/':
            return OpCode::divi;
        case '^':
            return OpCode::power;
        case '=':
            return OpCode::eq;
        case '<':
            return OpCode::lt;
        case '>':
            return OpCode::gt;
        case '&':
            return OpCode::land;
        case '|':
            return OpCode::lor;
        default:
            return OpCode::bsel;
    };
}

// Inverse of above
char to_char(uint32_t opcode) {
    switch (opcode) {
        case OpCode::add:
            return '+';
        case OpCode::sub:
            return '-';
        case OpCode::mul:
            return '*';
        case OpCode::divi:
            return '/';
        case OpCode::power:
            return '^';
        case OpCode::eq:
            return '=';
        case OpCode::lt:
            return '<';
        case OpCode::gt:
            return '>';
        case OpCode::land:
            return '&';
        case OpCode::lor:
            return '|';
        default:
            return 0;
    };
}

// Link parser functions to OpCodes here
const std::map<std::string, uint32_t, std::less<>>& funcname_to_opcode_map() {
    static std::map<std::string, uint32_t, std::less<>> func_opcodes = {
        {"pow", OpCode::power},    {"log", OpCode::logbase},
        {"max", OpCode::max},      {"min", OpCode::min},
        {"xor", OpCode::lxor},     {"not", OpCode::lnot},
        {"and", OpCode::land},     {"or", OpCode::lor},

        {"abs", OpCode::absb},     {"sqrt", OpCode::sqrtb},
        {"sgn", OpCode::sgn},      {"floor", OpCode::floorb},
        {"ceil", OpCode::ceilb},   {"round", OpCode::roundb},

        {"exp", OpCode::expb},     {"ln", OpCode::logb},
        {"log10", OpCode::log10b},

        {"sin", OpCode::sinb},     {"cos", OpCode::cosb},
        {"tan", OpCode::tanb},     {"arcsin", OpCode::asinb},
        {"arccos", OpCode::acosb}, {"arctan", OpCode::atanb},

        {"sinh", OpCode::sinhb},   {"cosh", OpCode::coshb},
        {"tanh", OpCode::tanhb},

        {"conj", OpCode::conjb},   {"re", OpCode::realb},
        {"im", OpCode::imagb},     {"Re", OpCode::realb},
        {"Im", OpCode::imagb},     {"arg", OpCode::argb},
    };
    return func_opcodes;
}

// Define constants here
const std::map<std::string, complex>& constant_value_map() {
    static std::map<std::string, complex> constant_values;
    if (constant_values.empty()) {
        // Set lookup tables
        constant_values["pi"] = M_PI;
        constant_values["e"] = M_E;
        constant_values["phi"] = 0.5 * (1. + sqrt(5));       // golden ratio
        constant_values["euler"] = 0.577215664901532860606;  // Euler-Mascheroni

        constant_values["nan"] = std::numeric_limits<double>::quiet_NaN();
        constant_values["i"] = complex(0.0, 1.0);
    }
    return constant_values;
}
}  // namespace OpCode
}  // namespace nivalis
