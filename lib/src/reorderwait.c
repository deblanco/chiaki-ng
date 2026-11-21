// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <chiaki/reorderwait.h>

#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define STATS_STORE(field, value) __atomic_store_n(&(field), (value), __ATOMIC_RELAXED)
#define STATS_LOAD(field) __atomic_load_n(&(field), __ATOMIC_RELAXED)
#else
// MSVC: aligned 32-bit volatile accesses are atomic
#define STATS_STORE(field, value) (*(volatile uint32_t *)&(field) = (value))
#define STATS_LOAD(field) (*(const volatile uint32_t *)&(field))
#endif

static int64_t clamp_wait(int64_t us)
{
	if(us < CHIAKI_REORDER_WAIT_MIN_US)
		return CHIAKI_REORDER_WAIT_MIN_US;
	if(us > CHIAKI_REORDER_WAIT_MAX_US)
		return CHIAKI_REORDER_WAIT_MAX_US;
	return us;
}

/** What the samples so far call for: srtt + 4 rttvar, clamped */
static int64_t target_wait(const ChiakiReorderWait *wait)
{
	return clamp_wait(wait->srtt_us + 4 * wait->rttvar_us);
}

/**
 * A waited-for hole was lost: wait * 0.95, at least the minimum, and not below the samples' target. Loss
 * says nothing about how late reordered packets come, so it only trims what is above that; else a burst
 * of loss would bring the wait under the reordering seen just before, and skip its next late packet.
 */
static void shrink(ChiakiReorderWait *wait)
{
	int64_t us = clamp_wait(wait->wait_us - wait->wait_us / 20);
	if(wait->sampled)
	{
		int64_t target = target_wait(wait);
		if(us < target)
			us = target < wait->wait_us ? target : wait->wait_us;
	}
	wait->wait_us = us;
}

CHIAKI_EXPORT void chiaki_reorder_wait_init(ChiakiReorderWait *wait)
{
	memset(wait, 0, sizeof(*wait));
	wait->wait_us = CHIAKI_REORDER_WAIT_INITIAL_US;
	wait->logged_wait_us = CHIAKI_REORDER_WAIT_INITIAL_US;
}

/**
 * One lateness sample into the smoother (RFC 6298). The wait follows srtt + 4 rttvar: up at once, down
 * by at most 5% per sample.
 */
static void sample(ChiakiReorderWait *wait, int64_t lateness_us)
{
	if(lateness_us < 0)
		lateness_us = 0;
	if(!wait->sampled)
	{
		wait->sampled = true;
		wait->srtt_us = lateness_us;
		wait->rttvar_us = lateness_us / 2;
	}
	else
	{
		int64_t err = lateness_us - wait->srtt_us;
		if(err < 0)
			err = -err;
		wait->rttvar_us += (err - wait->rttvar_us) / 4;
		wait->srtt_us += (lateness_us - wait->srtt_us) / 8;
	}

	int64_t target = target_wait(wait);
	int64_t floor = wait->wait_us - wait->wait_us / 20;
	if(target >= wait->wait_us)
		wait->wait_us = target;
	else
		wait->wait_us = clamp_wait(target > floor ? target : floor);
}

CHIAKI_EXPORT void chiaki_reorder_wait_filled(ChiakiReorderWait *wait, int64_t lateness_us)
{
	wait->stats.holes_filled++;
	sample(wait, lateness_us);
}

static void resolve(ChiakiReorderWait *wait, ChiakiReorderWaitSkip *skip)
{
	if(skip->late == 0)
	{
		// Nothing came for it: real loss. If it was waited for, that wait was wasted.
		wait->stats.holes_lost++;
		if(!skip->overflow)
			shrink(wait);
	}
	skip->used = false;
}

CHIAKI_EXPORT void chiaki_reorder_wait_skipped(ChiakiReorderWait *wait, ChiakiSeqNum16 begin, uint64_t count,
		int64_t hole_start_us, int64_t now_us, bool overflow)
{
	if(count == 0)
		return;
	if(overflow)
		wait->stats.overflow_skips++;
	if(count > CHIAKI_REORDER_WAIT_SKIP_PACKETS)
	{
		begin = (ChiakiSeqNum16)(begin + (count - CHIAKI_REORDER_WAIT_SKIP_PACKETS));
		count = CHIAKI_REORDER_WAIT_SKIP_PACKETS;
	}

	ChiakiReorderWaitSkip *skip = &wait->skips[wait->skips_next];
	if(skip->used)
		resolve(wait, skip);
	memset(skip, 0, sizeof(*skip));
	skip->used = true;
	skip->overflow = overflow;
	skip->begin = begin;
	skip->count = (uint16_t)count;
	skip->hole_start_us = hole_start_us;
	skip->skip_us = now_us;
	wait->skips_next = (wait->skips_next + 1) % CHIAKI_REORDER_WAIT_SKIPS;
}

CHIAKI_EXPORT bool chiaki_reorder_wait_late(ChiakiReorderWait *wait, ChiakiSeqNum16 seq_num, int64_t now_us)
{
	// Holes skipped CHIAKI_REORDER_WAIT_LOST_US ago were lost, even if no flush ran since to say so (no
	// video came in between): a packet of theirs now is too old to tell how late packets come.
	chiaki_reorder_wait_expire(wait, now_us);
	for(size_t i = 0; i < CHIAKI_REORDER_WAIT_SKIPS; i++)
	{
		ChiakiReorderWaitSkip *skip = &wait->skips[i];
		uint16_t offset = (uint16_t)(seq_num - skip->begin);
		if(!skip->used || offset >= skip->count)
			continue;
		uint8_t bit = (uint8_t)(1 << (offset % 8));
		if(skip->seen[offset / 8] & bit)
			return false; // a duplicate of a late packet
		skip->seen[offset / 8] |= bit;
		skip->late++;
		wait->stats.late_packets++;

		// The wait was too short for this one: cover it at once, and learn from it.
		int64_t lateness_us = now_us - skip->hole_start_us;
		// Later than the longest wait can't be covered anyway: as a sample, it would only hold the wait
		// at that ceiling for many samples after.
		sample(wait, lateness_us < CHIAKI_REORDER_WAIT_MAX_US ? lateness_us : CHIAKI_REORDER_WAIT_MAX_US);
		int64_t cover = clamp_wait(lateness_us + lateness_us / 4);
		if(cover > wait->wait_us)
			wait->wait_us = cover;
		return true;
	}
	return false;
}

CHIAKI_EXPORT void chiaki_reorder_wait_expire(ChiakiReorderWait *wait, int64_t now_us)
{
	for(size_t i = 0; i < CHIAKI_REORDER_WAIT_SKIPS; i++)
	{
		ChiakiReorderWaitSkip *skip = &wait->skips[i];
		if(skip->used && now_us - skip->skip_us >= CHIAKI_REORDER_WAIT_LOST_US)
			resolve(wait, skip);
	}
}

CHIAKI_EXPORT bool chiaki_reorder_wait_log_due(ChiakiReorderWait *wait, int64_t now_us)
{
	int64_t moved = wait->wait_us - wait->logged_wait_us;
	if(moved < 0)
		moved = -moved;
	if(moved < CHIAKI_REORDER_WAIT_LOG_STEP_US)
		return false;
	if(wait->logged && now_us - wait->logged_at_us < CHIAKI_REORDER_WAIT_LOG_INTERVAL_US)
		return false;
	wait->logged = true;
	wait->logged_wait_us = wait->wait_us;
	wait->logged_at_us = now_us;
	return true;
}

CHIAKI_EXPORT void chiaki_reorder_wait_stats_publish(const ChiakiReorderWait *wait, ChiakiReorderWaitStats *stats)
{
	STATS_STORE(stats->wait_us, (uint32_t)wait->wait_us);
	STATS_STORE(stats->holes_filled, wait->stats.holes_filled);
	STATS_STORE(stats->late_packets, wait->stats.late_packets);
	STATS_STORE(stats->holes_lost, wait->stats.holes_lost);
	STATS_STORE(stats->overflow_skips, wait->stats.overflow_skips);
}

CHIAKI_EXPORT void chiaki_reorder_wait_stats_load(const ChiakiReorderWaitStats *stats, ChiakiReorderWaitStats *out)
{
	out->wait_us = STATS_LOAD(stats->wait_us);
	out->holes_filled = STATS_LOAD(stats->holes_filled);
	out->late_packets = STATS_LOAD(stats->late_packets);
	out->holes_lost = STATS_LOAD(stats->holes_lost);
	out->overflow_skips = STATS_LOAD(stats->overflow_skips);
}
