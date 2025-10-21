#include "port_clock_gettime.h"
#include <errno.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/mach_time.h>
#include <mach/thread_act.h>
#include <sys/resource.h>

#define MPLS_EXPECT(x, v) __builtin_expect((x), (v))
#define MPLS_SLOWPATH(x) ((__typeof__(x))MPLS_EXPECT((long)(x), 0l))

/* Constants for scaling time values */
#define BILLION32 1000000000U

#define EXTRA_SHIFT 2
#define HIGH_SHIFT (32 + EXTRA_SHIFT)
#define HIGH_BITS (64 - HIGH_SHIFT)
#define NUMERATOR_MASK (~0U << HIGH_BITS)
#define NULL_SCALE (1ULL << HIGH_SHIFT)

/* The cached mach_time scale factors */
static mach_timebase_info_data_t mach_scale = {0};
static uint64_t mach_mult = 0;
static struct timespec res_mach = {0, 0};

/* Obtain the mach_time scale factor if needed, or return an error */
static int get_mach_scale(void) {
  if (mach_scale.numer) return 0;
  if (mach_timebase_info(&mach_scale) != KERN_SUCCESS) {
    /* On failure, make sure resulting scale is 0 */
    mach_scale.numer = 0;
    mach_scale.denom = 1;
    return -1;
  }
  return 0;
}

/* Set up the mach->nanoseconds multiplier, or return an error */
static int setup_mach_mult(void) {
  int ret = get_mach_scale();

  /* Set up main multiplier (0 if error getting scale) */
  if (!(mach_scale.numer & NUMERATOR_MASK)) {
    mach_mult = (((uint64_t)mach_scale.numer << HIGH_SHIFT) + mach_scale.denom / 2) / mach_scale.denom;
  } else {
    mach_mult = ((((uint64_t)mach_scale.numer << 32) + mach_scale.denom / 2) / mach_scale.denom) << EXTRA_SHIFT;
  }

  /* Also set up resolution as nanos/count rounded up */
  res_mach.tv_nsec = (mach_mult + (NULL_SCALE - 1)) >> HIGH_SHIFT;

  return ret;
}

#define MASK64LOW 0xFFFFFFFFULL

/*
 * 64x64->128 multiply, returning middle 64
 *
 * This code has been verified with a floating-zeroes/ones test, comparing
 * the results to Python's built-in multiprecision arithmetic.
 */
static inline uint64_t mmul64(uint64_t a, uint64_t b) {
  /* Split the operands into halves */
  uint32_t a_hi = a >> 32, a_lo = a;
  uint32_t b_hi = b >> 32, b_lo = b;
  uint64_t high, mid1, mid2, low;

  /* Compute the four cross products */
  low = (uint64_t)a_lo * b_lo;
  mid1 = (uint64_t)a_lo * b_hi;
  mid2 = (uint64_t)a_hi * b_lo;
  high = (uint64_t)a_hi * b_hi;

  /* Fold the results (must be in carry-propagation order) */
  mid1 += (mid2 & MASK64LOW) + (low >> 32);
  high += (mid1 >> 32) + (mid2 >> 32); /* Shifts must precede add */

  /* Combine and return the two middle chunks */
  return (high << 32) + (mid1 & MASK64LOW);
}

/* Convert mach units to nanoseconds */
static inline uint64_t mach2nanos(uint64_t mach_time) {
  /* If 1:1 scaling (x86), return as is */
  if (mach_mult == NULL_SCALE) return mach_time;

  /* Otherwise, return appropriately scaled value */
  return mmul64(mach_time, mach_mult) >> EXTRA_SHIFT;
}

/* Convert nanoseconds to timespec */
static inline void nanos2timespec(uint64_t nanos, struct timespec *ts) {
  uint64_t secs;
  uint32_t lownanos, lowsecs, nanorem;

  /* Divide nanoseconds to get seconds */
  secs = nanos / BILLION32;

  /*
   * Multiply & subtract (all 32-bit) to get nanosecond remainder.
   *
   * This is more efficient than using the '%' operator on all platforms,
   * and there's no version of *div() for a 64-bit dividend and 32-bit
   * divisor.  Since the divisor, and hence the remainder, are known to
   * fit in 32 bits, the entire computation can be done in 32 bits.
   */
  lownanos = nanos;
  lowsecs = secs;
  nanorem = lownanos - lowsecs * BILLION32;

  /* Return values as a timespec */
  ts->tv_sec = secs;
  ts->tv_nsec = nanorem;
}

/* Convert mach units to timespec */
static inline void mach2timespec(uint64_t mach_time, struct timespec *ts) {
  nanos2timespec(mach2nanos(mach_time), ts);
}

/* Common thread usage code */
static int get_thread_usage(thread_basic_info_data_t *info) {
  int ret;
  mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
  thread_port_t thread = mach_thread_self();

  ret = thread_info(thread, THREAD_BASIC_INFO, (thread_info_t)info, &count);
  mach_port_deallocate(mach_task_self(), thread);
  return ret;
}

/* Same but returning as timespec */
static inline int get_thread_usage_ts(struct timespec *ts) {
  thread_basic_info_data_t info;

  if (get_thread_usage(&info)) return -1;

  ts->tv_sec = info.user_time.seconds + info.system_time.seconds;
  ts->tv_nsec = (info.user_time.microseconds + info.system_time.microseconds) * 1000;
  if (ts->tv_nsec >= BILLION32) {
    ++ts->tv_sec;
    ts->tv_nsec -= BILLION32;
  }

  if (!ts->tv_sec && !ts->tv_nsec) ts->tv_nsec = 1;
  return 0;
}

// https://github.com/macports/macports-legacy-support/blob/master/src/time.c
// https://github.com/apple-oss-distributions/Libc/blob/main/gen/clock_gettime.c
int port_clock_gettime(clockid_t clk_id, struct timespec *ts) {
  int ret, mserr = 0;
  struct timeval tod;
  struct rusage ru;
  uint64_t mach_time, nanos;

  /* Set up mach scaling early, whether we need it or not. */
  if (MPLS_SLOWPATH(!mach_mult)) mserr = setup_mach_mult();

  switch (clk_id) {

  case CLOCK_REALTIME:
    ret = gettimeofday(&tod, NULL);
    ts->tv_sec = tod.tv_sec;
    ts->tv_nsec = tod.tv_usec * 1000;
    return ret;

  case CLOCK_PROCESS_CPUTIME_ID:
    ret = getrusage(RUSAGE_SELF, &ru);
    timeradd(&ru.ru_utime, &ru.ru_stime, &ru.ru_utime);
    TIMEVAL_TO_TIMESPEC(&ru.ru_utime, ts);
    return ret;

  case CLOCK_THREAD_CPUTIME_ID:
    return get_thread_usage_ts(ts);

  case CLOCK_MONOTONIC:
    mach_time = mach_continuous_time();
    nanos = mach2nanos(mach_time) / 1000 * 1000; /* Quantize to microseconds */
    nanos2timespec(nanos, ts);
    return mserr;

  case CLOCK_MONOTONIC_RAW:
    mach_time = mach_continuous_time();
    break;

  case CLOCK_MONOTONIC_RAW_APPROX:
    mach_time = mach_continuous_approximate_time();
    break;

  case CLOCK_UPTIME_RAW:
    mach_time = mach_absolute_time();
    break;

  case CLOCK_UPTIME_RAW_APPROX:
    mach_time = mach_approximate_time();
    break;

  default:
    errno = EINVAL;
    return -1;
  }

  /* Convert to timespec & return (error if scale couldn't be obtained) */
  mach2timespec(mach_time, ts);
  return mserr;
}
