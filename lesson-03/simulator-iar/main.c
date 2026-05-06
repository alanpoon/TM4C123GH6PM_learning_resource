int counter = 0;

int main(void) {
    int *p_int;
    p_int = &counter;
    while (*p_int < 21) {
        ++(*p_int);
    }

    p_int = (int *)0x20000002U;     // This is an invalid address for writing, as it's not properly aligned and may not be mapped to any valid memory region
    *p_int = 0xDEADBEEF;            // This will cause a hard fault since the address is not valid for writing

    return 0;
}
