all: draw-server

draw-server: ws_server.o ws_core.o
	gcc ws_server.o ws_core.o -o draw-server -lssl -lcrypto -pthread
	
ws_server.o: ws_server.c
	gcc ws_server.c -c -o ws_server.o -pthread
	
ws_core.o: ws_core.c
	gcc ws_core.c -c -o ws_core.o

clean:
	rm *.o draw-server

