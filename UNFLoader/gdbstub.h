#ifndef __HELPER_GDBSTUB
#define __HELPER_GDBSTUB 

    void gdb_thread(char* addr);
    void gdb_connect(char* fulladdr);
    bool gdb_isconnected();
    void gdb_disconnect();

#endif