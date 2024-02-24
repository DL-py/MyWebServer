TARGET = myhttp
GCC = gcc
GXX = g++
OBJ = main.o http_conn.o ConnPool.o
CFLAGS = -g
SHARED_LIBS = -lpthread -lmysqlclient

CPP_SRC = main.cpp \
	  http_conn.cpp \
	  ConnPool.cpp 

C_SRC = user_handle.c \
		cgi_exec.c \
		./logs/run_log.c \
		./xalgorithm/x_file.c
	  

CPP_OBJS    = $(patsubst %.c, %.o,  $(CPP_SRC))
C_OBJS = $(patsubst %.c, %.o,  $(C_SRC))

debug: $(CPP_OBJS) $(C_OBJS)
	$(GXX) -o $(TARGET) $(CFLAGS) $^ $(SHARED_LIBS)

%.o: %.c
	$(GCC) $(CFLAGS) $(SHARED_LIBS) -c $< -o $@

%.o: %.cpp
	$(GXX) $(CFLAGS) $(SHARED_LIBS) -c $< -o $@
# debug: $(SRC)
# 	$(CC) $(SRC) $(CFLAGS) $(SHARED_LIBS) -o $(TARGET)


.PHONY:
clean:
	rm -rf *.o $(TARGET)

