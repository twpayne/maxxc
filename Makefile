PREFIX=/usr/local

CC=gcc
CFLAGS=-g -Wall -Wextra -Wno-unused -std=c99 -D_GNU_SOURCE

SRCS=frcfd.c maxxc.c result.c track.c
HEADERS=maxxc.h frcfd.h
OBJS=$(SRCS:%.c=%.o)
LIBS=-lm
BINS=maxxc
DOCS=COPYING

.PHONY: all clean install reallyclean tarball

all: $(BINS)

tarball:
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

%.o: %.c $(HEADERS)
	@echo "  CC      $<"
	@$(CC) -c -o $@ $(CFLAGS) $<

%: %.o
	@echo "  LD      $<"
	@$(CC) -o $@ $(CFLAGS) $^ $(LIBS)
