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

#include "ran_func_rc.h"
#include "ran_func_rc_subs.h"
#include "ran_func_rc_extern.h"
#include "../../flexric/test/rnd/fill_rnd_data_rc.h"  // this dependancy will be taken out once RAN Function Definition is implemented
#include "../../flexric/src/sm/rc_sm/ie/ir/lst_ran_param.h"
#include "../../flexric/src/sm/rc_sm/ie/ir/ran_param_list.h"
#include "../../flexric/src/agent/e2_agent_api.h"
#include "openair2/LAYER2/NR_MAC_gNB/mac_proto.h"
#include "openair2/LAYER2/NR_MAC_gNB/nr_mac_gNB.h"
#include "openair2/LAYER2/NR_MAC_gNB/slicing/nr_slicing.h"
#include "openair2/RRC/NR/rrc_gNB_UE_context.h"
#include "rc_ctrl_service_style_2.h"

#include <stdio.h>
#include <unistd.h>
#include "common/ran_context.h"

static pthread_once_t once_rc_mutex = PTHREAD_ONCE_INIT;
static rc_subs_data_t rc_subs_data = {0};
static pthread_mutex_t rc_mutex = PTHREAD_MUTEX_INITIALIZER;

static void init_once_rc(void)
{
  init_rc_subs_data(&rc_subs_data);
}

void read_rc_setup_sm(void* data)
{
  assert(data != NULL);
//  assert(data->type == RAN_CTRL_V1_3_AGENT_IF_E2_SETUP_ANS_V0);
  rc_e2_setup_t* rc = (rc_e2_setup_t*)data;
  rc->ran_func_def = fill_rc_ran_func_def();

  // E2 Setup Request is sent periodically until the connection is established
  // RC subscritpion data should be initialized only once
  const int ret = pthread_once(&once_rc_mutex, init_once_rc);
  DevAssert(ret == 0);
}


RB_PROTOTYPE(ric_id_2_param_id_trees, ric_req_id_s, entries, cmp_ric_req_id);

static ue_id_e2sm_t fill_ue_id_data(const gNB_RRC_UE_t *rrc_ue_context)
{
  ue_id_e2sm_t ue_id = {0};

  // Check the E2 node type
  #if defined (NGRAN_GNB_CUUP) && defined (NGRAN_GNB_CUCP)
  ue_id.type = GNB_UE_ID_E2SM;
  if (RC.nrrrc[0]->node_type == ngran_gNB_CU) {
    // gNB-CU UE F1AP ID List
    // C-ifCUDUseparated
    ue_id.gnb.gnb_cu_ue_f1ap_lst_len = 1;
    ue_id.gnb.gnb_cu_ue_f1ap_lst = calloc(ue_id.gnb.gnb_cu_ue_f1ap_lst_len, sizeof(uint32_t));
    assert(ue_id.gnb.gnb_cu_ue_f1ap_lst != NULL && "Memory exhausted");

    for(size_t i = 0; i < ue_id.gnb.gnb_cu_ue_f1ap_lst_len; i++) {
      ue_id.gnb.gnb_cu_ue_f1ap_lst[i] = rrc_ue_context->rrc_ue_id;
    }
  } else if (RC.nrrrc[0]->node_type == ngran_gNB_CUCP) {
    //gNB-CU-CP UE E1AP ID List
    //C-ifCPUPseparated
    ue_id.gnb.gnb_cu_cp_ue_e1ap_lst_len = 1;
    ue_id.gnb.gnb_cu_cp_ue_e1ap_lst = calloc(ue_id.gnb.gnb_cu_cp_ue_e1ap_lst_len, sizeof(uint32_t));
    assert(ue_id.gnb.gnb_cu_cp_ue_e1ap_lst != NULL && "Memory exhausted");

    for(size_t i = 0; i < ue_id.gnb.gnb_cu_cp_ue_e1ap_lst_len; i++) {
      ue_id.gnb.gnb_cu_cp_ue_e1ap_lst[i] = rrc_ue_context->rrc_ue_id;
    }
  } else if (RC.nrrrc[0]->node_type == ngran_gNB_DU) {
    assert(false && "RRC state change cannot be triggered from DU\n");
  }

  #elif defined (NGRAN_GNB_CUUP)
  assert(false && "RRC state change cannot be triggered from CU-UP\n");

  #endif

  ue_id.gnb.amf_ue_ngap_id = rrc_ue_context->rrc_ue_id;
  ue_id.gnb.guami.amf_ptr = rrc_ue_context->ue_guami.amf_pointer;
  ue_id.gnb.guami.amf_region_id = rrc_ue_context->ue_guami.amf_region_id;
  ue_id.gnb.guami.amf_set_id = rrc_ue_context->ue_guami.amf_set_id;
  ue_id.gnb.guami.plmn_id.mcc = rrc_ue_context->ue_guami.mcc;
  ue_id.gnb.guami.plmn_id.mnc = rrc_ue_context->ue_guami.mnc;
  ue_id.gnb.guami.plmn_id.mnc_digit_len = rrc_ue_context->ue_guami.mnc_len;

  return ue_id;
}

static seq_ran_param_t fill_rrc_state_change_seq_ran(const rc_sm_rrc_state_e rrc_state)
{
  seq_ran_param_t seq_ran_param = {0};

  seq_ran_param.ran_param_id = RRC_STATE_CHANGED_TO_E2SM_RC_RAN_PARAM_ID;
  seq_ran_param.ran_param_val.type = ELEMENT_KEY_FLAG_FALSE_RAN_PARAMETER_VAL_TYPE;
  seq_ran_param.ran_param_val.flag_false = calloc(1, sizeof(ran_parameter_value_t));
  assert(seq_ran_param.ran_param_val.flag_false != NULL && "Memory exhausted");
  seq_ran_param.ran_param_val.flag_false->type = INTEGER_RAN_PARAMETER_VALUE;
  seq_ran_param.ran_param_val.flag_false->int_ran = rrc_state;

  return seq_ran_param;
}

static rc_ind_data_t* fill_ue_rrc_state_change(const gNB_RRC_UE_t *rrc_ue_context, const rc_sm_rrc_state_e rrc_state)
{
  rc_ind_data_t* rc_ind = calloc(1, sizeof(rc_ind_data_t));
  assert(rc_ind != NULL && "Memory exhausted");

  // Generate Indication Header
  rc_ind->hdr.format = FORMAT_1_E2SM_RC_IND_HDR;
  rc_ind->hdr.frmt_1.ev_trigger_id = NULL;

  // Generate Indication Message
  rc_ind->msg.format = FORMAT_2_E2SM_RC_IND_MSG;

  //Sequence of UE Identifier
  //[1-65535]
  rc_ind->msg.frmt_2.sz_seq_ue_id = 1;
  rc_ind->msg.frmt_2.seq_ue_id = calloc(rc_ind->msg.frmt_2.sz_seq_ue_id, sizeof(seq_ue_id_t));
  assert(rc_ind->msg.frmt_2.seq_ue_id != NULL && "Memory exhausted");

  // UE ID
  // Mandatory
  // 9.3.10
  rc_ind->msg.frmt_2.seq_ue_id[0].ue_id = fill_ue_id_data(rrc_ue_context);

  // Sequence of
  // RAN Parameter
  // [1- 65535]
  rc_ind->msg.frmt_2.seq_ue_id[0].sz_seq_ran_param = 1;
  rc_ind->msg.frmt_2.seq_ue_id[0].seq_ran_param = calloc(rc_ind->msg.frmt_2.seq_ue_id[0].sz_seq_ran_param, sizeof(seq_ran_param_t));
  assert(rc_ind->msg.frmt_2.seq_ue_id[0].seq_ran_param != NULL && "Memory exhausted");
  rc_ind->msg.frmt_2.seq_ue_id[0].seq_ran_param[0] = fill_rrc_state_change_seq_ran(rrc_state);

  return rc_ind;
}

void signal_rrc_state_changed_to(const gNB_RRC_UE_t *rrc_ue_context, const rc_sm_rrc_state_e rrc_state)
{
  pthread_mutex_lock(&rc_mutex);
  if (rc_subs_data.rb[RRC_STATE_CHANGED_TO_E2SM_RC_RAN_PARAM_ID].rbh_root == NULL) {
    pthread_mutex_unlock(&rc_mutex);
    return;
  }

  struct ric_req_id_s *node;
  RB_FOREACH(node, ric_id_2_param_id_trees, &rc_subs_data.rb[RRC_STATE_CHANGED_TO_E2SM_RC_RAN_PARAM_ID]) {
    rc_ind_data_t* rc_ind_data = fill_ue_rrc_state_change(rrc_ue_context, rrc_state);

    // Needs review: memory ownership of the type rc_ind_data_t is transferred to the E2 Agent. Bad
    async_event_agent_api(node->ric_req_id, rc_ind_data);
    printf( "Event for RIC Req ID %u generated\n", node->ric_req_id);
  }

  pthread_mutex_unlock(&rc_mutex);
}

static void free_aperiodic_subscription(uint32_t ric_req_id)
{
  remove_rc_subs_data(&rc_subs_data, ric_req_id);
}

sm_ag_if_ans_t write_subs_rc_sm(void const* src)
{
  assert(src != NULL); // && src->type == RAN_CTRL_SUBS_V1_03);

  wr_rc_sub_data_t* wr_rc = (wr_rc_sub_data_t*)src;

  assert(wr_rc->rc.ad != NULL && "Cannot be NULL");

  // 9.2.1.2  RIC ACTION DEFINITION IE
  switch (wr_rc->rc.ad->format) {
    case FORMAT_1_E2SM_RC_ACT_DEF: {
      // Parameters to be Reported List
      // [1-65535]
      const uint32_t ric_req_id = wr_rc->ric_req_id;
      arr_ran_param_id_t* arr_ran_param_id = calloc(1, sizeof(arr_ran_param_id_t));
      assert(arr_ran_param_id != NULL && "Memory exhausted");
      arr_ran_param_id->len = wr_rc->rc.ad->frmt_1.sz_param_report_def;
      arr_ran_param_id->ran_param_id = calloc(arr_ran_param_id->len, sizeof(ran_param_id_e));

      const size_t sz = arr_ran_param_id->len;
      for(size_t i = 0; i < sz; i++) {
        arr_ran_param_id->ran_param_id[i] = wr_rc->rc.ad->frmt_1.param_report_def[i].ran_param_id;
      }
      insert_rc_subs_data(&rc_subs_data, ric_req_id, arr_ran_param_id);
      break;
    }

    default:
      AssertFatal(wr_rc->rc.ad->format == FORMAT_1_E2SM_RC_ACT_DEF, "Action Definition Format %d not yet implemented", wr_rc->rc.ad->format);
  }

  sm_ag_if_ans_t ans = {.type = SUBS_OUTCOME_SM_AG_IF_ANS_V0};
  ans.subs_out.type = APERIODIC_SUBSCRIPTION_FLRC;
  ans.subs_out.aper.free_aper_subs = free_aperiodic_subscription;

  return ans;
}


#if defined (NGRAN_GNB_DU)
static int add_mod_dl_slice(int mod_id,
                            slice_algorithm_e current_algo,
                            int id,
                            uint8_t sst,
                            uint32_t sd,
                            char* label,
                            int64_t pct_reserved)
{
  nr_pp_impl_param_dl_t *dl = &RC.nrmac[mod_id]->pre_processor_dl;
  void *params = NULL;
  char *slice_algo = NULL;
  if (current_algo == NVS_SLICING) {
    params = malloc(sizeof(nvs_nr_slice_param_t));
    if (!params) return -1;
    // use SLICE_SM_NVS_V0_CAPACITY by default
    slice_algo = strdup("NVS_CAPACITY");
    ((nvs_nr_slice_param_t *)params)->type = NVS_RES;
    float remain_pct = 1-((nvs_nr_slice_param_t *)dl->slices->s[0]->algo_data)->pct_reserved;
    ((nvs_nr_slice_param_t *)params)->pct_reserved = pct_reserved/100.0*remain_pct;
  } else {
    assert(0 != 0 && "Unknow current_algo");
  }

  void *algo = &dl->dl_algo;
  char *l = NULL;
  if (label)
    l = strdup(label);
  nssai_t nssai = {.sst = sst, .sd = sd};
  LOG_W(NR_MAC, "add DL slice id %d, label %s, slice sched algo %s, pct_reserved %.2f, ue sched algo %s\n", id, l, slice_algo, ((nvs_nr_slice_param_t *)params)->pct_reserved, dl->dl_algo.name);
  return dl->addmod_slice(dl->slices, id, nssai, l, algo, params);
}

static void set_new_dl_slice_algo(int mod_id, int algo)
{
  gNB_MAC_INST *nrmac = RC.nrmac[mod_id];
  assert(nrmac);

  nr_pp_impl_param_dl_t dl = nrmac->pre_processor_dl;
  switch (algo) {
    case NVS_SLICING:
      nrmac->pre_processor_dl = nvs_nr_dl_init(mod_id);
      break;
    default:
      nrmac->pre_processor_dl.algorithm = 0;
      nrmac->pre_processor_dl = nr_init_fr1_dlsch_preprocessor(0); // assume CC_id = 0
      nrmac->pre_processor_dl.slices = NULL;
      break;
  }
  if (dl.slices)
    dl.destroy(&dl.slices);
  if (dl.dl_algo.data)
    dl.dl_algo.unset(&dl.dl_algo.data);
}

static char* copy_bytearr_to_str(const byte_array_t* ba)
{
  if (ba->len < 1)
    return NULL;

  char* str = calloc(ba->len + 1, sizeof(char));
  assert(str != NULL && "memory exhausted\n");
  memcpy(str, ba->buf, ba->len);
  str[ba->len] = '\0';
  return str;
}

static bool nssai_matches(nssai_t a_nssai, uint8_t b_sst, const uint32_t *b_sd)
{
  AssertFatal(b_sd == NULL || *b_sd <= 0xffffff, "illegal SD %d\n", *b_sd);
  if (b_sd == NULL) {
    return a_nssai.sst == b_sst && a_nssai.sd == 0xffffff;
  } else {
    return a_nssai.sst == b_sst && a_nssai.sd == *b_sd;
  }
}

static bool add_mod_rc_slice(int mod_id, size_t slices_len, ran_param_list_t* lst)
{
  gNB_MAC_INST *nrmac = RC.nrmac[mod_id];
  assert(nrmac);

  int current_algo = nrmac->pre_processor_dl.algorithm;
  // use NVS algorithm by default
  int new_algo = NVS_SLICING;

  pthread_mutex_lock(&nrmac->UE_info.mutex);
  if (current_algo != new_algo) {
    set_new_dl_slice_algo(mod_id, new_algo);
    current_algo = new_algo;
    if (new_algo > 0)
      LOG_D(NR_MAC, "set new algorithm %d\n", current_algo);
    else
      LOG_W(NR_MAC, "reset slicing algorithm as NONE\n");
  }

  for (size_t i = 0; i < slices_len; ++i) {
    lst_ran_param_t* RRM_Policy_Ratio_Group = &lst->lst_ran_param[i];
    //Bug in rc_enc_asn.c:1003, asn didn't define ran_param_id for lst_ran_param_t...
    //assert(RRM_Policy_Ratio_Group->ran_param_id == RRM_Policy_Ratio_Group_8_4_3_6 && "wrong RRM_Policy_Ratio_Group id");
    assert(RRM_Policy_Ratio_Group->ran_param_struct.sz_ran_param_struct == 4 && "wrong RRM_Policy_Ratio_Group->ran_param_struct.sz_ran_param_struct");
    assert(RRM_Policy_Ratio_Group->ran_param_struct.ran_param_struct != NULL && "NULL RRM_Policy_Ratio_Group->ran_param_struct.ran_param_struct");

    seq_ran_param_t* RRM_Policy = &RRM_Policy_Ratio_Group->ran_param_struct.ran_param_struct[0];
    assert(RRM_Policy->ran_param_id == RRM_Policy_8_4_3_6 && "wrong RRM_Policy id");
    assert(RRM_Policy->ran_param_val.type == STRUCTURE_RAN_PARAMETER_VAL_TYPE && "wrong RRM_Policy type");
    assert(RRM_Policy->ran_param_val.strct != NULL && "NULL RRM_Policy->ran_param_val.strct");
    assert(RRM_Policy->ran_param_val.strct->sz_ran_param_struct == 1 && "wrong RRM_Policy->ran_param_val.strct->sz_ran_param_struct");
    assert(RRM_Policy->ran_param_val.strct->ran_param_struct != NULL && "NULL RRM_Policy->ran_param_val.strct->ran_param_struct");

    seq_ran_param_t* RRM_Policy_Member_List = &RRM_Policy->ran_param_val.strct->ran_param_struct[0];
    assert(RRM_Policy_Member_List->ran_param_id == RRM_Policy_Member_List_8_4_3_6 && "wrong RRM_Policy_Member_List id");
    assert(RRM_Policy_Member_List->ran_param_val.type == LIST_RAN_PARAMETER_VAL_TYPE && "wrong RRM_Policy_Member_List type");
    assert(RRM_Policy_Member_List->ran_param_val.lst != NULL && "NULL RRM_Policy_Member_List->ran_param_val.lst");
    assert(RRM_Policy_Member_List->ran_param_val.lst->sz_lst_ran_param == 1 && "wrong RRM_Policy_Member_List->ran_param_val.lst->sz_lst_ran_param");
    assert(RRM_Policy_Member_List->ran_param_val.lst->lst_ran_param != NULL && "NULL RRM_Policy_Member_List->ran_param_val.lst->lst_ran_param");

    lst_ran_param_t* RRM_Policy_Member = &RRM_Policy_Member_List->ran_param_val.lst->lst_ran_param[0];
    //Bug in rc_enc_asn.c:1003, asn didn't define ran_param_id for lst_ran_param_t ...
    //assert(RRM_Policy_Member->ran_param_id == RRM_Policy_Member_8_4_3_6 && "wrong RRM_Policy_Member id");
    assert(RRM_Policy_Member->ran_param_struct.sz_ran_param_struct == 2 && "wrong RRM_Policy_Member->ran_param_struct.sz_ran_param_struct");
    assert(RRM_Policy_Member->ran_param_struct.ran_param_struct != NULL && "NULL RRM_Policy_Member->ran_param_struct.ran_param_struct");

    seq_ran_param_t* PLMN_Identity = &RRM_Policy_Member->ran_param_struct.ran_param_struct[0];
    assert(PLMN_Identity->ran_param_id == PLMN_Identity_8_4_3_6 && "wrong PLMN_Identity id");
    assert(PLMN_Identity->ran_param_val.type == ELEMENT_KEY_FLAG_FALSE_RAN_PARAMETER_VAL_TYPE && "wrong PLMN_Identity type");
    assert(PLMN_Identity->ran_param_val.flag_false != NULL && "NULL PLMN_Identity->ran_param_val.flag_false");
    assert(PLMN_Identity->ran_param_val.flag_false->type == OCTET_STRING_RAN_PARAMETER_VALUE && "wrong PLMN_Identity->ran_param_val.flag_false->type");
    ///// GET RC PLMN ////
    char* plmn_str = copy_bytearr_to_str(&PLMN_Identity->ran_param_val.flag_false->octet_str_ran);
    int RC_mnc, RC_mcc = 0;
    if (strlen(plmn_str) == 6)
      sscanf(plmn_str, "%3d%2d", &RC_mcc, &RC_mnc);
    else
      sscanf(plmn_str, "%3d%3d", &RC_mcc, &RC_mnc);
    LOG_D(NR_MAC, "RC PLMN %s, MCC %d, MNC %d\n", plmn_str, RC_mcc, RC_mnc);
    free(plmn_str);

    seq_ran_param_t* S_NSSAI = &RRM_Policy_Member->ran_param_struct.ran_param_struct[1];
    assert(S_NSSAI->ran_param_id == S_NSSAI_8_4_3_6 && "wrong S_NSSAI id");
    assert(S_NSSAI->ran_param_val.type == STRUCTURE_RAN_PARAMETER_VAL_TYPE && "wrong S_NSSAI type");
    assert(S_NSSAI->ran_param_val.strct != NULL && "NULL S_NSSAI->ran_param_val.strct");
    assert(S_NSSAI->ran_param_val.strct->sz_ran_param_struct == 2 && "wrong S_NSSAI->ran_param_val.strct->sz_ran_param_struct");
    assert(S_NSSAI->ran_param_val.strct->ran_param_struct != NULL && "NULL S_NSSAI->ran_param_val.strct->ran_param_struct");

    seq_ran_param_t* SST = &S_NSSAI->ran_param_val.strct->ran_param_struct[0];
    assert(SST->ran_param_id == SST_8_4_3_6 && "wrong SST id");
    assert(SST->ran_param_val.type == ELEMENT_KEY_FLAG_FALSE_RAN_PARAMETER_VAL_TYPE && "wrong SST type");
    assert(SST->ran_param_val.flag_false != NULL && "NULL SST->ran_param_val.flag_false");
    assert(SST->ran_param_val.flag_false->type == OCTET_STRING_RAN_PARAMETER_VALUE && "wrong SST->ran_param_val.flag_false type");
    seq_ran_param_t* SD = &S_NSSAI->ran_param_val.strct->ran_param_struct[1];
    assert(SD->ran_param_id == SD_8_4_3_6 && "wrong SD id");
    assert(SD->ran_param_val.type == ELEMENT_KEY_FLAG_FALSE_RAN_PARAMETER_VAL_TYPE && "wrong SD type");
    assert(SD->ran_param_val.flag_false != NULL && "NULL SD->ran_param_val.flag_false");
    assert(SD->ran_param_val.flag_false->type == OCTET_STRING_RAN_PARAMETER_VALUE && "wrong SD->ran_param_val.flag_false type");
    ///// GET RC NSSAI ////
    char* rc_sst_str = copy_bytearr_to_str(&SST->ran_param_val.flag_false->octet_str_ran);
    uint8_t RC_sst = atoi(rc_sst_str);
    char* rc_sd_str = copy_bytearr_to_str(&SD->ran_param_val.flag_false->octet_str_ran);
    uint32_t RC_sd = atoi(rc_sd_str);
    LOG_D(NR_MAC, "RC (oct) SST %s, SD %s -> (uint) SST %d, SD %d\n", rc_sst_str, rc_sd_str, RC_sst, RC_sd);
    ///// SLICE LABEL NAME /////
    char* sst_str = "SST";
    char* sd_str = "SD";
    size_t label_nssai_len = strlen(sst_str) + strlen(rc_sst_str) + strlen(sd_str) + strlen(rc_sd_str) + 1;
    char* label_nssai = (char*)malloc(label_nssai_len);
    assert(label_nssai != NULL && "Memory exhausted");
    sprintf(label_nssai, "%s%s%s%s", sst_str, rc_sst_str, sd_str, rc_sd_str);
    free(rc_sst_str);
    free(rc_sd_str);

    ///// SLICE NVS CAP /////
    seq_ran_param_t* Min_PRB_Policy_Ratio = &RRM_Policy_Ratio_Group->ran_param_struct.ran_param_struct[1];
    assert(Min_PRB_Policy_Ratio->ran_param_id == Min_PRB_Policy_Ratio_8_4_3_6 && "wrong Min_PRB_Policy_Ratio id");
    assert(Min_PRB_Policy_Ratio->ran_param_val.type == ELEMENT_KEY_FLAG_FALSE_RAN_PARAMETER_VAL_TYPE && "wrong Min_PRB_Policy_Ratio type");
    assert(Min_PRB_Policy_Ratio->ran_param_val.flag_false != NULL && "NULL Min_PRB_Policy_Ratio->ran_param_val.flag_false");
    assert(Min_PRB_Policy_Ratio->ran_param_val.flag_false->type == INTEGER_RAN_PARAMETER_VALUE && "wrong Min_PRB_Policy_Ratio->ran_param_val.flag_false type");
    int64_t nvs_cap = Min_PRB_Policy_Ratio->ran_param_val.flag_false->int_ran;
    LOG_I(NR_MAC, "configure slice %ld, label %s, Min_PRB_Policy_Ratio %ld\n", i, label_nssai, nvs_cap);
    // TODO: could be extended to support max prb ratio and dedicated prb ratio in the MAC scheduling algorithm
    //seq_ran_param_t* Dedicated_PRB_Policy_Ratio = &RRM_Policy_Ratio_Group->ran_param_struct.ran_param_struct[3];
    //assert(Dedicated_PRB_Policy_Ratio->ran_param_id == Dedicated_PRB_Policy_Ratio_8_4_3_6 && "wrong Dedicated_PRB_Policy_Ratio id");
    //assert(Dedicated_PRB_Policy_Ratio->ran_param_val.type == ELEMENT_KEY_FLAG_FALSE_RAN_PARAMETER_VAL_TYPE && "wrong Dedicated_PRB_Policy_Ratio type");
    //assert(Dedicated_PRB_Policy_Ratio->ran_param_val.flag_false != NULL && "NULL Dedicated_PRB_Policy_Ratio->ran_param_val.flag_false");
    //assert(Dedicated_PRB_Policy_Ratio->ran_param_val.flag_false->type == INTEGER_RAN_PARAMETER_VALUE && "wrong Dedicated_PRB_Policy_Ratio->ran_param_val.flag_false type");
    //int64_t dedicated_prb_ratio = Dedicated_PRB_Policy_Ratio->ran_param_val.flag_false->int_ran;
    //LOG_I(NR_MAC, "configure slice %ld, label %s, Dedicated_PRB_Policy_Ratio %ld\n", i, label_nssai, dedicated_prb_ratio);
    //seq_ran_param_t* Max_PRB_Policy_Ratio = &RRM_Policy_Ratio_Group->ran_param_struct.ran_param_struct[2];
    //assert(Max_PRB_Policy_Ratio->ran_param_id == Max_PRB_Policy_Ratio_8_4_3_6 && "wrong Max_PRB_Policy_Ratio id");
    //assert(Max_PRB_Policy_Ratio->ran_param_val.type == ELEMENT_KEY_FLAG_FALSE_RAN_PARAMETER_VAL_TYPE && "wrong Max_PRB_Policy_Ratio type");
    //assert(Max_PRB_Policy_Ratio->ran_param_val.flag_false != NULL && "NULL Max_PRB_Policy_Ratio->ran_param_val.flag_false");
    //assert(Max_PRB_Policy_Ratio->ran_param_val.flag_false->type == INTEGER_RAN_PARAMETER_VALUE && "wrong Max_PRB_Policy_Ratio->ran_param_val.flag_false type");
    //int64_t max_prb_ratio = Max_PRB_Policy_Ratio->ran_param_val.flag_false->int_ran;
    //LOG_I(NR_MAC, "configure slice %ld, label %s, Max_PRB_Policy_Ratio %ld\n", i, label_nssai, max_prb_ratio);

    ///// ADD SLICE /////
    const int rc = add_mod_dl_slice(mod_id, current_algo, i+1, RC_sst, RC_sd, label_nssai, (float)nvs_cap);
    free(label_nssai);
    if (rc < 0) {
      pthread_mutex_unlock(&nrmac->UE_info.mutex);
      LOG_E(NR_MAC, "error code %d while updating slices\n", rc);
      return false;
    }

    /// ASSOC SLICE ///
    if (nrmac->pre_processor_dl.algorithm <= 0)
      LOG_E(NR_MAC, "current slice algo is NONE, no UE can be associated\n");

    if (nrmac->UE_info.list[0] == NULL)
      LOG_E(NR_MAC, "no UE connected\n");


    nr_pp_impl_param_dl_t *dl = &RC.nrmac[mod_id]->pre_processor_dl;
    NR_UEs_t *UE_info = &RC.nrmac[mod_id]->UE_info;
    UE_iterator(UE_info->list, UE) {
      rnti_t rnti = UE->rnti;
      NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
      long lcid = 0;
      for (int l = 0; l < sched_ctrl->dl_lc_num; ++l) {
        lcid = sched_ctrl->dl_lc_ids[l];
        LOG_D(NR_MAC, "l %d, lcid %ld, sst %d, sd %d\n", l, lcid, sched_ctrl->dl_lc_nssai[lcid].sst, sched_ctrl->dl_lc_nssai[lcid].sd);
        if (nssai_matches(sched_ctrl->dl_lc_nssai[lcid], RC_sst, &RC_sd)) {
          rrc_gNB_ue_context_t* rrc_ue_context_list = rrc_gNB_get_ue_context_by_rnti_any_du(RC.nrrrc[mod_id], rnti);
          uint16_t UE_mcc = rrc_ue_context_list->ue_context.ue_guami.mcc;
          uint16_t UE_mnc = rrc_ue_context_list->ue_context.ue_guami.mnc;

          uint8_t UE_sst = sched_ctrl->dl_lc_nssai[lcid].sst;
          uint32_t UE_sd = sched_ctrl->dl_lc_nssai[lcid].sd;
          LOG_D(NR_MAC, "UE: mcc %d mnc %d, sst %d sd %d, RC: mcc %d mnc %d, sst %d sd %d\n",
                UE_mcc, UE_mnc, UE_sst, UE_sd, RC_mcc, RC_mnc, RC_sst, RC_sd);

          if (UE_mcc == RC_mcc && UE_mnc == RC_mnc && UE_sst == RC_sst && UE_sd == RC_sd) {
            dl->add_UE(dl->slices, UE);
          } else {
            LOG_W(NR_MAC, "cannot find specified PLMN (mcc %d mnc %d) NSSAI (sst %d sd %d) from the existing UE PLMN (mcc %d mnc %d) NSSAI (sst %d sd %d) \n",
                  RC_mcc, RC_mnc, RC_sst, RC_sd, UE_mcc, UE_mnc, UE_sst, UE_sd);
          }
        }
      }
    }

  }

  pthread_mutex_unlock(&nrmac->UE_info.mutex);
  LOG_D(NR_MAC, "All slices add/mod successfully!\n");
  return true;
}
#endif

sm_ag_if_ans_t write_ctrl_rc_sm(void const* data)
{
  assert(data != NULL);
//  assert(data->type == RAN_CONTROL_CTRL_V1_03 );

  rc_ctrl_req_data_t const* ctrl = (rc_ctrl_req_data_t const*)data;
  LOG_I(NR_MAC, "[E2-Agent]: RC CONTROL rx, RIC Style Type %d, Action ID %d\n", ctrl->hdr.frmt_1.ric_style_type, ctrl->hdr.frmt_1.ctrl_act_id);

  if(ctrl->hdr.format == FORMAT_1_E2SM_RC_CTRL_HDR){
    if(ctrl->hdr.frmt_1.ric_style_type == 1 && ctrl->hdr.frmt_1.ctrl_act_id == 2){
      printf("QoS flow mapping configuration \n");
      e2sm_rc_ctrl_msg_frmt_1_t const* frmt_1 = &ctrl->msg.frmt_1;
      for(size_t i = 0; i < frmt_1->sz_ran_param; ++i){
        seq_ran_param_t const* rp = frmt_1->ran_param;
        if(rp[i].ran_param_id == 1){
          assert(rp[i].ran_param_val.type == ELEMENT_KEY_FLAG_TRUE_RAN_PARAMETER_VAL_TYPE );
          printf("DRB ID %ld \n", rp[i].ran_param_val.flag_true->int_ran);
        } else if(rp[i].ran_param_id == 2){
          assert(rp[i].ran_param_val.type == LIST_RAN_PARAMETER_VAL_TYPE);
          printf("List of QoS Flows to be modified \n");
          for(size_t j = 0; j < ctrl->msg.frmt_1.ran_param[i].ran_param_val.lst->sz_lst_ran_param; ++j){
            lst_ran_param_t const* lrp = rp[i].ran_param_val.lst->lst_ran_param;
            // The following assertion should be true, but there is a bug in the std
            // check src/sm/rc_sm/enc/rc_enc_asn.c:1085 and src/sm/rc_sm/enc/rc_enc_asn.c:984
            // assert(lrp[j].ran_param_id == 3);
            assert(lrp[j].ran_param_struct.ran_param_struct[0].ran_param_id == 4) ;
            assert(lrp[j].ran_param_struct.ran_param_struct[0].ran_param_val.type == ELEMENT_KEY_FLAG_TRUE_RAN_PARAMETER_VAL_TYPE);

            int64_t qfi = lrp[j].ran_param_struct.ran_param_struct[0].ran_param_val.flag_true->int_ran;
            assert(qfi > -1 && qfi < 65);

            assert(lrp[j].ran_param_struct.ran_param_struct[1].ran_param_id == 5);
            assert(lrp[j].ran_param_struct.ran_param_struct[1].ran_param_val.type == ELEMENT_KEY_FLAG_FALSE_RAN_PARAMETER_VAL_TYPE);
            int64_t dir = lrp[j].ran_param_struct.ran_param_struct[1].ran_param_val.flag_false->int_ran;
            assert(dir == 0 || dir == 1);
            printf("qfi = %ld dir %ld \n", qfi, dir);
          }
        }
      }
    } else if (ctrl->hdr.frmt_1.ric_style_type == 2 && ctrl->hdr.frmt_1.ctrl_act_id == Slice_level_PRB_quotal_7_6_3_1) {
#if defined (NGRAN_GNB_DU)
      /// ADD/MOD SLICE ///
      e2sm_rc_ctrl_msg_frmt_1_t const* msg = &ctrl->msg.frmt_1;
      assert(msg->sz_ran_param == 1 && "not support msg->sz_ran_param != 1");
      seq_ran_param_t* RRM_Policy_Ratio_List = &msg->ran_param[0];
      assert(RRM_Policy_Ratio_List->ran_param_id == RRM_Policy_Ratio_List_8_4_3_6 && "wrong RRM_Policy_Ratio_List id");
      assert(RRM_Policy_Ratio_List->ran_param_val.type == LIST_RAN_PARAMETER_VAL_TYPE && "wrong RRM_Policy_Ratio_List type");
      if (RRM_Policy_Ratio_List->ran_param_val.lst) {
        size_t slices_len = RRM_Policy_Ratio_List->ran_param_val.lst->sz_lst_ran_param;
        const int mod_id = 0;
        bool rc = add_mod_rc_slice(mod_id, slices_len, RRM_Policy_Ratio_List->ran_param_val.lst);
        if (!rc)
          LOG_E(NR_MAC, "failed add/mod slices\n");
      } else {
        LOG_I(NR_MAC, "RRM_Policy_Ratio_List->ran_param_val.lst is NULL\n");
      }
#else
      LOG_W(NR_MAC, "NGRAN_GNB_DU is not defined, cannot support Slice_level_PRB_quotal_7_6_3_1\n");
#endif
    }
  }

  sm_ag_if_ans_t ans = {.type = CTRL_OUTCOME_SM_AG_IF_ANS_V0};
  ans.ctrl_out.type = RAN_CTRL_V1_3_AGENT_IF_CTRL_ANS_V0;
  return ans;
}


bool read_rc_sm(void* data)
{
  assert(data != NULL);
//  assert(data->type == RAN_CTRL_STATS_V1_03);
  assert(0!=0 && "Not implemented");

  return true;
}
