CCFLAGS=
LDFLAGS=

all: out modpack

out:
	mkdir out

clean:
	rm -rf out modpack

modpack: out/main.o out/protracker.o out/log.o out/buffer.o out/converter.o out/player61a.o
	$(CC) -o $@ $^ $(LDFLAGS)

out/%.o: src/%.c
	$(CC) -c -o $@ $(CCFLAGS) $<

out/main.o: src/main.c src/protracker.h src/converter.h src/buffer.h src/log.h
out/protracker.o: src/protracker.c src/protracker.h src/buffer.h src/log.h
out/player61a.o: src/player61a.c src/player61a.h src/protracker.h src/log.h
out/log.o: src/log.c src/log.h
out/converter.o: src/converter.c src/converter.h src/buffer.h src/log.h
out/buffer.o: src/buffer.c src/buffer.h

