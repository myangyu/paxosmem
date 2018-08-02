
install:
	gcc  -g server.c -o server ./lib/str.c  ./lib/GetConfig.c  -levent   
	gcc client.c -o client
