CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS = -I/usr/include/freetype2 -lX11 -lXft -lXrender -lXext  
SRC = emenu.c
BIN = emenu
INSTALL_DIR = /usr/local/bin
TARGET = $(INSTALL_DIR)/$(BIN)

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LDFLAGS)

install: $(BIN)
	install -m 755 $(BIN) $(TARGET)

clean:
	@rm -f $(BIN)

clean-install: clean install

.PHONY: all install clean clean-install

