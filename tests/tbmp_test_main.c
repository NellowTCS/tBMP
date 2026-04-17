#include <stdio.h>
#include "test_suites.h"

int g_pass = 0;
int g_fail = 0;
const char *g_current_suite = "";

int main(void) {
    test_reader();
    test_pixel_formats();
    test_decode_raw();
    test_decode_zero_range();
    test_decode_rle();
    test_decode_span();
    test_decode_sparse_pixel();
    test_decode_block_sparse();
    test_rotate_exact();
    test_rotate_arbitrary();
    test_writer_roundtrip();
    test_edge_cases();
    test_meta();
    test_fuzzing();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
