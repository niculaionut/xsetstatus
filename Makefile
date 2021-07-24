.DEFAULT_GOAL := xsetstatus

include config.mk

SRC = xsetstatus.cpp

clean:
	rm -f xsetstatus

xsetstatus:
	${CPPC} ${WFLAGS} ${CPPFLAGS} ${SRC} -o xsetstatus ${LIBS}

nox11:
	${CPPC} ${WFLAGS} ${NOX11FLAGS} ${SRC} -o xsetstatus ${LIBS}

.PHONY: clean xsetstatus nox11
