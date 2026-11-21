// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <munit.h>

#include <chiaki/reorderwait.h>

/**
 * Plays holes through the estimator as Takion would: each starts waiting at *now, and its packet comes
 * lateness_us later (or never, if negative). Within the wait it fills; after it, it was skipped at the
 * wait's end and comes late. Holes are 20 ms apart, so each skip is long forgotten by the next hole.
 */
typedef struct trace_t
{
	ChiakiReorderWait wait;
	int64_t now;
	ChiakiSeqNum16 seq_num;
} Trace;

static void trace_init(Trace *trace)
{
	chiaki_reorder_wait_init(&trace->wait);
	trace->now = 1000000;
	trace->seq_num = 100;
}

static void trace_hole(Trace *trace, int64_t lateness_us)
{
	int64_t wait_us = chiaki_reorder_wait_us(&trace->wait);
	if(lateness_us >= 0 && lateness_us <= wait_us)
		chiaki_reorder_wait_filled(&trace->wait, lateness_us);
	else
	{
		chiaki_reorder_wait_skipped(&trace->wait, trace->seq_num, 1, trace->now, trace->now + wait_us, false);
		if(lateness_us >= 0)
			munit_assert_true(chiaki_reorder_wait_late(&trace->wait, trace->seq_num, trace->now + lateness_us));
	}
	trace->now += 20000;
	trace->seq_num += 10;
	chiaki_reorder_wait_expire(&trace->wait, trace->now + CHIAKI_REORDER_WAIT_LOST_US);
}

static void trace_holes(Trace *trace, size_t count, int64_t lateness_us)
{
	for(size_t i = 0; i < count; i++)
		trace_hole(trace, lateness_us);
}

static MunitResult test_starts_at_16_ms(const MunitParameter params[], void *user)
{
	ChiakiReorderWait wait;
	chiaki_reorder_wait_init(&wait);
	munit_assert_int64(chiaki_reorder_wait_us(&wait), ==, 16000);
	munit_assert_uint32(wait.stats.holes_filled + wait.stats.late_packets + wait.stats.holes_lost + wait.stats.overflow_skips, ==, 0);
	return MUNIT_OK;
}

static MunitResult test_always_late_7_ms(const MunitParameter params[], void *user)
{
	Trace trace;
	trace_init(&trace);
	// 7 ms with ±1 ms of jitter
	for(size_t i = 0; i < 300; i++)
		trace_hole(&trace, 6000 + (int64_t)(i % 3) * 1000);
	// srtt + 4 rttvar: 7 ms and four times the jitter's mean deviation (~0.7 ms)
	int64_t wait_us = chiaki_reorder_wait_us(&trace.wait);
	munit_assert_int64(wait_us, >=, 8000);
	munit_assert_int64(wait_us, <=, 10500);
	// Within the wait from then on: they all fill
	uint32_t late = trace.wait.stats.late_packets;
	trace_holes(&trace, 30, 8000);
	munit_assert_uint32(trace.wait.stats.late_packets, ==, late);
	munit_assert_uint32(trace.wait.stats.holes_lost, ==, 0);
	return MUNIT_OK;
}

static MunitResult test_constant_lateness_settles_on_it(const MunitParameter params[], void *user)
{
	Trace trace;
	trace_init(&trace);
	trace_holes(&trace, 300, 7000);
	int64_t wait_us = chiaki_reorder_wait_us(&trace.wait);
	munit_assert_int64(wait_us, >=, 7000);
	munit_assert_int64(wait_us, <=, 10000);
	return MUNIT_OK;
}

static MunitResult test_pure_loss_drifts_to_3_ms(const MunitParameter params[], void *user)
{
	Trace trace;
	trace_init(&trace);
	trace_holes(&trace, 10, -1);
	// Slowly: 5% per lost hole
	munit_assert_int64(chiaki_reorder_wait_us(&trace.wait), >, 9000);
	trace_holes(&trace, 90, -1);
	munit_assert_int64(chiaki_reorder_wait_us(&trace.wait), ==, CHIAKI_REORDER_WAIT_MIN_US);
	munit_assert_uint32(trace.wait.stats.holes_lost, ==, 100);
	munit_assert_uint32(trace.wait.stats.late_packets, ==, 0);
	munit_assert_uint32(trace.wait.stats.holes_filled, ==, 0);
	return MUNIT_OK;
}

static MunitResult test_burst_then_calm(const MunitParameter params[], void *user)
{
	Trace trace;
	trace_init(&trace);
	trace_holes(&trace, 50, 2000);
	munit_assert_int64(chiaki_reorder_wait_us(&trace.wait), <, 4000);

	// A burst of packets 15 ms late: the first is skipped and comes late, the wait covers it at once
	trace_hole(&trace, 15000);
	munit_assert_uint32(trace.wait.stats.late_packets, ==, 1);
	munit_assert_int64(chiaki_reorder_wait_us(&trace.wait), >=, 18750);
	uint32_t late = trace.wait.stats.late_packets;
	trace_holes(&trace, 20, 15000);
	munit_assert_uint32(trace.wait.stats.late_packets, ==, late); // all filled while waiting

	// Calm again: back down, slowly
	trace_holes(&trace, 5, 2000);
	munit_assert_int64(chiaki_reorder_wait_us(&trace.wait), >, 14000);
	trace_holes(&trace, 200, 2000);
	munit_assert_int64(chiaki_reorder_wait_us(&trace.wait), <, 4000);
	return MUNIT_OK;
}

static MunitResult test_one_outlier_bumps_then_decays(const MunitParameter params[], void *user)
{
	Trace trace;
	trace_init(&trace);
	trace_holes(&trace, 200, 5000);
	int64_t settled_us = chiaki_reorder_wait_us(&trace.wait);
	munit_assert_int64(settled_us, <, 6000);

	trace_hole(&trace, 18000);
	munit_assert_uint32(trace.wait.stats.late_packets, ==, 1);
	munit_assert_int64(chiaki_reorder_wait_us(&trace.wait), ==, CHIAKI_REORDER_WAIT_MAX_US); // 1.25 × 18 ms, clamped

	trace_holes(&trace, 200, 5000);
	munit_assert_int64(chiaki_reorder_wait_us(&trace.wait), <, 6000);
	return MUNIT_OK;
}

static MunitResult test_loss_keeps_reordering_covered(const MunitParameter params[], void *user)
{
	Trace trace;
	trace_init(&trace);
	// A collapse: 25 holes of real loss for each packet 5 ms late (±1 ms), for a minute. The loss mustn't
	// bring the wait under what the late packets showed, or each of them is skipped.
	for(size_t i = 0; i < 60; i++)
	{
		trace_holes(&trace, 25, -1);
		trace_hole(&trace, 4000 + (int64_t)(i % 3) * 1000);
	}
	munit_assert_uint32(trace.wait.stats.holes_lost, ==, 60 * 25);
	munit_assert_uint32(trace.wait.stats.late_packets, <=, 1); // the first, before any sample
	munit_assert_int64(chiaki_reorder_wait_us(&trace.wait), >=, 6000);

	// Loss alone still trims what is above the samples' target, down to it
	int64_t wait_us = chiaki_reorder_wait_us(&trace.wait);
	trace.wait.wait_us = CHIAKI_REORDER_WAIT_MAX_US;
	trace_holes(&trace, 100, -1);
	munit_assert_int64(chiaki_reorder_wait_us(&trace.wait), ==, wait_us);
	return MUNIT_OK;
}

static MunitResult test_clamps(const MunitParameter params[], void *user)
{
	Trace trace;
	trace_init(&trace);
	trace_holes(&trace, 20, 100000); // far too late, every time
	munit_assert_int64(chiaki_reorder_wait_us(&trace.wait), ==, CHIAKI_REORDER_WAIT_MAX_US);
	munit_assert_uint32(trace.wait.stats.late_packets, ==, 20);

	ChiakiReorderWait wait;
	chiaki_reorder_wait_init(&wait);
	for(size_t i = 0; i < 200; i++)
		chiaki_reorder_wait_filled(&wait, i % 2 ? 0 : -500); // at once, or a clock step back
	munit_assert_int64(chiaki_reorder_wait_us(&wait), ==, CHIAKI_REORDER_WAIT_MIN_US);
	munit_assert_uint32(wait.stats.holes_filled, ==, 200);
	return MUNIT_OK;
}

static MunitResult test_late_packets(const MunitParameter params[], void *user)
{
	ChiakiReorderWait wait;
	chiaki_reorder_wait_init(&wait);
	// 10 missing from 65530, across the wrap; waiting since 1 s, skipped 16 ms later
	chiaki_reorder_wait_skipped(&wait, 65530, 10, 1000000, 1016000, false);

	munit_assert_false(chiaki_reorder_wait_late(&wait, 65529, 1017000)); // delivered before the hole
	munit_assert_false(chiaki_reorder_wait_late(&wait, 4, 1017000)); // delivered after it
	munit_assert_true(chiaki_reorder_wait_late(&wait, 65535, 1017000));
	munit_assert_true(chiaki_reorder_wait_late(&wait, 3, 1017000));
	munit_assert_false(chiaki_reorder_wait_late(&wait, 3, 1018000)); // a duplicate
	munit_assert_uint32(wait.stats.late_packets, ==, 2);
	munit_assert_int64(chiaki_reorder_wait_us(&wait), ==, CHIAKI_REORDER_WAIT_MAX_US); // 17 ms × 1.25

	// Some came: not lost, and the wait isn't shrunk for it
	chiaki_reorder_wait_expire(&wait, 1016000 + CHIAKI_REORDER_WAIT_LOST_US - 1);
	munit_assert_true(chiaki_reorder_wait_late(&wait, 0, 1020000)); // still remembered
	chiaki_reorder_wait_expire(&wait, 1016000 + CHIAKI_REORDER_WAIT_LOST_US);
	munit_assert_uint32(wait.stats.holes_lost, ==, 0);
	munit_assert_int64(chiaki_reorder_wait_us(&wait), ==, CHIAKI_REORDER_WAIT_MAX_US);
	munit_assert_false(chiaki_reorder_wait_late(&wait, 1, 1200000)); // forgotten
	munit_assert_uint32(wait.stats.late_packets, ==, 3);
	return MUNIT_OK;
}

static MunitResult test_late_after_lost_window(const MunitParameter params[], void *user)
{
	ChiakiReorderWait wait;
	chiaki_reorder_wait_init(&wait);
	// Skipped at 1.016 s, then no video (so no expire) until its packet 100 ms later: it was lost
	chiaki_reorder_wait_skipped(&wait, 10, 1, 1000000, 1016000, false);
	munit_assert_false(chiaki_reorder_wait_late(&wait, 10, 1016000 + CHIAKI_REORDER_WAIT_LOST_US));
	munit_assert_uint32(wait.stats.late_packets, ==, 0);
	munit_assert_uint32(wait.stats.holes_lost, ==, 1);
	munit_assert_int64(chiaki_reorder_wait_us(&wait), ==, 15200);
	munit_assert_false(wait.sampled);
	return MUNIT_OK;
}

static MunitResult test_lateness_sample_capped(const MunitParameter params[], void *user)
{
	ChiakiReorderWait wait;
	chiaki_reorder_wait_init(&wait);
	// 90 ms late: covered up to the ceiling, and a sample of no more than it
	chiaki_reorder_wait_skipped(&wait, 10, 1, 1000000, 1016000, false);
	munit_assert_true(chiaki_reorder_wait_late(&wait, 10, 1090000));
	munit_assert_int64(chiaki_reorder_wait_us(&wait), ==, CHIAKI_REORDER_WAIT_MAX_US);
	munit_assert_int64(wait.srtt_us, ==, CHIAKI_REORDER_WAIT_MAX_US);
	munit_assert_int64(wait.rttvar_us, ==, CHIAKI_REORDER_WAIT_MAX_US / 2);

	// So holes filling at once soon bring it down (17; with the raw 90 ms, 28)
	size_t fills = 0;
	while(chiaki_reorder_wait_us(&wait) == CHIAKI_REORDER_WAIT_MAX_US)
	{
		chiaki_reorder_wait_filled(&wait, 500);
		fills++;
	}
	munit_assert_size(fills, <=, 20);
	return MUNIT_OK;
}

static MunitResult test_overflow_skips(const MunitParameter params[], void *user)
{
	ChiakiReorderWait wait;
	chiaki_reorder_wait_init(&wait);
	// A hole of 300 skipped because the queue was full: only its last 256 are remembered
	chiaki_reorder_wait_skipped(&wait, 1000, 300, 1000000, 1001000, true);
	chiaki_reorder_wait_skipped(&wait, 2000, 0, 1000000, 1001000, true); // nothing skipped: not counted
	munit_assert_uint32(wait.stats.overflow_skips, ==, 1);
	munit_assert_false(chiaki_reorder_wait_late(&wait, 1000 + 43, 1002000));
	munit_assert_true(chiaki_reorder_wait_late(&wait, 1000 + 44, 1002000));
	munit_assert_true(chiaki_reorder_wait_late(&wait, 1000 + 299, 1002000));
	munit_assert_false(chiaki_reorder_wait_late(&wait, 1000 + 300, 1002000));
	// 2 ms late: a sample, not a bump past it
	munit_assert_int64(chiaki_reorder_wait_us(&wait), <, CHIAKI_REORDER_WAIT_INITIAL_US);

	// Lost after a full queue: counted, but no wait was wasted on it
	chiaki_reorder_wait_skipped(&wait, 5000, 5, 1003000, 1003000, true);
	int64_t wait_us = chiaki_reorder_wait_us(&wait);
	chiaki_reorder_wait_expire(&wait, 1003000 + CHIAKI_REORDER_WAIT_LOST_US);
	munit_assert_uint32(wait.stats.holes_lost, ==, 1);
	munit_assert_int64(chiaki_reorder_wait_us(&wait), ==, wait_us);
	return MUNIT_OK;
}

static MunitResult test_oldest_skip_makes_room(const MunitParameter params[], void *user)
{
	ChiakiReorderWait wait;
	chiaki_reorder_wait_init(&wait);
	for(uint16_t i = 0; i < CHIAKI_REORDER_WAIT_SKIPS; i++)
		chiaki_reorder_wait_skipped(&wait, (ChiakiSeqNum16)(i * 10), 1, 1000000, 1016000, false);
	munit_assert_uint32(wait.stats.holes_lost, ==, 0);
	// One more: the oldest is resolved early, as lost
	chiaki_reorder_wait_skipped(&wait, 1000, 1, 1000000, 1016000, false);
	munit_assert_uint32(wait.stats.holes_lost, ==, 1);
	munit_assert_int64(chiaki_reorder_wait_us(&wait), ==, 15200);
	munit_assert_false(chiaki_reorder_wait_late(&wait, 0, 1017000));
	munit_assert_true(chiaki_reorder_wait_late(&wait, 10, 1017000));
	munit_assert_true(chiaki_reorder_wait_late(&wait, 1000, 1017000));
	return MUNIT_OK;
}

static MunitResult test_log_due(const MunitParameter params[], void *user)
{
	ChiakiReorderWait wait;
	chiaki_reorder_wait_init(&wait);
	munit_assert_false(chiaki_reorder_wait_log_due(&wait, 1000000));
	wait.wait_us = 14500; // moved by less than 2 ms
	munit_assert_false(chiaki_reorder_wait_log_due(&wait, 1000000));
	wait.wait_us = 14000;
	munit_assert_true(chiaki_reorder_wait_log_due(&wait, 1000000));
	munit_assert_false(chiaki_reorder_wait_log_due(&wait, 1000000));
	wait.wait_us = 20000;
	munit_assert_false(chiaki_reorder_wait_log_due(&wait, 1000000 + CHIAKI_REORDER_WAIT_LOG_INTERVAL_US - 1));
	munit_assert_true(chiaki_reorder_wait_log_due(&wait, 1000000 + CHIAKI_REORDER_WAIT_LOG_INTERVAL_US));
	return MUNIT_OK;
}

static MunitResult test_stats_publish(const MunitParameter params[], void *user)
{
	ChiakiReorderWait wait;
	chiaki_reorder_wait_init(&wait);
	chiaki_reorder_wait_filled(&wait, 4000);
	chiaki_reorder_wait_skipped(&wait, 10, 2, 1000000, 1016000, true);
	chiaki_reorder_wait_skipped(&wait, 20, 1, 1000000, 1016000, false);
	munit_assert_true(chiaki_reorder_wait_late(&wait, 20, 1030000));
	chiaki_reorder_wait_expire(&wait, 2000000);

	ChiakiReorderWaitStats published = { 0 };
	ChiakiReorderWaitStats stats;
	chiaki_reorder_wait_stats_publish(&wait, &published);
	chiaki_reorder_wait_stats_load(&published, &stats);
	munit_assert_uint32(stats.wait_us, ==, (uint32_t)chiaki_reorder_wait_us(&wait));
	munit_assert_uint32(stats.holes_filled, ==, 1);
	munit_assert_uint32(stats.late_packets, ==, 1);
	munit_assert_uint32(stats.holes_lost, ==, 1);
	munit_assert_uint32(stats.overflow_skips, ==, 1);
	return MUNIT_OK;
}

MunitTest tests_reorder_wait[] = {
	{
		"/starts_at_16_ms",
		test_starts_at_16_ms,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{
		"/always_late_7_ms",
		test_always_late_7_ms,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{
		"/constant_lateness_settles_on_it",
		test_constant_lateness_settles_on_it,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{
		"/pure_loss_drifts_to_3_ms",
		test_pure_loss_drifts_to_3_ms,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{
		"/burst_then_calm",
		test_burst_then_calm,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{
		"/one_outlier_bumps_then_decays",
		test_one_outlier_bumps_then_decays,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{
		"/loss_keeps_reordering_covered",
		test_loss_keeps_reordering_covered,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{
		"/clamps",
		test_clamps,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{
		"/late_packets",
		test_late_packets,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{
		"/late_after_lost_window",
		test_late_after_lost_window,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{
		"/lateness_sample_capped",
		test_lateness_sample_capped,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{
		"/overflow_skips",
		test_overflow_skips,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{
		"/oldest_skip_makes_room",
		test_oldest_skip_makes_room,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{
		"/log_due",
		test_log_due,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{
		"/stats_publish",
		test_stats_publish,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{ NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};
