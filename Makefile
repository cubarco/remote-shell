all: src/bin/rmshell

src/bin/rmshell: src/rmshell.c
	mkdir -p ./src/bin
	gcc -W -Wall ./src/rmshell.c -o ./src/bin/rmshell

clean:
	rm -r ./src/bin/
