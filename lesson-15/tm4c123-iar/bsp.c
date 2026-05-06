/* Board Support Package */
#include "tm4c_cmsis.h"


/// assert_failed is called on assertion failure. See "assert_param" macro in "tm4c_cmsis.h".
__stackless void assert_failed (char const *file, int line) {
  /* TBD: damage control */
  NVIC_SystemReset(); /* reset the system */
}

/// The minimal vector table for a Cortex M3. Note that the proper constructs must be placed on this to ensure that it ends up at physical address 0x00000000.
/// The actual reset handler is defined in "startup_tm4c.c".
void SysTick_Handler(void) {
}