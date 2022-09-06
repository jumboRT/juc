NAME		:= converter
SOURCE_FILES	:= main.cc converter.cc
OBJECT_FILES	:= $(addsuffix .o,$(SOURCE_FILES))

CXX		:= g++

CXXFLAGS	:= -Wall -Wextra -std=c++20 -g3 -Og  \
		   `Magick++-config --cppflags --cxxflags`
LFLAGS		:= -lassimp \
		   `Magick++-config --ldflags --libs`

$(NAME): $(OBJECT_FILES)
	$(CXX) -o $(NAME) $(OBJECT_FILES) $(LFLAGS) 

%.cc.o: %.cc Makefile
	$(CXX) -o $@ -c $< $(CXXFLAGS)

clean:
	rm -f $(OBJECT_FILES)
	rm -f $(NAME)
