# Hisilicon Hi3535 sample Makefile
#  
#  include ../Makefile.param
#   
#   # target source
SRC  := $(wildcard *.c) 
OBJ  := $(SRC:%.c=%.o)
 
TARGET := helloworld
.PHONY : clean all
	 
all: $(TARGET)
	 
MPI_LIBS := $(REL_LIB)/libmpi.a
MPI_LIBS += $(REL_LIB)/libhdmi.a
CFLAGS += -I./include 
 
$(TARGET):$(OBJ)
		$(CC) $(CFLAGS) -lpthread -lm -o $@ $^ $(MPI_LIBS) $(AUDIO_LIBA) -L./lib -lminigui_ths
		 
%.o:%.c
		$(CC) -c $(CFLAGS) $^ -o $@
			
clean:
		@rm -f $(TARGET)
		@rm -f $(OBJ)


