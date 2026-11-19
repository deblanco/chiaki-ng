// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <munit.h>

#include <chiaki/packetstats.h>

static void push_range(ChiakiPacketStats *stats, ChiakiSeqNum16 first, unsigned count)
{
	for(unsigned i = 0; i < count; i++)
		chiaki_packet_stats_push_seq(stats, (ChiakiSeqNum16)(first + i));
}

static MunitResult test_no_loss_across_wrap(const MunitParameter params[], void *user)
{
	ChiakiPacketStats stats;
	munit_assert_int(chiaki_packet_stats_init(&stats), ==, CHIAKI_ERR_SUCCESS);
	uint64_t received, lost;

	push_range(&stats, 1, 65520); // up to 65520, then a reset window ...
	chiaki_packet_stats_get(&stats, true, &received, &lost);
	push_range(&stats, 65521, 30); // ... that wraps past 65535 to 14
	chiaki_packet_stats_get(&stats, true, &received, &lost);
	munit_assert_uint64(received, ==, 30);
	munit_assert_uint64(lost, ==, 0);

	chiaki_packet_stats_fini(&stats);
	return MUNIT_OK;
}

static MunitResult test_counts_real_loss_across_wrap(const MunitParameter params[], void *user)
{
	ChiakiPacketStats stats;
	munit_assert_int(chiaki_packet_stats_init(&stats), ==, CHIAKI_ERR_SUCCESS);
	uint64_t received, lost;

	push_range(&stats, 1, 65530);
	chiaki_packet_stats_get(&stats, true, &received, &lost);
	push_range(&stats, 65531, 3); // 65531..65533
	push_range(&stats, 2, 5); // 65534, 65535, 0 and 1 lost; 2..6 received
	chiaki_packet_stats_get(&stats, true, &received, &lost);
	munit_assert_uint64(received, ==, 8);
	munit_assert_uint64(lost, ==, 4);

	chiaki_packet_stats_fini(&stats);
	return MUNIT_OK;
}

static MunitResult test_duplicates_are_not_loss(const MunitParameter params[], void *user)
{
	ChiakiPacketStats stats;
	munit_assert_int(chiaki_packet_stats_init(&stats), ==, CHIAKI_ERR_SUCCESS);
	uint64_t received, lost;

	push_range(&stats, 1, 100);
	chiaki_packet_stats_get(&stats, true, &received, &lost);
	push_range(&stats, 101, 10);
	push_range(&stats, 95, 10); // late or repeated packets from before the window
	chiaki_packet_stats_get(&stats, true, &received, &lost);
	munit_assert_uint64(received, ==, 20);
	munit_assert_uint64(lost, ==, 0);

	chiaki_packet_stats_fini(&stats);
	return MUNIT_OK;
}

MunitTest tests_packet_stats[] = {
	{ "/no_loss_across_wrap", test_no_loss_across_wrap, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
	{ "/counts_real_loss_across_wrap", test_counts_real_loss_across_wrap, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
	{ "/duplicates_are_not_loss", test_duplicates_are_not_loss, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
	{ NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};
