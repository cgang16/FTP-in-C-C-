server: func.o Usr.o server.o 
	gcc -o server func.o Usr.o server.o -lpthread
func.o: func.c func.h
	gcc -c func.c
Usr.o: Usr.c Usr.h
	gcc -c Usr.c
server.o: server.c 
	gcc -c server.c -lpthread
clean:
	rm *.o server
