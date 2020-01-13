# Makefile for libiEQ30

CC = gcc
CFLAGS = -fPIC -Wall -Wextra -O2 -g -DSB_LINUX_BUILD -I. -I./../../
CPPFLAGS = -fPIC -Wall -Wextra -O2 -g -DSB_LINUX_BUILD -I. -I./../../
LDFLAGS = -shared -lstdc++
RM = rm -f
TARGET_LIB = libSkywatcher.so
TARGET_UI = "SkywatcherWiFi.ui"
TARGET_LIST = "mountlist Skywatcher.txt"

SRCS = Skywatcher.cpp main.cpp x2mount.cpp
OBJS = $(SRCS:.cpp=.o)

.PHONY: all
all: ${TARGET_LIB}

$(TARGET_LIB): $(OBJS)
	$(CC) ${LDFLAGS} -o $@ $^

$(SRCS:.cpp=.d):%.d:%.cpp
	$(CC) $(CFLAGS) $(CPPFLAGS) -MM $< >$@

include $(SRCS:.cpp=.d)

.PHONY: clean
clean:
	-${RM} ${TARGET_LIB} ${OBJS} $(SRCS:.cpp=.d)

.PHONY: install
install: $(TARGET_LIB)
	./installer/install.sh

.PHONY: distribution
distribution: $(TARGET_LIB)
	cp ${TARGET_LIB} ${TARGET_UI} ${TARGET_LIST} Install distribution



