CCFLAGS=
LDFLAGS=

all: out p61conv

out:
	mkdir out

clean:
	rm -rf out p61conv

p61conv: out/main.o out/protracker.o out/debug.o out/buffer.o out/converter.o out/player61a.o
	$(CC) -o $@ $^ $(LDFLAGS)

out/%.o: src/%.c
	$(CC) -c -o $@ $(CCFLAGS) $<

out/main.o: src/main.c src/protracker.h src/converter.h src/buffer.h
out/protracker.o: src/protracker.c src/protracker.h src/debug.h src/buffer.h
out/player61a.o: src/player61a.c src/player61a.h src/protracker.h
out/debug.o: src/debug.c src/debug.h
out/converter.o: src/converter.c src/converter.h src/buffer.h
out/buffer.o: src/buffer.c src/buffer.h

