int main(void) {
    *((unsigned int *)0x400FE608U) = 0x20U; // enable clock for Port F
    *((unsigned int *)0x40025400U) = 0x0EU; // set PF1, PF2, PF3 as output (LEDs connected to these pins)
    *((unsigned int *)0x4002551CU) = 0x0EU; // digital enable for PF1, PF2, PF3

    while (1) {
        *((unsigned int *)0x400253FCU) = 0x02U; // turn on the LED connected to PF1

        int counter = 0;
        while (counter < 1000000) { // simple delay loop
            ++counter;
        }

        *((unsigned int *)0x400253FCU) = 0x00U; // turn off the LED
        counter = 0;
        while (counter < 1000000) {
            ++counter; // simple delay loop
        }

    }
    //return 0;
}
