#include "bsp.h"
#include "gfx.h"
#include "scope.h"

int main(void)
{
    bspInit();
    scopeInit();

    while (1)
    {
        scopeLoop();
    }
}