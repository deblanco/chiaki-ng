// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <munit.h>

#include <chiaki/videoreceiver.h>

static MunitResult test_frames_lost_between(const MunitParameter params[], void *user)
{
	(void)params;
	(void)user;
	// The first frame handed over: nothing before it counts.
	munit_assert_int32(chiaki_video_receiver_frames_lost_between(-1, 1), ==, 0);
	munit_assert_int32(chiaki_video_receiver_frames_lost_between(-1, 500), ==, 0);
	// The next frame: none lost.
	munit_assert_int32(chiaki_video_receiver_frames_lost_between(10, 11), ==, 0);
	// Frames 11 to 19 never handed over (FEC failures, skipped while waiting for an IDR): 9, once.
	munit_assert_int32(chiaki_video_receiver_frames_lost_between(10, 20), ==, 9);
	// Across the 16-bit wrap.
	munit_assert_int32(chiaki_video_receiver_frames_lost_between(65534, 2), ==, 3);
	// The same frame again isn't a wrap of 65535 lost.
	munit_assert_int32(chiaki_video_receiver_frames_lost_between(7, 7), ==, 0);
	return MUNIT_OK;
}

MunitTest tests_video_receiver[] = {
	{
		"/frames_lost_between",
		test_frames_lost_between,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{ NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};
