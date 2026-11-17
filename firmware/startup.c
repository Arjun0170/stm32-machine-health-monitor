#include <stdint.h>
extern uint32_t _estack, _sidata, _sdata, _edata, _sbss, _ebss;
extern int main(void);
void SysTick_Handler(void);
void Default_Handler(void) {
    for (;;) {
    }
}
void Reset_Handler(void) {
    uint32_t *src = &_sidata;
    for (uint32_t *p = &_sdata; p < &_edata;)
        *p++ = *src++;
    for (uint32_t *p = &_sbss; p < &_ebss;)
        *p++ = 0;
    (void)main();
    for (;;) {
    }
}
__attribute__((section(".vectors"), used)) void (*const vectors[16])(void) = {
    (void (*)(void))&_estack,
    Reset_Handler,
    Default_Handler,
    Default_Handler,
    Default_Handler,
    Default_Handler,
    Default_Handler,
    0,
    0,
    0,
    0,
    Default_Handler,
    Default_Handler,
    0,
    Default_Handler,
    SysTick_Handler};
