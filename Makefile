
check: arena.c arena.h
	gcc -g -fsanitize=address -Wall -Werror $< -o $@

.PHONY: clean

clean:
	rm check
