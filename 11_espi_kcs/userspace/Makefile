CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Werror

PROGRAMS := kcs_inventory kcs_lab_responder

.PHONY: all clean

all: $(PROGRAMS)

kcs_inventory: kcs_inventory.c
	$(CC) $(CFLAGS) -o $@ $<

kcs_lab_responder: kcs_lab_responder.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	$(RM) $(PROGRAMS)

