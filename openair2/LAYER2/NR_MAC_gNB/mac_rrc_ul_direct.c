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
 *      conmnc_digit_lengtht@openairinterface.org
 */

#include "nr_mac_gNB.h"
#include "intertask_interface.h"

#include "mac_rrc_ul.h"

static void f1_setup_request_direct(const f1ap_setup_req_t *req)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_SETUP_REQ);
  f1ap_setup_req_t *f1ap_msg = &F1AP_SETUP_REQ(msg);
  f1ap_msg->gNB_DU_id = req->gNB_DU_id;
  f1ap_msg->gNB_DU_name = strdup(req->gNB_DU_name);
  f1ap_msg->num_cells_available = req->num_cells_available;
  for (int n = 0; n < req->num_cells_available; ++n) {
    f1ap_msg->cell[n].info = req->cell[n].info; // copy most fields
    if (req->cell[n].info.tac) {
      f1ap_msg->cell[n].info.tac = malloc(sizeof(*f1ap_msg->cell[n].info.tac));
      AssertFatal(f1ap_msg->cell[n].info.tac != NULL, "out of memory\n");
      *f1ap_msg->cell[n].info.tac = *req->cell[n].info.tac;
    }
    if (req->cell[n].info.measurement_timing_information)
      f1ap_msg->cell[n].info.measurement_timing_information = strdup(req->cell[n].info.measurement_timing_information);

    if (req->cell[n].sys_info) {
      f1ap_gnb_du_system_info_t *orig_sys_info = req->cell[n].sys_info;
      f1ap_gnb_du_system_info_t *copy_sys_info = calloc(1, sizeof(*copy_sys_info));
      AssertFatal(copy_sys_info != NULL, "out of memory\n");
      f1ap_msg->cell[n].sys_info = copy_sys_info;

      copy_sys_info->mib = calloc(orig_sys_info->mib_length, sizeof(uint8_t));
      AssertFatal(copy_sys_info->mib != NULL, "out of memory\n");
      memcpy(copy_sys_info->mib, orig_sys_info->mib, orig_sys_info->mib_length);
      copy_sys_info->mib_length = orig_sys_info->mib_length;

      if (orig_sys_info->sib1_length > 0) {
        copy_sys_info->sib1 = calloc(orig_sys_info->sib1_length, sizeof(uint8_t));
        AssertFatal(copy_sys_info->sib1 != NULL, "out of memory\n");
        memcpy(copy_sys_info->sib1, orig_sys_info->sib1, orig_sys_info->sib1_length);
        copy_sys_info->sib1_length = orig_sys_info->sib1_length;
      }
    }
  }

  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
}

static void ue_context_setup_response_direct(const f1ap_ue_context_setup_t *req, const f1ap_ue_context_setup_t *resp)
{
  DevAssert(req->drbs_to_be_setup_length == resp->drbs_to_be_setup_length);
  AssertFatal(req->drbs_to_be_setup_length == 0, "not implemented\n");

  (void)req; /* we don't need the request -- it is to set up GTP in F1 case */
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_UE_CONTEXT_SETUP_RESP);
  f1ap_ue_context_setup_t *f1ap_msg = &F1AP_UE_CONTEXT_SETUP_RESP(msg);
  /* copy all fields, but reallocate memory buffers! */
  *f1ap_msg = *resp;

  if (resp->srbs_to_be_setup_length > 0) {
    DevAssert(resp->srbs_to_be_setup != NULL);
    f1ap_msg->srbs_to_be_setup_length = resp->srbs_to_be_setup_length;
    f1ap_msg->srbs_to_be_setup = calloc(f1ap_msg->srbs_to_be_setup_length, sizeof(*f1ap_msg->srbs_to_be_setup));
    for (int i = 0; i < f1ap_msg->srbs_to_be_setup_length; ++i)
      f1ap_msg->srbs_to_be_setup[i] = resp->srbs_to_be_setup[i];
  }

  f1ap_msg->du_to_cu_rrc_information = malloc(sizeof(*resp->du_to_cu_rrc_information));
  AssertFatal(f1ap_msg->du_to_cu_rrc_information != NULL, "out of memory\n");
  f1ap_msg->du_to_cu_rrc_information_length = resp->du_to_cu_rrc_information_length;
  du_to_cu_rrc_information_t *du2cu = f1ap_msg->du_to_cu_rrc_information;
  du2cu->cellGroupConfig_length = resp->du_to_cu_rrc_information->cellGroupConfig_length;
  du2cu->cellGroupConfig = calloc(du2cu->cellGroupConfig_length, sizeof(*du2cu->cellGroupConfig));
  AssertFatal(du2cu->cellGroupConfig != NULL, "out of memory\n");
  memcpy(du2cu->cellGroupConfig, resp->du_to_cu_rrc_information->cellGroupConfig, du2cu->cellGroupConfig_length);

  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
}

static void ue_context_modification_response_direct(const f1ap_ue_context_modif_req_t *req,
                                                    const f1ap_ue_context_modif_resp_t *resp)
{
  (void)req; /* we don't need the request -- it is to set up GTP in F1 case */
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_UE_CONTEXT_MODIFICATION_RESP);
  f1ap_ue_context_modif_resp_t *f1ap_msg = &F1AP_UE_CONTEXT_MODIFICATION_RESP(msg);

  f1ap_msg->gNB_CU_ue_id = resp->gNB_CU_ue_id;
  f1ap_msg->gNB_DU_ue_id = resp->gNB_DU_ue_id;
  f1ap_msg->plmn = resp->plmn;
  f1ap_msg->nr_cellid = resp->nr_cellid;
  f1ap_msg->servCellIndex = resp->servCellIndex;
  AssertFatal(resp->cellULConfigured == NULL, "not handled\n");
  f1ap_msg->servCellId = resp->servCellId;

  DevAssert(resp->cu_to_du_rrc_information == NULL && resp->cu_to_du_rrc_information_length == 0);
  if (resp->du_to_cu_rrc_information) {
    f1ap_msg->du_to_cu_rrc_information = malloc(sizeof(*resp->du_to_cu_rrc_information));
    AssertFatal(f1ap_msg->du_to_cu_rrc_information != NULL, "out of memory\n");
    f1ap_msg->du_to_cu_rrc_information_length = resp->du_to_cu_rrc_information_length;
    du_to_cu_rrc_information_t *du2cu = f1ap_msg->du_to_cu_rrc_information;
    du2cu->cellGroupConfig_length = resp->du_to_cu_rrc_information->cellGroupConfig_length;
    du2cu->cellGroupConfig = calloc(du2cu->cellGroupConfig_length, sizeof(*du2cu->cellGroupConfig));
    AssertFatal(du2cu->cellGroupConfig != NULL, "out of memory\n");
    memcpy(du2cu->cellGroupConfig, resp->du_to_cu_rrc_information->cellGroupConfig, du2cu->cellGroupConfig_length);
  }

  if (resp->drbs_to_be_setup_length > 0) {
    DevAssert(resp->drbs_to_be_setup != NULL);
    f1ap_msg->drbs_to_be_setup_length = resp->drbs_to_be_setup_length;
    f1ap_msg->drbs_to_be_setup = calloc(f1ap_msg->drbs_to_be_setup_length, sizeof(*f1ap_msg->drbs_to_be_setup));
    for (int i = 0; i < f1ap_msg->drbs_to_be_setup_length; ++i)
      f1ap_msg->drbs_to_be_setup[i] = resp->drbs_to_be_setup[i];
  }

  DevAssert(resp->drbs_to_be_modified == NULL && resp->drbs_to_be_modified_length == 0);
  f1ap_msg->QoS_information_type = resp->QoS_information_type;
  AssertFatal(resp->drbs_failed_to_be_setup_length == 0 && resp->drbs_failed_to_be_setup == NULL, "not implemented yet\n");

  if (resp->srbs_to_be_setup_length > 0) {
    DevAssert(resp->srbs_to_be_setup != NULL);
    f1ap_msg->srbs_to_be_setup_length = resp->srbs_to_be_setup_length;
    f1ap_msg->srbs_to_be_setup = calloc(f1ap_msg->srbs_to_be_setup_length, sizeof(*f1ap_msg->srbs_to_be_setup));
    for (int i = 0; i < f1ap_msg->srbs_to_be_setup_length; ++i)
      f1ap_msg->srbs_to_be_setup[i] = resp->srbs_to_be_setup[i];
  }

  AssertFatal(resp->srbs_failed_to_be_setup_length == 0 && resp->srbs_failed_to_be_setup == NULL, "not implemented yet\n");

  DevAssert(resp->rrc_container == NULL && resp->rrc_container_length == 0);

  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
}

static void ue_context_modification_required_direct(const f1ap_ue_context_modif_required_t *required)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_UE_CONTEXT_MODIFICATION_REQUIRED);
  f1ap_ue_context_modif_required_t *f1ap_msg = &F1AP_UE_CONTEXT_MODIFICATION_REQUIRED(msg);
  f1ap_msg->gNB_CU_ue_id = required->gNB_CU_ue_id;
  f1ap_msg->gNB_DU_ue_id = required->gNB_DU_ue_id;
  f1ap_msg->du_to_cu_rrc_information = NULL;
  if (required->du_to_cu_rrc_information != NULL) {
    f1ap_msg->du_to_cu_rrc_information = calloc(1, sizeof(*f1ap_msg->du_to_cu_rrc_information));
    AssertFatal(f1ap_msg->du_to_cu_rrc_information != NULL, "out of memory\n");
    du_to_cu_rrc_information_t *du2cu = f1ap_msg->du_to_cu_rrc_information;
    AssertFatal(required->du_to_cu_rrc_information->cellGroupConfig != NULL
                    && required->du_to_cu_rrc_information->cellGroupConfig_length > 0,
                "cellGroupConfig is mandatory\n");
    du2cu->cellGroupConfig_length = required->du_to_cu_rrc_information->cellGroupConfig_length;
    du2cu->cellGroupConfig = malloc(du2cu->cellGroupConfig_length * sizeof(*du2cu->cellGroupConfig));
    AssertFatal(du2cu->cellGroupConfig != NULL, "out of memory\n");
    memcpy(du2cu->cellGroupConfig, required->du_to_cu_rrc_information->cellGroupConfig, du2cu->cellGroupConfig_length);
    AssertFatal(
        required->du_to_cu_rrc_information->measGapConfig == NULL && required->du_to_cu_rrc_information->measGapConfig_length == 0,
        "not handled yet\n");
    AssertFatal(required->du_to_cu_rrc_information->requestedP_MaxFR1 == NULL
                    && required->du_to_cu_rrc_information->requestedP_MaxFR1_length == 0,
                "not handled yet\n");
  }
  f1ap_msg->cause = required->cause;
  f1ap_msg->cause_value = required->cause_value;
  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
}

static void ue_context_release_request_direct(const f1ap_ue_context_release_req_t *req)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_UE_CONTEXT_RELEASE_REQ);
  f1ap_ue_context_release_req_t *f1ap_msg = &F1AP_UE_CONTEXT_RELEASE_REQ(msg);
  *f1ap_msg = *req;
  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
}

static void ue_context_release_complete_direct(const f1ap_ue_context_release_complete_t *complete)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_UE_CONTEXT_RELEASE_COMPLETE);
  f1ap_ue_context_release_complete_t *f1ap_msg = &F1AP_UE_CONTEXT_RELEASE_COMPLETE(msg);
  *f1ap_msg = *complete;
  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
}

static void initial_ul_rrc_message_transfer_direct(module_id_t module_id, const f1ap_initial_ul_rrc_message_t *ul_rrc)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_INITIAL_UL_RRC_MESSAGE);
  /* copy all fields, but reallocate rrc_containers! */
  f1ap_initial_ul_rrc_message_t *f1ap_msg = &F1AP_INITIAL_UL_RRC_MESSAGE(msg);
  *f1ap_msg = *ul_rrc;

  f1ap_msg->rrc_container = malloc(ul_rrc->rrc_container_length);
  DevAssert(f1ap_msg->rrc_container);
  memcpy(f1ap_msg->rrc_container, ul_rrc->rrc_container, ul_rrc->rrc_container_length);
  f1ap_msg->rrc_container_length = ul_rrc->rrc_container_length;

  f1ap_msg->du2cu_rrc_container = malloc(ul_rrc->du2cu_rrc_container_length);
  DevAssert(f1ap_msg->du2cu_rrc_container);
  memcpy(f1ap_msg->du2cu_rrc_container, ul_rrc->du2cu_rrc_container, ul_rrc->du2cu_rrc_container_length);
  f1ap_msg->du2cu_rrc_container_length = ul_rrc->du2cu_rrc_container_length;

  itti_send_msg_to_task(TASK_RRC_GNB, module_id, msg);
}

/* handlers of Position Information Transfer related NRPPA UL messages */
static void positioning_information_response(const f1ap_positioning_information_resp_t *resp)
{
  LOG_I(MAC,
        "Prepring PositioningInformationResponse gNB_CU_ue_id=%d, gNB_DU_ue_id=%d, ue_rnti= %04x\n",
        resp->gNB_CU_ue_id,
        resp->gNB_DU_ue_id,
        resp->nrppa_msg_info.ue_rnti);

  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_POSITIONING_INFORMATION_RESP);
  f1ap_positioning_information_resp_t *f1ap_msg = &F1AP_POSITIONING_INFORMATION_RESP(msg);

  /* TODO copy all fields, but reallocate memory buffers! */
  *f1ap_msg = *resp;
  f1ap_msg->gNB_CU_ue_id = resp->gNB_CU_ue_id;
  f1ap_msg->gNB_DU_ue_id = resp->gNB_DU_ue_id;
  f1ap_msg->nrppa_msg_info.nrppa_transaction_id = resp->nrppa_msg_info.nrppa_transaction_id;
  f1ap_msg->nrppa_msg_info.instance = resp->nrppa_msg_info.instance;
  f1ap_msg->nrppa_msg_info.gNB_ue_ngap_id = resp->nrppa_msg_info.gNB_ue_ngap_id;
  f1ap_msg->nrppa_msg_info.amf_ue_ngap_id = resp->nrppa_msg_info.amf_ue_ngap_id;
  f1ap_msg->nrppa_msg_info.ue_rnti = resp->nrppa_msg_info.ue_rnti;
  f1ap_msg->nrppa_msg_info.routing_id_buffer = resp->nrppa_msg_info.routing_id_buffer;
  f1ap_msg->nrppa_msg_info.routing_id_length = resp->nrppa_msg_info.routing_id_length;

  gNB_MAC_INST *mac = RC.nrmac[resp->nrppa_msg_info.instance];
  NR_UEs_t *UE_info = &mac->UE_info;
  UE_iterator(UE_info->list, UE)
  {
    if (UE->rnti == resp->nrppa_msg_info.ue_rnti) { // configuration details of specific UE // TODO manage non UE associated
      LOG_I(MAC,
            "Extracting SRS Configuration for Positioning_information_response for ue rnti= %04x \n",
            resp->nrppa_msg_info.ue_rnti ); ////uid_t uid = &UE->uid;
      NR_UE_UL_BWP_t *current_BWP = &UE->current_UL_BWP;
      NR_SRS_Config_t *srs_config = current_BWP->srs_Config;

      // IE 9.2.28 SRS Configuration Preparing SRS Configuration IE of PositioningInformationResponse
      int maxnoSRScarrier = 1; // gNB->max_nb_srs max value is 32; // TODO find the acutal number for carrier and add here
      f1ap_msg->srs_configuration.srs_carrier_list.srs_carrier_list_length = maxnoSRScarrier;
      f1ap_msg->srs_configuration.srs_carrier_list.srs_carrier_list_item =
          malloc(maxnoSRScarrier * sizeof(f1ap_srs_carrier_list_item_t));
      DevAssert(f1ap_msg->srs_configuration.srs_carrier_list.srs_carrier_list_item);
      f1ap_srs_carrier_list_item_t *srs_carrier_list_item = f1ap_msg->srs_configuration.srs_carrier_list.srs_carrier_list_item;
      LOG_D(MAC,
            "Preparing srs_carrier_list for NRPPA maxnoSRScarrier= %d",
            f1ap_msg->srs_configuration.srs_carrier_list.srs_carrier_list_length);
      for (int i = 0; i < maxnoSRScarrier; i++) {
        srs_carrier_list_item->pointA = 1; // (M)
        srs_carrier_list_item->pci = 1; // Optional Physical cell ID of the cell that contians the SRS carrier

        // Preparing Active UL BWP information IE of SRSCarrier_List f1ap_active_ul_bwp_ active_ul_bwp; //(M)
        srs_carrier_list_item->active_ul_bwp.locationAndBandwidth = 0; // long
        srs_carrier_list_item->active_ul_bwp.subcarrierSpacing = current_BWP->scs; // long
        srs_carrier_list_item->active_ul_bwp.cyclicPrefix = 0; //*(current_BWP->cyclicprefix);// long
        srs_carrier_list_item->active_ul_bwp.txDirectCurrentLocation = 0; // long //TODO
        srs_carrier_list_item->active_ul_bwp.shift7dot5kHz = 0; // long  Optional //TODO

        // Preparing sRSResource_List IE of SRSConfig (IE of activeULBWP)
        int maxnoSRSResources = srs_config->srs_ResourceToAddModList->list.count;
        srs_carrier_list_item->active_ul_bwp.sRSConfig.sRSResource_List.srs_resource_list_length = maxnoSRSResources;
        srs_carrier_list_item->active_ul_bwp.sRSConfig.sRSResource_List.srs_resource =
            malloc(maxnoSRSResources * sizeof(f1ap_srs_resource_t)); // TODO check null condition
        DevAssert(srs_carrier_list_item->active_ul_bwp.sRSConfig.sRSResource_List.srs_resource);
        f1ap_srs_resource_t *resource_item = srs_carrier_list_item->active_ul_bwp.sRSConfig.sRSResource_List.srs_resource;
        LOG_D(MAC,
              "Preparing sRSResource_List for NRPPA maxnoSRSResources=%d \n",
              srs_carrier_list_item->active_ul_bwp.sRSConfig.sRSResource_List.srs_resource_list_length);
        for (int k = 0; k < maxnoSRSResources; k++) { // Preparing SRS Resource List
          NR_SRS_Resource_t *srs_resource = srs_config->srs_ResourceToAddModList->list.array[k];

          resource_item->sRSResourceID = srs_resource->srs_ResourceId; //(M)
          resource_item->nrofSRS_Ports = srs_resource->nrofSRS_Ports; //(M) port1	= 0, ports2	= 1, ports4	= 2
          resource_item->startPosition = srs_resource->resourceMapping.startPosition; //(M)
          resource_item->nrofSymbols = srs_resource->resourceMapping.nrofSymbols; //(M)  n1	= 0, n2	= 1, n4	= 2
          resource_item->repetitionFactor = srs_resource->resourceMapping.repetitionFactor; //(M)  n1	= 0, n2	= 1, n4	= 2
          resource_item->freqDomainPosition = srs_resource->freqDomainPosition; //(M)
          resource_item->freqDomainShift = srs_resource->freqDomainShift; //(M)
          resource_item->c_SRS = srs_resource->freqHopping.c_SRS; //(M)
          resource_item->b_SRS = srs_resource->freqHopping.b_SRS; //(M)
          resource_item->b_hop = srs_resource->freqHopping.b_hop; //(M)
          resource_item->groupOrSequenceHopping = 0; // TODO not found(M) neither	= 0, groupHopping	= 1, sequenceHopping	= 2
          resource_item->slotOffset = 0; // TODO not found (M)
          resource_item->sequenceId = srs_resource->sequenceId; //(M)

          // IE transmissionComb
          switch(srs_resource->transmissionComb.present){
          case NR_SRS_Resource__transmissionComb_PR_n2:
            resource_item->transmissionComb.present = f1ap_transmission_comb_pr_n2;
            resource_item->transmissionComb.choice.n2.combOffset_n2 = srs_resource->transmissionComb.choice.n2->combOffset_n2;
            resource_item->transmissionComb.choice.n2.cyclicShift_n2 = srs_resource->transmissionComb.choice.n2->cyclicShift_n2;
          break;
          case NR_SRS_Resource__transmissionComb_PR_n4:
            resource_item->transmissionComb.present = f1ap_transmission_comb_pr_n4;
            resource_item->transmissionComb.choice.n4.combOffset_n4 =  srs_resource->transmissionComb.choice.n4->combOffset_n4;
            resource_item->transmissionComb.choice.n4.cyclicShift_n4 = srs_resource->transmissionComb.choice.n4->cyclicShift_n4;
          break;
          case NR_SRS_Resource__transmissionComb_PR_NOTHING:
            srs_resource->transmissionComb.present = f1ap_transmission_comb_pr_nothing;
          break;
          default:
          LOG_E(MAC, "Unknown Resource Item TransmissionComb\n");
          break;
          }

          // IE  resourceType
          switch(srs_resource->resourceType.present){
          case NR_SRS_Resource__resourceType_PR_periodic:
            resource_item->resourceType.present = f1ap_resource_type_pr_periodic;
            resource_item->resourceType.choice.periodic.periodicity =
                0; // TODO check sturctuce srs_resource->resourceType.choice.periodic->periodicityAndOffset_p.periodicity; //(M)
                   // choice periodic (uint8_t periodicity; uint16_t offset);
            resource_item->resourceType.choice.periodic.offset =
                0; // TODO srs_resource->resourceType.choice.periodic->offset; //(M)
          break;
          case NR_SRS_Resource__resourceType_PR_aperiodic:
            resource_item->resourceType.present = f1ap_resource_type_pr_aperiodic;
            resource_item->resourceType.choice.aperiodic.aperiodicResourceType =
                0; // not done in srs impelmentation srs_resource->resourceType.choice.aperiodic->aperiodicResourceType; //(M)
                   // aperiodic (uint8_t aperiodicResourceType;);
          break;
          case NR_SRS_Resource__resourceType_PR_semi_persistent:
            resource_item->resourceType.present = f1ap_resource_type_pr_semi_persistent;
            resource_item->resourceType.choice.semi_persistent.periodicity =
                0; // TODO not implemented in SRS srs_resource->resourceType.choice.semi_persistent->periodicity; //(M)
                   // semi_persistent (uint8_t periodicity;
            resource_item->resourceType.choice.semi_persistent.offset =
                0; // TODOnot implemented in SRS srs_resource->resourceType.choice.semi_persistent->offset; //(M)
          break;
          case NR_SRS_Resource__resourceType_PR_NOTHING:
            resource_item->resourceType.present = f1ap_resource_type_pr_nothing;
          break;
          default:
          LOG_E(MAC, "Unknown Resource Item resourceType\n");
          break;
          }

          if (k < maxnoSRSResources - 1) {
            resource_item++;
          }
        } // for(int k=0; k < nb_srsresource; k++)

        // Preparing sRSResourceSet_List IE of SRSConfig (IE of activeULBWP)
        int maxnoSRSResourceSets = 1; // srs_config->srs_ResourceSetToAddModList->list.count;
        srs_carrier_list_item->active_ul_bwp.sRSConfig.sRSResourceSet_List.srs_resource_set_list_length = maxnoSRSResourceSets;
        srs_carrier_list_item->active_ul_bwp.sRSConfig.sRSResourceSet_List.srs_resource_set =
            malloc(maxnoSRSResourceSets * sizeof(f1ap_srs_resource_set_t));
        DevAssert(srs_carrier_list_item->active_ul_bwp.sRSConfig.sRSResourceSet_List.srs_resource_set);
        f1ap_srs_resource_set_t *resourceSet_item =
            srs_carrier_list_item->active_ul_bwp.sRSConfig.sRSResourceSet_List.srs_resource_set;
        LOG_D(MAC, "Preparing sRSResourceSet_List for NRPPA  maxnoSRSResourceSets=%d \n", maxnoSRSResourceSets);
        for (int y = 0; y < maxnoSRSResourceSets; y++) { // Preparing SRS Resource Set List

          NR_SRS_ResourceSet_t *srs_resourceset = srs_config->srs_ResourceSetToAddModList->list.array[y];

          // IE sRSResourceSetID (M)
          resourceSet_item->sRSResourceSetID = srs_resourceset->srs_ResourceSetId;

          // IE resourceSetType
          // TODO IE not found srs_resourceset->resourceType.choice.periodic->associatedCSI_RS;//(M) F1AP_ResourceSetType_t// resourceSetType;
          switch(srs_resourceset->resourceType.present){
          case NR_SRS_ResourceSet__resourceType_PR_periodic:
          resourceSet_item->resourceSetType.present = f1ap_resource_set_type_pr_periodic;
          resourceSet_item->resourceSetType.choice.periodic.periodicSet =0;
          break;
          case NR_SRS_ResourceSet__resourceType_PR_aperiodic:
          resourceSet_item->resourceSetType.present = f1ap_resource_set_type_pr_aperiodic;
          resourceSet_item->resourceSetType.choice.aperiodic.sRSResourceTrigger =1; // range 1-3
          resourceSet_item->resourceSetType.choice.aperiodic.slotoffset =1; // range 1-32
          break;
          case NR_SRS_ResourceSet__resourceType_PR_semi_persistent:
          resourceSet_item->resourceSetType.present = f1ap_resource_set_type_pr_semi_persistent;
          resourceSet_item->resourceSetType.choice.semi_persistent.semi_persistentSet =0;
          break;
          case NR_SRS_ResourceSet__resourceType_PR_NOTHING:
          resourceSet_item->resourceSetType.present = f1ap_resource_set_type_pr_nothing;
          break;
          default:
          LOG_E(MAC, "Unknown NR_SRS_ResourceSet__resourceType \n");
          break;
          }

          // IE sRSResourceID_List
          int maxnoSRSResourcePerSets = 1; // TODO retrieve and add
          resourceSet_item->sRSResourceID_List.srs_resource_id_list_length = maxnoSRSResourcePerSets;
          resourceSet_item->sRSResourceID_List.srs_resource_id = malloc(maxnoSRSResourcePerSets * sizeof(uint8_t));
          DevAssert(resourceSet_item->sRSResourceID_List.srs_resource_id);
          for (int z = 0; z < maxnoSRSResourcePerSets; z++) {
            resourceSet_item->sRSResourceID_List.srs_resource_id = 0; // (M)F1AP_SRSResourceID_List_t	 sRSResourceID_List;
          }

          if (y < maxnoSRSResourceSets - 1) {
            resourceSet_item++;
          }
        } // for(int y=0; y < maxnoSRSResourceSets; y++)

        // Preparing posSRSResource_List IE of SRSConfig (IE of activeULBWP)  TODO IE not found in  OAI srs_config so filled zero
        // values
        int maxnoPosSRSResources = 1; // srs_config->possrs_ResourceToAddModList->list.count;
        srs_carrier_list_item->active_ul_bwp.sRSConfig.posSRSResource_List.pos_srs_resource_list_length = maxnoPosSRSResources;
        srs_carrier_list_item->active_ul_bwp.sRSConfig.posSRSResource_List.pos_srs_resource_item =
            malloc(maxnoPosSRSResources * sizeof(f1ap_pos_srs_resource_item_t));
        DevAssert(srs_carrier_list_item->active_ul_bwp.sRSConfig.posSRSResource_List.pos_srs_resource_item);
        f1ap_pos_srs_resource_item_t *pos_resource_item =
            srs_carrier_list_item->active_ul_bwp.sRSConfig.posSRSResource_List.pos_srs_resource_item;
        LOG_D(MAC,
              "Preparing posSRSResource_List IE for NRPPA maxnoPosSRSResources=%d \n",
              srs_carrier_list_item->active_ul_bwp.sRSConfig.posSRSResource_List.pos_srs_resource_list_length);
        for (int z = 0; z < maxnoPosSRSResources; z++) { // Preparing Pos SRS Resource List
          pos_resource_item->srs_PosResourceId = 0; // (M)
          pos_resource_item->startPosition = 0; // (M)  range (0,1,...13)
          pos_resource_item->nrofSymbols = 0; // (M)  n1	= 0, n2	= 1, n4	= 2, n8	= 3, n12 = 4
          pos_resource_item->freqDomainShift = 0; // (M)
          pos_resource_item->c_SRS = 0; // (M)
          pos_resource_item->groupOrSequenceHopping = 0; // (M)  neither	= 0, groupHopping	= 1, sequenceHopping	= 2
          pos_resource_item->sequenceId = 0; //(M)
          // pos_resource_item->spatialRelationPos;	// OPTIONAL

          pos_resource_item->transmissionCombPos.present=f1ap_transmission_comb_pos_pr_n2;
          pos_resource_item->transmissionCombPos.choice.n2.combOffset_n2 =
              0; // (M)  f1ap_transmission_comb_pos_n2_t n2 (combOffset_n2,cyclicShift_n2) ; f1ap_transmission_comb_pos_n2_t n4;
                 // f1ap_transmission_comb_pos_n8_t n8;
          pos_resource_item->transmissionCombPos.choice.n2.cyclicShift_n2 = 0;

          pos_resource_item->resourceTypePos.present=f1ap_resource_type_pos_pr_periodic;
          pos_resource_item->resourceTypePos.choice.periodic.periodicity =
              0; // (M)    f1ap_resource_type_periodic_pos_t	  periodic;	f1ap_resource_type_semi_persistent_pos_t semi_persistent;
                 // f1ap_resource_type_aperiodic_pos_t	        aperiodic;
          pos_resource_item->resourceTypePos.choice.periodic.offset = 0; // (M)

        } // for(int z=0; z < maxnoPosSRSResources; z++)

        // Preparing posSRSResourceSet_List IE of SRSConfig (IE of activeULBWP) TODO IE not found in  OAI srs_config so filled zero
        // values
        int maxnoPosSRSResourceSets = 1; // srs_config->possrs_ResourceSetToAddModList->list.count;
        srs_carrier_list_item->active_ul_bwp.sRSConfig.posSRSResourceSet_List.pos_srs_resource_set_list_length =
            maxnoPosSRSResourceSets;
        srs_carrier_list_item->active_ul_bwp.sRSConfig.posSRSResourceSet_List.pos_srs_resource_set_item =
            malloc(maxnoPosSRSResourceSets * sizeof(f1ap_pos_srs_resource_set_item_t));
        DevAssert(srs_carrier_list_item->active_ul_bwp.sRSConfig.posSRSResourceSet_List.pos_srs_resource_set_item);
        f1ap_pos_srs_resource_set_item_t *pos_resourceSet_item =
            srs_carrier_list_item->active_ul_bwp.sRSConfig.posSRSResourceSet_List.pos_srs_resource_set_item;
        LOG_D(MAC, "Preparing posSRSResourceSet_List for NRRPA  maxnoPosSRSResourceSets=%d \n", maxnoPosSRSResourceSets);
        for (int f = 0; f < maxnoPosSRSResourceSets; f++) { // Preparing Pos SRS Resource Set List
          pos_resourceSet_item->possrsResourceSetID = 0; // (M) srs_config->possrs_ResourceSetToAddModList->list.array[y]->srs_ResourceSetId; //// (M)

          // pos_resourceSet_item->possRSResourceID_List; //f1ap_pos_srs_resource_id_list_t	 possRSResourceID_List;
          int maxnoPosSRSResourcePerSets = 1;
          pos_resourceSet_item->possRSResourceID_List.pos_srs_resource_id_list_length = maxnoPosSRSResourcePerSets;
          pos_resourceSet_item->possRSResourceID_List.srs_pos_resource_id = malloc(maxnoPosSRSResourcePerSets * sizeof(uint8_t));
          DevAssert(pos_resourceSet_item->possRSResourceID_List.srs_pos_resource_id);
          for (int z = 0; z < maxnoPosSRSResourcePerSets; z++) {
            pos_resourceSet_item->possRSResourceID_List.srs_pos_resource_id = 0; // TODO pointer address update
          }

          //  IE posresourceSetType TODO
          //pos_resourceSet_item->posresourceSetType.present=f1ap_pos_resource_set_type_pr_nothing;
          pos_resourceSet_item->posresourceSetType.present=f1ap_pos_resource_set_type_pr_aperiodic;
          pos_resourceSet_item->posresourceSetType.choice.aperiodic.sRSResourceTrigger_List=1;
          //pos_resourceSet_item->posresourceSetType.periodic.posperiodicSet = 0; // f1ap_pos_resource_set_type_u	 posresourceSetType;
        } // for(int f=0; f < maxnoSRSResourceSets; f++)


        //  Preparing Uplink Channel BW Per SCS List information IE of SRSCarrier_List f1ap_uplink_channel_bw_per_scs_list_t
        //  uplink_channel_bw_per_scs_list ; //(M)
        int maxnoSCSs = 1; // TODO adeel retrieve and add
        srs_carrier_list_item->uplink_channel_bw_per_scs_list.scs_specific_carrier_list_length = maxnoSCSs;
        srs_carrier_list_item->uplink_channel_bw_per_scs_list.scs_specific_carrier =
            malloc(maxnoSCSs * sizeof(f1ap_scs_specific_carrier_t));
        DevAssert(srs_carrier_list_item->uplink_channel_bw_per_scs_list.scs_specific_carrier);
        f1ap_scs_specific_carrier_t *scs_specific_carrier_item =
            srs_carrier_list_item->uplink_channel_bw_per_scs_list.scs_specific_carrier;
        LOG_D(MAC, "Preparing Uplink Channel BW Per SCS List for NRPPA maxnoSCSs=%d \n", maxnoSCSs);
        for (int a = 0; a < maxnoSCSs; a++) {
          scs_specific_carrier_item->offsetToCarrier = 0; // (M)
          scs_specific_carrier_item->subcarrierSpacing = 0; // (M) kHz15	= 0, kHz30	= 1, kHz60	= 2, kHz120	= 3
          scs_specific_carrier_item->carrierBandwidth = 0; // (M)
        } // for(int a=0; a < maxnoSCSs; a++)
      } // for (int i = 0; i < maxnoSRScarrier; i++)
    } // Condition for the target UE of positioning if(UE_SUPI==?)
  } // UE_iterator UE_iterator(UE_info->list, UE)

  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
}

static void positioning_information_failure(const f1ap_positioning_information_failure_t *failure)
{
  LOG_I(MAC,
        "UL Prepring PositioningInformationFailure gNB_CU_ue_id=%d, gNB_DU_ue_id=%d \n",
        failure->gNB_CU_ue_id,
        failure->gNB_DU_ue_id);
  AssertFatal(false, "Not Implemented \n");
}

static void positioning_information_update(const f1ap_positioning_information_update_t *update)
{
  LOG_I(MAC,
        "UL Prepring PositioningInformationUPDATE gNB_CU_ue_id=%d, gNB_DU_ue_id=%d \n",
        update->gNB_CU_ue_id,
        update->gNB_DU_ue_id);
  AssertFatal(false, "Not Implemented \n");
}

static void positioning_activation_response(const f1ap_positioning_activation_resp_t *resp)
{
  LOG_I(MAC,
        "UL Prepring PositioningActivationResponse gNB_CU_ue_id=%d, gNB_DU_ue_id=%d \n",
        resp->gNB_CU_ue_id,
        resp->gNB_DU_ue_id);
  AssertFatal(false, " Not Implemented \n");
}

static void positioning_activation_failure(const f1ap_positioning_activation_failure_t *failure)
{
  LOG_I(MAC,
        "UL Prepring PositioningActivationFailure gNB_CU_ue_id=%d, gNB_DU_ue_id=%d \n",
        failure->gNB_CU_ue_id,
        failure->gNB_DU_ue_id);
  AssertFatal(false, " Not Implemented \n");
}

/* handlers of TRP Information Transfer related NRPPA UL messages */

static void trp_information_response(const f1ap_trp_information_resp_t *resp)
{
  LOG_I(MAC, "UL Prepring TRPInformationResponse transaction_id=%d\n", resp->transaction_id);
  AssertFatal(false, " Not Implemented \n");
}

static void trp_information_failure(const f1ap_trp_information_failure_t *failure)
{
  LOG_I(MAC, "UL Prepring TRPInformationFailure transaction_id=%d \n", failure->transaction_id);
  AssertFatal(false, " Not Implemented \n");
}

/* handlers of Measurement Information Transfer related NRPPA UL messages */
static void positioning_measurement_response(const f1ap_measurement_resp_t *resp)
{
  LOG_I(MAC,
        "UL Prepring MeasurementResponse transaction_id=%d, lmf_measurement_id=%d, ran_measurement_id=%d \n",
        resp->transaction_id,
        resp->lmf_measurement_id,
        resp->ran_measurement_id);

  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_MEASUREMENT_RESP);
      // prepare the common part of the response
  f1ap_measurement_resp_t f1ap_resp;
  f1ap_resp.transaction_id = resp->transaction_id;
  f1ap_resp.lmf_measurement_id = resp->lmf_measurement_id;
  f1ap_resp.ran_measurement_id = resp->ran_measurement_id;
  f1ap_resp.nrppa_msg_info = resp->nrppa_msg_info;
  f1ap_resp.pos_measurement_result_list.pos_measurement_result_list_length = 1;
  f1ap_resp.pos_measurement_result_list.pos_measurement_result_list_item = malloc(sizeof(f1ap_pos_measurement_result_list_item_t));
  f1ap_resp.pos_measurement_result_list.pos_measurement_result_list_item->tRPID = 0; //TODO: this needs to be added to the config file
  f1ap_resp.pos_measurement_result_list.pos_measurement_result_list_item->posMeasurementResult.f1ap_pos_measurement_result_length = 1;
  f1ap_resp.pos_measurement_result_list.pos_measurement_result_list_item->posMeasurementResult.pos_measurement_result_item = malloc(sizeof(f1ap_pos_measurement_result_item_t));

  //criticality_diagnostics;

  uint32_t Tc_inv = 4096*480000;
  uint16_t k = 1;
  uint64_t T_inv = Tc_inv/(1<<k);
  uint64_t T_ns_inv = 1000000000;


  gNB_MAC_INST *mac = RC.nrmac[resp->nrppa_msg_info.instance];
  NR_UEs_t *UE_info = &mac->UE_info;
  UE_iterator(UE_info->list, UE)
  {
    if (UE->rnti == resp->nrppa_msg_info.ue_rnti) { // configuration details of specific UE // TODO manage non UE associated
         LOG_I(MAC,
            "Extracting uL_RTOA info of MeasurementResponse for ue rnti= %04x \n", resp->nrppa_msg_info.ue_rnti ); ////uid_t uid = &UE->uid;
      // we assume we use UL_RTOA for now with k=1 (i.e. 8 times oversampling from 122.88e6 Msps)
      f1ap_resp.pos_measurement_result_list.pos_measurement_result_list_item->posMeasurementResult.pos_measurement_result_item->measuredResultsValue.present=f1ap_measured_results_value_pr_ul_rtoa;
      f1ap_resp.pos_measurement_result_list.pos_measurement_result_list_item->posMeasurementResult.pos_measurement_result_item->measuredResultsValue.choice.uL_RTOA.uL_RTOA_MeasurementItem.present=f1ap_ulrtoameas_pr_k1;
      f1ap_resp.pos_measurement_result_list.pos_measurement_result_list_item->posMeasurementResult.pos_measurement_result_item->measuredResultsValue.choice.uL_RTOA.uL_RTOA_MeasurementItem.choice.k1= (int32_t) (((int64_t)UE->ue_pos_info.toa_ns * (int64_t)T_inv) / T_ns_inv);
      LOG_I(MAC,"Extracting uL_RTOA info of MeasurementResponse for ue rnti= %04x, k1=%d \n", resp->nrppa_msg_info.ue_rnti, (int32_t) (((int64_t)UE->ue_pos_info.toa_ns * (int64_t)T_inv) / T_ns_inv));

      //TODO IE timeStamp.measurementTime
      // f1ap_resp.pos_measurement_result_list.pos_measurement_result_list_item->posMeasurementResult.pos_measurement_result_item->timeStamp.measurementTime
      //TODO IE timeStamp.slotIndex
      // f1ap_resp.pos_measurement_result_list.pos_measurement_result_list_item->posMeasurementResult.pos_measurement_result_item->timeStamp.slotIndex.present
      // f1ap_resp.pos_measurement_result_list.pos_measurement_result_list_item->posMeasurementResult.pos_measurement_result_item->timeStamp.slotIndex.choice
    }
  }


  F1AP_MEASUREMENT_RESP(msg) = f1ap_resp;
  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);

}

static void positioning_measurement_failure(const f1ap_measurement_failure_t *failure)
{
  LOG_I(MAC,
        "UL Prepring MeasurementFailure transaction_id=%d, lmf_measurement_id=%d \n",
        failure->transaction_id,
        failure->lmf_measurement_id);
  AssertFatal(false, " Not Implemented \n");
}

static void positioning_measurement_report(const f1ap_measurement_report_t *report)
{
  LOG_I(MAC,
        "UL Prepring MeasurementReport transaction_id=%d, lmf_measurement_id=%d \n",
        report->transaction_id,
        report->lmf_measurement_id);
  AssertFatal(false, " Not Implemented \n");
}

static void positioning_measurement_failure_indication(const f1ap_measurement_failure_ind_t *failure_ind)
{
  LOG_I(MAC,
        "UL Prepring MeasurementFailureIndication transaction_id=%d, lmf_measurement_id=%d \n",
        failure_ind->transaction_id,
        failure_ind->lmf_measurement_id);
  AssertFatal(false, " Not Implemented \n");
}

void mac_rrc_ul_direct_init(struct nr_mac_rrc_ul_if_s *mac_rrc)
{
  mac_rrc->f1_setup_request = f1_setup_request_direct;
  mac_rrc->ue_context_setup_response = ue_context_setup_response_direct;
  mac_rrc->ue_context_modification_response = ue_context_modification_response_direct;
  mac_rrc->ue_context_modification_required = ue_context_modification_required_direct;
  mac_rrc->ue_context_release_request = ue_context_release_request_direct;
  mac_rrc->ue_context_release_complete = ue_context_release_complete_direct;
  mac_rrc->initial_ul_rrc_message_transfer = initial_ul_rrc_message_transfer_direct;
  mac_rrc->positioning_information_response = positioning_information_response; // nrppa adeel
  mac_rrc->positioning_information_failure = positioning_information_failure;
  mac_rrc->positioning_information_update = positioning_information_update;
  mac_rrc->positioning_activation_response = positioning_activation_response;
  mac_rrc->positioning_activation_failure = positioning_activation_failure;
  mac_rrc->trp_information_response = trp_information_response;
  mac_rrc->trp_information_failure = trp_information_failure;
  mac_rrc->positioning_measurement_response = positioning_measurement_response;
  mac_rrc->positioning_measurement_failure = positioning_measurement_failure;
  mac_rrc->positioning_measurement_report = positioning_measurement_report;
  mac_rrc->positioning_measurement_failure_indication = positioning_measurement_failure_indication;
}
