NAME			:= converter
SOURCE_FILES	:= main.cc converter.cc
OBJECT_FILES	:= $(addsuffix .o,$(SOURCE_FILES))

OBJ_DIR			:= build
OBJECT_FILES	:= $(addprefix $(OBJ_DIR)/,$(OBJECT_FILES))

CXX				:= g++

CXXFLAGS	:= -Wall -Wextra -std=c++20 -g3 -Ofast \
			   `Magick++-config --cppflags --cxxflags`
LFLAGS		:= -lassimp -lboost_program_options \
			   `Magick++-config --ldflags --libs`

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
