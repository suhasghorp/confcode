// Thread 1
flag1 = 1;
if (flag2 == 0){
    critical section}

// Thread 2
flag2 = 1;
if (flag1 == 0){
    critical section}
