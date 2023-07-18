/* Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

#include <stdint.h>
#include "queue.h"
#include "tree.h"
#include "sctp_eNB_defs.h"
#include "xnap_messages_types.h"

#ifndef XNAP_GNB_DEFS_H_
#define XNAP_GNB_DEFS_H_

#define XNAP_GNB_NAME_LENGTH_MAX (150)

typedef enum {
  /* Disconnected state: initial state for any association. */
  XNAP_GNB_STATE_DISCONNECTED = 0x0,

  /* State waiting for xn Setup response message if the target gNB accepts or
   * Xn Setup failure if rejects the gNB.
   */
  XNAP_GNB_STATE_WAITING = 0x1,

  /* The gNB is successfully connected to another gNB. */
  XNAP_GNB_STATE_CONNECTED = 0x2,

  /* XnAP is ready, and the gNB is successfully connected to another gNB. */
  XNAP_GNB_STATE_READY = 0x3,

  XNAP_GNB_STATE_OVERLOAD = 0x4,

  XNAP_GNB_STATE_RESETTING = 0x5,

  /* Max number of states available */
  XNAP_GNB_STATE_MAX,
} xnap_gNB_state_t;

struct xnap_gNB_instance_s;

/* This structure describes association of gNBs over Xn  */
typedef struct xnap_gNB_data_t {
  /* gNB descriptors tree, ordered by sctp assoc id */
  RB_ENTRY(xnap_gNB_data_t) entry;
  char *gNB_name;
  /*  target gNB ID */
  uint64_t gNB_id;
  /* Current gNB->gNB XnAP association state */
  xnap_gNB_state_t state;
  /* Number of input/ouput streams */
  uint16_t in_streams;
  uint16_t out_streams;
  /* Connexion id used between SCTP/XNAP */
  uint16_t cnx_id;
  /* SCTP association id */
  sctp_assoc_t assoc_id;
} xnap_gNB_data_t;

typedef struct xnap_gNB_instance_s {
  /* Number of target gNBs requested by gNB (tree size) */
  uint32_t xn_target_gnb_nb;
  /* Number of target gNBs for which association is pending */
  uint32_t xn_target_gnb_pending_nb;
  /* Number of target gNB successfully associated to gNB */
  uint32_t xn_target_gnb_associated_nb;
  /* Tree of XNAP gNB associations ordered by association ID */
  RB_HEAD(xnap_gnb_tree, xnap_gNB_data_t) xnap_gnbs; // gNBs, indexed by assoc_id
  size_t num_gnbs;
  instance_t instance;
  xnap_setup_req_t setup_req;

  /* The gNB IP address to bind */
  xnap_net_config_t net_config;
  /* SCTP information */
  xnap_sctp_t sctp_streams;
  char *gNB_name;
} xnap_gNB_instance_t;

#endif /* XNAP_GNB_DEFS_H_ */
