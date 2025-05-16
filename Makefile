PROGNAME = bfinterpreter
CC = gcc
RC = windres
LD = $(CC)

CFLAGS = -Wall -Wextra -g -std=c11
LDFLAGS = -mwindows -lcomctl32 -lgdi32 -luser32 -lkernel32 -lcomdlg32

RM = rm -f
BINEXT = .exe
OBJEXT = .o

SRC = bf.c
RES_SCRIPT = bf.rc

C_OBJ = $(SRC:.c=.$(OBJEXT))
RES_OBJ = $(RES_SCRIPT:.rc=.res.$(OBJEXT))
OBJ = $(C_OBJ) $(RES_OBJ)

all: $(PROGNAME)$(BINEXT)

%.$(OBJEXT): %.c bf.h
	$(CC) $(CFLAGS) -c $< -o $@

%.res.$(OBJEXT): %.rc bf.h
	$(RC) $< -O coff -o $@

$(PROGNAME)$(BINEXT): $(OBJ)
	$(LD) -o $@ $(OBJ) $(LDFLAGS)

clean:
	$(RM) $(PROGNAME)$(BINEXT) *.$(OBJEXT) *.res.$(OBJEXT)

.PHONY: all clean

