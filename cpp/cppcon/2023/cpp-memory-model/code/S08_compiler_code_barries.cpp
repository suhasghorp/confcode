// Thread 1
value = very_long_calc()
asm volatile("mfence" ::: "memory")
done = true

// Thread2
while (not done){}
asm volatile("mfence" ::: "memory")
//do something
