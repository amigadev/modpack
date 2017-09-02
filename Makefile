CCFLAGS=
LDFLAGS=

all: out p61conv

out:
	mkdir out

clean:
	rm -rf out p61conv

p61conv: out/main.o out/protracker.o out/debug.o
	$(CC) -o $@ $^ $(LDFLAGS)

out/%.o: src/%.c
	$(CC) -c -o $@ $(CCFLAGS) $<

out/main.o: src/main.c src/protracker.h
out/protracker.o: src/protracker.c src/protracker.h src/debug.h
out/debug.o: src/debug.c src/debug.h

