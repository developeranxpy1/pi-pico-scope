#include <pico/stdio_usb.h>

#include "bsp.h"
#include "panel.h"
#include "scope.h"

int main(void)
{
    stdio_usb_init();
    bspInit();

#ifdef PI_PICO_SCOPE_PANEL_PROBE
    panelProbe();
    while (1)
    {
    }
#endif

    scopeInit();

    while (1)
        scopeLoop();
}
