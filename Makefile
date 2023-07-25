STB_INCLUDE_PATH = /home/josh/Repos/stb
TINY_OBJ_LOADER_INCLUDE_PATH = /home/josh/Repos/tinyobjloader
CFLAGS = -std=c++20 -O2 -I$(STB_INCLUDE_PATH) -I$(TINY_OBJ_LOADER_INCLUDE_PATH)

LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi


all: VulkanTest

VulkanTest: main.cpp
	g++ $(CFLAGS) -o VulkanTest main.cpp $(LDFLAGS)

.PHONY: test clean

test: VulkanTest
	./VulkanTest

clean:
	rm -f VulkanTest