// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <munit.h>

#include <chiaki/rpcrypt.h>
#include <chiaki/regist.h>
#include <chiaki/session.h>
#include <chiaki/thread.h>
#include <chiaki/remote/holepunch.h>

#include "test_log.h"

#include <string.h>
#include <stdlib.h>

#ifndef _WIN32
#include <arpa/inet.h>
#include <unistd.h>
#endif

static const uint8_t ambassador[CHIAKI_RPCRYPT_KEY_SIZE] = { 0x13, 0x37, 0xde, 0xad, 0xbe, 0xef, 0xc0, 0xff, 0xee, 0x42, 0x63, 0x68, 0x69, 0x61, 0x6b, 0x69 };
static const uint32_t pin = 13374201;
static const char * const psn_id = "ChiakiNanami1337";

static MunitResult test_aeropause_ps4_pre10(const MunitParameter params[], void *user)
{
	uint8_t expected[CHIAKI_RPCRYPT_KEY_SIZE] = { 0x0b, 0xe1, 0x2f, 0xbb, 0x4c, 0x7c, 0x99, 0x4a, 0x41, 0x1e, 0x2d, 0x4c, 0xa4, 0x19, 0xf4, 0x35 };
	uint8_t aeropause[CHIAKI_RPCRYPT_KEY_SIZE];
	chiaki_rpcrypt_aeropause_ps4_pre10(aeropause, ambassador);
	munit_assert_memory_equal(sizeof(expected), aeropause, expected);
	return MUNIT_OK;
}

static MunitResult test_pin_bright_ps4_pre10(const MunitParameter params[], void *user)
{
	uint8_t expected[CHIAKI_RPCRYPT_KEY_SIZE] = { 0x3f, 0xd0, 0xd6, 0x4f, 0xdc, 0xbb, 0x3e, 0xcc, 0x50, 0xba, 0xed, 0xef, 0x97, 0x34, 0xc7, 0xc9 };
	ChiakiRPCrypt rpcrypt;
	chiaki_rpcrypt_init_regist_ps4_pre10(&rpcrypt, ambassador, pin);
	munit_assert_memory_equal(sizeof(expected), rpcrypt.bright, expected);
	return MUNIT_OK;
}

static MunitResult test_request_payload_ps4_pre10(const MunitParameter params[], void *user)
{
	uint8_t expected[] = {
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x0b, 0xe1, 0x2f, 0xbb,
		0x4c, 0x7c, 0x99, 0x4a, 0x41, 0x1e, 0x2d, 0x4c, 0xa4, 0x19, 0xf4, 0x35, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		0xf2, 0xfd, 0x4d, 0xdc, 0x86, 0x21, 0xda, 0x3d, 0x9f, 0x35, 0xf3, 0xdf, 0x82, 0x58, 0x65, 0x08, 0x6f, 0xb5, 0xee, 0xd1, 0x1e, 0xdd, 0xdd, 0xaa, 0x5a, 0xce, 0x28, 0xdf, 0x2f, 0x13, 0x9c, 0x0c,
		0x66, 0x6f, 0xec, 0x1c, 0x5d, 0xd1, 0x9f, 0x08, 0x15, 0xdd, 0x5c, 0x61, 0x57, 0xe6, 0xad, 0x69, 0x60, 0x12, 0x67, 0x2d, 0x4b, 0x64,
	};

	ChiakiRPCrypt rpcrypt;

	uint8_t payload[0x400];
	size_t payload_size = sizeof(payload);
	ChiakiErrorCode err = chiaki_regist_request_payload_format(CHIAKI_TARGET_PS4_9, ambassador, payload, &payload_size, &rpcrypt, psn_id, NULL, pin, NULL);
	munit_assert_int(err, ==, CHIAKI_ERR_SUCCESS);
	munit_assert_size(payload_size, ==, sizeof(expected));
	munit_assert_memory_equal(sizeof(expected), payload, expected);
	return MUNIT_OK;
}

static uint32_t application_reason_of(const char *response)
{
	char *buf = strdup(response);
	munit_assert_ptr_not_null(buf);
	ChiakiHttpResponse parsed;
	munit_assert_int(chiaki_http_response_parse(&parsed, buf, strlen(buf)), ==, CHIAKI_ERR_SUCCESS);
	uint32_t reason = chiaki_regist_response_application_reason(&parsed);
	chiaki_http_response_fini(&parsed);
	free(buf);
	return reason;
}

static MunitResult test_response_application_reason(const MunitParameter params[], void *user)
{
	munit_assert_uint32(application_reason_of(
			"HTTP/1.1 403 Forbidden\r\n"
			"Content-Length: 0\r\n"
			"RP-Application-Reason: 80108b09\r\n"
			"\r\n"), ==, CHIAKI_RP_APPLICATION_REASON_REGIST_FAILED);
	munit_assert_uint32(application_reason_of(
			"HTTP/1.1 403 Forbidden\r\n"
			"RP-Application-Reason: 0x80108b02\r\n"
			"\r\n"), ==, CHIAKI_RP_APPLICATION_REASON_INVALID_PSN_ID);
	munit_assert_uint32(application_reason_of(
			"HTTP/1.1 403 Forbidden\r\n"
			"Content-Length: 0\r\n"
			"\r\n"), ==, 0);
	return MUNIT_OK;
}

static void record_event(ChiakiRegistEvent *event, void *user)
{
	*(ChiakiRegistEvent *)user = *event;
	((ChiakiRegistEvent *)user)->registered_host = NULL;
}

static ChiakiRegistInfo loopback_regist_info(void)
{
	ChiakiRegistInfo info = { 0 };
	info.target = CHIAKI_TARGET_PS5_1;
	info.host = "127.0.0.1";
	info.pin = pin;
	return info;
}

static MunitResult test_failure_before_the_console_answered(const MunitParameter params[], void *user)
{
	// Nothing listens on loopback: the search is refused, as by a console whose Pair Device screen is closed.
	ChiakiRegistInfo info = loopback_regist_info();
	ChiakiRegistEvent event = { 0 };
	ChiakiRegist regist;
	munit_assert_int(chiaki_regist_start(&regist, get_test_log(), &info, record_event, &event), ==, CHIAKI_ERR_SUCCESS);
	chiaki_regist_fini(&regist); // joins: the event is in
	munit_assert_int(event.type, ==, CHIAKI_REGIST_EVENT_TYPE_FINISHED_FAILED);
	munit_assert_int(event.error, ==, CHIAKI_ERR_CONNECTION_REFUSED);
	munit_assert_false(event.console_answered);
	munit_assert_int(event.http_code, ==, 0);
	return MUNIT_OK;
}

static MunitResult test_failure_after_the_console_answered(const MunitParameter params[], void *user)
{
	// A stand-in console answers the search, then refuses the request connection (nothing listens on TCP).
	chiaki_socket_t console = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	munit_assert_false(CHIAKI_SOCKET_IS_INVALID(console));
	struct sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_port = htons(9295);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if(bind(console, (struct sockaddr *)&addr, sizeof(addr)) != 0)
	{
		CHIAKI_SOCKET_CLOSE(console);
		return MUNIT_SKIP; // the registration port is taken on this machine
	}
	struct timeval timeout = { 5, 0 };
	setsockopt(console, SOL_SOCKET, SO_RCVTIMEO, (const void *)&timeout, sizeof(timeout));

	ChiakiRegistInfo info = loopback_regist_info();
	ChiakiRegistEvent event = { 0 };
	ChiakiRegist regist;
	munit_assert_int(chiaki_regist_start(&regist, get_test_log(), &info, record_event, &event), ==, CHIAKI_ERR_SUCCESS);
	char search[8] = { 0 };
	struct sockaddr_storage from;
	socklen_t from_size = sizeof(from);
	CHIAKI_SSIZET_TYPE n = recvfrom(console, search, sizeof(search) - 1, 0, (struct sockaddr *)&from, &from_size);
	munit_assert_string_equal(n > 0 ? search : "", "SRC3");
	munit_assert_int((int)sendto(console, "RES3", 5, 0, (struct sockaddr *)&from, from_size), ==, 5);
	chiaki_regist_fini(&regist);
	CHIAKI_SOCKET_CLOSE(console);

	munit_assert_int(event.type, ==, CHIAKI_REGIST_EVENT_TYPE_FINISHED_FAILED);
	// CONNECTION_REFUSED, or UNKNOWN on macOS, where a failed non-blocking connect hides its cause from getpeername
	munit_assert_int(event.error, !=, CHIAKI_ERR_SUCCESS);
	munit_assert_int(event.error, !=, CHIAKI_ERR_HTTP_NONOK);
	munit_assert_true(event.console_answered);
	munit_assert_int(event.http_code, ==, 0);
	return MUNIT_OK;
}

static void record_quit(ChiakiEvent *event, void *user)
{
	if(event->type == CHIAKI_EVENT_QUIT)
		*(ChiakiQuitReason *)user = event->quit.reason;
}

typedef struct
{
	ChiakiSession session;
	ChiakiQuitReason quit_reason;
	ChiakiBoolPredCond joined;
} StoppedSession;

static void *join_session(void *user)
{
	StoppedSession *stopped = user;
	chiaki_session_join(&stopped->session);
	chiaki_bool_pred_cond_signal(&stopped->joined);
	return NULL;
}

static MunitResult test_psn_stop_while_registering(const MunitParameter params[], void *user)
{
	// A stand-in console on loopback receives the PSN registration's RUDP init and never answers, as a slow PS5.
	chiaki_socket_t console = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	munit_assert_false(CHIAKI_SOCKET_IS_INVALID(console));
	struct sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	socklen_t addr_size = sizeof(addr);
	munit_assert_int(bind(console, (struct sockaddr *)&addr, sizeof(addr)), ==, 0);
	munit_assert_int(getsockname(console, (struct sockaddr *)&addr, &addr_size), ==, 0);
	struct timeval timeout = { 5, 0 };
	setsockopt(console, SOL_SOCKET, SO_RCVTIMEO, (const void *)&timeout, sizeof(timeout));
	chiaki_socket_t ctrl = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	munit_assert_int(connect(ctrl, (struct sockaddr *)&addr, sizeof(addr)), ==, 0);

	// The holepunch session stands for a punched hole: only its control socket is used (rudp closes it).
	ChiakiHolepunchSession holepunch = chiaki_holepunch_session_init("token", get_test_log());
	munit_assert_ptr_not_null(holepunch);
	*chiaki_get_holepunch_sock(holepunch, CHIAKI_HOLEPUNCH_PORT_TYPE_CTRL) = ctrl;
	ChiakiConnectInfo connect_info = { 0 };
	connect_info.ps5 = true;
	connect_info.auto_regist = true;
	connect_info.holepunch_session = holepunch;

	// On the heap: a session that never joins can't be freed, and its threads must not outlive the stack.
	StoppedSession *stopped = calloc(1, sizeof(StoppedSession));
	munit_assert_ptr_not_null(stopped);
	stopped->quit_reason = CHIAKI_QUIT_REASON_NONE;
	munit_assert_int(chiaki_bool_pred_cond_init(&stopped->joined), ==, CHIAKI_ERR_SUCCESS);
	munit_assert_int(chiaki_session_init(&stopped->session, &connect_info, get_test_log()), ==, CHIAKI_ERR_SUCCESS);
	chiaki_session_set_event_cb(&stopped->session, record_quit, &stopped->quit_reason);
	munit_assert_int(chiaki_session_start(&stopped->session), ==, CHIAKI_ERR_SUCCESS);

	uint8_t init[64];
	munit_assert_int((int)recv(console, (CHIAKI_SOCKET_BUF_TYPE)init, sizeof(init), 0), >, 0); // registering

	// Stopped mid-registration, the session must still end: the registration fails a few seconds later (no
	// answer), and its callback needs the session's state mutex while the session thread waits for it.
	chiaki_session_stop(&stopped->session);
	ChiakiThread joiner;
	munit_assert_int(chiaki_thread_create(&joiner, join_session, stopped), ==, CHIAKI_ERR_SUCCESS);
	chiaki_bool_pred_cond_lock(&stopped->joined);
	ChiakiErrorCode err = chiaki_bool_pred_cond_timedwait(&stopped->joined, 15000);
	chiaki_bool_pred_cond_unlock(&stopped->joined);
	CHIAKI_SOCKET_CLOSE(console);
	if(err != CHIAKI_ERR_SUCCESS)
		munit_error("the stopped session never ended: deadlocked on its state mutex"); // leaves it, still blocked

	chiaki_thread_join(&joiner, NULL);
	// The stop is the reason, not the registration that failed after it
	munit_assert_int(stopped->quit_reason, ==, CHIAKI_QUIT_REASON_STOPPED);
	chiaki_session_fini(&stopped->session);
	chiaki_bool_pred_cond_fini(&stopped->joined);
	free(stopped);
	return MUNIT_OK;
}

MunitTest tests_regist[] = {
	{
		"/aeropause_ps4_pre10",
		test_aeropause_ps4_pre10,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{
		"/pin_bright_ps4_pre10",
		test_pin_bright_ps4_pre10,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{
		"/request_payload_ps4_pre10",
		test_request_payload_ps4_pre10,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{
		"/response_application_reason",
		test_response_application_reason,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{
		"/failure_before_the_console_answered",
		test_failure_before_the_console_answered,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{
		"/failure_after_the_console_answered",
		test_failure_after_the_console_answered,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{
		"/psn_stop_while_registering",
		test_psn_stop_while_registering,
		NULL,
		NULL,
		MUNIT_TEST_OPTION_NONE,
		NULL
	},
	{ NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};
