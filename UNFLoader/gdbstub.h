#ifndef __HELPER_GDBSTUB
#define __HELPER_GDBSTUB 

    void gdb_thread(char* addr);
    void gdb_connect(char* fulladdr);
    void gdb_reply(char* reply);
    bool gdb_isconnected();
    void gdb_disconnect();

#endif