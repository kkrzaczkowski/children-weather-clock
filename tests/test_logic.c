#include <stdio.h>

#include "clock_logic.h"

int main(void)
{
    const char *error = clock_logic_self_test();
    if (error != NULL) {
        fprintf(stderr, "clock_logic_self_test: %s\n", error);
        return 1;
    }
    printf("clock_logic_self_test: ok\n");
    return 0;
}
