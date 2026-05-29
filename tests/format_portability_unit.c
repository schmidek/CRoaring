/*
 * format_portability_unit.c
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <roaring/roaring.h>
#include <roaring/misc/configreport.h>

#include "config.h"

#include "test.h"


long filesize(char const* path) {
    FILE* fp = fopen(path, "rb");
    assert_non_null(fp);

    assert_int_not_equal(fseek(fp, 0L, SEEK_END), -1);

    return ftell(fp);
}

char* readfile(char const* path) {
    FILE* fp = fopen(path, "rb");
    assert_non_null(fp);

    assert_int_not_equal(fseek(fp, 0L, SEEK_END), -1);

    long bytes = ftell(fp);
    char* buf = (char*)malloc(bytes);
    assert_non_null(buf);

    rewind(fp);
    assert_int_equal(bytes, fread(buf, 1, bytes, fp));

    fclose(fp);
    return buf;
}

int compare(char* x, char* y, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        if (x[i] != y[i]) {
            return i + 1;
        }
    }
    return 0;
}

void test_deserialize(char* filename) {
    char* input_buffer = readfile(filename);
    assert_non_null(input_buffer);

    roaring_bitmap_t* bitmap =
        roaring_bitmap_portable_deserialize(input_buffer);
    assert_non_null(bitmap);

    size_t expected_size = roaring_bitmap_portable_size_in_bytes(bitmap);

    assert_int_equal(expected_size, filesize(filename));

    char* output_buffer = (char*)malloc(expected_size);
    size_t actual_size =
        roaring_bitmap_portable_serialize(bitmap, output_buffer);

    assert_int_equal(actual_size, expected_size);
    assert_false(compare(input_buffer, output_buffer, actual_size));

    free(output_buffer);
    free(input_buffer);
    roaring_bitmap_free(bitmap);
}

void check_portable_deserialize_cardinality(const roaring_bitmap_t* bitmap) {
    size_t size = roaring_bitmap_portable_size_in_bytes(bitmap);
    char* buffer = (char*)malloc(size);
    assert_non_null(buffer);
    assert_int_equal(roaring_bitmap_portable_serialize(bitmap, buffer), size);

    uint64_t expected = roaring_bitmap_get_cardinality(bitmap);

    uint64_t card = UINT64_MAX;
    assert_true(roaring_bitmap_portable_deserialize_cardinality(buffer, size,
                                                                &card));
    assert_int_equal(card, expected);

    free(buffer);
}

DEFINE_TEST(test_portable_deserialize_cardinality_array) {
    roaring_bitmap_t* r = roaring_bitmap_create();
    for (uint32_t i = 0; i < 100; ++i) {
        roaring_bitmap_add(r, i * 7);
    }
    check_portable_deserialize_cardinality(r);
    roaring_bitmap_free(r);
}

DEFINE_TEST(test_portable_deserialize_cardinality_bitset) {
    roaring_bitmap_t* r = roaring_bitmap_create();
    for (uint32_t i = 0; i < 10000; ++i) {
        roaring_bitmap_add(r, i);
    }
    check_portable_deserialize_cardinality(r);
    roaring_bitmap_free(r);
}

DEFINE_TEST(test_portable_deserialize_cardinality_run) {
    roaring_bitmap_t* r = roaring_bitmap_create();
    roaring_bitmap_add_range_closed(r, 0, 1000);
    roaring_bitmap_add_range_closed(r, 100000, 200000);
    roaring_bitmap_run_optimize(r);
    check_portable_deserialize_cardinality(r);
    roaring_bitmap_free(r);
}

DEFINE_TEST(test_portable_deserialize_cardinality_mixed) {
    roaring_bitmap_t* r = roaring_bitmap_create();
    // array container
    for (uint32_t i = 0; i < 100; ++i) {
        roaring_bitmap_add(r, i);
    }
    // bitset container
    for (uint32_t i = 0; i < 10000; ++i) {
        roaring_bitmap_add(r, (1u << 16) + i);
    }
    // run container
    roaring_bitmap_add_range_closed(r, (2u << 16), (2u << 16) + 5000);
    roaring_bitmap_run_optimize(r);
    // singleton high key
    roaring_bitmap_add(r, (10u << 16) + 42);
    check_portable_deserialize_cardinality(r);
    roaring_bitmap_free(r);
}

DEFINE_TEST(test_portable_deserialize_cardinality_empty) {
    roaring_bitmap_t* r = roaring_bitmap_create();
    check_portable_deserialize_cardinality(r);
    roaring_bitmap_free(r);
}

DEFINE_TEST(test_deserialize_portable_norun) {
    char filename[1024];

    strcpy(filename, TEST_DATA_DIR);
    strcat(filename, "bitmapwithoutruns.bin");

    test_deserialize(filename);
}

DEFINE_TEST(test_deserialize_portable_wrun) {
    char filename[1024];

    strcpy(filename, TEST_DATA_DIR);
    strcat(filename, "bitmapwithruns.bin");

    test_deserialize(filename);
}

int main() {
    tellmeall();

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_deserialize_portable_norun),
        cmocka_unit_test(test_deserialize_portable_wrun),
        cmocka_unit_test(test_portable_deserialize_cardinality_array),
        cmocka_unit_test(test_portable_deserialize_cardinality_bitset),
        cmocka_unit_test(test_portable_deserialize_cardinality_run),
        cmocka_unit_test(test_portable_deserialize_cardinality_mixed),
        cmocka_unit_test(test_portable_deserialize_cardinality_empty),
        cmocka_unit_test(test_portable_deserialize_cardinality_invalid),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
