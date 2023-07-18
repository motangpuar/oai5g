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

/*! \file xnap_gNB_interface_management.c
 * \brief xnap handling interface procedures for gNB
 * \author Sreeshma Shiv <sreeshmau@iisc.ac.in>
 * \date Dec 2023
 * \version 1.0
 */

#include <stdint.h>
#include "intertask_interface.h"
#include "xnap_common.h"
#include "xnap_gNB_defs.h"
#include "xnap_gNB_interface_management.h"
#include "xnap_gNB_handler.h"
#include "assertions.h"
#include "conversions.h"
#include "XNAP_GlobalgNB-ID.h"
#include "XNAP_ServedCells-NR-Item.h"
#include "XNAP_NRFrequencyBandItem.h"
#include "XNAP_GlobalNG-RANNode-ID.h"
#include "XNAP_NRModeInfoFDD.h"
#include "XNAP_NRModeInfoTDD.h"
#include "XNAP_SupportedSULBandList.h"
#include "XNAP_TAISupport-Item.h"
#include "XNAP_BroadcastPLMNinTAISupport-Item.h"
#include "xnap_gNB_management_procedures.h"

int xnap_gNB_handle_xn_setup_request(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, XNAP_XnAP_PDU_t *pdu)
{
  XNAP_XnSetupRequest_t *xnSetupRequest;
  XNAP_XnSetupRequest_IEs_t *ie;
  // XNAP_ServedCells_NR_Item_t *servedCellMember;

  DevAssert(pdu != NULL);
  xnSetupRequest = &pdu->choice.initiatingMessage->value.choice.XnSetupRequest;
  if (stream != 0) { /* Xn Setup: Non UE related procedure ->stream 0 */
    LOG_E(XNAP, "Received new XN setup request on stream != 0\n");
    /* Send a xn setup failure with protocol cause unspecified */
    MessageDef *message_p = itti_alloc_new_message(TASK_XNAP, 0, XNAP_SETUP_FAILURE);
    message_p->ittiMsgHeader.originInstance = assoc_id;
    xnap_setup_failure_t *fail = &XNAP_SETUP_FAILURE(message_p);
    fail->cause_type = XNAP_CAUSE_PROTOCOL;
    fail->cause_value = 6;
    itti_send_msg_to_task(TASK_XNAP, 0, message_p);
  }
  LOG_D(XNAP, "Received a new XN setup request\n");
  MessageDef *message_p = itti_alloc_new_message(TASK_XNAP, 0, XNAP_SETUP_REQ);
  message_p->ittiMsgHeader.originInstance = assoc_id;
  xnap_setup_req_t *req = &XNAP_SETUP_REQ(message_p);

  // gNB_id
  XNAP_FIND_PROTOCOLIE_BY_ID(XNAP_XnSetupRequest_IEs_t, ie, xnSetupRequest, XNAP_ProtocolIE_ID_id_GlobalNG_RAN_node_ID, true);
  if (ie == NULL) {
    LOG_E(XNAP, "XNAP_ProtocolIE_ID_id_GlobalNG_RAN_node_ID is NULL pointer \n");
    return -1;
  } else {
    if (ie->value.choice.GlobalNG_RANNode_ID.choice.gNB->gnb_id.present == XNAP_GNB_ID_Choice_PR_gnb_ID) {
      // gNB ID = 28 bits
      uint8_t *gNB_id_buf = ie->value.choice.GlobalNG_RANNode_ID.choice.gNB->gnb_id.choice.gnb_ID.buf;
      if (ie->value.choice.GlobalNG_RANNode_ID.choice.gNB->gnb_id.choice.gnb_ID.size != 28) {
        // TODO: handle case where size != 28 -> notify ? reject ?
      }
      req->gNB_id = (gNB_id_buf[0] << 20) + (gNB_id_buf[1] << 12) + (gNB_id_buf[2] << 4) + ((gNB_id_buf[3] & 0xf0) >> 4);
    } else {
      // TODO if NSA setup
    }
  }
  LOG_D(XNAP, "Adding gNB to the list of associated gNBs: %lu\n", req->gNB_id);

  XNAP_FIND_PROTOCOLIE_BY_ID(XNAP_XnSetupRequest_IEs_t, ie, xnSetupRequest, XNAP_ProtocolIE_ID_id_TAISupport_list, true);
  if (ie == NULL) {
    LOG_E(XNAP, "XNAP_ProtocolIE_ID_id_TAISupport_list is NULL pointer \n");
    return -1;
  } else {
    OCTET_STRING_TO_INT24(&ie->value.choice.TAISupport_List.list.array[0]->tac, req->tai_support);
    LOG_I(XNAP, "tac %d \n", req->tai_support);
  }
  LOG_D(XNAP, "req->gNB id: %lu \n", req->gNB_id);

  LOG_D(XNAP, "Adding gNB to the list of associated gNBs\n");

  XNAP_FIND_PROTOCOLIE_BY_ID(XNAP_XnSetupRequest_IEs_t, ie, xnSetupRequest, XNAP_ProtocolIE_ID_id_List_of_served_cells_NR, true);
  req->num_cells_available = ie->value.choice.ServedCells_NR.list.count;
  LOG_D(XNAP, "req->num_cells_available %d \n", req->num_cells_available);

  for (int i = 0; i < req->num_cells_available; i++) {
    XNAP_ServedCellInformation_NR_t *servedCellMember =
        &(((XNAP_ServedCells_NR_Item_t *)ie->value.choice.ServedCells_NR.list.array[i])->served_cell_info_NR);
    req->info.nr_pci = servedCellMember->nrPCI;
    LOG_D(XNAP, "req->nr_pci[%d] %d \n", i, req->info.nr_pci);
    /* PLMN */
    PLMNID_TO_MCC_MNC(servedCellMember->broadcastPLMN.list.array[0],
                      req->info.plmn.mcc,
                      req->info.plmn.mnc,
                      req->info.plmn.mnc_digit_length);
    BIT_STRING_TO_NR_CELL_IDENTITY(&servedCellMember->cellID.nr_CI, req->info.nr_cellid);
    LOG_D(XNAP,
          "[SCTP %d] Received BroadcastPLMN: MCC %d, MNC %d, CELL_ID %llu\n",
          assoc_id,
          req->info.plmn.mcc,
          req->info.plmn.mnc,
          (long long unsigned int)req->info.nr_cellid);
    // FDD Cells
    if (servedCellMember->nrModeInfo.present == XNAP_NRModeInfo_PR_fdd) {
      req->info.mode = XNAP_MODE_FDD;
      xnap_fdd_info_t *FDDs = &req->info.fdd;
      XNAP_NRModeInfoFDD_t *fdd_Info = servedCellMember->nrModeInfo.choice.fdd;
      FDDs->ul_freqinfo.arfcn = fdd_Info->ulNRFrequencyInfo.nrARFCN;
      AssertFatal(fdd_Info->ulNRFrequencyInfo.frequencyBand_List.list.count == 1, "cannot handle more than one frequency band\n");
      for (int f = 0; f < fdd_Info->ulNRFrequencyInfo.frequencyBand_List.list.count; f++) {
        XNAP_NRFrequencyBandItem_t *FreqItem = fdd_Info->ulNRFrequencyInfo.frequencyBand_List.list.array[f];
        FDDs->ul_freqinfo.band = FreqItem->nr_frequency_band;
        AssertFatal(FreqItem->supported_SUL_Band_List->list.count == 0, "cannot handle SUL bands!\n");
      }
      FDDs->dl_freqinfo.arfcn = fdd_Info->dlNRFrequencyInfo.nrARFCN;
      int dlBands = fdd_Info->dlNRFrequencyInfo.frequencyBand_List.list.count;
      AssertFatal(dlBands == 0, "cannot handle more than one frequency band\n");
      for (int dlB = 0; dlB < dlBands; dlB++) {
        XNAP_NRFrequencyBandItem_t *FreqItem = fdd_Info->dlNRFrequencyInfo.frequencyBand_List.list.array[dlB];
        FDDs->dl_freqinfo.band = FreqItem->nr_frequency_band;
        int num_available_supported_SULBands = FreqItem->supported_SUL_Band_List->list.count;
        AssertFatal(num_available_supported_SULBands == 0, "cannot handle SUL bands!\n");
      }
      FDDs->ul_tbw.scs = fdd_Info->ulNRTransmissonBandwidth.nRSCS;
      FDDs->ul_tbw.nrb = fdd_Info->ulNRTransmissonBandwidth.nRNRB;
      FDDs->dl_tbw.scs = fdd_Info->dlNRTransmissonBandwidth.nRSCS;
      FDDs->dl_tbw.nrb = fdd_Info->dlNRTransmissonBandwidth.nRNRB;
    } else if (servedCellMember->nrModeInfo.present == XNAP_NRModeInfo_PR_tdd) {
      req->info.mode = XNAP_MODE_TDD;
      xnap_tdd_info_t *TDDs = &req->info.tdd;
      XNAP_NRModeInfoTDD_t *tdd_Info = servedCellMember->nrModeInfo.choice.tdd;
      TDDs->freqinfo.arfcn = tdd_Info->nrFrequencyInfo.nrARFCN;
      AssertFatal(tdd_Info->nrFrequencyInfo.frequencyBand_List.list.count == 1, "cannot handle more than one frequency band\n");
      for (int f = 0; f < tdd_Info->nrFrequencyInfo.frequencyBand_List.list.count; f++) {
        XNAP_NRFrequencyBandItem_t *FreqItem = tdd_Info->nrFrequencyInfo.frequencyBand_List.list.array[f];
        TDDs->freqinfo.band = FreqItem->nr_frequency_band;
      }
      TDDs->tbw.scs = tdd_Info->nrTransmissonBandwidth.nRSCS;
      TDDs->tbw.nrb = tdd_Info->nrTransmissonBandwidth.nRNRB;
    } else {
      AssertFatal(false, "unknown NR Mode info \n");
    }
  }

  itti_send_msg_to_task(TASK_RRC_GNB, instance, message_p);
  return 0;
}

int xnap_gNB_handle_xn_setup_response(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, XNAP_XnAP_PDU_t *pdu)
{
  LOG_D(XNAP, "xnap_gNB_handle_xn_setup_response\n");
  AssertFatal(pdu->present == XNAP_XnAP_PDU_PR_successfulOutcome, "pdu->present != XNAP_XnAP_PDU_PR_successfulOutcome,\n");
  AssertFatal(pdu->choice.successfulOutcome->procedureCode == XNAP_ProcedureCode_id_xnSetup,
              "pdu->choice.successfulOutcome->procedureCode != XNAP_ProcedureCode_id_xnSetup\n");
  AssertFatal(pdu->choice.successfulOutcome->criticality == XNAP_Criticality_reject,
              "pdu->choice.successfulOutcome->criticality != XNAP_Criticality_reject\n");
  AssertFatal(pdu->choice.successfulOutcome->value.present == XNAP_SuccessfulOutcome__value_PR_XnSetupResponse,
              "pdu->choice.successfulOutcome->value.present != XNAP_SuccessfulOutcome__value_PR_XnSetupResponse\n");

  XNAP_XnSetupResponse_t *xnSetupResponse = &pdu->choice.successfulOutcome->value.choice.XnSetupResponse;
  XNAP_XnSetupResponse_IEs_t *ie;
  uint32_t gNB_id = 0;
  MessageDef *msg = itti_alloc_new_message(TASK_XNAP, 0, XNAP_SETUP_RESP);
  msg->ittiMsgHeader.originInstance = assoc_id;
  xnap_setup_resp_t *resp = &XNAP_SETUP_RESP(msg);
  xnap_gNB_instance_t *instance_p = xnap_gNB_get_instance(instance);
  xnap_gNB_data_t *xnap_gnb_data_p = xnap_get_gNB(instance, assoc_id);
  for (int i = 0; i < xnSetupResponse->protocolIEs.list.count; i++) {
    ie = xnSetupResponse->protocolIEs.list.array[i];

    switch (ie->id) {
      case XNAP_ProtocolIE_ID_id_GlobalNG_RAN_node_ID:
        AssertFatal(ie->criticality == XNAP_Criticality_reject, "ie->criticality != XNAP_Criticality_reject\n");
        AssertFatal(ie->value.present == XNAP_XnSetupResponse_IEs__value_PR_GlobalNG_RANNode_ID,
                    "ie->value.present != XNAP_XnSetupResponse_IEs__value_PR_GlobalNG_RANNode_ID\n");
        uint8_t *gNB_id_buf = ie->value.choice.GlobalNG_RANNode_ID.choice.gNB->gnb_id.choice.gnb_ID.buf;
        gNB_id = (gNB_id_buf[0] << 20) + (gNB_id_buf[1] << 12) + (gNB_id_buf[2] << 4) + ((gNB_id_buf[3] & 0xf0) >> 4);
        LOG_D(XNAP, "Connected gNB id: %07x\n", gNB_id);
        LOG_D(XNAP, "Adding gNB to the list of associated gNBs\n");
        xnap_gnb_data_p->state = XNAP_GNB_STATE_CONNECTED;
        xnap_gnb_data_p->gNB_id = gNB_id;
        break;
      case XNAP_ProtocolIE_ID_id_TAISupport_list:
        AssertFatal(ie->criticality == XNAP_Criticality_reject, "ie->criticality != XNAP_Criticality_reject\n");
        AssertFatal(ie->value.present == XNAP_XnSetupResponse_IEs__value_PR_TAISupport_List,
                    "ie->value.present != XNAP_XnSetupResponse_IEs__value_PR_TAISupport_List\n");
        PLMNID_TO_MCC_MNC(&ie->value.choice.TAISupport_List.list.array[0]->broadcastPLMNs.list.array[0]->plmn_id,
                          resp->info.plmn.mcc,
                          resp->info.plmn.mnc,
                          resp->info.plmn.mnc_digit_length);
        /*resp.gNB_CU_name = malloc(ie->value.choice.GNB_CU_Name.size+1);
        memcpy(resp.gNB_CU_name, ie->value.choice.GNB_CU_Name.buf, ie->value.choice.GNB_CU_Name.size);
        resp.gNB_CU_name[ie->value.choice.GNB_CU_Name.size] = '\0';
        LOG_D(F1AP, "F1AP: F1Setup-Resp: gNB_CU_name %s\n", resp.gNB_CU_name);*/
        break;
    }
  }
  // XNAP_ServedCells_NR_Item_t *servedCellMember;

  /* We received a new valid XN Setup Response on a stream != 0.
   * Reject gNB xn setup response.*/

  /*if (stream != 0) {
    LOG_E(XNAP, "Received new xn setup response on stream != 0\n");
  }

  if ((xnap_gNB_data = xnap_get_gNB(NULL, assoc_id)) == NULL) {
    LOG_E(XNAP,
          "[SCTP %d] Received XN setup response for non existing "
          "gNB context\n",
          assoc_id);
    return -1;
  }

  if ((xnap_gNB_data->state == XNAP_GNB_STATE_CONNECTED) || (xnap_gNB_data->state == XNAP_GNB_STATE_READY))

  {
    LOG_E(XNAP, "Received Unexpexted XN Setup Response Message\n");
    return -1;
  }

  LOG_D(XNAP, "Received a new XN setup response\n");

  XNAP_FIND_PROTOCOLIE_BY_ID(XNAP_XnSetupResponse_IEs_t, ie, xnSetupResponse, XNAP_ProtocolIE_ID_id_GlobalNG_RAN_node_ID, true);

  if (ie == NULL) {
    LOG_E(XNAP, "%s %d: ie is a NULL pointer \n", __FILE__, __LINE__);
    return -1;
  }*/
  /* Set proper pci */
  XNAP_FIND_PROTOCOLIE_BY_ID(XNAP_XnSetupResponse_IEs_t, ie, xnSetupResponse, XNAP_ProtocolIE_ID_id_List_of_served_cells_NR, true);
  if (ie == NULL) {
    LOG_E(XNAP, "%s %d: ie is a NULL pointer \n", __FILE__, __LINE__);
    return -1;
  }

  //  if (ie->value.choice.ServedCells_NR.list.count > 0) {
  //  servedCellMember = (XNAP_ServedCells_NR_Item_t *)ie->value.choice.ServedCells_NR.list.array[0];
  // xnap_gNB_data->Nid_cell = servedCellMember->served_cell_info_NR.nrPCI;
  // XNAP_SETUP_RESP(msg).info.nr_cellid = xnap_gNB_data->Nid_cell;
  //}

  /* The association is now ready as source and target gNBs know parameters of each other.
   * Mark the association as connected */
  // xnap_gNB_data->state = XNAP_GNB_STATE_READY;

  // instance_p = xnap_gNB_get_instance(instance);
  // DevAssert(instance_p != NULL);

  instance_p->xn_target_gnb_associated_nb++;
  // xnap_handle_xn_setup_message(instance_p, xnap_gnb_data_p, 0);

  itti_send_msg_to_task(TASK_RRC_GNB, instance_p->instance, msg);

  return 0;
}

int xnap_gNB_handle_xn_setup_failure(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, XNAP_XnAP_PDU_t *pdu)
{
  DevAssert(pdu != NULL);
  XNAP_XnSetupFailure_t *xnSetupFailure;
  XNAP_XnSetupFailure_IEs_t *ie;

  // xnap_gNB_instance_t *instance_p;
  xnap_gNB_data_t *xnap_gNB_data;

  xnSetupFailure = &pdu->choice.unsuccessfulOutcome->value.choice.XnSetupFailure;

  /*
   * We received a new valid XN Setup Failure on a stream != 0.
   * * * * This should not happen -> reject gNB xn setup failure.
   */

  if (stream != 0) {
    LOG_W(XNAP, "[SCTP %d] Received xn setup failure on stream != 0 (%d)\n", assoc_id, stream);
  }

  if ((xnap_gNB_data = xnap_get_gNB(instance, assoc_id)) == NULL) {
    LOG_E(XNAP,
          "[SCTP %d] Received XN setup failure for non existing "
          "gNB context\n",
          assoc_id);
    return -1;
  }

  if ((xnap_gNB_data->state == XNAP_GNB_STATE_CONNECTED) || (xnap_gNB_data->state == XNAP_GNB_STATE_READY))

  {
    LOG_E(XNAP, "Received Unexpexted XN Setup Failure Message\n");
    return -1;
  }

  LOG_D(XNAP, "Received a new XN setup failure\n");

  XNAP_FIND_PROTOCOLIE_BY_ID(XNAP_XnSetupFailure_IEs_t, ie, xnSetupFailure, XNAP_ProtocolIE_ID_id_Cause, true);

  if (ie == NULL) {
    LOG_E(XNAP, "%s %d: ie is a NULL pointer \n", __FILE__, __LINE__);
    return -1;
  }
  if ((ie->value.choice.Cause.present == XNAP_Cause_PR_misc)
      && (ie->value.choice.Cause.choice.misc == XNAP_CauseMisc_unspecified)) {
    LOG_E(XNAP, "Received XN setup failure for gNB ... gNB is not ready\n");
    exit(1);
  } else {
    LOG_E(XNAP, "Received xn setup failure for gNB... please check your parameters\n");
    exit(1);
  }

  xnap_gNB_data->state = XNAP_GNB_STATE_WAITING;

  // instance_p = xnap_gNB_get_instance(instance);
  // DevAssert(instance_p != NULL);

  xnap_handle_xn_setup_message(instance, assoc_id, 0);

  return 0;
}
