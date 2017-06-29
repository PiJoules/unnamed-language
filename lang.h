#ifndef _LANG_H
#define _LANG_H

#include <string>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <unordered_map>
#include <stack>
#include <utility>
#include <tuple>
#include <algorithm>
#include <unordered_set>
#include <regex>

#include <iostream>
#include <cassert>

// Borrowed from python hash
#define _HASH_MULTIPLIER 1000003

namespace lang {
    namespace tokens {
        const std::string NEWLINE = "\n";
        const std::string INDENT = "INDENT";
        const std::string DEDENT = "DEDENT";
        const std::string END = "END";
    }
    namespace nonterminals {
        const std::string EPSILON = "EMPTY";
    }

    typedef struct LexToken LexToken;
    struct LexToken {
        std::string symbol;
        std::string value;
        int pos, lineno, colno;
    };

    // Custom exceptions 
    class IndentationError: public std::runtime_error {
        private:
            int lineno_;

        public:
            IndentationError(int lineno): std::runtime_error("Indentation error"),
                lineno_(lineno){}
            virtual const char* what() const throw();
    };

    typedef std::unordered_map<std::string, std::string> tokens_map_t;

    class Lexer {
        private:
            std::string lexcode_;
            int pos_ = 1, lineno_ = 1, colno_ = 1;
            std::unordered_map<std::string, std::regex> tokens_;

            // Indentation tracking
            std::vector<int> levels = {1};
            bool found_indent = false, found_dedent = false;
            LexToken next_tok_ = {tokens::END, "", pos_, lineno_, colno_};
            void load_next_tok();
            LexToken make_indent() const;
            LexToken make_dedent() const;

        public:
            Lexer(const tokens_map_t&);
            void input(const std::string& code);
            LexToken token();
            const std::unordered_map<std::string, std::regex>& tokens() const;
            bool empty() const;
            void advance(int count=1);
            void advancenl(int count=1);
    };

    /****** Nodes ********/ 
    class Node {
        public:
            // The string representation of this node 
            // I would make it pure virtual, but that would require parser.cpp always
            // having to be compiled with lang_nodes.cpp in the Makefile.
            virtual std::string str() const { return ""; };
    };

    class LexTokenWrapper: public Node {
        private:
            LexToken token_;

        public:
            LexTokenWrapper(const LexToken&);
            LexTokenWrapper& operator=(const LexToken& other){
                token_ = other;
                return *this;
            }
            LexToken token() const;
            std::string str() const;
    };

    class ModuleStmt: public Node {
        public:
            std::string str() const { return ""; }
    };

    //class Module: public Node {
    //    private:
    //        const std::vector<ModuleStmt> body_;

    //    public:
    //        Module(const std::vector<ModuleStmt>& body): body_(body){}
    //        const std::vector<ModuleStmt>& body() const { return body_; }

    //        std::string str() const {
    //            std::ostringstream stream;
    //            for (const ModuleStmt& stmt : body_){
    //                stream << stmt.str() << std::endl;
    //            }
    //            return stream.str();
    //        }
    //};

    /********** Shift reduce parsing *************/

    typedef std::vector<std::string> production_t;
    typedef void (*parse_func_t)(std::vector<Node>&);
    typedef std::tuple<std::string, production_t, parse_func_t> prod_rule_t;

    prod_rule_t make_pr(
            const std::string&, 
            const std::vector<std::string>&, 
            const parse_func_t& func = nullptr);

    struct ProdRuleHasher {
        std::size_t operator()(const prod_rule_t& prod_rule) const;
    };

    // Parse table generation
    typedef std::pair<prod_rule_t, int> lr_item_t;
    struct ItemHasher {
        std::size_t operator()(const lr_item_t& lr_item) const;
    };
    // TODO: Create an immutable container type (like tuples/frozensets in python)
    // so that we don't have to take the hash of a mutable container set at compile time.
    typedef std::unordered_set<lr_item_t, ItemHasher> item_set_t;

    struct ItemSetHasher {
        std::size_t operator()(const item_set_t&) const;
    };
    typedef std::unordered_set<item_set_t, ItemSetHasher> dfa_t;

    void init_closure(item_set_t&, const std::vector<prod_rule_t>&);
    item_set_t move_pos(const item_set_t&, const std::string&, const std::vector<prod_rule_t>&);
    void init_dfa(dfa_t& dfa, const std::vector<prod_rule_t>&);

    typedef struct ParseInstr ParseInstr;
    struct ParseInstr {
        enum Action {SHIFT, REDUCE, GOTO, ACCEPT} action;
        int value;
    };
    typedef std::unordered_map<int, std::unordered_map<std::string, ParseInstr>> parse_table_t;

    enum Associativity {
        LEFT_ASSOC,
        RIGHT_ASSOC,
        // Ply also implements nonassociativity, but ignoring that for now
    };
    typedef std::vector<std::pair<enum Associativity, std::vector<std::string>>> precedence_t;
    typedef struct {
        ParseInstr instr1;  // Default chosen instruction will be whatever appeared first in the rules
        ParseInstr instr2;
        std::string lookahead;
    } ParserConflict;

    ///******** Parser ********/ 

    extern const std::vector<prod_rule_t> LANG_RULES;
    extern const std::unordered_map<std::string, std::string> LANG_TOKENS;
    extern const precedence_t LANG_PRECEDENCE;

    class Parser {
        private:
            Lexer lexer_;

            // For Creating first/follow sets 
            std::unordered_set<std::string> nonterminals_;
            std::string start_nonterminal_;
            std::unordered_set<std::string> firsts_stack_;  // for keeping track of recursive calls 
            std::unordered_set<std::string> follows_stack_;
            std::unordered_map<std::string, std::unordered_set<std::string>> firsts_map_;  // memoization
            std::unordered_map<std::string, std::unordered_set<std::string>> follows_map_;

            item_set_t top_item_set_;
            const std::vector<prod_rule_t>& prod_rules_;  // list of produciton rules
            parse_table_t parse_table_;  // map of states to map of strings to parse instructions
            std::unordered_map<const item_set_t, int, ItemSetHasher> item_set_map_;  // map of item sets (states) to their state number
            std::unordered_map<const prod_rule_t, int, ProdRuleHasher> prod_rule_map_;  // map of production rule index to production rule (flipped keys + vals of prod_rules_)
            std::unordered_map<std::string, std::pair<std::size_t, enum Associativity>> precedence_map_;  // map of symbol to pair of the precedence value and associativity
            std::vector<ParserConflict> conflicts_;

            /******* Methods ********/
            void init_parse_table(const dfa_t&);
            bool is_terminal(const std::string&) const;
            void init_precedence(const precedence_t&);
            std::string key_for_instr(const ParseInstr&, const std::string&) const;
            void check_precedence(const ParseInstr&, const ParseInstr&, const std::string&,
                    std::unordered_map<std::string, ParseInstr>&);
            std::string conflict_str(const ParseInstr&, const std::string lookahead = "") const;
            std::string rightmost_terminal(const production_t&) const;

            // For creating firsts/follows sets
            std::unordered_set<std::string> make_nonterminal_firsts(const std::string&);

        public:
            Parser(Lexer&, const std::vector<prod_rule_t>& prod_rules,
                   const precedence_t& precedence={{}});
            void dump_grammar(std::ostream& stream=std::cerr) const;
            void dump_state(std::size_t, std::ostream& stream=std::cerr) const;
            const std::vector<ParserConflict>& conflicts() const;
            void parse(const std::string&);
            void reduce(const prod_rule_t&, std::vector<LexToken>&, std::vector<Node>&);
            
            // Firsts/follows methods 
            std::unordered_set<std::string> firsts(const std::string&);
            std::unordered_set<std::string> follows(const std::string&);
            const std::unordered_set<std::string>& firsts() const;
            const std::unordered_set<std::string>& follows() const;
            const std::unordered_set<std::string>& firsts_stack() const;
            const std::unordered_set<std::string>& follows_stack() const;
    };

    /**
     * Debugging
     */ 
    std::string str(const LexToken&);
    std::string str(const production_t& production);
    std::string str(const prod_rule_t& prod_rule);
    std::string str(const lr_item_t& lr_item);
    std::string str(const item_set_t& item_set);
    std::string str(const ParseInstr::Action&);
}

#endif
