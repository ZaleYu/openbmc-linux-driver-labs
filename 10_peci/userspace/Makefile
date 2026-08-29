CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Werror

PROGRAMS := peci_inventory peci_hwmon_dump

.PHONY: all clean

all: $(PROGRAMS)

peci_inventory: peci_inventory.c
	$(CC) $(CFLAGS) -o $@ $<

peci_hwmon_dump: peci_hwmon_dump.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	$(RM) $(PROGRAMS)

