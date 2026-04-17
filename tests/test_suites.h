#ifndef TBMP_TEST_SUITES_H
#define TBMP_TEST_SUITES_H

void test_reader(void);
void test_pixel_formats(void);
void test_decode_raw(void);
void test_decode_zero_range(void);
void test_decode_rle(void);
void test_decode_span(void);
void test_decode_sparse_pixel(void);
void test_decode_block_sparse(void);
void test_rotate_exact(void);
void test_rotate_arbitrary(void);
void test_writer_roundtrip(void);
void test_edge_cases(void);
void test_meta(void);
void test_fuzzing(void);

#endif /* TBMP_TEST_SUITES_H */
