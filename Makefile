.DEFAULT_GOAL := xsetstatus

include config.mk

SRC = xsetstatus.cpp

clean:
	rm -f xsetstatus

xsetstatus:
	${CPPC} ${WFLAGS} ${CPPFLAGS} ${SRC} -o xsetstatus ${LIBS}

nox11:
	${CPPC} ${WFLAGS} ${CPPFLAGS} -DNO_X11 ${SRC} -o xsetstatus ${LIBS}

.PHONY: clean xsetstatus nox11
