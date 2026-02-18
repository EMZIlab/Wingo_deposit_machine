
CC?=gcc
SRC_DIR:=src
BUILD_DIR:=build
BIN_DIR:=bin
TARGET:=$(BIN_DIR)/run

PKGS:= libgpiod

SRC:=$(shell find $(SRC_DIR) -type f -name '*.c')
INCLUDE_DIRS:=$(shell find $(SRC_DIR) -type d)
OBJ:=$(patsubst %.c,$(BUILD_DIR)/%.o,$(SRC))
DEP:=$(OBJ:.o=.d)

CPPFLAGS+=$(addprefix -I,$(INCLUDE_DIRS)) -MMD -MP -Iexternal/clay
CFLAGS+=-D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -std=c17 -O2 -Wall -Wextra -Wshadow -Wconversion -Wundef \
        $(shell pkg-config --cflags $(PKGS)) -pthread
LDLIBS+=$(shell pkg-config --libs $(PKGS)) -lm -pthread -latomic

.PHONY:all clean
all:$(TARGET)

$(TARGET):$(OBJ) | $(BIN_DIR)
	$(CC) -o $@ $^ $(LDLIBS)

$(BUILD_DIR) $(BIN_DIR):
	@mkdir -p $@

$(BUILD_DIR)/%.o:%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	$(RM) -r $(BUILD_DIR) $(BIN_DIR)

-include $(DEP)
