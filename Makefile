CCFLAGS=
LDFLAGS=

all: out modpack

out:
	mkdir out

clean:
	rm -rf out modpack

modpack: out/main.o out/protracker.o out/player61a.o out/log.o out/buffer.o out/options.o
	$(CC) -o $@ $^ $(LDFLAGS)

out/%.o: src/%.c
	$(CC) -c -o $@ $(CCFLAGS) $<

out/readme.h: README.md
	cat $< | tr "\`" " " | xxd -i > $@

SHARED_HEADERS=src/buffer.h src/log.h src/options.h

out/main.o: src/main.c src/protracker.h out/readme.h $(SHARED_HEADERS)
out/protracker.o: src/protracker.c src/protracker.h $(SHARED_HEADERS)
out/player61a.o: src/player61a.c src/player61a.h src/protracker.h $(SHARED_HEADERS)
out/log.o: src/log.c src/log.h
out/buffer.o: src/buffer.c src/buffer.h
out/options.o: src/options.c src/options.h

