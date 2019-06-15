OBJ=hantek.o \
	hantek_main.o

TARGET=hantek

OFLAGS=-O0 -ggdb

LIBUSB_CFLAGS=`pkg-config --cflags libusb-1.0`
LIBUSB_LIBS=`pkg-config --libs libusb-1.0`

inc=$(OBJ:%.o=%.d)

CFLAGS=$(OFLAGS) -Wall -Wextra -Wundef -Wstrict-prototypes -Wmissing-prototypes -Wno-trigraphs \
	   -std=c11 -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration -Wuninitialized \
	   -Wmissing-include-dirs -Wshadow -Wframe-larger-than=2047 -D_GNU_SOURCE \
	   -I. $(LIBUSB_CFLAGS) $(DEFINES)
LDFLAGS=$(LIBUSB_LIBS)

$(TARGET): $(OBJ)
	$(CC) -o $(TARGET) $(OBJ) $(LDFLAGS)

-include $(inc)

.c.o:
	$(CC) $(CFLAGS) -MMD -MP -c $<

clean:
	$(RM) $(OBJ) $(TARGET)
	$(RM) $(inc)

.PHONY: clean
