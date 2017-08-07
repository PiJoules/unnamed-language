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
		  lang_nodes.cpp \
		  cpp_nodes.cpp \
		  subprocess.cpp \
		  compiler.cpp

OBJS = $(SOURCES:.cpp=.o)

TEST_FILES = test_lexer.cpp \
			 test_table_generation.cpp \
			 test_lang.cpp \
			 test_cppnodes.cpp \
			 test_lang_files.cpp

EXE_FILES = $(TEST_FILES) \
			dump_lang.cpp \

EXE_OUTPUTS = $(EXE_FILES:.cpp=.out)

test: compile_clean test_lexer test_table_generation test_lang test_cppnodes test_lang_files

.PHONY: test

# Object files from cpp files
%.o: %.cpp 
	$(CPP) $(CPPFLAGS) -O2 -c $< -o $@

compile: $(OBJS) $(EXE_OUTPUTS)
compile_objs: $(OBJS)
compile_clean: $(OBJS) clean_exes $(EXE_OUTPUTS)

# Executable binaries from cpp files
%.out: %.cpp
	$(CPP) $(CPPFLAGS) $< $(OBJS) -o $@

clean_exes:
	rm -rf $(EXE_OUTPUTS)

language: $(OBJS) clean_exes
	$(CPP) $(CPPFLAGS) language.cpp $(OBJS) -o $@

# Tests
test_lexer: $(OBJS) clean_exes test_lexer.out
	./test_lexer.out
	if [ -x "$$(command -v valgrind)" ]; then $(MEMCHECK) ./test_lexer.out || (echo "memory leak"; exit 1); fi

test_table_generation: $(OBJS) clean_exes test_table_generation.out
	./test_table_generation.out
	if [ -x "$$(command -v valgrind)" ]; then $(MEMCHECK) ./test_table_generation.out || (echo "memory leak"; exit 1); fi 

test_lang: $(OBJS) clean_exes test_lang.out
	./test_lang.out
	if [ -x "$$(command -v valgrind)" ]; then $(MEMCHECK) ./test_lang.out || (echo "memory leak"; exit 1); fi

test_cppnodes: $(OBJS) clean_exes test_cppnodes.out
	./test_cppnodes.out
	if [ -x "$$(command -v valgrind)" ]; then $(MEMCHECK) ./test_cppnodes.out || (echo "memory leak"; exit 1); fi 

test_lang_files: $(OBJS) clean_exes test_lang_files.out
	./test_lang_files.out
	if [ -x "$$(command -v valgrind)" ]; then $(MEMCHECK) ./test_lang_files.out || (echo "memory leak"; exit 1); fi 

dump_lang: $(OBJS) clean_exes dump_lang.out
	./dump_lang.out

clean:
	rm -rf *.o *.out
