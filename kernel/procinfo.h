struct procinfo {
    int pid;
    int ppid;
    int state; // enum procstate value
    uint64 sz;
    char name[16];
};