CC=g++
CFLAGS=-g
BIN=loadbalance
OBJS=loadbalance.o sysutil.o log.o mgr.o fdwrapper.o conn.o
LIBS=-L. -ltinyxml

$(BIN):$(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)
%.o:%.cpp
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY:clean
clean:
	rm -f *.o $(BIN)
