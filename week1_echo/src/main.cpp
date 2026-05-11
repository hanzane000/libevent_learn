#include "logger.h"
#include "server.h"

int main(){
    logger::info("program started");

    EchoServer server(5555);

    if(!server.init()){
        logger::error("server init failed");
        return 1;
    }

    server.run();

    logger::info("program finished");

    return 0;
}