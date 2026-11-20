// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_REGIST_H
#define CHIAKI_REGIST_H

#include "common.h"
#include "log.h"
#include "thread.h"
#include "stoppipe.h"
#include "rpcrypt.h"
#include "http.h"
#include "remote/holepunch.h"
#include "remote/rudp.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CHIAKI_PSN_ACCOUNT_ID_SIZE 8
#define CHIAKI_SESSION_AUTH_SIZE 0x10

typedef struct chiaki_regist_info_t
{
	ChiakiTarget target;
	const char *host;
	bool broadcast;

	/**
	 * may be null, in which case psn_account_id will be used
	 */
	const char *psn_online_id;

	/**
	 * will be used if psn_online_id is null, for PS4 >= 7.0
	 */
	uint8_t psn_account_id[CHIAKI_PSN_ACCOUNT_ID_SIZE];

	uint32_t pin;
	uint32_t console_pin;
	/**
	 * may be null, in which regular regist (instead of PSN Regist will be used)
	 */
	ChiakiHolepunchRegistInfo* holepunch_info;
	/**
	 * NULL unless using PSN Regist
	 */
	ChiakiRudp rudp;
} ChiakiRegistInfo;

typedef struct chiaki_registered_host_t
{
	ChiakiTarget target;
	char ap_ssid[0x30];
	char ap_bssid[0x20];
	char ap_key[0x50];
	char ap_name[0x20];
	uint8_t server_mac[6];
	char server_nickname[0x20];
	char rp_regist_key[CHIAKI_SESSION_AUTH_SIZE]; // must be completely filled (pad with \0)
	uint32_t rp_key_type;
	uint8_t rp_key[0x10];
	uint32_t console_pin;
} ChiakiRegisteredHost;

typedef enum chiaki_regist_event_type_t {
	CHIAKI_REGIST_EVENT_TYPE_FINISHED_CANCELED,
	CHIAKI_REGIST_EVENT_TYPE_FINISHED_FAILED,
	CHIAKI_REGIST_EVENT_TYPE_FINISHED_SUCCESS
} ChiakiRegistEventType;

typedef struct chiaki_regist_event_t
{
	ChiakiRegistEventType type;
	ChiakiRegisteredHost *registered_host;

	/**
	 * Why a CHIAKI_REGIST_EVENT_TYPE_FINISHED_FAILED registration failed:
	 * CHIAKI_ERR_TIMEOUT: the console didn't answer in time;
	 * CHIAKI_ERR_CONNECTION_REFUSED: the console refused it (its Link Device (PS5) or Add Device (PS4) screen isn't open);
	 * CHIAKI_ERR_HTTP_NONOK: the console rejected the request, see http_code and application_reason;
	 * CHIAKI_ERR_NETWORK and other network errors: the console couldn't be reached.
	 */
	ChiakiErrorCode error;
	/**
	 * The console's HTTP status when it rejected the request, else 0.
	 */
	int http_code;
	/**
	 * The console's RP-Application-Reason (CHIAKI_RP_APPLICATION_REASON_*) when it rejected the request, else 0.
	 */
	uint32_t application_reason;
} ChiakiRegistEvent;

typedef void (*ChiakiRegistCb)(ChiakiRegistEvent *event, void *user);

typedef struct chiaki_regist_t
{
	ChiakiLog *log;
	ChiakiRegistInfo info;
	ChiakiRegistCb cb;
	void *cb_user;

	ChiakiThread thread;
	ChiakiStopPipe stop_pipe;
} ChiakiRegist;

CHIAKI_EXPORT ChiakiErrorCode chiaki_regist_start(ChiakiRegist *regist, ChiakiLog *log, const ChiakiRegistInfo *info, ChiakiRegistCb cb, void *cb_user);
CHIAKI_EXPORT void chiaki_regist_fini(ChiakiRegist *regist);
CHIAKI_EXPORT void chiaki_regist_stop(ChiakiRegist *regist);

/**
 * @return the RP-Application-Reason header of a regist response (hex), or 0 if it has none
 */
CHIAKI_EXPORT uint32_t chiaki_regist_response_application_reason(const ChiakiHttpResponse *response);

/**
 * @param psn_account_id must be exactly of size CHIAKI_PSN_ACCOUNT_ID_SIZE
 */
CHIAKI_EXPORT ChiakiErrorCode chiaki_regist_request_payload_format(ChiakiTarget target, const uint8_t *ambassador, uint8_t *buf, size_t *buf_size, ChiakiRPCrypt *crypt, const char *psn_online_id, const uint8_t *psn_account_id, uint32_t pin, ChiakiHolepunchRegistInfo *holepunch_info);

#ifdef __cplusplus
}
#endif

#endif // CHIAKI_REGIST_H
