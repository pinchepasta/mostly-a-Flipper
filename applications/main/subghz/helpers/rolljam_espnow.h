#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ROLLJAM_ESPNOW_MAGIC   0x524A414Du /* "RJAM" */
#define ROLLJAM_ESPNOW_VERSION 1
#define ROLLJAM_MAX_PEERS      4
#define ROLLJAM_BEACON_MS      250
#define ROLLJAM_PEER_TIMEOUT   3000

typedef enum {
    RolljamMsgBeacon = 1,
    RolljamMsgBye,
    RolljamMsgArm,
    RolljamMsgJamOn,
    RolljamMsgJamOff,
    RolljamMsgJamAck,
    RolljamMsgCode,
} RolljamMsgType;

typedef enum {
    RolljamRoleNone = 0,
    RolljamRoleSniffer,
    RolljamRoleJammer,
} RolljamRole;

typedef struct {
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint8_t role;
    uint8_t state;
    uint32_t seq;
    uint32_t frequency;
    uint32_t codes;
    uint32_t type_protocol;
    char protocol[24];
} RolljamPacket;

typedef struct {
    uint8_t mac[6];
    uint8_t role;
    int8_t rssi;
    uint32_t last_seen;
} RolljamPeer;

typedef struct RolljamEspNow RolljamEspNow;

RolljamEspNow* rolljam_espnow_alloc(void);
void rolljam_espnow_free(RolljamEspNow* instance);

bool rolljam_espnow_start(RolljamEspNow* instance, uint8_t channel);
void rolljam_espnow_stop(RolljamEspNow* instance);
bool rolljam_espnow_is_up(RolljamEspNow* instance);

bool rolljam_espnow_send(RolljamEspNow* instance, const RolljamPacket* packet);
bool rolljam_espnow_recv(
    RolljamEspNow* instance,
    RolljamPacket* packet,
    uint8_t* src_mac,
    int8_t* rssi,
    uint32_t timeout_ms);

uint8_t rolljam_espnow_peer_count(RolljamEspNow* instance);
bool rolljam_espnow_peer_get(RolljamEspNow* instance, uint8_t index, RolljamPeer* peer);
void rolljam_espnow_peer_expire(RolljamEspNow* instance, uint32_t timeout_ms);
void rolljam_espnow_peer_forget(RolljamEspNow* instance);

const char* rolljam_role_name(uint8_t role);

#ifdef __cplusplus
}
#endif
