#ifndef TBMP_CLI_META_H
#define TBMP_CLI_META_H

#include "tbmp_types.h"

int tbmp_cli_parse_encode_meta_flags(int argc, char **argv, TBmpMeta *meta,
                                     int *has_meta);

uint32_t tbmp_cli_print_meta(const TBmpImage *img);

#endif /* TBMP_CLI_META_H */
