NAME			:= converter
SOURCE_FILES	:= main.cc converter.cc
OBJECT_FILES	:= $(addsuffix .o,$(SOURCE_FILES))

OBJ_DIR			:= build
OBJECT_FILES	:= $(addprefix $(OBJ_DIR)/,$(OBJECT_FILES))

CXX				:= g++

CXXFLAGS	:= -Wall -Wextra -std=c++20 \
			   `Magick++-config --cppflags --cxxflags`
LFLAGS		:= -lassimp -lboost_program_options \
			   `Magick++-config --ldflags --libs`

ifndef config
	config	:= distr
endif

ifeq ($(config),debug)
	ifndef san
		san 		:= address,undefined
	endif
	CXXFLAGS		+= -g3 -Og -fsanitize=$(san)
	LFLAGS			+= -g3 -Og -fsanitize=$(san) -fno-inline
else ifeq ($(config),release)
	CXXFLAGS		+= -g -O2
	LFLAGS			+= -g -O2
	ifdef san
		CXXFLAGS	+= -fsanitize=$(san)
		LFLAGS		+= -fsanitize=$(san)
	endif
else ifeq ($(config),distr)
	CXXFLAGS	+= -g0 -O3 -march=native
	LFLAGS		+= -g0 -O3 -march=native -flto
else
$(error "unknown config $(config)")
endif

$(NAME): $(OBJECT_FILES)
	$(CXX) -o $(NAME) $(OBJECT_FILES) $(LFLAGS) 

$(OBJ_DIR)/%.cc.o: %.cc Makefile
	@mkdir -p $(@D)
	$(CXX) -o $@ -c $< $(CXXFLAGS)

re:
	${MAKE} clean
	${MAKE}

clean:
	rm -f $(OBJECT_FILES)
	rm -f $(NAME)
