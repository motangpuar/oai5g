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

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define TELNETSERVERCODE
#include "telnetsrv.h"

#include "openair2/RRC/NR/nr_rrc_defs.h"
#include "openair2/LAYER2/NR_MAC_gNB/nr_mac_gNB.h"
#include "openair2/RRC/NR/nr_rrc_config.h"
#include "openair2/LAYER2/NR_MAC_gNB/mac_proto.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_oai_api.c"
#include "common/utils/nr/nr_common.h"

#define ERROR_MSG_RET(mSG, aRGS...) do { prnt("FAILURE: " mSG, ##aRGS); return 1; } while (0)

#define ISINITBWP "bwp3gpp:isInitialBwp"
//#define CYCLPREF  "bwp3gpp:cyclicPrefix"
#define NUMRBS    "bwp3gpp:numberOfRBs"
#define STARTRB   "bwp3gpp:startRB"
#define BWPSCS    "bwp3gpp:subCarrierSpacing"

#define SSBFREQ "nrcelldu3gpp:ssbFrequency"
#define ARFCNDL "nrcelldu3gpp:arfcnDL"
#define BWDL    "nrcelldu3gpp:bSChannelBwDL"
#define ARFCNUL "nrcelldu3gpp:arfcnUL"
#define BWUL    "nrcelldu3gpp:bSChannelBwUL"
#define PCI     "nrcelldu3gpp:nRPCI"
#define TAC     "nrcelldu3gpp:nRTAC"
#define MCC     "nrcelldu3gpp:mcc"
#define MNC     "nrcelldu3gpp:mnc"
#define SD      "nrcelldu3gpp:sd"
#define SST     "nrcelldu3gpp:sst"

typedef struct b {
  long int dl;
  long int ul;
} b_t;

static int get_stats(char *buf, int debug, telnet_printfunc_t prnt)
{
  if (buf)
    ERROR_MSG_RET("no parameter allowed\n");

  const gNB_MAC_INST *mac = RC.nrmac[0];
  AssertFatal(mac != NULL, "need MAC\n");

  const gNB_RRC_INST *rrc = RC.nrrrc[0];
  const gNB_RrcConfigurationReq *conf = &rrc->configuration;
  AssertFatal(rrc != NULL, "need RRC\n");

  const NR_ServingCellConfigCommon_t *scc = mac->common_channels[0].ServingCellConfigCommon;
  const NR_FrequencyInfoDL_t *frequencyInfoDL = scc->downlinkConfigCommon->frequencyInfoDL;
  const NR_FrequencyInfoUL_t *frequencyInfoUL = scc->uplinkConfigCommon->frequencyInfoUL;
  frame_type_t frame_type = get_frame_type(*frequencyInfoDL->frequencyBandList.list.array[0], *scc->ssbSubcarrierSpacing);
  const NR_BWP_t *initialDL = &scc->downlinkConfigCommon->initialDownlinkBWP->genericParameters;
  const NR_BWP_t *initialUL = &scc->uplinkConfigCommon->initialUplinkBWP->genericParameters;

  int scs = initialDL->subcarrierSpacing;
  AssertFatal(scs == initialUL->subcarrierSpacing, "different SCS for UL/DL not supported!\n");
  int band = *frequencyInfoDL->frequencyBandList.list.array[0];
  int nrb = frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth;
  AssertFatal(nrb == frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth, "different BW for UL/DL not supported!\n");
  int bw_index = get_supported_band_index(scs, band, nrb);
  int bw_mhz = get_supported_bw_mhz(band > 256 ? FR2 : FR1, bw_index);

  int num_ues = 0;
  UE_iterator((NR_UE_info_t **)mac->UE_info.list, it) {
    num_ues++;
  }

  const mac_stats_t *stat = &mac->mac_stats;
  static mac_stats_t last = {0};
  int diff_used = stat->used_prb_aggregate - last.used_prb_aggregate;
  int diff_total = stat->total_prb_aggregate - last.total_prb_aggregate;
  int load = diff_total > 0 ? 100 * diff_used / diff_total : 0;
  last = *stat;

  static struct timespec tp_last = {0};
  struct timespec tp_now;
  clock_gettime(CLOCK_MONOTONIC, &tp_now);
  size_t diff_msec = (tp_now.tv_sec - tp_last.tv_sec) * 1000 + (tp_now.tv_nsec - tp_last.tv_nsec) / 1000000;
  tp_last = tp_now;

  const int srb_flag = 0;
  const int rb_id = 1;
  static b_t last_total[MAX_MOBILES_PER_GNB] = {0};
  b_t thr[MAX_MOBILES_PER_GNB] = {0};
  int i = 0;
  {
    UE_iterator((NR_UE_info_t **)mac->UE_info.list, it) {
      nr_rlc_statistics_t rlc = {0};
      nr_rlc_get_statistics(it->rnti, srb_flag, rb_id, &rlc);
      // static var last_total: we might have old data, larger than what
      // reports RLC, leading to a huge number -> cut off to zero
      if (last_total[i].dl > rlc.txpdu_bytes)
        last_total[i].dl = rlc.txpdu_bytes;
      if (last_total[i].ul > rlc.rxpdu_bytes)
        last_total[i].ul = rlc.rxpdu_bytes;
      thr[i].dl = (rlc.txpdu_bytes - last_total[i].dl) * 8 / diff_msec;
      thr[i].ul = (rlc.rxpdu_bytes - last_total[i].ul) * 8 / diff_msec;
      last_total[i].dl = rlc.txpdu_bytes;
      last_total[i].ul = rlc.rxpdu_bytes;
      i++;
    }
  }

  prnt("{\n");
    prnt("  \"o1-config\": {\n");

    prnt("    \"BWP\": {\n");
    prnt("      \"dl\": [{\n");
    prnt("        \"" ISINITBWP "\": true,\n");
    //prnt("      \"" CYCLPREF "\": %ld,\n", *initialDL->cyclicPrefix);
    prnt("        \"" NUMRBS "\": %ld,\n", NRRIV2BW(initialDL->locationAndBandwidth, MAX_BWP_SIZE));
    prnt("        \"" STARTRB "\": %ld,\n", NRRIV2PRBOFFSET(initialDL->locationAndBandwidth, MAX_BWP_SIZE));
    prnt("        \"" BWPSCS "\": %ld\n", 15 * (1U << scs));
    prnt("      }],\n");
    prnt("      \"ul\": [{\n");
    prnt("        \"" ISINITBWP "\": true,\n");
    //prnt("      \"" CYCLPREF "\": %ld,\n", *initialUL->cyclicPrefix);
    prnt("        \"" NUMRBS "\": %ld,\n", NRRIV2BW(initialUL->locationAndBandwidth, MAX_BWP_SIZE));
    prnt("        \"" STARTRB "\": %ld,\n", NRRIV2PRBOFFSET(initialUL->locationAndBandwidth, MAX_BWP_SIZE));
    prnt("        \"" BWPSCS "\": %ld\n", 15 * (1U << scs));
    prnt("      }]\n");
    prnt("    },\n");

    prnt("    \"NRCELLDU\": {\n");
    prnt("      \"" SSBFREQ "\": %ld,\n", *scc->downlinkConfigCommon->frequencyInfoDL->absoluteFrequencySSB);
    prnt("      \"" ARFCNDL "\": %ld,\n", frequencyInfoDL->absoluteFrequencyPointA);
    prnt("      \"" BWDL "\": %ld,\n", bw_mhz);
    prnt("      \"" ARFCNUL "\": %ld,\n", frequencyInfoUL->absoluteFrequencyPointA ? *frequencyInfoUL->absoluteFrequencyPointA : frequencyInfoDL->absoluteFrequencyPointA);
    prnt("      \"" BWUL "\": %ld,\n", bw_mhz);
    prnt("      \"" PCI "\": %ld,\n", *scc->physCellId);
    prnt("      \"" TAC "\": %ld,\n", conf->tac);
    prnt("      \"" MCC "\": \"%03d\",\n", conf->mcc[0]);
    prnt("      \"" MNC "\": \"%0*d\",\n", conf->mnc_digit_length[0], conf->mnc[0]);
    prnt("      \"" SD  "\": %d,\n", conf->sd);
    prnt("      \"" SST "\": %d\n", conf->sst);
    prnt("    },\n");
    prnt("    \"device\": {\n");
    prnt("      \"gnbId\": %d,\n", rrc->nr_cellid); // TODO something else, e.g., gnb_id?
    prnt("      \"gnbName\": \"%s\",\n", rrc->node_name);
    prnt("      \"vendor\": \"OpenAirInterface\"\n");
    prnt("    }\n");
    prnt("  },\n");

    prnt("  \"O1-Operational\": {\n");
    prnt("    \"frame-type\": \"%s\",\n", frame_type == TDD ? "tdd" : "fdd");
    prnt("    \"band-number\": %ld,\n", band);
    prnt("    \"num-ues\": %d,\n", num_ues);
    prnt("    \"ues\": [");
    {
      bool first = true;
      UE_iterator((NR_UE_info_t **)mac->UE_info.list, it) {
        if (!first) { prnt(", "); }
        prnt("%d", it->rnti);
        first = false;
      }
    }
    prnt("    ],\n");
    prnt("    \"load\": %d,\n", load);
    prnt("    \"ues-thp\": [\n");
    {
      bool first = true;
      int i = 0;
      UE_iterator((NR_UE_info_t **)mac->UE_info.list, it) {
        if (!first) { prnt(", "); }
        prnt("      {\"rnti\": %d, \"dl\": %ld, \"ul\": %ld}\n", it->rnti, thr[i].dl, thr[i].ul);
        i++;
        first = false;
      }
    }
    prnt("    ]\n");
    prnt("  }\n");
  prnt("}\n");
  prnt("OK\n");
  return 0;
}

static int read_long(const char *buf, const char *end, const char *id, long *val)
{
  const char *curr = buf;
  while (isspace(*curr) && curr < end) // skip leading spaces
    curr++;
  int len = strlen(id);
  if (curr + len >= end)
    return -1;
  if (strncmp(curr, id, len) != 0) // check buf has id
    return -1;
  curr += len;
  while (isspace(*curr) && curr < end) // skip middle spaces
    curr++;
  if (curr >= end)
    return -1;
  int nread = sscanf(curr, "%ld", val);
  if (nread != 1)
    return -1;
  while (isdigit(*curr) && curr < end) // skip all digits read above
    curr++;
  if (curr > end)
    return -1;
  return curr - buf;
}

bool running = true; // in the beginning, the softmodem is started automatically
static int set_config(char *buf, int debug, telnet_printfunc_t prnt)
{
  if (!buf)
    ERROR_MSG_RET("need param: o1 config param1 val1 [param2 val2 ...]\n");
  if (running)
    ERROR_MSG_RET("cannot set parameters while L1 is running\n");
  const char *end = buf + strlen(buf);

  /* we need to update the following fields to change frequency and/or
   * bandwidth:
   * --gNBs.[0].servingCellConfigCommon.[0].absoluteFrequencySSB 620736            -> SSBFREQ
   * --gNBs.[0].servingCellConfigCommon.[0].dl_absoluteFrequencyPointA 620020      -> ARFCNDL
   * --gNBs.[0].servingCellConfigCommon.[0].dl_carrierBandwidth 51                 -> BWDL
   * --gNBs.[0].servingCellConfigCommon.[0].initialDLBWPlocationAndBandwidth 13750 -> NUMRBS + STARTRB
   * --gNBs.[0].servingCellConfigCommon.[0].ul_carrierBandwidth 51                 -> BWUL?
   * --gNBs.[0].servingCellConfigCommon.[0].initialULBWPlocationAndBandwidth 13750 -> ?
   */

  int processed = 0;
  int pos = 0;

  long ssbfreq;
  processed = read_long(buf + pos, end, SSBFREQ, &ssbfreq);
  if (processed < 0)
    ERROR_MSG_RET("could not read " SSBFREQ " at index %d\n", pos + processed);
  pos += processed;
  prnt("setting " SSBFREQ ":   %ld [len %d]\n", ssbfreq, pos);

  long arfcn;
  processed = read_long(buf + pos, end, ARFCNDL, &arfcn);
  if (processed < 0)
    ERROR_MSG_RET("could not read " ARFCNDL " at index %d\n", pos + processed);
  pos += processed;
  prnt("setting " ARFCNDL ":        %ld [len %d]\n", arfcn, pos);

  long bwdl;
  processed = read_long(buf + pos, end, BWDL, &bwdl);
  if (processed < 0)
    ERROR_MSG_RET("could not read " BWDL " at index %d\n", pos + processed);
  pos += processed;
  prnt("setting " BWDL ":  %ld [len %d]\n", bwdl, pos);

  long numrbs;
  processed = read_long(buf + pos, end, NUMRBS, &numrbs);
  if (processed < 0)
    ERROR_MSG_RET("could not read " NUMRBS " at index %d\n", pos + processed);
  pos += processed;
  prnt("setting " NUMRBS ":         %ld [len %d]\n", numrbs, pos);

  long startrb;
  processed = read_long(buf + pos, end, STARTRB, &startrb);
  if (processed < 0)
    ERROR_MSG_RET("could not read " STARTRB " at index %d\n", pos + processed);
  pos += processed;
  prnt("setting " STARTRB ":             %ld [len %d]\n", startrb, pos);

  int locationAndBandwidth = PRBalloc_to_locationandbandwidth0(numrbs, startrb, MAX_BWP_SIZE);
  prnt("inferred locationAndBandwidth:       %d\n", locationAndBandwidth);

  /*
  gNB_RRC_INST *rrc = RC.nrrrc[0];
  NR_ServingCellConfigCommon_t *scc = rrc->carrier.servingcellconfigcommon;
  NR_FrequencyInfoDL_t *frequencyInfoDL = scc->downlinkConfigCommon->frequencyInfoDL;
  NR_BWP_t *initialDL = &scc->downlinkConfigCommon->initialDownlinkBWP->genericParameters;
  NR_FrequencyInfoUL_t *frequencyInfoUL = scc->uplinkConfigCommon->frequencyInfoUL;
  NR_BWP_t *initialUL = &scc->uplinkConfigCommon->initialUplinkBWP->genericParameters;

  //--gNBs.[0].servingCellConfigCommon.[0].absoluteFrequencySSB 620736            -> SSBFREQ
  *scc->downlinkConfigCommon->frequencyInfoDL->absoluteFrequencySSB = ssbfreq;

  // --gNBs.[0].servingCellConfigCommon.[0].dl_absoluteFrequencyPointA 620020      -> ARFCNDL
  frequencyInfoDL->absoluteFrequencyPointA = arfcn;
  AssertFatal(frequencyInfoUL->absoluteFrequencyPointA == NULL, "only handle TDD\n");

  // --gNBs.[0].servingCellConfigCommon.[0].dl_carrierBandwidth 51                 -> BWDL
  frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth = bwdl;

  // --gNBs.[0].servingCellConfigCommon.[0].initialDLBWPlocationAndBandwidth 13750 -> NUMRBS + STARTRB
  initialDL->locationAndBandwidth = locationAndBandwidth;

  // --gNBs.[0].servingCellConfigCommon.[0].ul_carrierBandwidth 51                 -> BWUL?
  // we assume the same BW as DL
  frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth = bwdl;

  // --gNBs.[0].servingCellConfigCommon.[0].initialULBWPlocationAndBandwidth 13750 -> ?
  // we assume same locationAndBandwidth as DL
  initialUL->locationAndBandwidth = locationAndBandwidth;
  */

  prnt("OK\n");
  return 0;
}

extern int8_t threequarter_fs;
extern openair0_config_t openair0_cfg[MAX_CARDS];
static int set_bwconfig(char *buf, int debug, telnet_printfunc_t prnt)
{
  if (running)
    ERROR_MSG_RET("cannot set parameters while L1 is running\n");
  if (!buf)
    ERROR_MSG_RET("need param: o1 bwconfig <BW>\n");

  char *end = NULL;
  if (NULL != (end = strchr(buf, '\n')))
    *end = 0;
  if (NULL != (end = strchr(buf, '\r')))
    *end = 0;

  gNB_MAC_INST *mac = RC.nrmac[0];
  NR_ServingCellConfigCommon_t *scc = mac->common_channels[0].ServingCellConfigCommon;
  NR_FrequencyInfoDL_t *frequencyInfoDL = scc->downlinkConfigCommon->frequencyInfoDL;
  NR_BWP_t *initialDL = &scc->downlinkConfigCommon->initialDownlinkBWP->genericParameters;
  NR_FrequencyInfoUL_t *frequencyInfoUL = scc->uplinkConfigCommon->frequencyInfoUL;
  NR_BWP_t *initialUL = &scc->uplinkConfigCommon->initialUplinkBWP->genericParameters;
  if (strcmp(buf, "40") == 0) {
    *scc->downlinkConfigCommon->frequencyInfoDL->absoluteFrequencySSB = 641280;
    frequencyInfoDL->absoluteFrequencyPointA = 640008;
    AssertFatal(frequencyInfoUL->absoluteFrequencyPointA == NULL, "only handle TDD\n");
    frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth = 106;
    initialDL->locationAndBandwidth = 28875;
    frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth = 106;
    initialUL->locationAndBandwidth = 28875;
    threequarter_fs = 1;
    openair0_cfg[0].threequarter_fs = 1;
  } else if (strcmp(buf, "20") == 0) {
    *scc->downlinkConfigCommon->frequencyInfoDL->absoluteFrequencySSB = 641280;
    frequencyInfoDL->absoluteFrequencyPointA = 640596;
    AssertFatal(frequencyInfoUL->absoluteFrequencyPointA == NULL, "only handle TDD\n");
    frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth = 51;
    initialDL->locationAndBandwidth = 13750;
    frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth = 51;
    initialUL->locationAndBandwidth = 13750;
    threequarter_fs = 0;
    openair0_cfg[0].threequarter_fs = 0;
  } else {
    ERROR_MSG_RET("unhandled option %s\n", buf);
  }

  free(RC.nrmac[0]->sched_ctrlCommon);
  RC.nrmac[0]->sched_ctrlCommon = NULL;


  free_MIB_NR(mac->common_channels[0].mib);
  mac->common_channels[0].mib = get_new_MIB_NR(scc);

  // due to outrightly CRAZY memory handling in get_SIB1_NR(), we need to set
  // some structures to zero to prevent that we shoot ourselves into the foot
  //struct NR_SIB1 *xyz = rrc->carrier.siblock1->message.choice.c1->choice.systemInformationBlockType1;
  //xyz->servingCellConfigCommon = NULL;
  //free_SIB1_NR(rrc->carrier.siblock1);
  const f1ap_served_cell_info_t *info = &mac->f1_config.setup_req->cell[0].info;
  /*
  NR_BCCH_DL_SCH_Message_t *sib1 = get_SIB1_NR(scc, &info->plmn, into->nr_cellid, *info.tac);
  rrc->carrier.SIB1 = calloc(NR_MAX_SIB_LENGTH / 8, sizeof(*rrc->carrier.SIB1));
  AssertFatal(rrc->carrier.SIB1 != NULL, "out of memory\n");
  rrc->carrier.sizeof_SIB1 = encode_SIB1_NR(sib1, rrc->carrier.SIB1, NR_MAX_SIB_LENGTH / 8);
  rrc->carrier.siblock1 = sib1;
  */
  nr_mac_configure_sib1(mac, &info->plmn, info->nr_cellid, *info->tac);

  //nr_mac_config_scc(mac, scc, &mac->radio_config);

  prnt("OK\n");
  return 0;
}

extern int stop_L1L2(module_id_t gnb_id);
static int stop_modem(char *buf, int debug, telnet_printfunc_t prnt)
{
  if (!running)
    ERROR_MSG_RET("cannot stop, nr-softmodem not running\n");

  /* make UEs out of sync and wait 20ms to ensure no PUCCH is scheduled. After
   * a restart, the frame/slot numbers will be different, which "confuses" the
   * scheduler, which has many PUCCH structures filled with expected frame/slot
   * combinations that won't happen. */
  const gNB_MAC_INST *mac = RC.nrmac[0];
  UE_iterator((NR_UE_info_t **)mac->UE_info.list, it) {
    it->UE_sched_ctrl.rrc_processing_timer = 1000;
  }
  usleep(50000);

  stop_L1L2(0);
  running = false;
  prnt("OK\n");
  return 0;
}

extern int start_L1L2(module_id_t gnb_id);
static int start_modem(char *buf, int debug, telnet_printfunc_t prnt)
{
  if (running)
    ERROR_MSG_RET("cannot start, nr-softmodem already running\n");
  start_L1L2(0);
  running = true;
  prnt("OK\n");
  return 0;
}

static telnetshell_cmddef_t o1cmds[] = {
  {"stats", "", get_stats},
  {"config", "[]", set_config},
  {"bwconfig", "", set_bwconfig},
  {"stop_modem", "", stop_modem},
  {"start_modem", "", start_modem},
  {"", "", NULL},
};

static telnetshell_vardef_t o1vars[] = {
  {"", 0, 0, NULL}
};

void add_o1_cmds(void) {
  add_telnetcmd("o1", o1vars, o1cmds);
}
