#include "lang.h"

/**
 * Borrowed from python3.6's tuple hash
 */
std::size_t lang::ProdRuleHasher::operator()(const prod_rule_t& prod_rule) const {
    std::string rule = std::get<0>(prod_rule);
    production_t prod = std::get<1>(prod_rule);
    std::hash<std::string> str_hasher;
    std::size_t rule_hash = str_hasher(rule);

    std::size_t hash_mult = _HASH_MULTIPLIER;
    std::size_t prod_hash = 0x345678;
    std::size_t len = prod.size();
    for (std::string& r : prod){
        prod_hash = (prod_hash ^ str_hasher(r)) * hash_mult;
        hash_mult += 82520 + len + len;
    }
    prod_hash += 97531;
    return prod_hash ^ rule_hash;
}

std::size_t lang::ItemHasher::operator()(const lang::lr_item_t& lr_item) const {
    ProdRuleHasher hasher;
    return hasher(lr_item.first) ^ static_cast<std::size_t>(lr_item.second);
}

/**
 * Borrowed from python3.6's frozenset hash
 */ 
static std::size_t _shuffle_bits(std::size_t h){
    return ((h ^ 89869747UL) ^ (h << 16)) * 3644798167UL;
}

std::size_t lang::ItemSetHasher::operator()(const lang::item_set_t& item_set) const {
    std::size_t hash = 0;
    ItemHasher item_hasher;
    for (const auto& lr_item : item_set){
        hash ^= _shuffle_bits(item_hasher(lr_item));  // entry hashes
    }
    hash ^= item_set.size() * 1927868237UL;  // # of active entrues
    hash = hash * 69069U + 907133923UL;
    return hash;
};

/**
 * Initialize a closure from an item set and list of productions.
 */
void lang::init_closure(lang::item_set_t& item_set, const std::vector<lang::prod_rule_t>& prod_rules){
    std::size_t last_size;

    do {
        last_size = item_set.size();
        for (const auto& item : item_set){
            std::size_t pos = item.second;  // dot position
            lang::production_t prod = std::get<1>(item.first);  // production list
            if (pos < prod.size()){
                std::string next_symbol = prod[pos];
                
                // Find all productions that start with the next_symbol
                // TODO: See if we can optomize this later
                for (const auto prod_rule : prod_rules){
                    if (next_symbol == std::get<0>(prod_rule)){
                        item_set.insert({prod_rule, 0});
                    }
                }
            }
        }
    } while (item_set.size() != last_size); // while the item set is changing
}

/**
 * Move the parser position over by 1.
 */ 
lang::item_set_t lang::move_pos(const lang::item_set_t& item_set, 
                                const std::string& symbol,
                                const std::vector<lang::prod_rule_t>& prod_rules){
    item_set_t moved_item_set;
    for (const auto lr_item : item_set){
        auto prod_rule = lr_item.first;
        const production_t& prod = std::get<1>(prod_rule);
        std::size_t pos = lr_item.second;

        if (pos < prod.size()){
            if (prod[pos] == symbol){
                moved_item_set.insert({prod_rule, pos + 1});
            }
        }
    }
    init_closure(moved_item_set, prod_rules);
    return item_set_t(moved_item_set);
}

/**
 * Create the canonical collections of the DFA.
 */ 
void lang::init_dfa(lang::dfa_t& dfa, const std::vector<lang::prod_rule_t>& prod_rules){
    std::size_t last_size;
    do {
        last_size = dfa.size();
        for (const auto& item_set : dfa){
            for (const auto& lr_item : item_set){
                auto production = std::get<1>(std::get<0>(lr_item));
                std::size_t pos = lr_item.second;
                if (pos < production.size()){
                    auto next_symbol = production[pos];
                    auto moved_item_set = lang::move_pos(item_set, next_symbol, prod_rules);
                    dfa.insert(moved_item_set);
                }
            }
        }
    } while (dfa.size() != last_size);  // while dfa did not change
}

void lang::Parser::init_precedence(const precedence_t& precedence){
    precedence_map_.reserve(precedence.size());
    for (std::size_t i = 0; i < precedence.size(); ++i){
        const auto& entry = precedence[i];
        enum Associativity assoc = entry.first;
        const auto& tokens = entry.second;
        for (const std::string& tok : tokens){
            precedence_map_[tok] = {i, assoc};
        }
    }
}

/**
 * Initialize the parse table from the production rules.
 */
lang::Parser::Parser(Lexer& lexer, const std::vector<prod_rule_t>& prod_rules, 
                     const precedence_t& precedence):
    lexer_(lexer),
    prod_rules_(prod_rules)
{
    init_precedence(precedence);
    init_firsts();
    init_follow();

    const auto& entry = prod_rules_.front();
    top_item_set_ = {{entry, 0}};
    init_closure(top_item_set_, prod_rules_);
    dfa_t dfa = {top_item_set_};
    init_dfa(dfa, prod_rules_);
    init_parse_table(dfa);
}

bool lang::Parser::is_terminal(const std::string& symbol) const {
    return lexer_.tokens().find(symbol) != lexer_.tokens().end();
}

const std::vector<lang::ParserConflict>& lang::Parser::conflicts() const {
    return conflicts_;
}

void lang::Parser::init_parse_table(const dfa_t& dfa){
    const auto& top_prod_rule = prod_rules_.front();
    parse_table_.reserve(dfa.size());
    item_set_map_.reserve(dfa.size());

    // Map the item_sets to their final indeces in the map 
    std::size_t i = 0;
    for (const auto& item_set : dfa){
        std::unordered_map<std::string, ParseInstr> action_map;
        parse_table_[i] = action_map;
        item_set_map_[item_set] = i;
        ++i;
    }

    // Map the production rules to the order in which they appear
    for (i = 0; i < prod_rules_.size(); ++i){
        prod_rule_map_[prod_rules_[i]] = i;
    }

    i = 0;
    for (const auto& item_set : dfa){
        for (const auto& lr_item : item_set){
            const auto& prod_rule = lr_item.first;
            const auto& prod = std::get<1>(prod_rule);
            const std::size_t& pos = lr_item.second;
            auto& action_table = parse_table_[i];
            if (pos < prod.size()){
                // If A -> x . a y and GOTO(I_i, a) == I_j, then ACTION[i, a] = Shift j 
                // If we have a rule where the symbol following the parser position is 
                // a terminal, shift to the jth state which is equivalent to GOTO(I_i, a).
                const auto& next_symbol = prod[pos];
                const auto I_j = move_pos(item_set, next_symbol, prod_rules_);
                int j = item_set_map_.at(I_j);

                if (is_terminal(next_symbol)){

                    std::cerr << "Checking shift for " << next_symbol << " in state " << i << std::endl;

                    // next_symbol is a token 
                    auto existing_it = action_table.find(next_symbol);
                    ParseInstr shift_instr = {lang::ParseInstr::Action::SHIFT, j};
                    if (existing_it != action_table.cend()){

                        std::cerr << "Checking conflict between " << action_table[next_symbol].action << " and " << shift_instr.action << " for " << next_symbol << " in state " << i << std::endl;

                        // Possible action conlfict. Check for precedence 
                        check_precedence(existing_it->second, shift_instr, next_symbol, action_table);
                    }
                    else {
                        // No conflict. Fill with shift
                        action_table[next_symbol] = shift_instr;
                    }
                }
                else {
                    action_table[next_symbol] = {lang::ParseInstr::Action::GOTO, j};
                }
            }
            else {
                if (prod_rule == top_prod_rule){
                    // Finished whole module; cannot reduce further
                    action_table[tokens::END] = {lang::ParseInstr::Action::ACCEPT, 0};
                }
                else {
                    // End of rule; Reduce 
                    // If A -> a ., then ACTION[i, b] = Reduce A -> a for all terminals 
                    // in B -> A . b where b is a terminal
                    int rule_num = prod_rule_map_.at(prod_rule);
                    const std::string& rule = std::get<0>(prod_rule);
                    ParseInstr instr = {lang::ParseInstr::Action::REDUCE, rule_num};
                    
                    for (const std::string& follow : follow_map_[rule]){

                        std::cerr << "Checking reduce for " << follow << " in state " << i << std::endl;

                        if (action_table.find(follow) != action_table.cend()){

                            std::cerr << "Checking conflict between " << action_table[follow].action << " and " << instr.action << " for " << follow << " in state " << i << std::endl;

                            // Possible conflict 
                            check_precedence(action_table[follow], instr, follow, action_table);
                        }
                        else {
                            action_table[follow] = instr;
                        }
                    }

                    // Search all other rules
                    //for (const prod_rule_t& pr : prod_rules_){
                    //    const production_t& other_prod = std::get<1>(pr);

                    //    // Search for the symbol behind the dot
                    //    // Only need to iterate up to the second to last symbol 
                    //    for (std::size_t j = 0; j < other_prod.size()-1; ++j){
                    //        const std::string& current_prod = other_prod[j];
                    //        const std::string& next_prod = other_prod[j+1];
                    //        if (current_prod == rule && is_terminal(next_prod)){
                    //            auto existing_it = action_table.find(next_prod);
                    //            ParseInstr instr = {lang::ParseInstr::Action::REDUCE, rule_num};
                    //            if (existing_it != action_table.cend()){
                    //                // Possible conflict. Check for precedence.
                    //                check_precedence(existing_it->second, instr, next_prod, action_table);
                    //            }
                    //            else {
                    //                // No conflict. Fill with reduce.
                    //                action_table[next_prod] = instr;
                    //            }
                    //        }
                    //    }
                    //}
                }
            }
        }
        ++i;
    }
}

std::string lang::Parser::key_for_instr(const ParseInstr& instr, const std::string& lookahead) const {
    std::string key_term;
    if (instr.action == ParseInstr::REDUCE){
        std::size_t rule_num = instr.value;
        const production_t& reduce_prod = std::get<1>(prod_rules_[rule_num]);
        key_term = rightmost_terminal(reduce_prod);
    }
    else {
        key_term = lookahead;
    }
    return key_term;
}

void lang::Parser::check_precedence(
        const ParseInstr& existing_instr,
        const ParseInstr& new_instr,
        const std::string& lookahead,
        std::unordered_map<std::string, ParseInstr>& action_table){
    // Possible action conlfict. Check for precedence 
    // Tterminal key for existing instr
    const std::string key_existing = key_for_instr(existing_instr, lookahead);

    // Terminal key for new instr 
    const std::string key_new = key_for_instr(new_instr, lookahead);

    if (precedence_map_.find(key_existing) != precedence_map_.cend() &&
        precedence_map_.find(key_new) != precedence_map_.cend()){

        std::cerr << "checking precedence between " << key_existing << " and " << key_new << std::endl;

        // Both have precedence rules. No conflict
        const auto& prec_existing = precedence_map_[key_existing];
        const auto& prec_new = precedence_map_[key_new];
        if (prec_new.first > prec_existing.first){
            // Take the new action over current
            action_table[key_new] = new_instr;
        }
        else if (prec_new.first < prec_existing.first){
            // Take existing over new. Don't need to do anything.
        }
        else {
            // Same precedence 
            // Take account into associativity if either are shift
            if (new_instr.action == ParseInstr::SHIFT || existing_instr.action == ParseInstr::SHIFT){
                // Find the shift instr
                ParseInstr shift_instr, reduce_instr;
                if (new_instr.action == ParseInstr::SHIFT){
                    shift_instr = new_instr;
                    reduce_instr = existing_instr;
                }
                else {
                    shift_instr = existing_instr;
                    reduce_instr = new_instr;
                }

                // Assign based on associativity
                // If left assoc, reduce. If right assoc, shift.
                if (prec_new.second == LEFT_ASSOC){
                    action_table[lookahead] = reduce_instr;
                }
                else {
                    action_table[lookahead] = shift_instr;
                }
            }
            else {
                // Both are reduce. Cannot resolve this, so add it as a conflict.
                conflicts_.push_back({
                    existing_instr,
                    new_instr,
                    lookahead,
                });
            }
        }
    }
    else {
        // Conflict
        conflicts_.push_back({
            existing_instr,
            new_instr,
            lookahead,
        });
    }
}

void lang::Parser::dump_state(std::size_t state, std::ostream& stream) const {
    item_set_t item_sets[item_set_map_.size()];
    for (auto it = item_set_map_.cbegin(); it != item_set_map_.cend(); ++it){
        item_sets[it->second] = it->first;
    }
        
    stream << "state " << state << std::endl << std::endl;

    // Print item sets
    const auto& item_set = item_sets[state];
    for (const auto& lr_item : item_set){
        stream << "\t" << lang::str(lr_item) << std::endl;
    }
    stream << std::endl;

    // Print parse instructions
    const auto& action_map = parse_table_.at(state);
    for (auto it = action_map.cbegin(); it != action_map.cend(); ++it){
        const auto& symbol = it->first;
        const auto& instr = it->second;
        const auto& action = instr.action;

        if (action == lang::ParseInstr::SHIFT || action == lang::ParseInstr::REDUCE){
            int val = instr.value;
            stream << "\t" << symbol << "\t\t";

            if (action == lang::ParseInstr::SHIFT){
                stream << "shift and go to state ";
            }
            else {
                stream << "reduce using rule ";
            }
            stream << val << std::endl;
        }
    }
    stream << std::endl;
}

/**
 * Pretty print the parse table similar to how ply prints it.
 */
void lang::Parser::dump_grammar(std::ostream& stream) const {
    // Grammar 
    stream << "Grammar" << std::endl << std::endl;
    for (std::size_t i = 0; i < prod_rules_.size(); ++i){
        stream << "Rule " << i << ": " << str(prod_rules_[i]) << std::endl;
    }
    stream << std::endl;

    // States 
    for (std::size_t i = 0; i < parse_table_.size(); ++i){
        dump_state(i, stream);
    }
    stream << std::endl;

    // Conflicts  
    stream << "Conflicts (" << conflicts_.size() << ")" << std::endl << std::endl;
    for (const ParserConflict& conflict : conflicts_){
        const ParseInstr& chosen = conflict.instr1;
        const ParseInstr& other = conflict.instr2;
        const std::string& lookahead = conflict.lookahead;

        const ParseInstr::Action& act1 = chosen.action;
        const ParseInstr::Action& act2 = other.action;
        stream << str(act1) << "/" << str(act2) << " conflict (defaulting to " << str(act1)
               << ")" << std::endl;
        stream << "- " << conflict_str(chosen, lookahead) << std::endl;
        stream << "- " << conflict_str(other, lookahead) << std::endl;
    }
}

std::string lang::Parser::conflict_str(const ParseInstr& instr, const std::string lookahead) const {
    std::ostringstream stream;
    production_t prod;
    switch (instr.action){
        case ParseInstr::SHIFT: 
            stream << "shift and go to state " << instr.value << " on lookahead " << lookahead;
            break;
        case ParseInstr::REDUCE: 
            prod = std::get<1>(prod_rules_[instr.value]);
            stream << "reduce using rule " << instr.value << " on terminal " << rightmost_terminal(prod);
            break;
        case ParseInstr::GOTO: 
            stream << "go to state " << instr.value;
            break;
        case ParseInstr::ACCEPT: 
            stream << "accept";
    }
    return stream.str();
}

std::string lang::Parser::rightmost_terminal(const production_t& prod) const {
    for (auto it = prod.crbegin(); it != prod.crend(); ++it){
        if (is_terminal(*it)){
            return *it;
        }
    }
    return "";
}

/**
 * All terminal symbols on the stack have the same precedence and associativity.
 * Reduce depending on the type of associativity.
 */
void lang::Parser::reduce(
        const prod_rule_t& prod_rule, 
        std::vector<std::string>& symbol_stack,
        std::vector<Node>& token_stack){
    const std::string& rule = std::get<0>(prod_rule);
    const production_t& prod = std::get<1>(prod_rule);
    const parse_func_t func = std::get<2>(prod_rule);

    assert(symbol_stack.size() == prod.size());
    assert(token_stack.size() == prod.size());

    if (func){
        func(token_stack);
    }

    std::cerr << "Reduced using " << str(prod) << std::endl;

    symbol_stack.clear();
    symbol_stack.push_back(rule);
    token_stack.erase(token_stack.cbegin()+1, token_stack.cend());
}

/**
 * The actual parsing.
 */
void lang::Parser::parse(const std::string& code){
    // Input the string
    lexer_.input(code);

    std::vector<std::string> symbol_stack;
    std::vector<Node> token_stack;

    assert(prod_rule_map_.find(prod_rules_.front()) != prod_rule_map_.end());
    std::cerr << "Entry point:" << std::endl;
    std::cerr << str(prod_rules_.front()) << std::endl;

    std::size_t state = item_set_map_.at(top_item_set_);

    std::cerr << "Starting in state " << state << std::endl;

    while (1){
        const LexTokenWrapper lookahead(lexer_.token());

        if (parse_table_[state].find(lookahead.token().symbol) == parse_table_[state].cend()){
            // Parse error
            std::ostringstream err;
            err << "Unable to handle lookahead '" << lookahead.token().symbol << "' in state " << state
                << std::endl << std::endl;
            dump_state(state, err);
            throw std::runtime_error(err.str());
        }

        const ParseInstr& next_instr = parse_table_[state][lookahead.token().symbol];
        switch (next_instr.action){
            case ParseInstr::SHIFT:
                symbol_stack.push_back(lookahead.token().symbol);
                token_stack.push_back(lookahead);
                state = next_instr.value;

                std::cerr << "Shifted " << lookahead.token().symbol << " and goto state " << state << std::endl;

                break;
            case ParseInstr::REDUCE:
                reduce(prod_rules_[next_instr.value], symbol_stack, token_stack);
                break;
            case ParseInstr::ACCEPT:
                // Reached end. Stack should be empty and loop is exited.
                assert(symbol_stack.empty());
                assert(token_stack.empty());
                return;
            case ParseInstr::GOTO:
                // Should not reach
                break;
        }
    }
}

/**
 * Firsts table
 */  
void lang::Parser::init_firsts(){
    for (const auto& prod_rule : prod_rules_){
        const std::string& rule = std::get<0>(prod_rule);
        const production_t& prod = std::get<1>(prod_rule);
        const std::string& first_symbol = prod.front();

        if (is_terminal(first_symbol)){
            firsts_map_[rule].insert(first_symbol);
        }
        else {
            std::unordered_set<std::string>& other_symbols = firsts_map_[first_symbol];
            firsts_map_[rule].insert(other_symbols.cbegin(), other_symbols.cend());
        }
    }
}

/**
 * Follow table
 */ 
void lang::Parser::init_follow(){
    const std::string& entry_rule = std::get<0>(prod_rules_.front());
    for (const auto& prod_rule : prod_rules_){
        const std::string& rule = std::get<0>(prod_rule);
        const production_t& prod = std::get<1>(prod_rule);

        if (rule == entry_rule){
            follow_map_[rule].insert(tokens::END);
        }

        for (std::size_t i = 0; i < prod_rules_.size(); ++i){
            const std::string& other_rule = std::get<0>(prod_rules_[i]);

            // If this rule is in the production 
            auto other_rule_it = std::find(prod.cbegin(), prod.cend(), other_rule);
            if (other_rule_it != prod.cend()){
                std::size_t non_term_idx = other_rule_it - prod.cend();

                std::unordered_set<std::string>& follow_symbols = follow_map_[other_rule];
                if (non_term_idx == prod.size() - 1){
                    // This rule is the last symbol in this production 
                    follow_symbols.insert(
                            follow_map_[entry_rule].cbegin(),
                            follow_map_[entry_rule].cend());
                }
                else {
                    // Extend the next   
                    const std::string& next_rule = std::get<0>(prod_rules_[(i + 1) % prod_rules_.size()]);
                    follow_symbols.insert(
                            firsts_map_[next_rule].cbegin(),
                            firsts_map_[next_rule].cend());
                }
            }
        }
    }
}

const std::unordered_set<std::string>& lang::Parser::firsts(const std::string& rule) const {
    return firsts_map_.at(rule);
}

const std::unordered_set<std::string>& lang::Parser::follow(const std::string& rule) const {
    return follow_map_.at(rule);
}
