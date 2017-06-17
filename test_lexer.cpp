#include "lang.h"
#include <cassert>

#define quote(x) #x  // Converts x to a quoted string
#define assert_str_equal(s1, s2) __assert_str_equal(s1, s2, __LINE__, __FILE__)
#define assert_int_equal(i1, i2) __assert_int_equal(i1, i2, __LINE__, __FILE__)
#define assert_raises(expr, exception_t) \
    try { \
        expr; \
        throw std::runtime_error("Expected exception_t to be throwin in expr (line __LINE__ in __FILE__)"); \
    } \
    catch (const exception_t& e){}

void __assert_str_equal(const std::string& s1, const std::string& s2, 
                      int lineno, const std::string filename){
    if (s1 != s2){
        std::ostringstream err;
        err << "'" << s1 << "' != '" << s2 << "' (line " << lineno << " in " << filename << ")";
        throw std::runtime_error(err.str());
    }
}

void __assert_int_equal(const int& i1, const int& i2, 
                        int lineno, const std::string filename){
    if (i1 != i2){
        std::ostringstream err;
        err << "'" << i1 << "' != '" << i2 << "' (line " << lineno << " in " << filename << ")";
        throw std::runtime_error(err.str());
    }
}

/**
 * Test creating an empty lexer.
 */
void test_lexer_creation(){
    lang::Lexer lex;
    auto tok = lex.token();
    assert(tok.value == "");
    assert_int_equal(tok.lineno, 1);
    assert(tok.colno == 1);
    assert(tok.symbol == lang::eof_tok);

    // Same output
    tok = lex.token();
    assert(tok.value == "");
    assert(tok.lineno == 1);
    assert(tok.colno == 1);
    assert(tok.symbol == lang::eof_tok);
}

/**
 * Test basic input
 */
void test_lexer_input(){
    std::string code("x + y\n4-3");
    lang::Lexer lex;
    lex.input(code);

    auto tok = lex.token();
    assert_str_equal(tok.value, "x");
    assert(tok.lineno == 1);
    assert(tok.colno == 1);
    assert(tok.symbol == lang::name_tok);

    tok = lex.token();
    assert(tok.value == "+");
    assert(tok.lineno == 1);
    assert(tok.colno == 3);
    assert(tok.symbol == lang::add_tok);

    tok = lex.token();
    assert(tok.value == "y");
    assert(tok.lineno == 1);
    assert(tok.colno == 5);
    assert(tok.symbol == lang::name_tok);

    // We listen for newlines
    tok = lex.token();
    assert_str_equal(tok.value, "\n");
    assert(tok.lineno == 1);
    assert(tok.colno == 6);
    assert(tok.symbol == lang::newline_tok);

    tok = lex.token();
    assert(tok.value == "4");
    assert(tok.lineno == 2);
    assert(tok.colno == 1);
    assert(tok.symbol == lang::int_tok);

    tok = lex.token();
    assert(tok.value == "-");
    assert(tok.lineno == 2);
    assert(tok.colno == 2);
    assert(tok.symbol == lang::sub_tok);

    tok = lex.token();
    assert(tok.value == "3");
    assert(tok.lineno == 2);
    assert(tok.colno == 3);
    assert(tok.symbol == lang::int_tok);
}

/**
 * Test reading names
 */
void test_name(){
    lang::Lexer lex;
    lex.input("_x");
    auto tok = lex.token();
    assert(tok.value == "_x");
    assert(tok.lineno == 1);
    assert(tok.colno == 1);
    assert(tok.symbol == lang::name_tok);

    lex.input("_92");
    tok = lex.peek();
    assert(tok.value == "_92");
    assert(tok.lineno == 1);
    assert(tok.colno == 3);
    assert(tok.symbol == lang::name_tok);

    tok = lex.token();
    assert(tok.value == "_92");
    assert(tok.lineno == 1);
    assert(tok.colno == 3);
    assert(tok.symbol == lang::name_tok);
}

/**
 * Test indentation
 */
void test_indentation(){
    lang::Lexer lex;
    lex.input(R"(x

    a
      b

    d

    e
6
7)");

    auto tok = lex.token();
    assert(tok.value == "x");
    assert(tok.lineno == 1);
    assert(tok.colno == 1);
    assert(tok.symbol == lang::name_tok);

    tok = lex.token();
    assert_str_equal(tok.value, "\n\n");
    assert(tok.lineno == 1);
    assert_int_equal(tok.colno, 2);
    assert(tok.symbol == lang::newline_tok);

    tok = lex.token();
    assert_str_equal(tok.value, "");
    assert(tok.lineno == 3);
    assert_int_equal(tok.colno, 1);
    assert(tok.symbol == lang::indent_tok);

    tok = lex.token();
    assert_str_equal(tok.value, "a");
    assert(tok.lineno == 3);
    assert_int_equal(tok.colno, 5);
    assert(tok.symbol == lang::name_tok);

    tok = lex.token();
    assert_str_equal(tok.value, "\n");
    assert(tok.lineno == 3);
    assert_int_equal(tok.colno, 6);
    assert(tok.symbol == lang::newline_tok);

    tok = lex.token();
    assert_str_equal(tok.value, "");
    assert(tok.lineno == 4);
    assert_int_equal(tok.colno, 1);
    assert(tok.symbol == lang::indent_tok);

    tok = lex.token();
    assert_str_equal(tok.value, "b");
    assert(tok.lineno == 4);
    assert_int_equal(tok.colno, 7);
    assert(tok.symbol == lang::name_tok);

    tok = lex.token();
    assert_str_equal(tok.value, "\n\n");
    assert(tok.lineno == 4);
    assert_int_equal(tok.colno, 8);
    assert(tok.symbol == lang::newline_tok);

    tok = lex.token();
    assert_str_equal(tok.value, "");
    assert(tok.lineno == 6);
    assert_int_equal(tok.colno, 1);
    assert(tok.symbol == lang::dedent_tok);

    tok = lex.token();
    assert_str_equal(tok.value, "d");
    assert(tok.lineno == 6);
    assert_int_equal(tok.colno, 5);
    assert(tok.symbol == lang::name_tok);

    tok = lex.token();
    assert_str_equal(tok.value, "\n\n");
    assert(tok.lineno == 6);
    assert_int_equal(tok.colno, 6);
    assert(tok.symbol == lang::newline_tok);

    tok = lex.token();
    assert_str_equal(tok.value, "e");
    assert(tok.lineno == 8);
    assert_int_equal(tok.colno, 5);
    assert(tok.symbol == lang::name_tok);

    tok = lex.token();
    assert_str_equal(tok.value, "\n");
    assert(tok.lineno == 8);
    assert_int_equal(tok.colno, 6);
    assert(tok.symbol == lang::newline_tok);

    tok = lex.token();
    assert_str_equal(tok.value, "");
    assert(tok.lineno == 9);
    assert_int_equal(tok.colno, 1);
    assert(tok.symbol == lang::dedent_tok);

    tok = lex.token();
    assert_str_equal(tok.value, "6");
    assert(tok.lineno == 9);
    assert_int_equal(tok.colno, 1);
    assert(tok.symbol == lang::int_tok);

    tok = lex.token();
    assert_str_equal(tok.value, "\n");
    assert(tok.lineno == 9);
    assert_int_equal(tok.colno, 2);
    assert(tok.symbol == lang::newline_tok);

    tok = lex.token();
    assert_str_equal(tok.value, "7");
    assert(tok.lineno == 10);
    assert_int_equal(tok.colno, 1);
    assert(tok.symbol == lang::int_tok);
}

/**
 * Test bad indentation throws an indentation error.
 */ 
void bad_indeation_code(){
    lang::Lexer lex;
    lex.input(R"(x
  y
 z
)");
    lex.token();  // x
    lex.token();  // newline
    lex.token();  // indent 
    lex.token();  // y 
    lex.token();  // newline 
    lex.token();  // dedent (IndentationError thrown here)
}

void test_indentation_error(){
    assert_raises(bad_indeation_code(), lang::IndentationError);
}

int main(){
    test_lexer_creation();
    test_lexer_input();
    test_name();
    test_indentation();
    test_indentation_error();

    return 0;
}
