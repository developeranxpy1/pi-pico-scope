#include <pico/stdio_usb.h>
#include <stdio.h>

#include "bsp.h"
#include "gfx.h"
#include "panel.h"
#include "scope.h"

int main(void)
{
    stdio_usb_init();
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("\n=== pi-pico-scope boot ===\n");

    bspInit();
    printf("bsp ok\n");

    scopeInit();
    printf("scopeInit ok\n");
    fflush(stdout);

    panelSelfTest();
    printf("self test done, entering main loop\n");

    while (1)
    {
        scopeLoop();
    }
}