#ifndef TBMP_CLI_IMGIO_H
#define TBMP_CLI_IMGIO_H

#include "tbmp_types.h"

TBmpRGBA *tbmp_cli_img_load(const char *path, int *out_w, int *out_h,
                            int *out_channels, int *out_uses_stb);

void tbmp_cli_img_free(TBmpRGBA *pixels, int uses_stb);

#endif /* TBMP_CLI_IMGIO_H */
