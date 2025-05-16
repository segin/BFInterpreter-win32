PROGNAME = bfinterpreter
CC = gcc
RC = windres
LD = $(CC)

CFLAGS = -Wall -Wextra -g -std=c11
LDFLAGS = -mwindows -lcomctl32 -lgdi32 -luser32 -lkernel32 -lcomdlg32

RM = rm -f
BINEXT = .exe
OBJEXT = o # Corrected: No leading dot

SRC = bf.c
RES_SCRIPT = bf.rc

# Correctly appends .o (e.g., bf.o)
C_OBJ = $(SRC:.c=.$(OBJEXT))
# Correctly appends .res.o (e.g., bf.res.o)
RES_OBJ = $(RES_SCRIPT:.rc=.res.$(OBJEXT))
OBJ = $(C_OBJ) $(RES_OBJ)

all: $(PROGNAME)$(BINEXT)

# Pattern rule for .c to .o files
%.$(OBJEXT): %.c bf.h
	$(CC) $(CFLAGS) -c $< -o $@

# Pattern rule for .rc to .res.o files
%.res.$(OBJEXT): %.rc bf.h
	$(RC) $< -O coff -o $@

$(PROGNAME)$(BINEXT): $(OBJ)
	$(LD) -o $@ $(OBJ) $(LDFLAGS)

clean:
	$(RM) $(PROGNAME)$(BINEXT) *.$(OBJEXT) *.res.$(OBJEXT)

.PHONY: all clean
