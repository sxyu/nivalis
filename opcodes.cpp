#include "opcodes.hpp"

#include<boost/math/constants/constants.hpp>
namespace nivalis {
namespace OpCode {

namespace {

// Add the operator here if it is binary
constexpr bool is_binary(uint32_t opcode) {
    using namespace nivalis::OpCode;
    switch(opcode) {
        case bsel: case add: case sub: case mul: case div: case mod:
        case power: case logbase: case max: case min:
        case land: case lor: case lxor:
        case gcd: case lcm:
        case choose: case fafact: case rifact: case beta: case polygamma:
        case lt: case le: case eq: case ne: case ge: case gt:
            return true;
    }
    return false;
}

}  // namespace

// Design format of operator expression here
const char* repr(uint32_t opcode) {
    using namespace OpCode;
    switch (opcode) {
        case OpCode::null:  return "nan";
        case val:   return "#";
        case ref:   return "&";
        case sums:     return "sum(&, @, @)[@]";
        case prods:    return "prod(&, @, @)[@]";
        case bnz:   return "{@: @, @}";
        case bsel:  return "bsel(@, @)";
        case add:   return "(@ + @)";
        case sub:   return "(@ - @)";
        case mul:   return "(@ * @)";
        case div:   return "(@ / @)";
        case mod:   return "(@ % @)";
        case power: return "(@ ^ @)";
        case logbase:  return "log(@, @)";
        case max:   return "max(@, @)";
        case min:   return "min(@, @)";
        case land:  return "and(@, @)";
        case lor:   return "or(@, @)";
        case lxor:  return "xor(@, @)";
        case lt:    return "(@ < @)";
        case le:    return "(@ <= @)";
        case eq:    return "(@ == @)";
        case ne:    return "(@ != @)";
        case ge:    return "(@ >= @)";
        case gt:    return "(@ > @)";
        case gcd:   return "gcd(@, @)";
        case lcm:   return "lcm(@, @)";
        case choose:    return "choose(@, @)";
        case fafact:    return "fafact(@, @)";
        case rifact:    return "rifact(@, @)";
        case beta:      return "beta(@, @)";
        case polygamma: return "polygamma(@, @)";
        case uminusb:   return "(-@)";
        case lnotb:     return "not(@)";
        case absb:      return "abs(@)";
        case sqrtb:     return "sqrt(@)";
        case sqrb:      return "@^2";
        case sgnb:      return "sgn(@)";
        case floorb:    return "floor(@)";
        case ceilb:     return "ceil(@)";
        case roundb:    return "round(@)";
        case expb:      return "exp(@)";
        case exp2b:     return "exp2(@)";
        case logb:      return "ln(@)";
        case log10b:    return "log10(@)";
        case log2b:     return "log2(@)";
        case factb:     return "fact(@)";
        case sinb:      return "sin(@)";
        case cosb:      return "cos(@)";
        case tanb:      return "tan(@)";
        case asinb:     return "asin(@)";
        case acosb:     return "acos(@)";
        case atanb:     return "atan(@)";
        case sinhb:     return "sinh(@)";
        case coshb:     return "cosh(@)";
        case tanhb:     return "tanh(@)";
        case gammab:    return "gamma(@)";
        case lgammab:   return "lgamma(@)";
        case digammab:  return "digamma(@)";
        case trigammab: return "trigamma(@)";
        case erfb:      return "erf(@)";
        case zetab:     return "zeta(@)";
        default: return "";
    };
}

// Typically don't need to change this
// (related to the above by removing all chars except @#&)
const char* subexpr_repr(uint32_t opcode) {
    using namespace OpCode;
    switch (opcode) {
        case OpCode::null:  return "";
        case val:   return "#";
        case ref:   return "&";
        case sums: case prods:
                    return "&@@@";
        case bnz:   return "@@@";
    };
    if (is_binary(opcode)) return "@@";
    else return "@";
}

// Used only for operator assignment (not important)
uint32_t from_char(char c) {
    switch (c) {
        case '+': return OpCode::add;
        case '-': return OpCode::sub;
        case '*': return OpCode::mul;
        case '/': return OpCode::div;
        case '%': return OpCode::mod;
        case '^': return OpCode::power;
        case '<': return OpCode::lt;
        case '>': return OpCode::gt;
        case '=': return OpCode::eq;
        default: return OpCode::bsel;
    };
}

// Link parser functions to OpCodes here
const std::map<std::string, uint32_t>& funcname_to_opcode_map() {
    static std::map<std::string, uint32_t> func_opcodes;
    if (func_opcodes.empty()) {
        func_opcodes["pow"] = OpCode::power;
        func_opcodes["log"] = OpCode::logbase;
        func_opcodes["max"] = OpCode::max;
        func_opcodes["min"] = OpCode::min;
        func_opcodes["and"] = OpCode::land;
        func_opcodes["or"] = OpCode::lor;
        func_opcodes["xor"] = OpCode::lxor;
        func_opcodes["not"] = OpCode::lnotb;

        func_opcodes["abs"] = OpCode::absb;
        func_opcodes["sqrt"] = OpCode::sqrtb;
        func_opcodes["sgn"] = OpCode::sgnb;
        func_opcodes["floor"] = OpCode::floorb;
        func_opcodes["ceil"] = OpCode::ceilb;
        func_opcodes["round"] = OpCode::roundb;

        func_opcodes["exp"] = OpCode::expb;
        func_opcodes["exp2"] = OpCode::exp2b;
        func_opcodes["ln"] = OpCode::logb;
        func_opcodes["log10"] = OpCode::log10b;
        func_opcodes["log2"] = OpCode::log2b;

        func_opcodes["sin"] = OpCode::sinb;
        func_opcodes["cos"] = OpCode::cosb;
        func_opcodes["tan"] = OpCode::tanb;
        func_opcodes["asin"] = OpCode::asinb;
        func_opcodes["acos"] = OpCode::acosb;
        func_opcodes["atan"] = OpCode::atanb;

        func_opcodes["sinh"] = OpCode::sinhb;
        func_opcodes["cosh"] = OpCode::coshb;
        func_opcodes["tanh"] = OpCode::tanhb;

        func_opcodes["gamma"] = OpCode::gammab;
        func_opcodes["fact"] = -1; // pseudo
        func_opcodes["ifact"] = OpCode::factb;
        func_opcodes["lgamma"] = OpCode::lgammab;
        func_opcodes["digamma"] = OpCode::digammab;
        func_opcodes["trigamma"] = OpCode::trigammab;
        func_opcodes["polygamma"] = OpCode::polygamma;
        func_opcodes["erf"] = OpCode::erfb;
        func_opcodes["zeta"] = OpCode::zetab;
        func_opcodes["beta"] = OpCode::beta;
        func_opcodes["gcd"] = OpCode::gcd;
        func_opcodes["lcm"] = OpCode::lcm;
        func_opcodes["choose"] = OpCode::choose;
        func_opcodes["fafact"] = OpCode::fafact;
        func_opcodes["rifact"] = OpCode::rifact;

        func_opcodes["bsel"] = OpCode::bsel;
    }
    return func_opcodes;
}

// Define constants here
const std::map<std::string, double>& constant_value_map() {
    static std::map<std::string, double> constant_values;
    if (constant_values.empty()){
        // Set lookup tables
        using namespace boost::math;
        constant_values["pi"] = double_constants::pi;
        constant_values["e"] = double_constants::e;
        constant_values["phi"] = double_constants::phi;
        constant_values["euler"] = double_constants::euler;

        constant_values["nan"] = std::numeric_limits<double>::quiet_NaN();
    }
    return constant_values;
}
}  // namespace OpCode
}  // namespace nivalis
