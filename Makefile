all: cserver.c cclient.c
		gcc -o cserver cserver.c -lssl
		gcc -o cclient cclient.c -lssl
		
clean: 
		rm -f *.o cclient cserver
