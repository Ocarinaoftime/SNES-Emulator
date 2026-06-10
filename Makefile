CXX = clang++

EXE = snes
IMGUI_DIR = ./imgui
SRC_DIR = src
OBJ_DIR = obj
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
SOURCES += $(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_demo.cpp $(IMGUI_DIR)/imgui_draw.cpp $(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/imgui_widgets.cpp
SOURCES += $(IMGUI_DIR)/backends/imgui_impl_sdl3.cpp $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp
OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(filter $(SRC_DIR)/%.cpp,$(SOURCES)))
IMGUI_SRC = $(filter $(IMGUI_DIR)/%.cpp,$(SOURCES))
IMGUI_SRC := $(filter-out $(IMGUI_DIR)/backends/%.cpp,$(IMGUI_SRC))
OBJS += $(patsubst $(IMGUI_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(IMGUI_SRC))
OBJS += $(patsubst $(IMGUI_DIR)/backends/%.cpp,$(OBJ_DIR)/%.o,$(filter $(IMGUI_DIR)/backends/%.cpp,$(SOURCES)))
UNAME_S := $(shell uname -s)
LINUX_GL_LIBS = -lGL
SDL_DIR := ./SDL

CXXFLAGS = -std=c++17 -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends -I./include -I$(SDL_DIR)/include
CXXFLAGS += -g -Wall -Wformat
LIBS =

.DEFAULT_GOAL := all

##---------------------------------------------------------------------
## OPENGL ES
##---------------------------------------------------------------------

## This assumes a GL ES library available in the system, e.g. libGLESv2.so
# CXXFLAGS += -DIMGUI_IMPL_OPENGL_ES2
# LINUX_GL_LIBS = -lGLESv2
## If you're on a Raspberry Pi and want to use the legacy drivers,
## use the following instead:
# LINUX_GL_LIBS = -L/opt/vc/lib -lbrcmGLESv2

##---------------------------------------------------------------------
## BUILD FLAGS PER PLATFORM
##---------------------------------------------------------------------

ifeq ($(UNAME_S), Linux) #LINUX
	ECHO_MESSAGE = "Linux"
	LIBS += $(LINUX_GL_LIBS) -ldl `pkg-config sdl3 --libs`

	CXXFLAGS += `pkg-config sdl3 --cflags`
	CFLAGS = $(CXXFLAGS)
endif

ifeq ($(UNAME_S), Darwin) #APPLE
	ECHO_MESSAGE = "Mac OS X"
	LIBS += -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
	LIBS += `pkg-config --libs sdl3`
	LIBS += -L/usr/local/lib -L/opt/local/lib

	CXXFLAGS += `pkg-config --cflags sdl3`
	CXXFLAGS += -I/usr/local/include -I/opt/local/include 
	CFLAGS = $(CXXFLAGS)
endif

ifeq ($(OS), Windows_NT)
	ECHO_MESSAGE = "MinGW"
	LIBS += -lgdi32 -lopengl32 -limm32 `pkg-config --static -lSDL3 -I SDL3-3.4.8/x86_64-w64-mingw32/include -L SDL3-3.4.8/x86_64-w64-mingw32/lib -mwindows`
	CXXFLAGS += `pkg-config --cflags sdl3`
	CFLAGS = $(CXXFLAGS)
endif

##---------------------------------------------------------------------
## BUILD RULES
##---------------------------------------------------------------------

obj:
	mkdir -p $@

obj/%.o: src/%.cpp | obj
	$(CXX) $(CXXFLAGS) -c -o $@ $<

obj/%.o: $(IMGUI_DIR)/%.cpp | obj
	$(CXX) $(CXXFLAGS) -c -o $@ $<

obj/%.o: $(IMGUI_DIR)/backends/%.cpp | obj
	$(CXX) $(CXXFLAGS) -c -o $@ $<

all: $(EXE)
	@echo Build complete for $(ECHO_MESSAGE)

$(EXE): $(OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS)

clean:
	rm -f $(EXE) $(OBJS)