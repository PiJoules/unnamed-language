#include "lang.h"

/****************** Lexer tokens *****************/

std::unordered_map<std::string, std::string> RESERVED_NAMES = {
    {"def", "DEF"},
};

lang::LexToken reserved_name(lang::Lexer* lexer, lang::LexToken tok){
    assert(lexer);
    if (RESERVED_NAMES.find(tok.value) == RESERVED_NAMES.end()){
        return tok;
    }
    tok.symbol = RESERVED_NAMES[tok.value];
    return tok;
}

const lang::tokens_map_t lang::LANG_TOKENS = {
    // Values
    {"INT", {R"(\d+)", nullptr}},
    {"NAME", {R"([a-zA-Z_][a-zA-Z0-9_]*)", reserved_name}},

    // Binary operators
    {"ADD", {R"(\+)", nullptr}},
    {"SUB", {R"(-)", nullptr}},
    {"MUL", {R"(\*)", nullptr}},
    {"DIV", {R"(\\)", nullptr}},

    // Containers 
    {"LPAR", {R"(\()", nullptr}},
    {"RPAR", {R"(\))", nullptr}},

    // Misc 
    {"DEF", {R"(def)", nullptr}},
    {"NEWLINE", {R"(\n+)", nullptr}},
    {"COLON", {R"(\:)", nullptr}},
    {lang::tokens::INDENT, {lang::tokens::INDENT, nullptr}},
    {lang::tokens::DEDENT, {lang::tokens::DEDENT, nullptr}},
};

/********** Parser rules ***************/ 

template<typename Target>
struct NodeCast {
    Target* operator()(lang::Node* node) const {
        return reinterpret_cast<Target*>(node);
    }
};

// module : module_stmt_list
void parse_module(std::vector<lang::Node*>& args){
    //std::vector<lang::ModuleStmt*> module_stmt_args;
    //module_stmt_args.resize(args.size()-1);
    //std::transform(args.begin()+1, args.end(), module_stmt_args.begin(), NodeCast<lang::ModuleStmt>());
    std::vector<lang::ModuleStmt*>* stmt_list = reinterpret_cast<std::vector<lang::ModuleStmt*>>(args[1]);

    delete args[0];
    args[0] = new lang::Module(stmt_list);
}

// module_stmt_list : module_stmt 
void parse_module_stmt_list(std::vector<lang::Node*>& args){
    std::vector<lang::ModuleStmt*>* stmt_list = new std::vector<lang::ModuleStmt*>;
    stmt_list->push_back(reinterpret_cast<lang::ModuleStmt*>(args[1]));

    delete args[0];
    args[0] = stmt_list;
}

const std::vector<lang::prod_rule_t> lang::LANG_RULES = {
    // Entry point
    lang::make_pr("module", {"module_stmt_list"}),
    lang::make_pr("module_stmt_list", {"module_stmt"}),
    lang::make_pr("module_stmt_list", {"module_stmt_list", "module_stmt"}),
    lang::make_pr("module_stmt", {"func_def"}),
    lang::make_pr("module_stmt", {"NEWLINE"}),

    // Functions 
    lang::make_pr("func_def", {"DEF", "NAME", "LPAR", "RPAR", "COLON", "func_suite"}),
    lang::make_pr("func_suite", {"NEWLINE", lang::tokens::INDENT, "func_stmts", lang::tokens::DEDENT}),
    lang::make_pr("func_stmts", {"func_stmt"}),
    lang::make_pr("func_stmts", {"func_stmts", "func_stmt"}),
    lang::make_pr("func_stmt", {"simple_func_stmt", "NEWLINE"}),
    //lang::make_pr({"func_stmt", {"compound_func_stmt", lang::tokens::NEWLINE}),
    lang::make_pr("simple_func_stmt", {"expr_stmt"}),

    // Simple statements - one line 
    lang::make_pr("expr_stmt", {"expr"}),

    // Binary Expressions
    lang::make_pr("expr", {"expr", "SUB", "expr"}),
    lang::make_pr("expr", {"expr", "ADD", "expr"}),
    lang::make_pr("expr", {"expr", "MUL", "expr"}),
    lang::make_pr("expr", {"expr", "DIV", "expr"}),

    // Atoms
    lang::make_pr("expr", {"NAME"}),
    lang::make_pr("expr", {"INT"}),
};

/**************** Associativity ***************/ 

const lang::precedence_t lang::LANG_PRECEDENCE = {
    {lang::LEFT_ASSOC, {"ADD", "SUB"}},
    {lang::RIGHT_ASSOC, {"MUL", "DIV"}},
};
