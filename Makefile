PREFIX=/usr/local

CC=gcc
CFLAGS=-O2 -g -Wall -Wextra -Wno-unused -std=c99 -D_GNU_SOURCE
RAGEL=ragel
RLGEN=rlgen-cd
RLGENFLAGS=-G2

RAGELS=igc_record_parse_b.rl igc_record_parse_hfdte.rl
RAGEL_SRCS=$(RAGELS:%.rl=%.c)
SRCS=frcfd.c maxxc.c result.c track.c $(RAGEL_SRCS)
HEADERS=maxxc.h frcfd.h
OBJS=$(SRCS:%.c=%.o)
LIBS=-lm
BINS=maxxc
DOCS=COPYING

.PHONY: all clean install reallyclean tarball
.PRECIOUS: $(RAGEL_SRCS)

all: $(BINS)

tarball: $(RAGEL_SRCS)
	mkdir maxxc-$(VERSION)
	cp Makefile $(SRCS) $(HEADERS) $(DOCS) maxxc-$(VERSION)
	tar -czf maxxc-$(VERSION).tar.gz maxxc-$(VERSION)
	rm -Rf maxxc-$(VERSION)

install: $(BINS)
	@echo "  INSTALL maxxc"
	@mkdir -p $(PREFIX)/bin
	@cp maxxc $(PREFIX)/bin/maxxc

maxxc: $(OBJS)

reallyclean: clean
	@echo "  CLEAN   $(RAGEL_SRCS)"
	@rm -f $(RAGEL_SRCS)

clean:
	@echo "  CLEAN   $(BINS) $(OBJS)"
	@rm -f $(BINS) $(OBJS)

%.c: %.rl
	@echo "  RAGEL   $<"
	@$(RAGEL) $< | $(RLGEN) -o $@ $(RLGENFLAGS)

%.o: %.c $(HEADERS)
	@echo "  CC      $<"
	@$(CC) -c -o $@ $(CFLAGS) $<

%: %.o
	@echo "  LD      $<"
	@$(CC) -o $@ $(CFLAGS) $^ $(LIBS)
