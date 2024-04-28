PROJECT_NAME	= cpuemu
SRC		= cpuemu.cpp
CC		= clang++
CFLAGS		= -g -Wall -Wextra


.PHONY: default
default: all

.PHONY: all
all: clean $(PROJECT_NAME)

.PHONY: $(PROJECT_NAME)
$(PROJECT_NAME): $(SRC)
	@$(CC) $(CFLAGS) -o $(PROJECT_NAME) $(SRC)
	@./$(PROJECT_NAME)

.PHONY: clean
clean:
	@rm -f $(PROJECT_NAME)