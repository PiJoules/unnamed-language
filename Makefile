CPP = g++-4.9
STD = c++11

CPPFLAGS = -Wall -Werror -std=$(STD) -Wfatal-errors $(MACROS)

MEMCHECK = valgrind --error-exitcode=1 --leak-check=full

SOURCES = lexer.cpp \
		  parser.cpp \
		  lang_lexer.cpp \
		  lang_parser.cpp \
		  lang_utils.cpp \
		  lang_rules.cpp \
		  lang_nodes.cpp
OBJS = $(SOURCES:.cpp=.o)

TEST_FILES = test_lexer.cpp \
			 test_table_generation.cpp \
			 test_parser.cpp

EXE_FILES = $(TEST_FILES) \
			dump_lang.cpp \
			#lang.cpp
EXE_OUTPUTS = $(EXE_FILES:.cpp=.out)

test: compile test_lexer test_table_generation test_parser 

.PHONY: test

# Object files from cpp files
%.o: %.cpp 
	$(CPP) $(CPPFLAGS) -O2 -c $< -o $@

compile: $(OBJS) clean_exes $(EXE_OUTPUTS)

# Executable binaries from cpp files
%.out: %.cpp
	$(CPP) $(CPPFLAGS) $< $(OBJS) -o $@

clean_exes:
	rm -rf $(EXE_OUTPUTS)

test_lexer: clean_exes test_lexer.out
	./test_lexer.out
	if [ -x "$$(command -v valgrind)" ]; then $(MEMCHECK) ./test_lexer.out || (echo "memory leak"; exit 1); fi

test_table_generation: clean_exes test_table_generation.out
	./test_table_generation.out
	valgrind ./test_table_generation.out || echo 'Valgrind not available' 
	if [ -x "$$(command -v valgrind)" ]; then $(MEMCHECK) ./test_table_generation.out || (echo "memory leak"; exit 1); fi 

clean_test_parser:
	rm -rf test_parser.out

test_parser: $(OBJS) clean_exes test_parser.out
	./test_parser.out
	if [ -x "$$(command -v valgrind)" ]; then $(MEMCHECK) ./test_parser.out || (echo "memory leak"; exit 1); fi

clean_dump_lang:
	rm -rf dump_lang.out

dump_lang: $(OBJS) clean_dump_lang dump_lang.out
	./dump_lang.out

clean:
	rm -rf *.o *.out
