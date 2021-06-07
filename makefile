all: shell352

shell352: project1.c
	gcc -o shell352 project1.c

clean:
	rm -f shell352 *.txt *~
