#include <stdio.h>
#include <string.h>
#include "test_suites.h"

int g_pass = 0;
int g_fail = 0;
const char *g_current_suite = "";

static void run_test(const char *name, void (*fn)(void)) {
    printf("=== %s ===\n", name);
    g_current_suite = name;
    fn();
}

int main(int argc, char *argv[]) {
    /* Default: run all tests */
    int run_all = 1;

    if (argc >= 3 && strcmp(argv[1], "--test") == 0) {
        run_all = 0;
        const char *target = argv[2];

        if (strcmp(target, "reader") == 0) run_test("reader", test_reader);
        else if (strcmp(target, "pixel") == 0) run_test("pixel", test_pixel_formats);
        else if (strcmp(target, "decode") == 0) {
            test_decode_raw();
            test_decode_zero_range();
            test_decode_rle();
            test_decode_span();
            test_decode_sparse_pixel();
            test_decode_block_sparse();
        } else if (strcmp(target, "rotate") == 0) {
            test_rotate_exact();
            test_rotate_arbitrary();
        } else if (strcmp(target, "write") == 0) run_test("write", test_writer_roundtrip);
        else if (strcmp(target, "edge") == 0) run_test("edge", test_edge_cases);
        else if (strcmp(target, "meta") == 0) run_test("meta", test_meta);
        else if (strcmp(target, "fuzz") == 0) run_test("fuzz", test_fuzzing);
        else {
            printf("Unknown test: %s\n", target);
            return 1;
        }
    }

    if (run_all) {
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
    }

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
