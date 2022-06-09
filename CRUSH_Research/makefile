CC = gcc
CXX = g++
CFLAGS = -g -pthread
LDFLAGS = -I./include

DIR_OBJS = object
SRC_CPP = $(wildcard *.cpp)
SRC_C = $(wildcard *.c)
OBJS_CPP = $(patsubst %.cpp,%.o,$(SRC_CPP))
OBJS_C = $(patsubst %.c,%.o,$(SRC_C))
OBJ_CPP = $(addprefix $(DIR_OBJS)/,$(OBJS_CPP))
OBJ_C = $(addprefix $(DIR_OBJS)/,$(OBJS_C))
OBJS_ALL = $(OBJ_CPP) $(OBJ_C)
OBJ = main

all: $(OBJS_CPP) $(OBJS_C) $(OBJ)

test:
	@echo "SRC_CPP: $(SRC_CPP)"
	@echo "SRC_C: $(SRC_C)"
	@echo "OBJS_CPP: $(OBJS_CPP)"
	@echo "OBJS_C: $(OBJS_C)"
	@echo "OBJS_ALL: $(OBJS_ALL)"
	@echo "OBJ: $(OBJ)"

clean:
	rm -f $(OBJ) $(OBJS_ALL)

.PHONY: all clean
$(OBJS_CPP):%.o:%.cpp
	$(CXX) $(CFLAGS) -c $< -o $(DIR_OBJS)/$@
	
$(OBJS_C):%.o:%.c
	$(CXX) $(CFLAGS) -c $< -o $(DIR_OBJS)/$@

$(OBJ):$(OBJS_ALL)
	$(CXX) $(CFLAGS) -o $@ $^


