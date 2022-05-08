TARGET=jsonp

CC=gcc
LEX=flex
LEX_LIB=-lfl

LIBS=/usr/local/lib/libstr.a
BIN=/usr/local/bin/

ifneq "$(CYGWIN)" ""
YY_SIZE_T=
SUDO=
else
YY_SIZE_T=-DYY_SIZE_T
SUDO=sudo
endif

$(TARGET): $(TARGET).o
	ln -nfs $< $@

%.o: %.lex.c %.parse.c %.tab.h
	$(CC) $(YY_SIZE_T) -o $@ $^ $(LEX_LIB) $(LIBS)

%.lex.c: %.l %.tab.h
	$(LEX) -o $(patsubst %.l,%.lex.c,$<) $<
	#cp $(TARGET).lex.c bu/

install:
	$(SUDO) cp -v --update --preserve=all $(TARGET) $(BIN)

.PHONY: clean

clean:
	rm -v -f $(TARGET).o.stackdump *.lex.c *.o $(TARGET)
