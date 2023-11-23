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

/*! \file xnap_gNB_handler.c
 * \brief xnap handler procedures for gNB
 * \author Sreeshma Shiv <sreeshmau@iisc.ac.in>
 * \date August 2023
 * \version 1.0
 */

#include <stdint.h>
#include "intertask_interface.h"
#include "xnap_common.h"
#include "xnap_gNB_defs.h"
#include "xnap_gNB_handler.h"
#include "xnap_gNB_interface_management.h"
#include "assertions.h"
#include "conversions.h"

/* Placement of callback functions according to XNAP_ProcedureCode.h */
static const xnap_message_decoded_callback xnap_messages_callback[][3] = {
    {xnap_gNB_handle_handover_request, 0, 0}, /*handover preperation*/
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {xnap_gNB_handle_xn_setup_request, xnap_gNB_handle_xn_setup_response, xnap_gNB_handle_xn_setup_failure}, /* xnSetup */
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0}};

static const char *const xnap_direction_String[] = {
    "", /* Nothing */
    "Originating message", /* originating message */
    "Successfull outcome", /* successfull outcome */
    "UnSuccessfull outcome", /* successfull outcome */
};
const char *xnap_direction2String(int xnap_dir)
{
  return (xnap_direction_String[xnap_dir]);
}

int xnap_gNB_handle_message(instance_t instance,
                            sctp_assoc_t assoc_id,
                            int32_t stream,
                            const uint8_t *const data,
                            const uint32_t data_length)
{
  XNAP_XnAP_PDU_t pdu;
  int ret = 0;

  DevAssert(data != NULL);

  memset(&pdu, 0, sizeof(pdu));

  printf("Data length received: %d\n", data_length);
  if (xnap_gNB_decode_pdu(&pdu, data, data_length) < 0) {
    LOG_E(XNAP, "Failed to decode PDU\n");
    return -1;
  }

  switch (pdu.present) {
    case XNAP_XnAP_PDU_PR_initiatingMessage:
      LOG_I(XNAP, "xnap_gNB_decode_initiating_message!\n");
      /* Checking procedure Code and direction of message */
      if (pdu.choice.initiatingMessage->procedureCode
          >= sizeof(xnap_messages_callback) / (3 * sizeof(xnap_message_decoded_callback))) {
        //|| (pdu.present > XNAP_XnAP_PDU_PR_unsuccessfulOutcome)) {
        LOG_E(XNAP, "[SCTP %d] Either procedureCode %ld exceed expected\n", assoc_id, pdu.choice.initiatingMessage->procedureCode);
        ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_XNAP_XnAP_PDU, &pdu);
        return -1;
      }

      /* No handler present */
      if (xnap_messages_callback[pdu.choice.initiatingMessage->procedureCode][pdu.present - 1] == NULL) {
        LOG_E(XNAP,
              "[SCTP %d] No handler for procedureCode %ld in %s\n",
              assoc_id,
              pdu.choice.initiatingMessage->procedureCode,
              xnap_direction2String(pdu.present - 1));
        ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_XNAP_XnAP_PDU, &pdu);
        return -1;
      }
      /* Calling the right handler */
      ret =
          (*xnap_messages_callback[pdu.choice.initiatingMessage->procedureCode][pdu.present - 1])(instance, assoc_id, stream, &pdu);
      break;

    case XNAP_XnAP_PDU_PR_successfulOutcome:
      LOG_I(XNAP, "xnap_gNB_decode_successfuloutcome_message!\n");
      /* Checking procedure Code and direction of message */
      if (pdu.choice.successfulOutcome->procedureCode
          >= sizeof(xnap_messages_callback) / (3 * sizeof(xnap_message_decoded_callback))) {
        LOG_E(XNAP, "[SCTP %d] Either procedureCode %ld exceed expected\n", assoc_id, pdu.choice.successfulOutcome->procedureCode);
        ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_XNAP_XnAP_PDU, &pdu);
        return -1;
      }

      /* No handler present.*/
      if (xnap_messages_callback[pdu.choice.successfulOutcome->procedureCode][pdu.present - 1] == NULL) {
        LOG_E(XNAP,
              "[SCTP %d] No handler for procedureCode %ld in %s\n",
              assoc_id,
              pdu.choice.successfulOutcome->procedureCode,
              xnap_direction2String(pdu.present - 1));
        ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_XNAP_XnAP_PDU, &pdu);
        return -1;
      }
      /* Calling the right handler */
      ret =
          (*xnap_messages_callback[pdu.choice.successfulOutcome->procedureCode][pdu.present - 1])(instance, assoc_id, stream, &pdu);
      break;

    case XNAP_XnAP_PDU_PR_unsuccessfulOutcome:
      LOG_I(XNAP, "xnap_gNB_decode_unsuccessfuloutcome_message!\n");
      /* Checking procedure Code and direction of message */
      if (pdu.choice.unsuccessfulOutcome->procedureCode
          >= sizeof(xnap_messages_callback) / (3 * sizeof(xnap_message_decoded_callback))) {
        LOG_E(XNAP,
              "[SCTP %d] Either procedureCode %ld exceed expected\n",
              assoc_id,
              pdu.choice.unsuccessfulOutcome->procedureCode);
        ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_XNAP_XnAP_PDU, &pdu);
        return -1;
      }

      /* No handler present */
      if (xnap_messages_callback[pdu.choice.unsuccessfulOutcome->procedureCode][pdu.present - 1] == NULL) {
        LOG_E(XNAP,
              "[SCTP %d] No handler for procedureCode %ld in %s\n",
              assoc_id,
              pdu.choice.unsuccessfulOutcome->procedureCode,
              xnap_direction2String(pdu.present - 1));
        ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_XNAP_XnAP_PDU, &pdu);
        return -1;
      }
      /* Calling the right handler */
      ret = (*xnap_messages_callback[pdu.choice.unsuccessfulOutcome->procedureCode][pdu.present - 1])(instance,
                                                                                                      assoc_id,
                                                                                                      stream,
                                                                                                      &pdu);
      break;

    default:
      LOG_E(XNAP, "[SCTP %d] Direction %d exceed expected\n", assoc_id, pdu.present);
      break;
  }

  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_XNAP_XnAP_PDU, &pdu);
  return ret;
}

static int xnap_gNB_handle_handover_request(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, XNAP_XnAP_PDU_t *pdu)
{
  XNAP_HandoverRequest_t *xnHandoverRequest;
  XNAP_HandoverRequest_IEs_t *ie;
  XNAP_QoSFlowsToBeSetup_Item_t *qosflowstobesetup_item;
  XNAP_PDUSessionResourcesToBeSetup_Item_t *pduSession_ToBeSetup_Item;

  xnap_gNB_instance_t *instance_p;
  xnap_gNB_data_t *xnap_gNB_data;
  MessageDef *msg;
  int ue_id;

  DevAssert(pdu != NULL);
  xnHandoverRequest = &pdu->choice.initiatingMessage->value.choice.HandoverRequest;

  if (stream == 0) {
    LOG_E(XNAP, "Received new XN handover request on stream == 0\n");
    /* TODO: send a xn failure response */
    return 0;
  }

  LOG_D(XNAP, "Received a new XN handover request\n");

  xnap_gNB_data = xnap_get_gNB(NULL, assoc_id, 0);
  DevAssert(xnap_gNB_data != NULL);

  instance_p = xnap_gNB_get_instance(instance);
  DevAssert(instance_p != NULL);

  msg = itti_alloc_new_message(TASK_XNAP, 0, XNAP_HANDOVER_REQ);

  XNAP_FIND_PROTOCOLIE_BY_ID(XNAP_HandoverRequest_IEs_t, ie, xnHandoverRequest, XNAP_ProtocolIE_ID_id_oldNG_RANnodeUEXnAPID, true);
  if (ie == NULL) {
    LOG_E(XNAP, "%s %d: ie is a NULL pointer \n", __FILE__, __LINE__);
    itti_free(ITTI_MSG_ORIGIN_ID(msg), msg);
    return -1;
  }
  /* allocate a new XNAP UE ID */
  ue_id = xnap_allocate_new_id(&instance_p->id_manager);
  if (ue_id == -1) {
    LOG_E(XNAP, "could not allocate a new XNAP UE ID\n");
    /* TODO: cancel handover: send HO preparation failure to source gNB */
    exit(1);
  }
  /* rnti is unknown yet, must not be set to -1, 0 is fine */
  xnap_set_ids(&instance_p->id_manager, ue_id, 0, ie->value.choice.NG_RANnodeUEXnAPID, ue_id);
  xnap_id_set_state(&instance_p->id_manager, ue_id, XNID_STATE_TARGET);

  XNAP_HANDOVER_REQ(msg).xn_id = ue_id;

  XNAP_FIND_PROTOCOLIE_BY_ID(XNAP_HandoverRequest_IEs_t, ie, xnHandoverRequest, XNAP_ProtocolIE_ID_id_GUAMI, true);
  if (ie == NULL) {
    LOG_E(XNAP, "%s %d: ie is a NULL pointer \n", __FILE__, __LINE__);
    itti_free(ITTI_MSG_ORIGIN_ID(msg), msg);
    return -1;
  }
  TBCD_TO_MCC_MNC(&ie->value.choice.GUAMI.plmn_ID,
                  XNAP_HANDOVER_REQ(msg).ue_guami.mcc,
                  XNAP_HANDOVER_REQ(msg).ue_guami.mnc,
                  XNAP_HANDOVER_REQ(msg).ue_guami.mnc_len);
  OCTET_STRING_TO_INT8(&ie->value.choice.GUAMI.amf_region_id, XNAP_HANDOVER_REQ(msg).ue_guami.amf_region_id); // bit string
  OCTET_STRING_TO_INT16(&ie->value.choice.GUAMI.amf_set_id, XNAP_HANDOVER_REQ(msg).ue_guami.amf_set_id); // bit string
  OCTET_STRING_TO_INT16(&ie->value.choice.GUAMI.amf_pointer, XNAP_HANDOVER_REQ(msg).ue_guami.amf_pointer); // bit string

  XNAP_FIND_PROTOCOLIE_BY_ID(XNAP_HandoverRequest_IEs_t, ie, xnHandoverRequest, XNAP_ProtocolIE_ID_id_UEContextInfoHORequest, true);

  if (ie == NULL) {
    LOG_E(XNAP, "%s %d: ie is a NULL pointer \n", __FILE__, __LINE__);
    itti_free(ITTI_MSG_ORIGIN_ID(msg), msg);
    return -1;
  }
  XNAP_HANDOVER_REQ(msg).ue_ngap_id = ie->value.choice.UEContextInfoHORequest.ng_c_UE_reference;

  /* TODO: properly store Target Cell ID */

  XNAP_HANDOVER_REQ(msg).target_assoc_id = assoc_id;

  XNAP_HANDOVER_REQ(msg).security_capabilities.encryption_algorithms =
      BIT_STRING_to_uint16(&ie->value.choice.UEContextInfoHORequest.ueSecurityCapabilities.nr_EncyptionAlgorithms);
  XNAP_HANDOVER_REQ(msg).security_capabilities.integrity_algorithms =
      BIT_STRING_to_uint16(&ie->value.choice.UEContextInfoHORequest.ueSecurityCapabilities.nr_IntegrityProtectionAlgorithms);

  // XNAP_HANDOVER_REQ(msg).ue_ambr=ue_context_pP->ue_context.ue_ambr;

  if ((ie->value.choice.UEContextInfoHORequest.securityInformation.key_NG_RAN_Star.buf)
      && (ie->value.choice.UEContextInfoHORequest.securityInformation.key_NG_RAN_Star.size == 32)) {
    memcpy(XNAP_HANDOVER_REQ(msg).kenb, ie->value.choice.UEContextInfoHORequest.securityInformation.key_NG_RAN_Star.buf, 32);
    XNAP_HANDOVER_REQ(msg).kenb_ncc = ie->value.choice.UEContextInfoHORequest.securityInformation.ncc;
  } else {
    LOG_W(XNAP, "Size of key NG-RAN star does not match the expected value\n");
  }

  if (ie->value.choice.UEContextInfoHORequest.pduSessionResourcesToBeSetup_List.list.count > 0) {
    XNAP_HANDOVER_REQ(msg).nb_pdu_resources_tobe_setup =
        ie->value.choice.UEContextInfoHORequest.pduSessionResourcesToBeSetup_List.list.count;

    for (int i = 0; i < ie->value.choice.UEContextInfoHORequest.pduSessionResourcesToBeSetup_List.list.count; i++) {
      pduSession_ToBeSetup_Item = (XNAP_PDUSessionResourcesToBeSetup_Item_t *)
                                      ie->value.choice.UEContextInfoHORequest.pduSessionResourcesToBeSetup_List.list.array[i];
      // pduSession_ToBeSetup_Item = &pduSession_ToBeSetup_ItemIEs->value.choice.pduSession_ToBeSetup_Item;

      XNAP_HANDOVER_REQ(msg).pdu_param[i].pdusession_id = pduSession_ToBeSetup_Item->pduSessionId;

      memcpy(XNAP_HANDOVER_REQ(msg).pdu_param[i].gNB_addr.buffer,
             pduSession_ToBeSetup_Item->uL_NG_U_TNLatUPF.choice.gtpTunnel->tnl_address.buf,
             pduSession_ToBeSetup_Item->uL_NG_U_TNLatUPF.choice.gtpTunnel->tnl_address.size);

      XNAP_HANDOVER_REQ(msg).pdu_param[i].gNB_addr.length =
          pduSession_ToBeSetup_Item->uL_NG_U_TNLatUPF.choice.gtpTunnel->tnl_address.size * 8
          - pduSession_ToBeSetup_Item->uL_NG_U_TNLatUPF.choice.gtpTunnel->tnl_address.bits_unused;

      OCTET_STRING_TO_INT32(&pduSession_ToBeSetup_Item->uL_NG_U_TNLatUPF.choice.gtpTunnel->gtp_teid,
                            XNAP_HANDOVER_REQ(msg).pdu_param[i].gtp_teid);
      for (int j = 0; j < pduSession_ToBeSetup_Item->qosFlowsToBeSetup_List.list.count; j++) {
        qosflowstobesetup_item = (XNAP_QoSFlowsToBeSetup_Item_t *)pduSession_ToBeSetup_Item->qosFlowsToBeSetup_List.list.array[j];
        XNAP_HANDOVER_REQ(msg).pdu_param[i].qos[j].qfi = qosflowstobesetup_item->qfi;
        if (qosflowstobesetup_item->qosFlowLevelQoSParameters.qos_characteristics.present
            == XNAP_QoSCharacteristics_PR_non_dynamic) {
          XNAP_HANDOVER_REQ(msg).pdu_param[i].qos[j].non_dynamic_fiveQI =
              qosflowstobesetup_item->qosFlowLevelQoSParameters.qos_characteristics.choice.non_dynamic->fiveQI;
        } else if (qosflowstobesetup_item->qosFlowLevelQoSParameters.qos_characteristics.present
                   == XNAP_QoSCharacteristics_PR_dynamic) {
          XNAP_HANDOVER_REQ(msg).pdu_param[i].qos[j].dynamic_priorityLevelQoS =
              qosflowstobesetup_item->qosFlowLevelQoSParameters.qos_characteristics.choice.dynamic->priorityLevelQoS;
          XNAP_HANDOVER_REQ(msg).pdu_param[i].qos[j].dynamic_packetDelayBudget =
              qosflowstobesetup_item->qosFlowLevelQoSParameters.qos_characteristics.choice.dynamic->packetDelayBudget;
          XNAP_HANDOVER_REQ(msg).pdu_param[i].qos[j].dynamic_packetErrorRate_scalar =
              qosflowstobesetup_item->qosFlowLevelQoSParameters.qos_characteristics.choice.dynamic->packetErrorRate.pER_Scalar;
          XNAP_HANDOVER_REQ(msg).pdu_param[i].qos[j].dynamic_packetErrorRate_exponent =
              qosflowstobesetup_item->qosFlowLevelQoSParameters.qos_characteristics.choice.dynamic->packetErrorRate.pER_Exponent;
        }
        XNAP_HANDOVER_REQ(msg).pdu_param[i].qos[j].allocation_retention_priority.priority_level =
            qosflowstobesetup_item->qosFlowLevelQoSParameters.allocationAndRetentionPrio.priorityLevel;
        XNAP_HANDOVER_REQ(msg).pdu_param[i].qos[j].allocation_retention_priority.pre_emp_capability =
            qosflowstobesetup_item->qosFlowLevelQoSParameters.allocationAndRetentionPrio.pre_emption_capability;
        XNAP_HANDOVER_REQ(msg).pdu_param[i].qos[j].allocation_retention_priority.pre_emp_vulnerability =
            qosflowstobesetup_item->qosFlowLevelQoSParameters.allocationAndRetentionPrio.pre_emption_vulnerability;
      }
    }
  } else {
    LOG_E(XNAP, "Can't decode the QoS to be setup item \n");
  }

  OCTET_STRING_t *c = &ie->value.choice.UEContextInfoHORequest.rrc_Context;

  if (c->size > 8192 /* TODO: this is the size of rrc_buffer in struct xnap_handover_req_s */) {
    printf("%s:%d: fatal: buffer too big\n", __FILE__, __LINE__);
    abort();
  }

  memcpy(XNAP_HANDOVER_REQ(msg).rrc_buffer, c->buf, c->size);
  XNAP_HANDOVER_REQ(msg).rrc_buffer_size = c->size;

  itti_send_msg_to_task(TASK_RRC_GNB, instance_p->instance, msg);

  return 0;
}
