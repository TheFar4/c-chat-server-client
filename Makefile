all: build

build:
	gcc session_server.c -o session_server
	gcc chat_coordinator.c -o chat_coordinator
	gcc chat_client.c -o chat_client

clean:
	rm session_server
	rm chat_coordinator
	rm chat_client
	rm -rf *~
