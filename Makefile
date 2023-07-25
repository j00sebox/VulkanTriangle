STB_INCLUDE_PATH = /home/josh/Repos/stb
TINY_OBJ_LOADER_INCLUDE_PATH = /home/josh/Repos/tinyobjloader

# Compiler
CC   = g++
OPTS = -std=c++20

# Project name
PROJECT = VulkanTriangle

# Libraries
LIBS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi
INCS = -I$(STB_INCLUDE_PATH) -I$(TINY_OBJ_LOADER_INCLUDE_PATH)

SRCS = $(shell find -name '*.cpp')
# DIRS = $(shell find src -type d | sed 's/src/./g' )
OBJS = $(patsubst %.cpp,out/%.o,$(SRCS))

# Targets
$(PROJECT): buildrepo $(OBJS)
	$(CC) $(OPTS) $(OBJS) $(LIBS) $(INCS) -o $@

out/%.o: %.cpp
	$(CC) $(OPTS) -c $< $(INCS) -o $@

clean:
	rm $(PROJECT) out -Rf

buildrepo:
	mkdir -p out
    # for dir in $(DIRS); do mkdir -p out/$$dir; done