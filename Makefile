CXX = c++

NAME = server.app

CPPFLAGS = -Wall -Wextra -Werror #-g3 -fsanitize=address

SRC =	main.cpp			\
		Multiplex.cpp		\
		SocketManager.cpp	\

OBJ = $(SRC:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CPPFLAGS) $(OBJ) -o $(NAME)  -lssl -lcrypto

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re