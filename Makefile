CCFLAGS=
LDFLAGS=

all: out out/libmodpack.a

out:
	mkdir out

clean:
	rm -rf out modpack

#modpack: out/main.o out/protracker.o out/player61a.o out/log.o out/buffer.o out/options.o
#	$(CC) -o $@ $^ $(LDFLAGS)

out/%.o: src/lib/%.c
	$(CC) -c -o $@ $(CCFLAGS) $<

#out/readme.h: README.md
#	cat $< | tr "\`" " " | xxd -i > $@

out/libmodpack.a: out/lib.o

out/lib.o: src/lib/lib.c
