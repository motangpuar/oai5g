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

/*! \file l2_nr_interface.c
 * \brief layer 2 interface, used to support different RRC sublayer
 * \author Raymond Knopp and Navid Nikaein, WEI-TAI CHEN
 * \date 2010-2014, 2018
 * \version 1.0
 * \company Eurecom, NTUST
 * \email: raymond.knopp@eurecom.fr, kroempa@gmail.com
 */

#include <f1ap_du_rrc_message_transfer.h>
#include "platform_types.h"
#include "nr_rrc_defs.h"
#include "nr_rrc_extern.h"
#include "common/utils/LOG/log.h"
#include "common/ran_context.h"
#include "LAYER2/NR_MAC_COMMON/nr_mac_common.h"
#include "LAYER2/NR_MAC_COMMON/nr_mac_extern.h"
#include "LAYER2/NR_MAC_gNB/nr_mac_gNB.h"
#include "LAYER2/NR_MAC_gNB/mac_proto.h"
#include "LAYER2/nr_rlc/nr_rlc_oai_api.h"

#include "intertask_interface.h"

#include "NR_MIB.h"
#include "NR_BCCH-BCH-Message.h"
#include "rrc_gNB_UE_context.h"
#include <openair2/RRC/NR/MESSAGES/asn1_msg.h>
#include "nr_pdcp/nr_pdcp_oai_api.h"


extern RAN_CONTEXT_t RC;


  gNB_MAC_INST *nrmac = RC.nrmac[0];
  NR_SCHED_LOCK(&nrmac->sched_lock);
  mac_remove_nr_ue(nrmac, rntiMaybeUEid);
  NR_SCHED_UNLOCK(&nrmac->sched_lock);
}

void nr_rrc_mac_update_cellgroup(rnti_t rntiMaybeUEid, NR_CellGroupConfig_t *cgc)
{

  MessageDef *message_p;
  // Uses a new buffer to avoid issue with PDCP buffer content that could be changed by PDCP (asynchronous message handling).
  uint8_t *message_buffer;
  message_buffer = itti_malloc (
                     ctxt_pP->enb_flag ? TASK_RRC_GNB : TASK_RRC_UE,
                     ctxt_pP->enb_flag ? TASK_PDCP_ENB : TASK_PDCP_UE,
                     sdu_sizeP);
  memcpy (message_buffer, buffer_pP, sdu_sizeP);
  message_p = itti_alloc_new_message (ctxt_pP->enb_flag ? TASK_RRC_GNB : TASK_RRC_UE, 0, RRC_DCCH_DATA_REQ);
  RRC_DCCH_DATA_REQ (message_p).frame     = ctxt_pP->frame;
  RRC_DCCH_DATA_REQ (message_p).enb_flag  = ctxt_pP->enb_flag;
  RRC_DCCH_DATA_REQ (message_p).rb_id     = rb_idP;
  RRC_DCCH_DATA_REQ (message_p).muip      = muiP;
  RRC_DCCH_DATA_REQ (message_p).confirmp  = confirmP;
  RRC_DCCH_DATA_REQ (message_p).sdu_size  = sdu_sizeP;
  RRC_DCCH_DATA_REQ (message_p).sdu_p     = message_buffer;
  //memcpy (NR_RRC_DCCH_DATA_REQ (message_p).sdu_p, buffer_pP, sdu_sizeP);
  RRC_DCCH_DATA_REQ (message_p).mode      = modeP;
  RRC_DCCH_DATA_REQ (message_p).module_id = ctxt_pP->module_id;
  RRC_DCCH_DATA_REQ(message_p).rnti = ctxt_pP->rntiMaybeUEid;
  RRC_DCCH_DATA_REQ (message_p).eNB_index = ctxt_pP->eNB_index;
  itti_send_msg_to_task (
    ctxt_pP->enb_flag ? TASK_PDCP_ENB : TASK_PDCP_UE,
    ctxt_pP->instance,
    message_p);
  LOG_I(NR_RRC,"send RRC_DCCH_DATA_REQ to PDCP\n");

  /* Hack: only trigger PDCP if in CU, otherwise it is triggered by RU threads
   * Ideally, PDCP would not neet to be triggered like this but react to ITTI
   * messages automatically */
  if (ctxt_pP->enb_flag && NODE_IS_CU(RC.nrrrc[ctxt_pP->module_id]->node_type))
    pdcp_run(ctxt_pP);

  return true; // TODO should be changed to a CNF message later, currently RRC lite does not used the returned value anyway.
}

uint16_t mac_rrc_nr_data_req(const module_id_t Mod_idP,
                             const int         CC_id,
                             const frame_t     frameP,
                             const rb_id_t     Srb_id,
                             const rnti_t      rnti,
                             const uint8_t     Nb_tb,
                             uint8_t *const    buffer_pP)
{

#ifdef DEBUG_RRC
  LOG_D(RRC,"[eNB %d] mac_rrc_data_req to SRB ID=%ld\n",Mod_idP,Srb_id);
#endif

  // MIBCH
  if ((Srb_id & RAB_OFFSET) == MIBCH) {

    asn_enc_rval_t enc_rval;
    uint8_t sfn_msb = (uint8_t)((frameP>>4)&0x3f);
    rrc_gNB_carrier_data_t *carrier = &RC.nrrrc[Mod_idP]->carrier;
    NR_BCCH_BCH_Message_t *mib = &carrier->mib;

    mib->message.choice.mib->pdcch_ConfigSIB1.controlResourceSetZero = carrier->pdcch_ConfigSIB1->controlResourceSetZero;
    mib->message.choice.mib->pdcch_ConfigSIB1.searchSpaceZero = carrier->pdcch_ConfigSIB1->searchSpaceZero;

    mib->message.choice.mib->systemFrameNumber.buf[0] = sfn_msb << 2;
    enc_rval = uper_encode_to_buffer(&asn_DEF_NR_BCCH_BCH_Message,
                                     NULL,
                                     (void *) mib,
                                     carrier->MIB,
                                     24);
    LOG_D(NR_RRC, "Encoded MIB for frame %d sfn_msb %d (%p), bits %lu\n", frameP, sfn_msb, carrier->MIB,
          enc_rval.encoded);
    buffer_pP[0] = carrier->MIB[0];
    buffer_pP[1] = carrier->MIB[1];
    buffer_pP[2] = carrier->MIB[2];
    LOG_D(NR_RRC, "MIB PDU buffer_pP[0]=%x , buffer_pP[1]=%x, buffer_pP[2]=%x\n", buffer_pP[0], buffer_pP[1],
          buffer_pP[2]);
    AssertFatal (enc_rval.encoded > 0, "ASN1 message encoding failed (%s, %lu)!\n",
                 enc_rval.failed_type->name, enc_rval.encoded);
    return 3;
  }

  // TODO BCCH SIB1 SIBs
  if ((Srb_id & RAB_OFFSET) == BCCH) {
    memcpy(&buffer_pP[0], RC.nrrrc[Mod_idP]->carrier.SIB1, RC.nrrrc[Mod_idP]->carrier.sizeof_SIB1);
    return RC.nrrrc[Mod_idP]->carrier.sizeof_SIB1;
  }

  // CCCH
  if ((Srb_id & RAB_OFFSET) == CCCH) {
    AssertFatal(0, "CCCH is managed by rlc of srb 0, not anymore by mac_rrc_nr_data_req\n");
  }

  return 0;
}

int8_t nr_mac_rrc_bwp_switch_req(const module_id_t     module_idP,
                                 const frame_t         frameP,
                                 const sub_frame_t     sub_frameP,
                                 const rnti_t          rntiP,
                                 const int             dl_bwp_id,
                                 const int             ul_bwp_id) {

  struct rrc_gNB_ue_context_s *ue_context_p = rrc_gNB_get_ue_context(RC.nrrrc[module_idP], rntiP);

  protocol_ctxt_t ctxt;
  PROTOCOL_CTXT_SET_BY_MODULE_ID(&ctxt, module_idP, GNB_FLAG_YES, rntiP, frameP, sub_frameP, 0);
  nr_rrc_reconfiguration_req(ue_context_p, &ctxt, dl_bwp_id, ul_bwp_id);

  return 0;
}

int8_t nr_mac_rrc_bwp_switch_req(const module_id_t     module_idP,
                                 const frame_t         frameP,
                                 const sub_frame_t     sub_frameP,
                                 const rnti_t          rntiP,
                                 const int             dl_bwp_id,
                                 const int             ul_bwp_id) {
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context_by_rnti(RC.nrrrc[module_idP], rntiP);

  protocol_ctxt_t ctxt;
  PROTOCOL_CTXT_SET_BY_MODULE_ID(&ctxt, module_idP, GNB_FLAG_YES, rntiP, frameP, sub_frameP, 0);
  nr_rrc_reconfiguration_req(ue_context_p, &ctxt, dl_bwp_id, ul_bwp_id);

  return 0;
}

void nr_mac_gNB_rrc_ul_failure(const module_id_t Mod_instP,
                               const int CC_idP,
                               const frame_t frameP,
                               const sub_frame_t subframeP,
                               const rnti_t rntiP) {
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context_by_rnti(RC.nrrrc[Mod_instP], rntiP);

  if (ue_context_p != NULL) {
    LOG_D(RRC,"Frame %d, Subframe %d: UE %x UL failure, activating timer\n",frameP,subframeP,rntiP);
    if(ue_context_p->ue_context.ul_failure_timer == 0)
      ue_context_p->ue_context.ul_failure_timer=1;
  } else {
    LOG_D(RRC,"Frame %d, Subframe %d: UL failure: UE %x unknown \n",frameP,subframeP,rntiP);
  }
}

void nr_mac_gNB_rrc_ul_failure_reset(const module_id_t Mod_instP,
                                     const frame_t frameP,
                                     const sub_frame_t subframeP,
                                     const rnti_t rntiP) {
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context_by_rnti(RC.nrrrc[Mod_instP], rntiP);
  if (ue_context_p != NULL) {
    LOG_W(RRC,"Frame %d, Subframe %d: UE %x UL failure reset, deactivating timer\n",frameP,subframeP,rntiP);
    ue_context_p->ue_context.ul_failure_timer=0;
  } else {
    LOG_W(RRC,"Frame %d, Subframe %d: UL failure reset: UE %x unknown \n",frameP,subframeP,rntiP);
  }
}
