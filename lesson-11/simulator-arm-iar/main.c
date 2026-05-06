#include <stdint.h> // C99 standard integers

#include "tm4c.h"   // the same as "lm4f120h5qr.h" in the video
#include "delay.h"

#define LED_RED   (1U << 1)
#define LED_BLUE  (1U << 2)
#define LED_GREEN (1U << 3)

// uint8_t  u8a, u8b;      // unsigned 8-bit integers
// uint16_t u16c, u16d;    // unsigned 16-bit integers
// uint32_t u32e, u32f;    // unsigned 32-bit integers

// int8_t  s8;
// int16_t s16;
// int32_t s32;

// uint32_t a[32];

struct Point {
    // doind a 2d point with 16-bit coordinates
    uint16_t x;
    uint8_t  y;
} Point; // typedef struct __packed Point Point; is equivalent to struct __packed Point { ... } Point;

Point p1, p2;

typedef struct {
    Point top_left;
    Point bottom_right;
} Window;

typedef struct {
    Point corners[3];
} Triangle;

Window w;
Triangle t;

int main(void) {

    // u8a  = sizeof(u8a);     // sizeof operator returns the size of the type in bytes
    // u16c = sizeof(uint16_t);
    // u32e = sizeof(uint32_t);

    // u8a  = 0xa1U;
    // u16c = 0xc1c2U;
    // u32e = 0xe1e2e3e4U;

    // u8b  = u8a;
    // u16d = u16c;
    // u32f = u32e;

    // u16c = 40000U;
    // u16d = 30000U;
    // //u32e = u16c + u16d; // NOT portable!
    // u32e = (uint32_t)u16c + u16d; // cast to uint32_t to avoid overflow and ensure correct result

    // u16c = 100U;
    // //s32  = 10 - u16c;  // NOT portable!
    // //s32  = 10 - (int16_t)u16c; // INCORRECT: unintended sign extension
    // s32  = 10 - (int32_t)u16c;

    // //if (u32e > -1) {  // ALWAYS false!
    // if ((int32_t)u32e > -1) {
    //     u8a = 1U; // true
    // }
    // else {
    //     u8a = 0U; // false
    // }

    // u8a = 0xffU;
    // //if (~u8a == 0x00U) { // ALWAYS false!
    // if ((uint8_t)(~u8a) == 0x00U) {
    //     u8b = 1U; // true
    // }

    //pointers to structs
    Point *pp;
    Window *wp;


    //access the individual struct
    p1.c = sizeof(Point);
    p1.y = 0xAAU; // 0xAA is 170 in decimal
    w.top_left.x = 1U;
    w.bottom_right.y = 2U;

    t.corners[0].x = 1U;
    t.corners[2].y = 2U;

    p2 = p1; // struct assignment, copies all members of p1 to p2
    w2 = w;  // struct assignment, copies all members of w to w2

    pp = &p1; // pointer to struct, pp points to p1
    wp = &w2;  // pointer to struct, wp points to w

    (*pp).x = 1U; // dereference pointer to struct and access member, equivalent to pp->x = 3U;
   (*wp)->top_left = *pp; // arrow operator to access member of struct

    pp -> x = 1u;
    wp -> top_left = *pp;


    SYSCTL_GPIOHBCTL_R |= (1U << 5); /* enable AHB for GPIOF */
    SYSCTL_RCGCGPIO_R |= (1U << 5);  /* enable clock for GPIOF */
    GPIO_PORTF_AHB_DIR_R |= (LED_RED | LED_BLUE | LED_GREEN);
    GPIO_PORTF_AHB_DEN_R |= (LED_RED | LED_BLUE | LED_GREEN);

    /* turn all LEDs off */
    GPIOF_HS->DATA_BITS[LED_BLUE] = LED_BLUE; // alternative way to access the data register for LED_BLUE
    // GPIO_PORTF_AHB_DATA_BITS_R[LED_RED | LED_BLUE | LED_GREEN] = 0U;

    while (1) {
        GPIO_PORTF_AHB_DATA_BITS_R[LED_RED] = LED_RED;
        delay(500000);

        GPIO_PORTF_AHB_DATA_BITS_R[LED_RED] = 0;

        delay(500000);
    }
    //return 0; // unreachable code
}
