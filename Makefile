CXX = g++_1220
CC = gcc_1220
MPICC = mpicc

PLUGIN_FLAGS = -I`$(CC) -print-file-name=plugin`/include -g -Wall -fno-rtti -shared -fPIC
CFLAGS = -g -O3
DFLAGS = -DDEBUG

SRC_DIR = src
TEST_DIR = tests
BIN_DIR = bin
GRAPH_DIR = graph

TARGET = test1 test2 test3 test4 test5 test6

all: $(BIN_DIR)/libplugin.so $(TARGET)
debug: clean_all
debug: PLUGIN_FLAGS+=$(DFLAGS)
debug: $(BIN_DIR)/libplugin.so

test1: $(BIN_DIR)/test1
test2: $(BIN_DIR)/test2
test3: $(BIN_DIR)/test3
test4: $(BIN_DIR)/test4
test5: $(BIN_DIR)/test5
test6: $(BIN_DIR)/test6

$(BIN_DIR)/libplugin.so: $(SRC_DIR)/mpi_plugin.cpp
	mkdir -p $(BIN_DIR)
	$(CXX) $(PLUGIN_FLAGS) -o $@ $<

$(BIN_DIR)/test%: $(TEST_DIR)/test%.c $(BIN_DIR)/libplugin.so
	$(MPICC) $< $(CFLAGS) -o $@ -fplugin=./$(BIN_DIR)/libplugin.so

.PHONY: graph 
graph: $(GRAPH_DIR)/*.dot
	for file in $(GRAPH_DIR)/*.dot; do \
		dot -Tpng $$file -o $${file%.dot}.png; \
	done

clean:
	rm -rf $(BIN_DIR)/*

clean_all: clean
	rm -rf $(BIN_DIR)/libplugin.so $(GRAPH_DIR)/*.dot $(GRAPH_DIR)/*.png
