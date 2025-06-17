CC = gcc
CFLAGS = -g -Wall -Iinclude
LDFLAGS = -g
#paths
SRC_DIR = server/src
BIN_DIR = server/src
OBJ_DIR = obj

MANAGER_SRC = $(SRC_DIR)/nfs_manager.c $(SRC_DIR)/process_comm.c $(SRC_DIR)/timer.c $(SRC_DIR)/queue.c $(SRC_DIR)/parse_config.c
CONSOLE_SRC = console/src/nfs_console.c
CLIENT_SRC = $(SRC_DIR)/nfs_client.c

MANAGER_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(MANAGER_SRC))
CONSOLE_OBJ = $(OBJ_DIR)/nfs_console.o
CLIENT_OBJ = $(OBJ_DIR)/nfs_client.o

#execs 
MANAGER = $(BIN_DIR)/nfs_manager
CONSOLE = console/src/nfs_console
CLIENT = $(BIN_DIR)/nfs_client

#rules
all: $(OBJ_DIR) $(MANAGER) $(CONSOLE) $(CLIENT)
nfs_manager: $(OBJ_DIR) $(MANAGER)
nfs_console: $(OBJ_DIR) $(CONSOLE)
nfs_client: $(OBJ_DIR) $(CLIENT)

$(OBJ_DIR):
		mkdir -p $(OBJ_DIR)

#compile
$(OBJ_DIR)/%.o: console/src/%.c
		$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
		$(CC) $(CFLAGS) -c $< -o $@

$(MANAGER): $(MANAGER_OBJS)
		$(CC) $(LDFLAGS) $^ -o $@ -lpthread

$(CONSOLE): $(CONSOLE_OBJ)
		$(CC) $(LDFLAGS) $^ -o $@

$(CLIENT): $(CLIENT_OBJ)
		$(CC) $(LDFLAGS) $^ -o $@ -lpthread

clean:
	rm -rf $(OBJ_DIR) $(MANAGER) $(CONSOLE) $(CLIENT)