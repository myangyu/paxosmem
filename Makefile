
install:
	gcc -g main.c -o server ./lib/str.c  -levent   
	gcc client.c -o client
