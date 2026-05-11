#include <event2/buffer.h>

#include <cstring>
#include <iostream>

int main(){
    evbuffer* buf1 = evbuffer_new();
    evbuffer* buf2 = evbuffer_new();

    if(!buf1 || !buf2){
        std::cerr << "failed to create evbuffer" << std::endl;
        if(buf1) evbuffer_free(buf1);
        if(buf2) evbuffer_free(buf2);
        return 1;
    }

    const char* msg1 = "hello evbuffer";
    evbuffer_add(buf1, msg1, std::strlen(msg1));

    std::cout << "[step1] buf1 length = " << evbuffer_get_length(buf1) << std::endl;

    char out[6] = {0};  // 读出前5个字节
    int removed = evbuffer_remove(buf1, out, 5);
    std::cout << "[step2] removed " << removed
              << "bytes, content = \"" << out << "\"" << std::endl;
    
    std::cout << "[step2] buf1 length = " << evbuffer_get_length(buf1) << std::endl;

    const char* msg2 = " + more data";
    evbuffer_add(buf1, msg2, std::strlen(msg2));

    std::cout << "[step3] after append, buf1 length = "
              << evbuffer_get_length(buf1) << std::endl;
    
    evbuffer_add_buffer(buf2, buf1);

    std::cout << "[step4] after move buf1 -> buf2" << std::endl;
    std::cout << "        buf1 length = " << evbuffer_get_length(buf1) << std::endl;
    std::cout << "        buf2 length = " << evbuffer_get_length(buf2) << std::endl;

    size_t buf2_len = evbuffer_get_length(buf2);
    char* all = new char[buf2_len + 1];
    std::memset(all, 0, buf2_len + 1);

    int total_removed = evbuffer_remove(buf2, all, buf2_len);
    std::cout << "[step5] buf2 content = \"" << all << "\"" << std::endl;
    std::cout << "[step5] removed " << total_removed << " bytes from buf2" << std::endl;

    delete[] all;

    std::cout << "[step5] buf2 length = " << evbuffer_get_length(buf2) << std::endl;

    evbuffer_add_printf(buf2, "temporary data");
    std::cout << "[step6] buf2 length before drain = " << evbuffer_get_length(buf2) << std::endl;

    evbuffer_drain(buf2, evbuffer_get_length(buf2));
    std::cout << "[step6] buf2 length after drain = " << evbuffer_get_length(buf2) << std::endl;

    evbuffer_free(buf1);
    evbuffer_free(buf2);

    return 0;

}