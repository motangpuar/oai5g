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

/* \file l2_interface_ue.c
 * \brief layer 2 interface, used to support different RRC sublayer
 * \author R. Knopp, K.H. HSU
 * \date 2018
 * \version 0.1
 * \company Eurecom / NTUST
 * \email: knopp@eurecom.fr, kai-hsiang.hsu@eurecom.fr
 * \note
 * \warning
 */

#include "rrc_defs.h"
#include "rrc_proto.h"
#include "assertions.h"
#include "MAC/mac.h"
#include "LAYER2/NR_MAC_COMMON/nr_mac.h"
#include "openair2/LAYER2/NR_MAC_UE/mac_proto.h"

typedef uint32_t channel_t;

void nr_mac_rrc_sync_ind(const module_id_t module_id,
                         const frame_t frame,
                         const bool in_sync)
{
  MessageDef *message_p = itti_alloc_new_message(TASK_MAC_UE, 0, NR_RRC_MAC_SYNC_IND);
  NR_RRC_MAC_SYNC_IND (message_p).frame = frame;
  NR_RRC_MAC_SYNC_IND (message_p).in_sync = in_sync;
  itti_send_msg_to_task(TASK_RRC_NRUE, GNB_MODULE_ID_TO_INSTANCE(module_id), message_p);
}

int8_t nr_mac_rrc_data_ind_ue(const module_id_t module_id,
                              const int CC_id,
                              const uint8_t gNB_index,
                              const frame_t frame,
                              const int slot,
                              const rnti_t rnti,
                              const channel_t channel,
                              const uint8_t* pduP,
                              const sdu_size_t pdu_len)
{
  sdu_size_t sdu_size = 0;

  switch(channel) {
    case NR_BCCH_BCH:
    case NR_BCCH_DL_SCH:
      if (pdu_len>0) {
        LOG_T(NR_RRC, "[UE %d] Received SDU for NR-BCCH-DL-SCH on SRB %u from gNB %d\n", module_id, channel & RAB_OFFSET,
              gNB_index);

        MessageDef *message_p;
        int msg_sdu_size = BCCH_SDU_SIZE;

        if (pdu_len > msg_sdu_size) {
          LOG_E(NR_RRC, "SDU larger than NR-BCCH-DL-SCH SDU buffer size (%d, %d)", sdu_size, msg_sdu_size);
          sdu_size = msg_sdu_size;
        } else {
          sdu_size = pdu_len;
        }

        message_p = itti_alloc_new_message(TASK_MAC_UE, 0, NR_RRC_MAC_BCCH_DATA_IND);
        memset(NR_RRC_MAC_BCCH_DATA_IND (message_p).sdu, 0, BCCH_SDU_SIZE);
        memcpy(NR_RRC_MAC_BCCH_DATA_IND (message_p).sdu, pduP, sdu_size);
        NR_RRC_MAC_BCCH_DATA_IND (message_p).frame = frame; //frameP
        NR_RRC_MAC_BCCH_DATA_IND (message_p).slot = slot;
        NR_RRC_MAC_BCCH_DATA_IND (message_p).sdu_size = sdu_size;
        NR_RRC_MAC_BCCH_DATA_IND (message_p).gnb_index = gNB_index;
        NR_RRC_MAC_BCCH_DATA_IND (message_p).is_bch = (channel == NR_BCCH_BCH);
        itti_send_msg_to_task(TASK_RRC_NRUE, GNB_MODULE_ID_TO_INSTANCE(module_id), message_p);
      }
      break;

    case CCCH:
      if (pdu_len>0) {
        LOG_T(NR_RRC,"[UE %d] Received SDU for CCCH on SRB %u from gNB %d\n",module_id,channel & RAB_OFFSET,gNB_index);

        MessageDef *message_p;
        int msg_sdu_size = CCCH_SDU_SIZE;

        if (pdu_len > msg_sdu_size) {
          LOG_E(NR_RRC, "SDU larger than CCCH SDU buffer size (%d, %d)", sdu_size, msg_sdu_size);
          sdu_size = msg_sdu_size;
        } else {
          sdu_size =  pdu_len;
        }

        message_p = itti_alloc_new_message (TASK_MAC_UE, 0, NR_RRC_MAC_CCCH_DATA_IND);
        memset (NR_RRC_MAC_CCCH_DATA_IND (message_p).sdu, 0, CCCH_SDU_SIZE);
        memcpy (NR_RRC_MAC_CCCH_DATA_IND (message_p).sdu, pduP, sdu_size);
        NR_RRC_MAC_CCCH_DATA_IND (message_p).frame     = frame; //frameP
        NR_RRC_MAC_CCCH_DATA_IND (message_p).slot = slot;
        NR_RRC_MAC_CCCH_DATA_IND (message_p).sdu_size  = sdu_size;
        NR_RRC_MAC_CCCH_DATA_IND (message_p).gnb_index = gNB_index;
        NR_RRC_MAC_CCCH_DATA_IND (message_p).rnti      = rnti;  //rntiP
        itti_send_msg_to_task (TASK_RRC_NRUE, GNB_MODULE_ID_TO_INSTANCE( module_id ), message_p);
      }
      break;

    default:
      break;
  }

  return(0);
}

int8_t nr_mac_rrc_data_req_ue(const module_id_t Mod_idP,
                              const int CC_id,
                              const uint8_t gNB_id,
                              const frame_t frameP,
                              const rb_id_t Srb_id,
                              uint8_t *buffer_pP)
{
  switch(Srb_id) {

    case CCCH:
      return mac_rrc_srb0_req(Mod_idP, gNB_id, buffer_pP);
    case DCCH:
      AssertFatal(1==0, "SRB1 not implemented yet!\n");
    case DCCH1:
      AssertFatal(1==0, "SRB2 not implemented yet!\n");
    default:
      AssertFatal(1==0, "Invalid SRB id!\n");
  }

  return 0;
}

void nr_mac_process_rrc_msg(instance_t instance)
{
  MessageDef *msg;
  do {
    // Checks if a message has been sent to MAC sub-task
    itti_poll_msg(TASK_MAC_UE, &msg);
    if (!msg)
      return;

    LOG_I(NR_MAC,
          "Received %s from %s: instance %ld\n",
          ITTI_MSG_NAME(msg),
          ITTI_MSG_ORIGIN_NAME(msg),
          ITTI_MSG_DESTINATION_INSTANCE(msg));
    switch (ITTI_MSG_ID(msg)) {
      case MAC_MIB_REQ: {
        mac_mib_req_t *req = &msg->ittiMsg.macMibReq;
        nr_rrc_mac_config_req_mib(req->module_id, req->cc_idP, req->mib, req->sched_sib);
      } break;

      default:
        LOG_E(NR_MAC, "Received unexpected message %s\n", ITTI_MSG_NAME(msg));
        break;
    }

    int result = itti_free(ITTI_MSG_ORIGIN_ID(msg), msg);
    AssertFatal(result == EXIT_SUCCESS, "Failed to free memory (%d)!\n", result);
  } while (msg != NULL);
}

void nr_mac_rrc_ra_ind(const module_id_t mod_id, int frame, bool success)
{
  MessageDef *message_p = itti_alloc_new_message(TASK_MAC_UE, 0, NR_RRC_MAC_RA_IND);
  NR_RRC_MAC_RA_IND (message_p).frame = frame;
  NR_RRC_MAC_RA_IND (message_p).RA_succeeded = success;
  itti_send_msg_to_task(TASK_RRC_NRUE, GNB_MODULE_ID_TO_INSTANCE(mod_id), message_p);
}
