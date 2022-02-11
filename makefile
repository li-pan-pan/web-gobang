BIN=WebGobang
LDPATH=-L /usr/lib64/mysql
LDFLAG=-lpthread -ljsoncpp -lmysqlclient -lcrypto

$(BIN):webgobang.cpp
	g++ $^ -o $@ $(LDPATH) $(LDFLAG)

.PHONY:clean
clean:
	rm $(BIN)
