/* Copyright 2011-2020 Bas van den Berg
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CTEST_H
#define CTEST_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __GNUC__
#define CTEST_IMPL_FORMAT_PRINTF(a, b) __attribute__ ((format(printf, a, b)))
#else
#define CTEST_IMPL_FORMAT_PRINTF(a, b)
#endif

#include <inttypes.h> /* intmax_t, uintmax_t, PRI* */
#include <stddef.h> /* size_t, offsetof */
#include <string.h> /* strcmp */

typedef void (*ctest_nullary_run_func)(void);
typedef void (*ctest_unary_run_func)(void*);
typedef void (*ctest_fixture_func)(void*);

union ctest_run_func_union {
    ctest_nullary_run_func nullary;
    ctest_unary_run_func unary;
};

#define CTEST_IMPL_PRAGMA(x) _Pragma (#x)

#if defined(__GNUC__)
#if defined(__clang__) || __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
/* the GCC argument will work for both gcc and clang  */
#define CTEST_IMPL_DIAG_PUSH_IGNORED(w) \
    CTEST_IMPL_PRAGMA(GCC diagnostic push) \
    CTEST_IMPL_PRAGMA(GCC diagnostic ignored "-W" #w)
#define CTEST_IMPL_DIAG_POP() \
    CTEST_IMPL_PRAGMA(GCC diagnostic pop)
#else
/* the push/pop functionality wasn't in gcc until 4.6, fallback to "ignored"  */
#define CTEST_IMPL_DIAG_PUSH_IGNORED(w) \
    CTEST_IMPL_PRAGMA(GCC diagnostic ignored "-W" #w)
#define CTEST_IMPL_DIAG_POP()
#endif
#else
/* leave them out entirely for non-GNUC compilers  */
#define CTEST_IMPL_DIAG_PUSH_IGNORED(w)
#define CTEST_IMPL_DIAG_POP()
#endif

struct ctest_impl_list_head {
    struct ctest_impl_list_head* next;
    struct ctest_impl_list_head* prev;
};

#define CTEST_IMPL_LIST_HEAD_INIT(head) { &(head), &(head) }

static inline void ctest_impl_list_add_between(struct ctest_impl_list_head* prev,
                                               struct ctest_impl_list_head* curr,
                                               struct ctest_impl_list_head* next)
{
    prev->next = curr;
    curr->prev = prev;
    curr->next = next;
    next->prev = curr;
}

static inline void ctest_impl_list_add_tail(struct ctest_impl_list_head* node,
                                            struct ctest_impl_list_head* head)
{
    ctest_impl_list_add_between(head->prev, node, head);
}

struct ctest_impl_slist_head {
    struct ctest_impl_slist_head* next;
};

#define CTEST_IMPL_SLIST_HEAD_INIT(head) { &(head) }

static inline void ctest_impl_slist_add(struct ctest_impl_slist_head* node,
                                        struct ctest_impl_slist_head* head)
{
    node->next = head->next;
    head->next = node;
}

#define ctest_impl_list_entry(head, type, member) \
    ((type*) ((char*) (head) - offsetof(type, member)))

#define ctest_impl_list_for_each_entry(iter, head, type, member) \
    for (iter = ctest_impl_list_entry((head)->next, type, member); \
         &iter->member != (head); \
         iter = ctest_impl_list_entry(iter->member.next, type, member))

#define DECLARE_HASHTABLE(name, size) \
    struct ctest_impl_slist_head name[size];

#define DEFINE_HASHTABLE(name, size...) \
    struct ctest_impl_slist_head name[size]; \
    __attribute__ ((constructor)) void name##_ctor(void) \
    { \
        ctest_impl_hash_init_once(name); \
    }

#define CTEST_IMPL_ARRAY_SIZE(array) \
    (sizeof(array) / sizeof(*(array)))

static inline void ctest_impl_hash_init_helper(struct ctest_impl_slist_head* heads, size_t size)
{
    for (size_t i = 0; i < size; ++i) {
        heads[i] = (struct ctest_impl_slist_head)CTEST_IMPL_SLIST_HEAD_INIT(heads[i]);
    }
}
#define ctest_impl_hash_init(hashtable) \
    ctest_impl_hash_init_helper(hashtable, CTEST_IMPL_ARRAY_SIZE(hashtable))

static inline void ctest_impl_hash_init_once_helper(struct ctest_impl_slist_head* heads, size_t size)
{
    if (heads[0].next == NULL) {
        ctest_impl_hash_init_helper(heads, size);
    }
}
#define ctest_impl_hash_init_once(hashtable) \
    ctest_impl_hash_init_once_helper(hashtable, CTEST_IMPL_ARRAY_SIZE(hashtable))

static inline void ctest_impl_slist_add_helper(struct ctest_impl_slist_head* heads, size_t size,
                                               struct ctest_impl_slist_head* node,
                                               unsigned int key)
{
    ctest_impl_slist_add(node, &heads[key % size]);
}
#define ctest_impl_hash_add(hashtable, node, key) \
    ctest_impl_slist_add_helper(hashtable, CTEST_IMPL_ARRAY_SIZE(hashtable), \
                                node, key)

#define ctest_impl_hash_for_each_possible(hashtable, iter, type, member, key) \
    ctest_impl_list_for_each_entry(iter, \
                                   &(hashtable)[(key) % (CTEST_IMPL_ARRAY_SIZE(hashtable))], \
                                   type, member)

static inline unsigned int ctest_impl_fnv1a32(const void* s, size_t size)
{
    return (
        (size ? ctest_impl_fnv1a32(s, size - 1) : 2166136261)
        ^ (unsigned int)((const char*)s)[size]
    ) * 16777619;
}

extern struct ctest_impl_list_head ctest_impl_test_list;
extern DECLARE_HASHTABLE(ctest_impl_suite_hashtable, 1024);

struct ctest {
    const char* ssname;  // suite name
    const char* ttname;  // test name
    union ctest_run_func_union run;

    void* data;
    struct ctest_impl_list_head list;

    int skip;
};

struct ctest_impl_suite {
    const char* name;
    void (*setup)(void*);
    void (*teardown)(void*);
    struct ctest_impl_slist_head list;
};

static inline unsigned int ctest_impl_hash_suite(const char* sname)
{
    return ctest_impl_fnv1a32(sname, strlen(sname));
}

static inline struct ctest_impl_suite* ctest_impl_find_suite(const char* sname)
{
    const unsigned int key = ctest_impl_hash_suite(sname);
    struct ctest_impl_suite* iter;
    ctest_impl_hash_for_each_possible(ctest_impl_suite_hashtable,
                                      iter, struct ctest_impl_suite, list, key) {
        if (strcmp(iter->name, sname) == 0) {
            return iter;
        }
    }
    return NULL;
}

static inline void ctest_impl_add_suite(struct ctest_impl_suite* suite)
{
    ctest_impl_hash_add(ctest_impl_suite_hashtable, &suite->list,
                        ctest_impl_hash_suite(suite->name));
}

static inline struct ctest_impl_suite* ctest_impl_emplace_suite(struct ctest_impl_suite* suite)
{
    struct ctest_impl_suite* const found = ctest_impl_find_suite(suite->name);
    if (found != NULL) {
        return found;
    }

    ctest_impl_add_suite(suite);
    return suite;
}

#define CTEST_IMPL_NAME(name) ctest_##name
#define CTEST_IMPL_FNAME(sname, tname) CTEST_IMPL_NAME(sname##_##tname##_run)
#define CTEST_IMPL_TNAME(sname, tname) CTEST_IMPL_NAME(sname##_##tname)
#define CTEST_IMPL_CNAME(sname, tname) CTEST_IMPL_NAME(sname##_##tname##_ctor)
#define CTEST_IMPL_DATA_SNAME(sname) CTEST_IMPL_NAME(sname##_data)
#define CTEST_IMPL_DATA_TNAME(sname, tname) CTEST_IMPL_NAME(sname##_##tname##_data)
#define CTEST_IMPL_SETUP_FNAME(sname) CTEST_IMPL_NAME(sname##_setup)
#define CTEST_IMPL_SETUP_CNAME(sname) CTEST_IMPL_NAME(sname##_setup_ctor)
#define CTEST_IMPL_TEARDOWN_FNAME(sname) CTEST_IMPL_NAME(sname##_teardown)
#define CTEST_IMPL_TEARDOWN_CNAME(sname) CTEST_IMPL_NAME(sname##_teardown_ctor)

#define CTEST_IMPL_STRUCT(sname, tname, tskip, tdata) \
    static struct ctest CTEST_IMPL_TNAME(sname, tname) = { \
        #sname, \
        #tname, \
        { (ctest_nullary_run_func) CTEST_IMPL_FNAME(sname, tname) }, \
        tdata, \
        CTEST_IMPL_LIST_HEAD_INIT(CTEST_IMPL_TNAME(sname, tname).list), \
        tskip, \
    }; \
    __attribute__ ((constructor)) static void CTEST_IMPL_CNAME(sname, tname)(void) \
    { \
        ctest_impl_list_add_tail(&CTEST_IMPL_TNAME(sname, tname).list, \
                                 &ctest_impl_test_list); \
    }

#define CTEST_IMPL_FIXTURE(sname, member, FNAME, CNAME) \
    __attribute__ ((constructor)) static void CNAME(sname)(void) \
    { \
        ctest_impl_hash_init_once(ctest_impl_suite_hashtable); \
        static struct ctest_impl_suite suite = { \
            #sname, \
            NULL, \
            NULL, \
            CTEST_IMPL_SLIST_HEAD_INIT(suite.list), \
        }; \
        struct ctest_impl_suite* const emplaced = ctest_impl_emplace_suite(&suite); \
        emplaced->member = (ctest_fixture_func) &FNAME(sname); \
    }

#define CTEST_SETUP(sname) \
    CTEST_DATA(sname); \
    static void CTEST_IMPL_SETUP_FNAME(sname)(struct CTEST_IMPL_DATA_SNAME(sname)* data); \
    CTEST_IMPL_FIXTURE(sname, setup, CTEST_IMPL_SETUP_FNAME, CTEST_IMPL_SETUP_CNAME) \
    static void CTEST_IMPL_SETUP_FNAME(sname)(struct CTEST_IMPL_DATA_SNAME(sname)* data)

#define CTEST_TEARDOWN(sname) \
    CTEST_DATA(sname); \
    static void CTEST_IMPL_TEARDOWN_FNAME(sname)(struct CTEST_IMPL_DATA_SNAME(sname)* data); \
    CTEST_IMPL_FIXTURE(sname, teardown, CTEST_IMPL_TEARDOWN_FNAME, CTEST_IMPL_TEARDOWN_CNAME) \
    static void CTEST_IMPL_TEARDOWN_FNAME(sname)(struct CTEST_IMPL_DATA_SNAME(sname)* data)

#define CTEST_DATA(sname) \
    struct CTEST_IMPL_DATA_SNAME(sname)

#define CTEST_IMPL_CTEST(sname, tname, tskip) \
    static void CTEST_IMPL_FNAME(sname, tname)(void); \
    CTEST_IMPL_STRUCT(sname, tname, tskip, NULL); \
    static void CTEST_IMPL_FNAME(sname, tname)(void)

#define CTEST_IMPL_CTEST2(sname, tname, tskip) \
    static struct CTEST_IMPL_DATA_SNAME(sname) CTEST_IMPL_DATA_TNAME(sname, tname); \
    static void CTEST_IMPL_FNAME(sname, tname)(struct CTEST_IMPL_DATA_SNAME(sname)* data); \
    CTEST_IMPL_STRUCT(sname, tname, tskip, &CTEST_IMPL_DATA_TNAME(sname, tname)); \
    static void CTEST_IMPL_FNAME(sname, tname)(struct CTEST_IMPL_DATA_SNAME(sname)* data)

void CTEST_LOG(const char* fmt, ...) CTEST_IMPL_FORMAT_PRINTF(1, 2);
void CTEST_ERR(const char* fmt, ...) CTEST_IMPL_FORMAT_PRINTF(1, 2);  // doesn't return

#define CTEST(sname, tname) CTEST_IMPL_CTEST(sname, tname, 0)
#define CTEST_SKIP(sname, tname) CTEST_IMPL_CTEST(sname, tname, 1)

#define CTEST2(sname, tname) CTEST_IMPL_CTEST2(sname, tname, 0)
#define CTEST2_SKIP(sname, tname) CTEST_IMPL_CTEST2(sname, tname, 1)


void assert_str(const char* exp, const char* real, const char* caller, int line);
#define ASSERT_STR(exp, real) assert_str(exp, real, __FILE__, __LINE__)

void assert_wstr(const wchar_t *exp, const wchar_t *real, const char* caller, int line);
#define ASSERT_WSTR(exp, real) assert_wstr(exp, real, __FILE__, __LINE__)

void assert_data(const unsigned char* exp, size_t expsize,
                 const unsigned char* real, size_t realsize,
                 const char* caller, int line);
#define ASSERT_DATA(exp, expsize, real, realsize) \
    assert_data(exp, expsize, real, realsize, __FILE__, __LINE__)

void assert_equal(intmax_t exp, intmax_t real, const char* caller, int line);
#define ASSERT_EQUAL(exp, real) assert_equal(exp, real, __FILE__, __LINE__)

void assert_equal_u(uintmax_t exp, uintmax_t real, const char* caller, int line);
#define ASSERT_EQUAL_U(exp, real) assert_equal_u(exp, real, __FILE__, __LINE__)

void assert_not_equal(intmax_t exp, intmax_t real, const char* caller, int line);
#define ASSERT_NOT_EQUAL(exp, real) assert_not_equal(exp, real, __FILE__, __LINE__)

void assert_not_equal_u(uintmax_t exp, uintmax_t real, const char* caller, int line);
#define ASSERT_NOT_EQUAL_U(exp, real) assert_not_equal_u(exp, real, __FILE__, __LINE__)

void assert_interval(intmax_t exp1, intmax_t exp2, intmax_t real, const char* caller, int line);
#define ASSERT_INTERVAL(exp1, exp2, real) assert_interval(exp1, exp2, real, __FILE__, __LINE__)

void assert_null(void* real, const char* caller, int line);
#define ASSERT_NULL(real) assert_null((void*)real, __FILE__, __LINE__)

void assert_not_null(const void* real, const char* caller, int line);
#define ASSERT_NOT_NULL(real) assert_not_null(real, __FILE__, __LINE__)

void assert_true(int real, const char* caller, int line);
#define ASSERT_TRUE(real) assert_true(real, __FILE__, __LINE__)

void assert_false(int real, const char* caller, int line);
#define ASSERT_FALSE(real) assert_false(real, __FILE__, __LINE__)

void assert_fail(const char* caller, int line);
#define ASSERT_FAIL() assert_fail(__FILE__, __LINE__)

void assert_dbl_near(double exp, double real, double tol, const char* caller, int line);
#define ASSERT_DBL_NEAR(exp, real) assert_dbl_near(exp, real, 1e-4, __FILE__, __LINE__)
#define ASSERT_DBL_NEAR_TOL(exp, real, tol) assert_dbl_near(exp, real, tol, __FILE__, __LINE__)

void assert_dbl_far(double exp, double real, double tol, const char* caller, int line);
#define ASSERT_DBL_FAR(exp, real) assert_dbl_far(exp, real, 1e-4, __FILE__, __LINE__)
#define ASSERT_DBL_FAR_TOL(exp, real, tol) assert_dbl_far(exp, real, tol, __FILE__, __LINE__)

#ifdef CTEST_MAIN

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>

struct ctest_impl_list_head ctest_impl_test_list = CTEST_IMPL_LIST_HEAD_INIT(ctest_impl_test_list);
DEFINE_HASHTABLE(ctest_impl_suite_hashtable);

static size_t ctest_errorsize;
static char* ctest_errormsg;
#define MSG_SIZE 4096
static char ctest_errorbuffer[MSG_SIZE];
static jmp_buf ctest_err;
static int color_output = 1;
static const char* suite_name;

typedef int (*ctest_filter_func)(struct ctest*);

#define ANSI_BLACK    "\033[0;30m"
#define ANSI_RED      "\033[0;31m"
#define ANSI_GREEN    "\033[0;32m"
#define ANSI_YELLOW   "\033[0;33m"
#define ANSI_BLUE     "\033[0;34m"
#define ANSI_MAGENTA  "\033[0;35m"
#define ANSI_CYAN     "\033[0;36m"
#define ANSI_GREY     "\033[0;37m"
#define ANSI_DARKGREY "\033[01;30m"
#define ANSI_BRED     "\033[01;31m"
#define ANSI_BGREEN   "\033[01;32m"
#define ANSI_BYELLOW  "\033[01;33m"
#define ANSI_BBLUE    "\033[01;34m"
#define ANSI_BMAGENTA "\033[01;35m"
#define ANSI_BCYAN    "\033[01;36m"
#define ANSI_WHITE    "\033[01;37m"
#define ANSI_NORMAL   "\033[0m"

static void vprint_errormsg(const char* const fmt, va_list ap) CTEST_IMPL_FORMAT_PRINTF(1, 0);
static void print_errormsg(const char* const fmt, ...) CTEST_IMPL_FORMAT_PRINTF(1, 2);

static void vprint_errormsg(const char* const fmt, va_list ap) {
    // (v)snprintf returns the number that would have been written
    const int ret = vsnprintf(ctest_errormsg, ctest_errorsize, fmt, ap);
    if (ret < 0) {
        ctest_errormsg[0] = 0x00;
    } else {
        const size_t size = (size_t) ret;
        const size_t s = (ctest_errorsize <= size ? size -ctest_errorsize : size);
        // ctest_errorsize may overflow at this point
        ctest_errorsize -= s;
        ctest_errormsg += s;
    }
}

static void print_errormsg(const char* const fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
    vprint_errormsg(fmt, argp);
    va_end(argp);
}

static void msg_start(const char* color, const char* title) {
    if (color_output) {
        print_errormsg("%s", color);
    }
    print_errormsg("  %s: ", title);
}

static void msg_end(void) {
    if (color_output) {
        print_errormsg(ANSI_NORMAL);
    }
    print_errormsg("\n");
}

void CTEST_LOG(const char* fmt, ...)
{
    va_list argp;
    msg_start(ANSI_BLUE, "LOG");

    va_start(argp, fmt);
    vprint_errormsg(fmt, argp);
    va_end(argp);

    msg_end();
}

CTEST_IMPL_DIAG_PUSH_IGNORED(missing-noreturn)

void CTEST_ERR(const char* fmt, ...)
{
    va_list argp;
    msg_start(ANSI_YELLOW, "ERR");

    va_start(argp, fmt);
    vprint_errormsg(fmt, argp);
    va_end(argp);

    msg_end();
    longjmp(ctest_err, 1);
}

CTEST_IMPL_DIAG_POP()

void assert_str(const char* exp, const char*  real, const char* caller, int line) {
    if ((exp == NULL && real != NULL) ||
        (exp != NULL && real == NULL) ||
        (exp && real && strcmp(exp, real) != 0)) {
        CTEST_ERR("%s:%d  expected '%s', got '%s'", caller, line, exp, real);
    }
}

void assert_wstr(const wchar_t *exp, const wchar_t *real, const char* caller, int line) {
    if ((exp == NULL && real != NULL) ||
        (exp != NULL && real == NULL) ||
        (exp && real && wcscmp(exp, real) != 0)) {
        CTEST_ERR("%s:%d  expected '%ls', got '%ls'", caller, line, exp, real);
    }
}

void assert_data(const unsigned char* exp, size_t expsize,
                 const unsigned char* real, size_t realsize,
                 const char* caller, int line) {
    size_t i;
    if (expsize != realsize) {
        CTEST_ERR("%s:%d  expected %" PRIuMAX " bytes, got %" PRIuMAX, caller, line, (uintmax_t) expsize, (uintmax_t) realsize);
    }
    for (i=0; i<expsize; i++) {
        if (exp[i] != real[i]) {
            CTEST_ERR("%s:%d expected 0x%02x at offset %" PRIuMAX " got 0x%02x",
                caller, line, exp[i], (uintmax_t) i, real[i]);
        }
    }
}

void assert_equal(intmax_t exp, intmax_t real, const char* caller, int line) {
    if (exp != real) {
        CTEST_ERR("%s:%d  expected %" PRIdMAX ", got %" PRIdMAX, caller, line, exp, real);
    }
}

void assert_equal_u(uintmax_t exp, uintmax_t real, const char* caller, int line) {
    if (exp != real) {
        CTEST_ERR("%s:%d  expected %" PRIuMAX ", got %" PRIuMAX, caller, line, exp, real);
    }
}

void assert_not_equal(intmax_t exp, intmax_t real, const char* caller, int line) {
    if ((exp) == (real)) {
        CTEST_ERR("%s:%d  should not be %" PRIdMAX, caller, line, real);
    }
}

void assert_not_equal_u(uintmax_t exp, uintmax_t real, const char* caller, int line) {
    if ((exp) == (real)) {
        CTEST_ERR("%s:%d  should not be %" PRIuMAX, caller, line, real);
    }
}

void assert_interval(intmax_t exp1, intmax_t exp2, intmax_t real, const char* caller, int line) {
    if (real < exp1 || real > exp2) {
        CTEST_ERR("%s:%d  expected %" PRIdMAX "-%" PRIdMAX ", got %" PRIdMAX, caller, line, exp1, exp2, real);
    }
}

void assert_dbl_near(double exp, double real, double tol, const char* caller, int line) {
    double diff = exp - real;
    double absdiff = diff;
    /* avoid using fabs and linking with a math lib */
    if(diff < 0) {
      absdiff *= -1;
    }
    if (absdiff > tol) {
        CTEST_ERR("%s:%d  expected %0.3e, got %0.3e (diff %0.3e, tol %0.3e)", caller, line, exp, real, diff, tol);
    }
}

void assert_dbl_far(double exp, double real, double tol, const char* caller, int line) {
    double diff = exp - real;
    double absdiff = diff;
    /* avoid using fabs and linking with a math lib */
    if(diff < 0) {
      absdiff *= -1;
    }
    if (absdiff <= tol) {
        CTEST_ERR("%s:%d  expected %0.3e, got %0.3e (diff %0.3e, tol %0.3e)", caller, line, exp, real, diff, tol);
    }
}

void assert_null(void* real, const char* caller, int line) {
    if ((real) != NULL) {
        CTEST_ERR("%s:%d  should be NULL", caller, line);
    }
}

void assert_not_null(const void* real, const char* caller, int line) {
    if (real == NULL) {
        CTEST_ERR("%s:%d  should not be NULL", caller, line);
    }
}

void assert_true(int real, const char* caller, int line) {
    if ((real) == 0) {
        CTEST_ERR("%s:%d  should be true", caller, line);
    }
}

void assert_false(int real, const char* caller, int line) {
    if ((real) != 0) {
        CTEST_ERR("%s:%d  should be false", caller, line);
    }
}

void assert_fail(const char* caller, int line) {
    CTEST_ERR("%s:%d  shouldn't come here", caller, line);
}


static int suite_all(struct ctest* t) {
    (void) t; // fix unused parameter warning
    return 1;
}

static int suite_filter(struct ctest* t) {
    return strncmp(suite_name, t->ssname, strlen(suite_name)) == 0;
}

static uint64_t getCurrentTime(void) {
    struct timeval now;
    gettimeofday(&now, NULL);
    uint64_t now64 = (uint64_t) now.tv_sec;
    now64 *= 1000000;
    now64 += ((uint64_t) now.tv_usec);
    return now64;
}

static void color_print(const char* color, const char* text) {
    if (color_output)
        printf("%s%s" ANSI_NORMAL "\n", color, text);
    else
        printf("%s\n", text);
}

#ifdef CTEST_SEGFAULT
#include <signal.h>
static void sighandler(int signum)
{
    const char msg_color[] = ANSI_BRED "[SIGSEGV: Segmentation fault]" ANSI_NORMAL "\n";
    const char msg_nocolor[] = "[SIGSEGV: Segmentation fault]\n";

    const char* msg = color_output ? msg_color : msg_nocolor;
    write(STDOUT_FILENO, msg, strlen(msg));

    /* "Unregister" the signal handler and send the signal back to the process
     * so it can terminate as expected */
    signal(signum, SIG_DFL);
    kill(getpid(), signum);
}
#endif

int ctest_main(int argc, const char *argv[])
{
    static int total = 0;
    static int num_ok = 0;
    static int num_fail = 0;
    static int num_skip = 0;
    static int idx = 1;
    static ctest_filter_func filter = suite_all;

#ifdef CTEST_SEGFAULT
    signal(SIGSEGV, sighandler);
#endif

    if (argc == 2) {
        suite_name = argv[1];
        filter = suite_filter;
    }
#ifdef CTEST_NO_COLORS
    color_output = 0;
#else
    color_output = isatty(1);
#endif
    uint64_t t1 = getCurrentTime();
    static struct ctest* test;
    static struct ctest_impl_suite* suite;

    ctest_impl_list_for_each_entry(test, &ctest_impl_test_list, struct ctest, list) {
        if (filter(test)) total++;
    }

    ctest_impl_list_for_each_entry(test, &ctest_impl_test_list, struct ctest, list) {
        if (filter(test)) {
            ctest_errorbuffer[0] = 0;
            ctest_errorsize = MSG_SIZE-1;
            ctest_errormsg = ctest_errorbuffer;
            printf("TEST %d/%d %s:%s ", idx, total, test->ssname, test->ttname);
            fflush(stdout);
            if (test->skip) {
                color_print(ANSI_BYELLOW, "[SKIPPED]");
                num_skip++;
            } else {
                int result = setjmp(ctest_err);
                if (result == 0) {
                    suite = ctest_impl_find_suite(test->ssname);
                    if (suite && suite->setup && test->data) suite->setup(test->data);
                    if (test->data)
                        test->run.unary(test->data);
                    else
                        test->run.nullary();
                    if (suite && suite->teardown && test->data) suite->teardown(test->data);
                    // if we got here it's ok
#ifdef CTEST_COLOR_OK
                    color_print(ANSI_BGREEN, "[OK]");
#else
                    printf("[OK]\n");
#endif
                    num_ok++;
                } else {
                    color_print(ANSI_BRED, "[FAIL]");
                    num_fail++;
                }
                if (ctest_errorsize != MSG_SIZE-1) printf("%s", ctest_errorbuffer);
            }
            idx++;
        }
    }
    uint64_t t2 = getCurrentTime();

    const char* color = (num_fail) ? ANSI_BRED : ANSI_GREEN;
    char results[80];
    snprintf(results, sizeof(results), "RESULTS: %d tests (%d ok, %d failed, %d skipped) ran in %" PRIu64 " ms", total, num_ok, num_fail, num_skip, (t2 - t1)/1000);
    color_print(color, results);
    return num_fail;
}

#endif

#ifdef __cplusplus
}
#endif

#endif
