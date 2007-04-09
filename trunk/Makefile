
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
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -fo $@

%.hex :
	$(LD) $(LDCMDFILE) $+ $(LDLIBS) /o $@ /m $*.map $(LDFLAGS)


build/main.hex : build/main.o build/debug.o