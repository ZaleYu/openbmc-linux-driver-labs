CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Werror

PROGRAMS := mctp_request mctp_echo

.PHONY: all clean

all: $(PROGRAMS)

mctp_request: mctp_request.c
	$(CC) $(CFLAGS) -o $@ $<

mctp_echo: mctp_echo.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	$(RM) $(PROGRAMS)

