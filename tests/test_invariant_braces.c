#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>

/* Declaration of the actual production function from braces.c */
extern char **brace_expand(const char *str);

#define MAX_RESULTS 100000
#define TIMEOUT_SECONDS 5

static volatile int timed_out = 0;

static void handle_alarm(int sig) {
    (void)sig;
    timed_out = 1;
}

START_TEST(test_brace_expansion_bounded)
{
    /* Invariant: brace expansion must not produce unbounded results
     * or consume unbounded memory/CPU for any input */
    const char *payloads[] = {
        "{a,b}{c,d}{e,f}{g,h}{i,j}{k,l}{m,n}{o,p}{q,r}{s,t}",  /* exploit: exponential */
        "{a,b}{c,d}{e,f}",                                         /* boundary: moderate */
        "{a,b}{a,b}{a,b}{a,b}{a,b}{a,b}{a,b}{a,b}{a,b}{a,b}{a,b}{a,b}{a,b}{a,b}{a,b}{a,b}{a,b}",  /* over-limit: 2^17=131072 > BRACE_EXPANSION_LIMIT */
        "{hello,world}",                                            /* valid: simple */
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        timed_out = 0;
        signal(SIGALRM, handle_alarm);
        struct itimerval timer = {
            .it_value = { .tv_sec = TIMEOUT_SECONDS, .tv_usec = 0 },
            .it_interval = { .tv_sec = 0, .tv_usec = 0 }
        };
        setitimer(ITIMER_REAL, &timer, NULL);

        char **results = brace_expand(payloads[i]);

        /* Cancel timer */
        struct itimerval cancel = {0};
        setitimer(ITIMER_REAL, &cancel, NULL);

        /* Must not have timed out (no unbounded CPU) */
        ck_assert_msg(timed_out == 0,
            "brace_expand timed out on input: %s", payloads[i]);

        if (results != NULL) {
            int count = 0;
            while (results[count] != NULL) {
                free(results[count]);
                count++;
                /* Must not produce unbounded number of results */
                ck_assert_msg(count <= MAX_RESULTS,
                    "brace_expand produced too many results (%d) for: %s",
                    count, payloads[i]);
            }
            free(results);
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, TIMEOUT_SECONDS + 2);
    tcase_add_test(tc_core, test_brace_expansion_bounded);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
