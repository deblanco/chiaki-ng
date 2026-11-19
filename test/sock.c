// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <munit.h>

#include "../lib/src/utils.h"
#include "test_log.h"

#ifndef _WIN32
#include <unistd.h>
#endif

static int int_option(chiaki_socket_t sock, int level, int name)
{
	int value = -1;
	socklen_t size = sizeof(value);
	munit_assert_int(getsockopt(sock, level, name, (CHIAKI_SOCKET_BUF_TYPE)&value, &size), ==, 0);
	return value;
}

static MunitResult test_tcp_socket_tune(const MunitParameter params[], void *user)
{
	chiaki_socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	munit_assert_false(CHIAKI_SOCKET_IS_INVALID(sock));
	munit_assert_int(int_option(sock, IPPROTO_TCP, TCP_NODELAY), ==, 0);

	chiaki_tcp_socket_tune(sock, get_test_log(), "Test");

	munit_assert_int(int_option(sock, IPPROTO_TCP, TCP_NODELAY), !=, 0);
#ifdef __APPLE__
	munit_assert_int(int_option(sock, SOL_SOCKET, SO_NOSIGPIPE), !=, 0);
#if defined(SO_NET_SERVICE_TYPE) && defined(NET_SERVICE_TYPE_RV)
	munit_assert_int(int_option(sock, SOL_SOCKET, SO_NET_SERVICE_TYPE), ==, NET_SERVICE_TYPE_RV);
#endif
#endif
	CHIAKI_SOCKET_CLOSE(sock);
	return MUNIT_OK;
}

MunitTest tests_sock[] = {
	{ "/tcp_socket_tune", test_tcp_socket_tune, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
	{ NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};
