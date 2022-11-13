C_SRC	=	$(sort $(wildcard *.c)) # Get all .c files and make them sorted 
OBJ	=	$(patsubst %.c, %.o, $(C_SRC)) # Replace .c to .o and get all object files

all:$(OBJ) #
# file = print.o

%.o:%.c
	${GCC}  ${CFLAG} -c $<

clean:
	$(shell rm *.o 2>/dev/null)