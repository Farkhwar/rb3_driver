LDLIBS += -lusb-1.0 -lportmidi
LDFLAGS = -L /usr/local/lib
INC_DIR += -I/usr/local/include -I/usr/local/include/libusb-1.0
CC=gcc
SOURCES=rb3_driver.c myusb_utils.c myusb_atexit.c my_atexit.c
OBJECTS=$(SOURCES:.c=.o c)
EXECUTABLE=rb3_driver

all: $(EXECUTABLE)
    
$(EXECUTABLE): $(SOURCES) 
	$(CC) $^ $(LDFLAGS) $(LDLIBS) $(INC_DIR) -o $@

clean:
	rm rb3_driver
