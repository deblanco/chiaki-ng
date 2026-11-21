// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_REORDERWAIT_H
#define CHIAKI_REORDERWAIT_H

#include "common.h"
#include "seqnum.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * How long Takion waits for a missing video packet before skipping it, measured from the link.
 *
 * A hole is a run of missing packets at the head of the reorder queue; it starts waiting when it becomes
 * the head. Its lateness d (from then to its packets arriving) is learned from two signals:
 * - the hole fills while waiting (chiaki_reorder_wait_filled);
 * - a packet arrives after its hole was skipped (chiaki_reorder_wait_late): the wait was too short, so it
 *   grows to at least 1.25 d at once.
 * Both feed a TCP RTO style smoother (RFC 6298: srtt, rttvar), and the wait follows srtt + 4 rttvar: up at
 * once, down by at most 5% per sample. A sample is at most CHIAKI_REORDER_WAIT_MAX_US: later can't be
 * covered anyway. A skipped hole that nothing arrives for within CHIAKI_REORDER_WAIT_LOST_US was real
 * loss, the wait for it was wasted: the wait shrinks by 5%, but not below srtt + 4 rttvar once there are
 * samples (loss says nothing about how late reordered packets come).
 *
 * Skipping too early loses a frame (FEC has few units to spare), waiting too long only delays the frames
 * behind the hole: so late packets are covered generously and the wait shrinks slowly.
 *
 * Not thread-safe: Takion's receive thread owns it. Other threads read a published copy
 * (ChiakiReorderWaitStats).
 */

#define CHIAKI_REORDER_WAIT_MIN_US 3000
#define CHIAKI_REORDER_WAIT_MAX_US 20000
#define CHIAKI_REORDER_WAIT_INITIAL_US 16000 // ~1 frame at 60 fps, the fixed wait this replaces
/** A skipped hole nothing arrived for by then was lost. */
#define CHIAKI_REORDER_WAIT_LOST_US 100000
/** Skipped holes kept to recognize late packets. */
#define CHIAKI_REORDER_WAIT_SKIPS 32
/** Packets a kept hole covers at most (the reorder queue's size): of a longer one, the last. */
#define CHIAKI_REORDER_WAIT_SKIP_PACKETS 256
/** The wait is logged when it moved by this much, at most once per CHIAKI_REORDER_WAIT_LOG_INTERVAL_US. */
#define CHIAKI_REORDER_WAIT_LOG_STEP_US 2000
#define CHIAKI_REORDER_WAIT_LOG_INTERVAL_US 5000000

typedef struct chiaki_reorder_wait_skip_t
{
	bool used;
	bool overflow; // skipped because the queue was full, not after waiting
	ChiakiSeqNum16 begin;
	uint16_t count;
	int64_t hole_start_us;
	int64_t skip_us;
	uint32_t late; // packets of it that came after the skip
	uint8_t seen[CHIAKI_REORDER_WAIT_SKIP_PACKETS / 8];
} ChiakiReorderWaitSkip;

typedef struct chiaki_reorder_wait_stats_t
{
	uint32_t wait_us;
	uint32_t holes_filled; // holes that filled while waiting
	uint32_t late_packets; // packets that came after their hole was skipped
	uint32_t holes_lost; // skipped holes nothing came for
	uint32_t overflow_skips; // holes skipped because the queue was full
} ChiakiReorderWaitStats;

typedef struct chiaki_reorder_wait_t
{
	int64_t wait_us;
	bool sampled;
	int64_t srtt_us;
	int64_t rttvar_us;
	ChiakiReorderWaitSkip skips[CHIAKI_REORDER_WAIT_SKIPS];
	size_t skips_next; // the slot the next skip goes to: the oldest when all are used
	ChiakiReorderWaitStats stats;
	bool logged;
	int64_t logged_wait_us;
	int64_t logged_at_us;
} ChiakiReorderWait;

CHIAKI_EXPORT void chiaki_reorder_wait_init(ChiakiReorderWait *wait);

static inline int64_t chiaki_reorder_wait_us(const ChiakiReorderWait *wait)
{
	return wait->wait_us;
}

/**
 * The hole at the head filled while waiting, lateness_us after it started waiting.
 */
CHIAKI_EXPORT void chiaki_reorder_wait_filled(ChiakiReorderWait *wait, int64_t lateness_us);

/**
 * The count missing packets from begin were skipped at now_us; the hole had been waiting since
 * hole_start_us. overflow: skipped because the queue was full, not after the wait.
 */
CHIAKI_EXPORT void chiaki_reorder_wait_skipped(ChiakiReorderWait *wait, ChiakiSeqNum16 begin, uint64_t count,
		int64_t hole_start_us, int64_t now_us, bool overflow);

/**
 * A packet older than the reorder queue's start arrived at now_us. Expires the skipped holes first
 * (chiaki_reorder_wait_expire), so a packet of one skipped CHIAKI_REORDER_WAIT_LOST_US ago isn't late.
 * @return true if it belongs to a skipped hole and is the first of its number: a late packet. False for
 * anything else (a duplicate of a delivered or a late packet, or one of a hole forgotten already).
 */
CHIAKI_EXPORT bool chiaki_reorder_wait_late(ChiakiReorderWait *wait, ChiakiSeqNum16 seq_num, int64_t now_us);

/**
 * Forgets the skipped holes older than CHIAKI_REORDER_WAIT_LOST_US: those nothing came for were lost.
 */
CHIAKI_EXPORT void chiaki_reorder_wait_expire(ChiakiReorderWait *wait, int64_t now_us);

/**
 * @return true if the wait should be logged now (it moved by CHIAKI_REORDER_WAIT_LOG_STEP_US since it
 * last was, and not within CHIAKI_REORDER_WAIT_LOG_INTERVAL_US of that); it then counts as logged.
 */
CHIAKI_EXPORT bool chiaki_reorder_wait_log_due(ChiakiReorderWait *wait, int64_t now_us);

/**
 * Copies the wait and counters to stats with atomic stores, for chiaki_reorder_wait_stats_load() on
 * another thread.
 */
CHIAKI_EXPORT void chiaki_reorder_wait_stats_publish(const ChiakiReorderWait *wait, ChiakiReorderWaitStats *stats);

/**
 * Reads stats published by chiaki_reorder_wait_stats_publish() with atomic loads. Each field is
 * consistent on its own; together they may be one update apart.
 */
CHIAKI_EXPORT void chiaki_reorder_wait_stats_load(const ChiakiReorderWaitStats *stats, ChiakiReorderWaitStats *out);

#ifdef __cplusplus
}
#endif

#endif // CHIAKI_REORDERWAIT_H
