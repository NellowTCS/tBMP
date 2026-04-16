#ifndef TBMP_CLI_PNGIO_H
#define TBMP_CLI_PNGIO_H

#include <stdint.h>

int tbmp_cli_write_png_rgba(const char *path, int width, int height,
                            const uint8_t *rgba);

#endif /* TBMP_CLI_PNGIO_H */
