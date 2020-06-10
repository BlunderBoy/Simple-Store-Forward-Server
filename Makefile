build:
	g++ -Wall -g subscriber.cpp -o subscriber
	g++ -Wall -g server.cpp -o server

rserver:
	./server ${PORT}

client: 
	./subscriber ${IP_SERVER} ${PORT}

clean:
	rm -rf subscriber 
	rm -rf client