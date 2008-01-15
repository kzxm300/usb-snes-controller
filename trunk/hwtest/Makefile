
# predefined rule: $(CC) -c $(CPPFLAGS) $(CFLAGS)
# predefined rule: $(CC) $(LDFLAGS) n.o $(LOADLIBES) $(LDLIBS)

CC = C:\Programme\MCC18\bin\mcc18.exe
CPPFLAGS = -I=C:\Programme\MCC18\h
CFLAGS = -w=3 -p=18f2450
LD = C:\Programme\MCC18\bin\mplink.exe
LDCMDFILE = C:\Programme\MCC18\lkr\18f2450.lkr
LDLIBS =
LDFLAGS = /l C:\Programme\MCC18\lib

build/%.o : %.c
	@if not exist build mkdir build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -fo $@

%.hex :
	$(LD) $(LDCMDFILE) $+ $(LDLIBS) /o $@ /m $*.map $(LDFLAGS)

all : build/pintest_intrc.hex build/pintest_xtpll.hex

build/pintest_intrc.hex : build/pintest_intrc.o

build/pintest_xtpll.hex : build/pintest_xtpll.o

build/pintest_intrc.o   : pintest_intrc.c

build/pintest_xtpll.o   : pintest_xtpll.c

