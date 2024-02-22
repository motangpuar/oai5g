/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
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

#define _GNU_SOURCE
#include "nr_sdap.h"
#include "openair2/LAYER2/RLC/rlc.h"
#include "openair2/RRC/NAS/nas_config.h"
#include "openair1/SIMULATION/ETH_TRANSPORT/proto.h"
#include <pthread.h>

struct thread_args {
  int sock_fd;
};

static void *gnb_tun_read_thread(void *arg)
{
  int sock_fd = ((struct thread_args *)arg)->sock_fd;
  char rx_buf[NL_MAX_PAYLOAD];
  int len;
  protocol_ctxt_t ctxt;
  ue_id_t UEid;

  int rb_id = 1;
  pthread_setname_np(pthread_self(), "gnb_tun_read");

  while (1) {
    len = read(sock_fd, &rx_buf, NL_MAX_PAYLOAD);
    if (len == -1) {
      LOG_E(SDAP, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
      exit(1);
    }

    LOG_D(SDAP, "%s read data of size %d\n", __func__, len);

    const bool has_ue = nr_sdap_get_first_ue_id(&UEid);

    if (!has_ue)
      continue;

    ctxt.enb_flag = 1;
    ctxt.rntiMaybeUEid = UEid;

    uint8_t qfi = 7;
    bool rqi = 0;
    int pdusession_id = 10;

    sdap_data_req(&ctxt,
                  UEid,
                  SRB_FLAG_NO,
                  rb_id,
                  RLC_MUI_UNDEFINED,
                  RLC_SDU_CONFIRM_NO,
                  len,
                  (unsigned char *)rx_buf,
                  PDCP_TRANSMISSION_MODE_DATA,
                  NULL,
                  NULL,
                  qfi,
                  rqi,
                  pdusession_id);
  }

  free(arg);

  return NULL;
}

void start_sdap_tun_gnb(int id)
{
  pthread_t t;

  struct thread_args *arg = malloc(sizeof(struct thread_args));
  char ifname[20];
  nas_config_interface_name(id + 1, 1, "oaitun_", ifname);
  arg->sock_fd = init_single_tun(ifname);
  nas_config(id + 1, 1, 1, ifname, 1);
  {
    // default ue id & pdu session id in nos1 mode
    nr_sdap_entity_t *entity = nr_sdap_get_entity(1, 10);
    DevAssert(entity != NULL);
    entity->pdusession_sock = arg->sock_fd;
  }
  if (pthread_create(&t, NULL, gnb_tun_read_thread, (void *)arg) != 0) {
    LOG_E(PDCP, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
}

static void *ue_tun_read_thread(void *arg)
{
  nr_sdap_entity_t *entity = (nr_sdap_entity_t *)arg;
  char rx_buf[NL_MAX_PAYLOAD];
  int len;
  protocol_ctxt_t ctxt;

  int rb_id = 1;
  char thread_name[64];
  sprintf(thread_name, "ue_tun_read_%d\n", entity->pdusession_id);
  pthread_setname_np(pthread_self(), thread_name);
  while (1) {
    len = read(entity->pdusession_sock, &rx_buf, NL_MAX_PAYLOAD);

    if (entity->stop_thread)
      break;

    if (len == -1) {
      LOG_E(PDCP, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
      exit(1);
    }

    LOG_D(PDCP, "%s(): pdusession_sock read returns len %d\n", __func__, len);

    ctxt.enb_flag = 0;
    ctxt.rntiMaybeUEid = entity->ue_id;

    bool dc = SDAP_HDR_UL_DATA_PDU;

    entity->tx_entity(entity,
                      &ctxt,
                      SRB_FLAG_NO,
                      rb_id,
                      RLC_MUI_UNDEFINED,
                      RLC_SDU_CONFIRM_NO,
                      len,
                      (unsigned char *)rx_buf,
                      PDCP_TRANSMISSION_MODE_DATA,
                      NULL,
                      NULL,
                      entity->qfi,
                      dc);
  }

  return NULL;
}

void start_sdap_tun_ue(ue_id_t ue_id, int pdu_session_id, int sock)
{
  nr_sdap_entity_t *entity = nr_sdap_get_entity(ue_id, pdu_session_id);
  DevAssert(entity != NULL);
  entity->pdusession_sock = sock;
  entity->stop_thread = false;
  if (pthread_create(&entity->pdusession_thread, NULL, ue_tun_read_thread, (void *)entity) != 0) {
    LOG_E(PDCP, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
}

bool sdap_data_req(protocol_ctxt_t *ctxt_p,
                   const ue_id_t ue_id,
                   const srb_flag_t srb_flag,
                   const rb_id_t rb_id,
                   const mui_t mui,
                   const confirm_t confirm,
                   const sdu_size_t sdu_buffer_size,
                   unsigned char *const sdu_buffer,
                   const pdcp_transmission_mode_t pt_mode,
                   const uint32_t *sourceL2Id,
                   const uint32_t *destinationL2Id,
                   const uint8_t qfi,
                   const bool rqi,
                   const int pdusession_id) {
  nr_sdap_entity_t *sdap_entity;
  sdap_entity = nr_sdap_get_entity(ue_id, pdusession_id);

  if(sdap_entity == NULL) {
    LOG_E(SDAP, "%s:%d:%s: Entity not found with ue: 0x%"PRIx64" and pdusession id: %d\n", __FILE__, __LINE__, __FUNCTION__, ue_id, pdusession_id);
    return 0;
  }

  bool ret = sdap_entity->tx_entity(sdap_entity,
                                    ctxt_p,
                                    srb_flag,
                                    rb_id,
                                    mui,
                                    confirm,
                                    sdu_buffer_size,
                                    sdu_buffer,
                                    pt_mode,
                                    sourceL2Id,
                                    destinationL2Id,
                                    qfi,
                                    rqi);
  return ret;
}

void sdap_data_ind(rb_id_t pdcp_entity,
                   int is_gnb,
                   bool has_sdap_rx,
                   int pdusession_id,
                   ue_id_t ue_id,
                   char *buf,
                   int size) {
  nr_sdap_entity_t *sdap_entity;
  sdap_entity = nr_sdap_get_entity(ue_id, pdusession_id);

  if (sdap_entity == NULL) {
    LOG_E(SDAP, "%s:%d:%s: Entity not found for ue rnti/ue_id: %lx and pdusession id: %d\n", __FILE__, __LINE__, __FUNCTION__, ue_id, pdusession_id);
    return;
  }

  sdap_entity->rx_entity(sdap_entity,
                         pdcp_entity,
                         is_gnb,
                         has_sdap_rx,
                         pdusession_id,
                         ue_id,
                         buf,
                         size);
}
