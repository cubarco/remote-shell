all:
	mkdir -p ./src/bin
	gcc ./src/rmshell.c -o ./src/bin/rmshell

clean:
	rm -r ./src/bin/
