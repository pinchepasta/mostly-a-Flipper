#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Mock the slot structure and capture function interface */
#define SLOT_DATA_SIZE 256

typedef struct {
    uint8_t data[SLOT_DATA_SIZE];
    uint16_t len;
} capture_slot_t;

/* External declaration - the actual function should validate length */
extern int capture_hs_store(capture_slot_t *slot, const uint8_t *payload, uint16_t len);

START_TEST(test_capture_hs_buffer_overflow)
{
    /* Invariant: Buffer reads/writes never exceed SLOT_DATA_SIZE */
    const uint16_t test_lengths[] = {
        SLOT_DATA_SIZE * 2,    /* 2x overflow - exploit case */
        SLOT_DATA_SIZE * 10,   /* 10x overflow - extreme case */
        SLOT_DATA_SIZE + 1,    /* Boundary: just over limit */
        SLOT_DATA_SIZE,        /* Boundary: exactly at limit */
        128                    /* Valid input within bounds */
    };
    int num_tests = sizeof(test_lengths) / sizeof(test_lengths[0]);

    for (int i = 0; i < num_tests; i++) {
        capture_slot_t slot;
        uint8_t *payload = malloc(test_lengths[i]);
        ck_assert_ptr_nonnull(payload);
        memset(payload, 'A', test_lengths[i]);
        memset(&slot, 0, sizeof(slot));

        int result = capture_hs_store(&slot, payload, test_lengths[i]);

        /* Invariant: stored length must never exceed buffer size */
        ck_assert_msg(slot.len <= SLOT_DATA_SIZE,
            "Buffer overflow: len=%u exceeds max=%u (input=%u)",
            slot.len, SLOT_DATA_SIZE, test_lengths[i]);

        /* If oversized, function should reject or truncate */
        if (test_lengths[i] > SLOT_DATA_SIZE) {
            ck_assert_msg(result != 0 || slot.len <= SLOT_DATA_SIZE,
                "Oversized input not rejected/truncated");
        }

        free(payload);
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s = suite_create("Security");
    TCase *tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_capture_hs_buffer_overflow);
    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = security_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}