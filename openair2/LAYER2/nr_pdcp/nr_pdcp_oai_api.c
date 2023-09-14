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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "nr_pdcp_asn1_utils.h"
#include "nr_pdcp_ue_manager.h"
#include "nr_pdcp_timer_thread.h"
#include "NR_RadioBearerConfig.h"
#include "NR_RLC-BearerConfig.h"
#include "NR_RLC-Config.h"
#include "NR_CellGroupConfig.h"
#include "openair2/RRC/NR/nr_rrc_proto.h"
#include <stdint.h>

/* from OAI */
#include "oai_asn1.h"
#include "pdcp.h"
#include "LAYER2/nr_rlc/nr_rlc_oai_api.h"
#include <openair3/ocp-gtpu/gtp_itf.h>
#include "openair2/SDAP/nr_sdap/nr_sdap.h"
#include "nr_pdcp_e1_api.h"
#include "gnb_config.h"
#include "executables/softmodem-common.h"


#define TODO do { \
    printf("%s:%d:%s: todo\n", __FILE__, __LINE__, __FUNCTION__); \
    exit(1); \
  } while (0)


nr_pdcp_ue_manager_t *nr_pdcp_ue_manager;

/* TODO: handle time a bit more properly */
static uint64_t nr_pdcp_current_time;
static int      nr_pdcp_current_time_last_frame;
static int      nr_pdcp_current_time_last_subframe;

/* necessary globals for OAI, not used internally */
hash_table_t  *pdcp_coll_p;
static uint64_t pdcp_optmask;

uint8_t first_dcch = 0;
uint8_t proto_agent_flag = 0;

static ngran_node_t node_type;

/****************************************************************************/
/* rlc_data_req queue - begin                                               */
/****************************************************************************/


#include <pthread.h>

/* NR PDCP and RLC both use "big locks". In some cases a thread may do
 * lock(rlc) followed by lock(pdcp) (typically when running 'rx_sdu').
 * Another thread may first do lock(pdcp) and then lock(rlc) (typically
 * the GTP module calls 'pdcp_data_req' that, in a previous implementation
 * was indirectly calling 'rlc_data_req' which does lock(rlc)).
 * To avoid the resulting deadlock it is enough to ensure that a call
 * to lock(pdcp) will never be followed by a call to lock(rlc). So,
 * here we chose to have a separate thread that deals with rlc_data_req,
 * out of the PDCP lock. Other solutions may be possible.
 * So instead of calling 'rlc_data_req' directly we have a queue and a
 * separate thread emptying it.
 */

typedef struct {
  protocol_ctxt_t ctxt_pP;
  srb_flag_t      srb_flagP;
  MBMS_flag_t     MBMS_flagP;
  rb_id_t         rb_idP;
  mui_t           muiP;
  confirm_t       confirmP;
  sdu_size_t      sdu_sizeP;
  mem_block_t     *sdu_pP;
} rlc_data_req_queue_item;

#define RLC_DATA_REQ_QUEUE_SIZE 10000

typedef struct {
  rlc_data_req_queue_item q[RLC_DATA_REQ_QUEUE_SIZE];
  volatile int start;
  volatile int length;
  pthread_mutex_t m;
  pthread_cond_t c;
} rlc_data_req_queue;

static rlc_data_req_queue q;

static bool pdcp_data_req_drb(protocol_ctxt_t  *ctxt_pP,
                              const rb_id_t rb_id,
                              const mui_t muiP,
                              const confirm_t confirmP,
                              const sdu_size_t sdu_buffer_size,
                              unsigned char *const sdu_buffer);

static void *rlc_data_req_thread(void *_)
{
  int i;

  pthread_setname_np(pthread_self(), "RLC queue");
  while (1) {
    if (pthread_mutex_lock(&q.m) != 0) abort();
    while (q.length == 0)
      if (pthread_cond_wait(&q.c, &q.m) != 0) abort();
    i = q.start;
    if (pthread_mutex_unlock(&q.m) != 0) abort();

    rlc_data_req(&q.q[i].ctxt_pP,
                 q.q[i].srb_flagP,
                 q.q[i].MBMS_flagP,
                 q.q[i].rb_idP,
                 q.q[i].muiP,
                 q.q[i].confirmP,
                 q.q[i].sdu_sizeP,
                 q.q[i].sdu_pP,
                 NULL,
                 NULL);

    if (pthread_mutex_lock(&q.m) != 0) abort();

    q.length--;
    q.start = (q.start + 1) % RLC_DATA_REQ_QUEUE_SIZE;

    if (pthread_cond_signal(&q.c) != 0) abort();
    if (pthread_mutex_unlock(&q.m) != 0) abort();
  }
}

static void init_nr_rlc_data_req_queue(void)
{
  pthread_t t;

  pthread_mutex_init(&q.m, NULL);
  pthread_cond_init(&q.c, NULL);

  if (pthread_create(&t, NULL, rlc_data_req_thread, NULL) != 0) {
    LOG_E(PDCP, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
}

static void enqueue_rlc_data_req(const protocol_ctxt_t *const ctxt_pP,
                                 const srb_flag_t   srb_flagP,
                                 const MBMS_flag_t  MBMS_flagP,
                                 const rb_id_t      rb_idP,
                                 const mui_t        muiP,
                                 confirm_t    confirmP,
                                 sdu_size_t   sdu_sizeP,
                                 mem_block_t *sdu_pP)
{
  int i;
  int logged = 0;

  if (pthread_mutex_lock(&q.m) != 0) abort();
  while (q.length == RLC_DATA_REQ_QUEUE_SIZE) {
    if (!logged) {
      logged = 1;
      LOG_W(PDCP, "%s: rlc_data_req queue is full\n", __FUNCTION__);
    }
    if (pthread_cond_wait(&q.c, &q.m) != 0) abort();
  }

  i = (q.start + q.length) % RLC_DATA_REQ_QUEUE_SIZE;
  q.length++;

  q.q[i].ctxt_pP    = *ctxt_pP;
  q.q[i].srb_flagP  = srb_flagP;
  q.q[i].MBMS_flagP = MBMS_flagP;
  q.q[i].rb_idP     = rb_idP;
  q.q[i].muiP       = muiP;
  q.q[i].confirmP   = confirmP;
  q.q[i].sdu_sizeP  = sdu_sizeP;
  q.q[i].sdu_pP     = sdu_pP;

  if (pthread_cond_signal(&q.c) != 0) abort();
  if (pthread_mutex_unlock(&q.m) != 0) abort();
}

void du_rlc_data_req(const protocol_ctxt_t *const ctxt_pP,
                     const srb_flag_t   srb_flagP,
                     const MBMS_flag_t  MBMS_flagP,
                     const rb_id_t      rb_idP,
                     const mui_t        muiP,
                     confirm_t    confirmP,
                     sdu_size_t   sdu_sizeP,
                     mem_block_t *sdu_pP)
{
  enqueue_rlc_data_req(ctxt_pP,
                       srb_flagP,
                       MBMS_flagP,
                       rb_idP, muiP,
                       confirmP,
                       sdu_sizeP,
                       sdu_pP);
}

/****************************************************************************/
/* rlc_data_req queue - end                                                 */
/****************************************************************************/

/****************************************************************************/
/* pdcp_data_ind thread - begin                                             */
/****************************************************************************/

typedef struct {
  protocol_ctxt_t ctxt_pP;
  srb_flag_t      srb_flagP;
  MBMS_flag_t     MBMS_flagP;
  rb_id_t         rb_id;
  sdu_size_t      sdu_buffer_size;
  mem_block_t     *sdu_buffer;
} pdcp_data_ind_queue_item;

#define PDCP_DATA_IND_QUEUE_SIZE 10000

typedef struct {
  pdcp_data_ind_queue_item q[PDCP_DATA_IND_QUEUE_SIZE];
  volatile int start;
  volatile int length;
  pthread_mutex_t m;
  pthread_cond_t c;
} pdcp_data_ind_queue;

static pdcp_data_ind_queue pq;

static void do_pdcp_data_ind(
  const protocol_ctxt_t *const  ctxt_pP,
  const srb_flag_t srb_flagP,
  const MBMS_flag_t MBMS_flagP,
  const rb_id_t rb_id,
  const sdu_size_t sdu_buffer_size,
  mem_block_t *const sdu_buffer)
{
  nr_pdcp_ue_t *ue;
  nr_pdcp_entity_t *rb = NULL;
  ue_id_t rntiMaybeUEid = ctxt_pP->rntiMaybeUEid;

  if (ctxt_pP->module_id != 0 ||
      //ctxt_pP->enb_flag != 1 ||
      ctxt_pP->instance != 0 ||
      ctxt_pP->eNB_index != 0 ||
      ctxt_pP->brOption != 0) {
    LOG_E(PDCP, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  /** TRACE PDCP PDU */
  nr_pdcp_pkt_info_t pdcp_pkt;
  pdcp_pkt.direction = 0; //PDCP_NR_DIRECTION_UPLINK
  pdcp_pkt.ueid      = rntiMaybeUEid;

  if (ctxt_pP->enb_flag)
    T(T_ENB_PDCP_UL, T_INT(ctxt_pP->module_id), T_INT(rntiMaybeUEid), T_INT(rb_id), T_INT(sdu_buffer_size));

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, rntiMaybeUEid);

  if (srb_flagP == 1) {
    if (rb_id < 1 || rb_id > 2)
      rb = NULL;
    else {
      if (ue != NULL && ue->srb[rb_id -1] != NULL)
      {
        rb = ue->srb[rb_id - 1];
      }
      (rb_id == 1)?(pdcp_pkt.bearerType = 4):(pdcp_pkt.bearerType = 1);
      (rb_id == 1)?(pdcp_pkt.bearerId = 0):(pdcp_pkt.bearerId = 1);
      pdcp_pkt.plane     = 1;
    }
  } else {
    if (rb_id < 1 || rb_id > MAX_DRBS_PER_UE)
      rb = NULL;
    else {
      if (ue != NULL)
      {
        rb = ue->drb[rb_id - 1];
      }
      pdcp_pkt.bearerType = 8;
      pdcp_pkt.bearerId   = rb_id -1;
      pdcp_pkt.plane      = 2;
    }
  }

  if (rb != NULL) {
    pdcp_pkt.seqnum_length = rb->sn_size;
    pdcp_pkt.maci_present = (rb->has_integrity)?1:0;
    pdcp_pkt.ciphering_disabled = (rb->has_ciphering)?1:0;
    pdcp_pkt.sdap_header = (rb->has_sdap_rx)?1:0;
    pdcp_pkt.is_retx = 0;
    pdcp_pkt.pdu_length = sdu_buffer_size;
    rb->recv_pdu(rb, (char *)sdu_buffer->data, sdu_buffer_size);
    LOG_PDCP_P(OAILOG_INFO, "UL_PDCP_PDU", -1, -1, (pdcp_pkt), (unsigned char *)sdu_buffer->data, sdu_buffer_size);
  } else {
    LOG_E(PDCP, "%s:%d:%s: no RB found (rb_id %ld, srb_flag %d)\n",
          __FILE__, __LINE__, __FUNCTION__, rb_id, srb_flagP);
  }

  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);

  free_mem_block(sdu_buffer, __FUNCTION__);
}

static void *pdcp_data_ind_thread(void *_)
{
  int i;

  pthread_setname_np(pthread_self(), "PDCP data ind");
  while (1) {
    if (pthread_mutex_lock(&pq.m) != 0) abort();
    while (pq.length == 0)
      if (pthread_cond_wait(&pq.c, &pq.m) != 0) abort();
    i = pq.start;
    if (pthread_mutex_unlock(&pq.m) != 0) abort();

    do_pdcp_data_ind(&pq.q[i].ctxt_pP,
                     pq.q[i].srb_flagP,
                     pq.q[i].MBMS_flagP,
                     pq.q[i].rb_id,
                     pq.q[i].sdu_buffer_size,
                     pq.q[i].sdu_buffer);

    if (pthread_mutex_lock(&pq.m) != 0) abort();

    pq.length--;
    pq.start = (pq.start + 1) % PDCP_DATA_IND_QUEUE_SIZE;

    if (pthread_cond_signal(&pq.c) != 0) abort();
    if (pthread_mutex_unlock(&pq.m) != 0) abort();
  }
}

static void init_nr_pdcp_data_ind_queue(void)
{
  pthread_t t;

  pthread_mutex_init(&pq.m, NULL);
  pthread_cond_init(&pq.c, NULL);

  if (pthread_create(&t, NULL, pdcp_data_ind_thread, NULL) != 0) {
    LOG_E(PDCP, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
}

static void enqueue_pdcp_data_ind(
  const protocol_ctxt_t *const  ctxt_pP,
  const srb_flag_t srb_flagP,
  const MBMS_flag_t MBMS_flagP,
  const rb_id_t rb_id,
  const sdu_size_t sdu_buffer_size,
  mem_block_t *const sdu_buffer)
{
  int i;
  int logged = 0;

  if (pthread_mutex_lock(&pq.m) != 0) abort();
  while (pq.length == PDCP_DATA_IND_QUEUE_SIZE) {
    if (!logged) {
      logged = 1;
      LOG_W(PDCP, "%s: pdcp_data_ind queue is full\n", __FUNCTION__);
    }
    if (pthread_cond_wait(&pq.c, &pq.m) != 0) abort();
  }

  i = (pq.start + pq.length) % PDCP_DATA_IND_QUEUE_SIZE;
  pq.length++;

  pq.q[i].ctxt_pP         = *ctxt_pP;
  pq.q[i].srb_flagP       = srb_flagP;
  pq.q[i].MBMS_flagP      = MBMS_flagP;
  pq.q[i].rb_id           = rb_id;
  pq.q[i].sdu_buffer_size = sdu_buffer_size;
  pq.q[i].sdu_buffer      = sdu_buffer;

  if (pthread_cond_signal(&pq.c) != 0) abort();
  if (pthread_mutex_unlock(&pq.m) != 0) abort();
}

bool pdcp_data_ind(const protocol_ctxt_t *const  ctxt_pP,
                   const srb_flag_t srb_flagP,
                   const MBMS_flag_t MBMS_flagP,
                   const rb_id_t rb_id,
                   const sdu_size_t sdu_buffer_size,
                   mem_block_t *const sdu_buffer,
                   const uint32_t *const srcID,
                   const uint32_t *const dstID)
{
  enqueue_pdcp_data_ind(ctxt_pP,
                        srb_flagP,
                        MBMS_flagP,
                        rb_id,
                        sdu_buffer_size,
                        sdu_buffer);
  return true;
}

/****************************************************************************/
/* pdcp_data_ind thread - end                                               */
/****************************************************************************/

/****************************************************************************/
/* hacks to be cleaned up at some point - begin                             */
/****************************************************************************/

#include "LAYER2/MAC/mac_extern.h"

static void reblock_tun_socket(void)
{
  extern int nas_sock_fd[];
  int f;

  f = fcntl(nas_sock_fd[0], F_GETFL, 0);
  f &= ~(O_NONBLOCK);
  if (fcntl(nas_sock_fd[0], F_SETFL, f) == -1) {
    LOG_E(PDCP, "reblock_tun_socket failed\n");
    exit(1);
  }
}

static void *enb_tun_read_thread(void *_)
{
  extern int nas_sock_fd[];
  char rx_buf[NL_MAX_PAYLOAD];
  int len;
  protocol_ctxt_t ctxt;
  ue_id_t rntiMaybeUEid;

  int rb_id = 1;
  pthread_setname_np( pthread_self(),"enb_tun_read");

  while (1) {
    len = read(nas_sock_fd[0], &rx_buf, NL_MAX_PAYLOAD);
    if (len == -1) {
      LOG_E(PDCP, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
      exit(1);
    }

    LOG_D(PDCP, "%s(): nas_sock_fd read returns len %d\n", __func__, len);

    nr_pdcp_manager_lock(nr_pdcp_ue_manager);
    const bool has_ue = nr_pdcp_get_first_ue_id(nr_pdcp_ue_manager, &rntiMaybeUEid);
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);

    if (!has_ue) continue;

    ctxt.module_id = 0;
    ctxt.enb_flag = 1;
    ctxt.instance = 0;
    ctxt.frame = 0;
    ctxt.subframe = 0;
    ctxt.eNB_index = 0;
    ctxt.brOption = 0;
    ctxt.rntiMaybeUEid = rntiMaybeUEid;

    uint8_t qfi = 7;
    bool rqi = 0;
    int pdusession_id = 10;

    sdap_data_req(&ctxt, rntiMaybeUEid, SRB_FLAG_NO, rb_id, RLC_MUI_UNDEFINED, RLC_SDU_CONFIRM_NO, len, (unsigned char *)rx_buf, PDCP_TRANSMISSION_MODE_DATA, NULL, NULL, qfi, rqi, pdusession_id);
  }

  return NULL;
}

static void *ue_tun_read_thread(void *_)
{
  extern int nas_sock_fd[];
  char rx_buf[NL_MAX_PAYLOAD];
  int len;
  protocol_ctxt_t ctxt;
  ue_id_t rntiMaybeUEid;
  int has_ue;

  int rb_id = 1;
  pthread_setname_np( pthread_self(),"ue_tun_read"); 
  while (1) {
    len = read(nas_sock_fd[0], &rx_buf, NL_MAX_PAYLOAD);
    if (len == -1) {
      LOG_E(PDCP, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
      exit(1);
    }

    LOG_D(PDCP, "%s(): nas_sock_fd read returns len %d\n", __func__, len);

    nr_pdcp_manager_lock(nr_pdcp_ue_manager);
    has_ue = nr_pdcp_get_first_ue_id(nr_pdcp_ue_manager, &rntiMaybeUEid);
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);

    if (!has_ue) continue;

    ctxt.module_id = 0;
    ctxt.enb_flag = 0;
    ctxt.instance = 0;
    ctxt.frame = 0;
    ctxt.subframe = 0;
    ctxt.eNB_index = 0;
    ctxt.brOption = 0;
    ctxt.rntiMaybeUEid = rntiMaybeUEid;

    bool dc = SDAP_HDR_UL_DATA_PDU;
    extern uint8_t nas_qfi;
    extern uint8_t nas_pduid;

    sdap_data_req(&ctxt, rntiMaybeUEid, SRB_FLAG_NO, rb_id, RLC_MUI_UNDEFINED, RLC_SDU_CONFIRM_NO, len, (unsigned char *)rx_buf, PDCP_TRANSMISSION_MODE_DATA, NULL, NULL, nas_qfi, dc, nas_pduid);
  }

  return NULL;
}

static void start_pdcp_tun_enb(void)
{
  pthread_t t;

  reblock_tun_socket();

  if (pthread_create(&t, NULL, enb_tun_read_thread, NULL) != 0) {
    LOG_E(PDCP, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
}

static void start_pdcp_tun_ue(void)
{
  pthread_t t;

  reblock_tun_socket();

  if (pthread_create(&t, NULL, ue_tun_read_thread, NULL) != 0) {
    LOG_E(PDCP, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
}

void enqueue_sdap_data_req(
    const uint8_t gnb_flag,
    const ue_id_t ue_id,
    const srb_flag_t srb_flag,
    const rb_id_t rb_id,
    const mui_t mui,
    const confirm_t confirm,
    const sdu_size_t sdu_buffer_size,
    unsigned char *const sdu_buffer,
    const pdcp_transmission_mode_t pt_mode,
    const uint8_t qfi,
    const bool rqi,
    const int pdu_sessionId,
    const uint8_t delaySeconds)
{
    protocol_ctxt_t ctxt;
    PROTOCOL_CTXT_SET_BY_MODULE_ID(
        &ctxt,
        0,
        gnb_flag,
        ue_id,
        nr_pdcp_current_time_last_frame,
        nr_pdcp_current_time_last_subframe,
        0);

    MessageDef *message_p;
    uint8_t *message_buffer = itti_malloc(
                      ctxt.enb_flag ? TASK_PDCP_ENB : TASK_PDCP_UE,
                      ctxt.enb_flag ? TASK_PDCP_ENB : TASK_PDCP_UE,
                      sdu_buffer_size);
    memcpy(message_buffer, sdu_buffer, sdu_buffer_size);
    message_p = itti_alloc_new_message(ctxt.enb_flag ? TASK_PDCP_ENB : TASK_PDCP_UE, 0, NR_SDAP_DATA_REQ);
    NR_SDAP_DATA_REQ (message_p).gnb_flag  = gnb_flag;
    NR_SDAP_DATA_REQ (message_p).rb_id     = rb_id;
    NR_SDAP_DATA_REQ (message_p).muip      = mui;
    NR_SDAP_DATA_REQ (message_p).confirmp  = confirm;
    NR_SDAP_DATA_REQ (message_p).sdu_size  = sdu_buffer_size;
    NR_SDAP_DATA_REQ (message_p).sdu_p     = message_buffer;
    NR_SDAP_DATA_REQ (message_p).mode      = pt_mode;
    NR_SDAP_DATA_REQ (message_p).module_id = ctxt.module_id;
    NR_SDAP_DATA_REQ (message_p).rnti      = ue_id;
    NR_SDAP_DATA_REQ (message_p).gNB_index = 0;
    NR_SDAP_DATA_REQ (message_p).qfi = qfi;
    NR_SDAP_DATA_REQ (message_p).rqi = rqi;
    NR_SDAP_DATA_REQ (message_p).pdu_sessionId = pdu_sessionId;
    itti_send_msg_to_task(
      ctxt.enb_flag ? TASK_PDCP_ENB : TASK_PDCP_UE,
      ctxt.instance,
      message_p);
    LOG_I(SDAP, "send NR_SDAP_DATA_REQ to PDCP\n");

}


/****************************************************************************/
/* hacks to be cleaned up at some point - end                               */
/****************************************************************************/

static void enqueue_pdcp_data_req(
  protocol_ctxt_t *ctxt_pP,
  const srb_flag_t srb_flagP,
  const rb_id_t rb_idP,
  const mui_t muiP,
  const confirm_t confirmP,
  const sdu_size_t sdu_buffer_size,
  char *const sdu_buffer,
  const pdcp_transmission_mode_t modeP)
{
  if (!srb_flagP) {
    MessageDef *message_p;
    uint8_t *message_buffer = itti_malloc(
                      ctxt_pP->enb_flag ? TASK_PDCP_ENB : TASK_PDCP_UE,
                      ctxt_pP->enb_flag ? TASK_PDCP_ENB : TASK_PDCP_UE,
                      sdu_buffer_size);
    memcpy(message_buffer, sdu_buffer, sdu_buffer_size);
    message_p = itti_alloc_new_message(ctxt_pP->enb_flag ? TASK_PDCP_ENB : TASK_PDCP_UE, 0, NR_DTCH_DATA_REQ);
    NR_DTCH_DATA_REQ (message_p).frame     = ctxt_pP->frame;
    NR_DTCH_DATA_REQ (message_p).gnb_flag  = ctxt_pP->enb_flag;
    NR_DTCH_DATA_REQ (message_p).rb_id     = rb_idP;
    NR_DTCH_DATA_REQ (message_p).muip      = muiP;
    NR_DTCH_DATA_REQ (message_p).confirmp  = confirmP;
    NR_DTCH_DATA_REQ (message_p).sdu_size  = sdu_buffer_size;
    NR_DTCH_DATA_REQ (message_p).sdu_p     = message_buffer;
    NR_DTCH_DATA_REQ (message_p).mode      = modeP;
    NR_DTCH_DATA_REQ (message_p).module_id = ctxt_pP->module_id;
    NR_DTCH_DATA_REQ (message_p).rnti      = ctxt_pP->rntiMaybeUEid;
    NR_DTCH_DATA_REQ (message_p).gNB_index = ctxt_pP->eNB_index;
    itti_send_msg_to_task(
      ctxt_pP->enb_flag ? TASK_PDCP_ENB : TASK_PDCP_UE,
      ctxt_pP->instance,
      message_p);
    LOG_I(PDCP, "send NR_DTCH_DATA_REQ to PDCP\n");
  } else {
    LOG_E(PDCP, "not implemented for srb flag\n");
      LOG_E(PDCP, "%s:%d:%s: fatal, not implemented\n", __FILE__, __LINE__, __FUNCTION__);
      exit(1);
  }
}

int pdcp_fifo_flush_sdus(const protocol_ctxt_t *const ctxt_pP)
{
  return 0;
}

static void set_node_type() {
  node_type = get_node_type();
}

void pdcp_layer_init(void)
{
  /* hack: be sure to initialize only once */
  static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
  static int initialized = 0;
  if (pthread_mutex_lock(&m) != 0) abort();
  if (initialized) {
    if (pthread_mutex_unlock(&m) != 0) abort();
    return;
  }
  initialized = 1;
  if (pthread_mutex_unlock(&m) != 0) abort();

  nr_pdcp_ue_manager = new_nr_pdcp_ue_manager(1);

  set_node_type();

  if ((RC.nrrrc == NULL) || (!NODE_IS_CU(node_type))) {
    init_nr_rlc_data_req_queue();
  }

  init_nr_pdcp_data_ind_queue();
  nr_pdcp_init_timer_thread(nr_pdcp_ue_manager);
}

#include "nfapi/oai_integration/vendor_ext.h"
#include "executables/lte-softmodem.h"
#include "openair2/RRC/NAS/nas_config.h"

uint64_t nr_pdcp_module_init(uint64_t _pdcp_optmask, int id)
{
  /* hack: be sure to initialize only once */
  static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
  static int initialized = 0;
  if (pthread_mutex_lock(&m) != 0) abort();
  if (initialized) {
    abort();
  }
  initialized = 1;
  if (pthread_mutex_unlock(&m) != 0) abort();

#if 0
  pdcp_optmask = _pdcp_optmask;
  return pdcp_optmask;
#endif
  /* temporary enforce netlink when UE_NAS_USE_TUN is set,
     this is while switching from noS1 as build option
     to noS1 as config option                               */
  if ( _pdcp_optmask & UE_NAS_USE_TUN_BIT) {
    pdcp_optmask = pdcp_optmask | PDCP_USE_NETLINK_BIT ;
  }

  pdcp_optmask = pdcp_optmask | _pdcp_optmask ;
  LOG_I(PDCP, "pdcp init,%s %s\n",
        ((LINK_ENB_PDCP_TO_GTPV1U)?"usegtp":""),
        ((PDCP_USE_NETLINK)?"usenetlink":""));

  if (PDCP_USE_NETLINK) {
    nas_getparams();

    if(UE_NAS_USE_TUN) {
      char *ifsuffix_ue = get_softmodem_params()->nsa ? "nrue" : "ue";
      int num_if = (NFAPI_MODE == NFAPI_UE_STUB_PNF || IS_SOFTMODEM_SIML1 || NFAPI_MODE == NFAPI_MODE_STANDALONE_PNF)? MAX_MOBILES_PER_ENB : 1;
      netlink_init_tun(ifsuffix_ue, num_if, id);
      //Add --nr-ip-over-lte option check for next line
      if (IS_SOFTMODEM_NOS1){
        nas_config(1, 1, !get_softmodem_params()->nsa ? 2 : 3, ifsuffix_ue);
        set_qfi_pduid(7, 10);
      }
      LOG_I(PDCP, "UE pdcp will use tun interface\n");
      start_pdcp_tun_ue();
    } else if(ENB_NAS_USE_TUN) {
      char *ifsuffix_base_s = get_softmodem_params()->nsa ? "gnb" : "enb";
      netlink_init_tun(ifsuffix_base_s, 1, id);
      nas_config(1, 1, 1, ifsuffix_base_s);
      LOG_I(PDCP, "ENB pdcp will use tun interface\n");
      start_pdcp_tun_enb();
    } else {
      LOG_I(PDCP, "pdcp will use kernel modules\n");
      abort();
      netlink_init();
    }
  }
  return pdcp_optmask ;
}

bool pdcp_test_loop = false;/*TODO: should have multiple DRB list and corresponding scalling configuration from nas CLOSE UE LOOP (mode A)*/
void set_pdcp_loopback(bool enable)
{
  pdcp_test_loop = enable;
}

static void deliver_sdu_drb(void *_ue, nr_pdcp_entity_t *entity,
                            char *buf, int size)
{
  nr_pdcp_ue_t *ue = _ue;
  int rb_id;
  int i;

  /*This should use test loop mode instead of the tun flag */
  /* For UE, TS38.509 5.3.4.1 UE test loop mode A operation
      For gNB: report the data to SS */
  if ((entity->is_gnb== GNB_FLAG_NO && pdcp_test_loop)  || (entity->is_gnb== GNB_FLAG_YES && RC.ss.mode >= SS_SOFTMODEM && RC.nr_drb_data_type == DRB_PdcpSdu) ){
    protocol_ctxt_t ctxt;
    PROTOCOL_CTXT_SET_BY_MODULE_ID(
        &ctxt,
        0,
        entity->is_gnb,
        ue->rntiMaybeUEid,
        nr_pdcp_current_time_last_frame,
        nr_pdcp_current_time_last_subframe,
        0);
    LOG_D(PDCP, "%s(): (drb %d) calling enqueue_pdcp_data_req size %d\n", __func__, entity->rb_id, size);
    enqueue_pdcp_data_req(&ctxt, SRB_FLAG_NO, entity->rb_id,
                          RLC_MUI_UNDEFINED, RLC_SDU_CONFIRM_NO,
                          size, buf, PDCP_TRANSMISSION_MODE_DATA);
    return;
  }

  if (IS_SOFTMODEM_NOS1 || UE_NAS_USE_TUN) {
    LOG_D(PDCP, "IP packet received with size %d, to be sent to SDAP interface, UE ID/RNTI: %ld\n", size, ue->rntiMaybeUEid);
    sdap_data_ind(entity->rb_id, entity->is_gnb, entity->has_sdap_rx, entity->pdusession_id, ue->rntiMaybeUEid, buf, size);
  }
  else{
    for (i = 0; i < MAX_DRBS_PER_UE; i++) {
        if (entity == ue->drb[i]) {
          rb_id = i+1;
          goto rb_found;
        }
      }

      LOG_E(PDCP, "%s:%d:%s: fatal, no RB found for UE ID/RNTI %ld\n", __FILE__, __LINE__, __FUNCTION__, ue->rntiMaybeUEid);
      exit(1);

    rb_found:
    {
      LOG_D(PDCP, "%s() (drb %d) sending message to SDAP size %d\n", __func__, rb_id, size);
      sdap_data_ind(rb_id, ue->drb[rb_id - 1]->is_gnb, ue->drb[rb_id - 1]->has_sdap_rx, ue->drb[rb_id - 1]->pdusession_id, ue->rntiMaybeUEid, buf, size);
    }
  }
}

static void deliver_pdu_drb(void *_ue, nr_pdcp_entity_t *entity,
                            char *buf, int size, int sdu_id)
{
  nr_pdcp_ue_t *ue = _ue;
  int rb_id;
  protocol_ctxt_t ctxt;
  int i;
  mem_block_t *memblock;

  for (i = 0; i < MAX_DRBS_PER_UE; i++) {
    if (entity == ue->drb[i]) {
      rb_id = i+1;
      goto rb_found;
    }
  }

  LOG_E(PDCP, "%s:%d:%s: fatal, no RB found for UE ID/RNTI %ld\n", __FILE__, __LINE__, __FUNCTION__, ue->rntiMaybeUEid);
  exit(1);

rb_found:
  ctxt.module_id = 0;
  ctxt.enb_flag = 1;
  ctxt.instance = 0;
  ctxt.frame = 0;
  ctxt.subframe = 0;
  ctxt.eNB_index = 0;
  ctxt.brOption = 0;

  ctxt.rntiMaybeUEid = ue->rntiMaybeUEid;
  if (NODE_IS_CU(node_type)) {
    MessageDef  *message_p = itti_alloc_new_message_sized(TASK_PDCP_ENB, 0,
							  GTPV1U_TUNNEL_DATA_REQ,
							  sizeof(gtpv1u_tunnel_data_req_t)
							  + size
							  + GTPU_HEADER_OVERHEAD_MAX);
    AssertFatal(message_p != NULL, "OUT OF MEMORY");
    gtpv1u_tunnel_data_req_t *req=&GTPV1U_TUNNEL_DATA_REQ(message_p);
    uint8_t *gtpu_buffer_p = (uint8_t*)(req+1);
    memcpy(gtpu_buffer_p+GTPU_HEADER_OVERHEAD_MAX, 
	   buf, size);
    req->buffer        = gtpu_buffer_p;
    req->length        = size;
    req->offset        = GTPU_HEADER_OVERHEAD_MAX;
    req->ue_id = ue->rntiMaybeUEid;
    req->bearer_id = rb_id;
    LOG_I(PDCP, "%s() (drb %d) sending message to gtp size %d\n",
	  __func__, rb_id, size);
    extern instance_t CUuniqInstance;
    itti_send_msg_to_task(TASK_GTPV1_U, CUuniqInstance, message_p);
  } else {
    memblock = get_free_mem_block(size, __FUNCTION__);
    memcpy(memblock->data, buf, size);
    LOG_D(PDCP, "%s(): (drb %d) calling rlc_data_req size %d\n", __func__, rb_id, size);
    //for (i = 0; i < size; i++) printf(" %2.2x", (unsigned char)memblock->data[i]);
    //printf("\n");
    enqueue_rlc_data_req(&ctxt, 0, MBMS_FLAG_NO, rb_id, sdu_id, 0, size, memblock);
  }
}

static void deliver_sdu_srb(void *_ue, nr_pdcp_entity_t *entity,
                            char *buf, int size)
{
  nr_pdcp_ue_t *ue = _ue;
  int srb_id;
  int i;

  for (i = 0; i < sizeofArray(ue->srb) ; i++) {
    if (entity == ue->srb[i]) {
      srb_id = i+1;
      goto srb_found;
    }
  }

  LOG_E(PDCP, "%s:%d:%s: fatal, no SRB found for UE ID/RNTI %ld\n", __FILE__, __LINE__, __FUNCTION__, ue->rntiMaybeUEid);
  exit(1);

srb_found:
  {
      LOG_I(PDCP,"PDCP Sending DCCH_PDU_IND/SS_NRRRC_PDU_IND (msg_Id:%d) to TASK_SS_SRB_GNB \n", SS_NRRRC_PDU_IND);
      MessageDef *message_p = itti_alloc_new_message (TASK_SS_SRB_GNB, 0, SS_NRRRC_PDU_IND);
      if (message_p) {
        /* Populate the message and send to SS */
        SS_NRRRC_PDU_IND (message_p).sdu_size = size;
        SS_NRRRC_PDU_IND (message_p).srb_id = srb_id;
        SS_NRRRC_PDU_IND (message_p).frame = nr_pdcp_current_time_last_frame;
        SS_NRRRC_PDU_IND (message_p).rnti = ue->rntiMaybeUEid;
        SS_NRRRC_PDU_IND (message_p).subframe = nr_pdcp_current_time_last_subframe;
        memset (SS_NRRRC_PDU_IND (message_p).sdu, 0, SDU_SIZE);
        memcpy (SS_NRRRC_PDU_IND (message_p).sdu, buf, size);

        int send_res = itti_send_msg_to_task (TASK_SS_SRB_GNB, 0, message_p);
        if(send_res < 0) {
          LOG_E(PDCP,"Error in sending DCCH_PDU_IND/SS_NRRRC_PDU_IND(msg_Id:%d) to TASK_SS_SRB_GNB\n", SS_NRRRC_PDU_IND);
        }
      }
  }
  {
  if (entity->is_gnb) {
    MessageDef *message_p = itti_alloc_new_message(TASK_PDCP_GNB, 0, F1AP_UL_RRC_MESSAGE);
    AssertFatal(message_p != NULL, "OUT OF MEMORY\n");
    f1ap_ul_rrc_message_t *ul_rrc = &F1AP_UL_RRC_MESSAGE(message_p);
    ul_rrc->rnti = ue->rntiMaybeUEid;
    ul_rrc->srb_id = srb_id;
    ul_rrc->rrc_container = malloc(size);
    AssertFatal(ul_rrc->rrc_container != NULL, "OUT OF MEMORY\n");
    memcpy(ul_rrc->rrc_container, buf, size);
    ul_rrc->rrc_container_length = size;
    itti_send_msg_to_task(TASK_RRC_GNB, 0, message_p);
  } else {
    uint8_t *rrc_buffer_p = itti_malloc(TASK_PDCP_UE, TASK_RRC_NRUE, size);
    AssertFatal(rrc_buffer_p != NULL, "OUT OF MEMORY\n");
    memcpy(rrc_buffer_p, buf, size);
    MessageDef *message_p = itti_alloc_new_message(TASK_PDCP_UE, 0, NR_RRC_DCCH_DATA_IND);
    AssertFatal(message_p != NULL, "OUT OF MEMORY\n");
    NR_RRC_DCCH_DATA_IND(message_p).dcch_index = srb_id;
    NR_RRC_DCCH_DATA_IND(message_p).sdu_p = rrc_buffer_p;
    NR_RRC_DCCH_DATA_IND(message_p).sdu_size = size;
    NR_RRC_DCCH_DATA_IND(message_p).rnti = ue->rntiMaybeUEid;
    itti_send_msg_to_task(TASK_RRC_NRUE, 0, message_p);
    }
  }
}

static void deliver_pdu_srb(void *_ue, nr_pdcp_entity_t *entity,
                            char *buf, int size, int sdu_id)
{
  nr_pdcp_ue_t *ue = _ue;
  int srb_id;
  int i;

  for (i = 0; i < sizeofArray(ue->srb) ; i++) {
    if (entity == ue->srb[i]) {
      srb_id = i+1;
      goto srb_found;
    }
  }

  LOG_E(PDCP, "%s:%d:%s: fatal, no SRB found for UE ID/RNTI %ld\n", __FILE__, __LINE__, __FUNCTION__, ue->rntiMaybeUEid);
  exit(1);

srb_found:
  LOG_D(PDCP, "%s(): (srb %d) calling rlc_data_req size %d\n", __func__, srb_id, size);
  LOG_DUMPMSG(PDCP, DEBUG_PDCP, buf, size, "%s: PDCP => RLC, rb_id=%d: ", __FUNCTION__, srb_id);
  //for (i = 0; i < size; i++) printf(" %2.2x", (unsigned char)memblock->data[i]);
  //printf("\n");
  if ((RC.nrrrc == NULL) || (!NODE_IS_CU(node_type))) {
    if (entity->is_gnb) {
      f1ap_dl_rrc_message_t dl_rrc = {.old_gNB_DU_ue_id = 0xFFFFFF, .rrc_container = (uint8_t *)buf, .rrc_container_length = size, .rnti = ue->rntiMaybeUEid, .srb_id = srb_id};
      gNB_RRC_INST *rrc = RC.nrrrc[0];
      rrc->mac_rrc.dl_rrc_message_transfer(0, &dl_rrc);
    } else { // UE
      mem_block_t *memblock;
      protocol_ctxt_t ctxt = {.module_id = 0, .enb_flag = 1, .instance = 0, .frame = 0, .subframe = 0, .eNB_index = 0, .brOption = 0, .rntiMaybeUEid = ue->rntiMaybeUEid};
      memblock = get_free_mem_block(size, __FUNCTION__);
      memcpy(memblock->data, buf, size);
      enqueue_rlc_data_req(&ctxt, 1, MBMS_FLAG_NO, srb_id, sdu_id, 0, size, memblock);
    }
  } else {
    MessageDef  *message_p = itti_alloc_new_message (TASK_RRC_GNB, 0, F1AP_DL_RRC_MESSAGE);
    uint8_t *message_buffer = itti_malloc (TASK_RRC_GNB, TASK_CU_F1, size);
    memcpy (message_buffer, buf, size);
    F1AP_DL_RRC_MESSAGE (message_p).rrc_container        = message_buffer;
    F1AP_DL_RRC_MESSAGE (message_p).rrc_container_length = size;
    F1AP_DL_RRC_MESSAGE (message_p).gNB_CU_ue_id         = 0;
    F1AP_DL_RRC_MESSAGE (message_p).gNB_DU_ue_id         = 0;
    F1AP_DL_RRC_MESSAGE (message_p).old_gNB_DU_ue_id     = 0xFFFFFFFF; // unknown
    F1AP_DL_RRC_MESSAGE(message_p).rnti = ue->rntiMaybeUEid;
    F1AP_DL_RRC_MESSAGE (message_p).srb_id               = srb_id;
    F1AP_DL_RRC_MESSAGE (message_p).execute_duplication  = 1;
    F1AP_DL_RRC_MESSAGE (message_p).RAT_frequency_priority_information.en_dc = 0;
    itti_send_msg_to_task (TASK_CU_F1, 0, message_p);
    LOG_D(PDCP, "Send F1AP_DL_RRC_MESSAGE with ITTI\n");
  }
}

void pdcp_run(const protocol_ctxt_t *const  ctxt_pP)
{
  MessageDef      *msg_p = NULL;
  int             result = 0;
  protocol_ctxt_t ctxt = {.module_id = 0, .enb_flag = 1, .instance = 0, .rntiMaybeUEid = 0, .frame = -1, .subframe = -1, .eNB_index = 0, .brOption = false};

  SET_LOG_DUMP(DEBUG_PDCP);

  while (1) {
    itti_poll_msg(ctxt_pP->enb_flag ? TASK_PDCP_ENB : TASK_PDCP_UE, &msg_p);
    if (msg_p == NULL){
     break;
    }
    switch (ITTI_MSG_ID(msg_p))
    {
    case RRC_DCCH_DATA_REQ:
      LOG_I(PDCP, "Received RRC_DCCH_DATA_REQ type at PDCP task \n");
      PROTOCOL_CTXT_SET_BY_MODULE_ID(
          &ctxt,
          RRC_DCCH_DATA_REQ(msg_p).module_id,
          RRC_DCCH_DATA_REQ(msg_p).enb_flag,
          RRC_DCCH_DATA_REQ(msg_p).rnti,
          RRC_DCCH_DATA_REQ(msg_p).frame,
          0,
          RRC_DCCH_DATA_REQ(msg_p).eNB_index);
      result = pdcp_data_req(&ctxt,
                             SRB_FLAG_YES,
                             RRC_DCCH_DATA_REQ(msg_p).rb_id,
                             RRC_DCCH_DATA_REQ(msg_p).muip,
                             RRC_DCCH_DATA_REQ(msg_p).confirmp,
                             RRC_DCCH_DATA_REQ(msg_p).sdu_size,
                             RRC_DCCH_DATA_REQ(msg_p).sdu_p,
                             RRC_DCCH_DATA_REQ(msg_p).mode,
                             NULL, NULL);

      if (result != true)
        LOG_E(PDCP, "PDCP data request failed!\n");
      result = itti_free(ITTI_MSG_ORIGIN_ID(msg_p), RRC_DCCH_DATA_REQ(msg_p).sdu_p);
      AssertFatal(result == EXIT_SUCCESS, "Failed to free memory (%d)!\n", result);
      break;

    case SS_DRB_PDU_REQ:
    {
      LOG_I(PDCP, "Received SS_DRB_PDU_REQ type at PDCP task\n");

      instance_t instance = ITTI_MSG_DESTINATION_INSTANCE(msg_p);
      PROTOCOL_CTXT_SET_BY_INSTANCE(&ctxt,
                                    instance,
                                    GNB_FLAG_YES,
                                    SS_DRB_PDU_REQ(msg_p).rnti,
                                    msg_p->ittiMsgHeader.lte_time.frame,
                                    msg_p->ittiMsgHeader.lte_time.slot);
      if(SS_DRB_PDU_REQ(msg_p).data_type == DRB_SdapSdu){
        sdap_data_req(&ctxt,
                        SS_DRB_PDU_REQ(msg_p).rnti,
                        SRB_FLAG_NO,
                        SS_DRB_PDU_REQ(msg_p).drb_id,
                        RLC_MUI_UNDEFINED,
                        RLC_SDU_CONFIRM_NO,
                        SS_DRB_PDU_REQ(msg_p).sdu_size,
                        SS_DRB_PDU_REQ(msg_p).sdu,
                        PDCP_TRANSMISSION_MODE_DATA, NULL, NULL,
                        SS_DRB_PDU_REQ(msg_p).qfi,
                        0,
                        SS_DRB_PDU_REQ(msg_p).pdu_sessionId);

      } else if(SS_DRB_PDU_REQ(msg_p).data_type == DRB_PdcpSdu){
        pdcp_data_req_drb(&ctxt,
                        SS_DRB_PDU_REQ(msg_p).drb_id,
                        0,
                        0,
                        SS_DRB_PDU_REQ(msg_p).sdu_size,
                        SS_DRB_PDU_REQ(msg_p).sdu);
      }
    }
    break;

    case NR_DTCH_DATA_REQ:
    {
      int result = 0;
      LOG_I(PDCP, "Received NR_DTCH_DATA_REQ type at PDCP task \n");
      PROTOCOL_CTXT_SET_BY_MODULE_ID(&ctxt,
                                     NR_DTCH_DATA_REQ(msg_p).module_id,
                                     NR_DTCH_DATA_REQ(msg_p).gnb_flag,
                                     NR_DTCH_DATA_REQ(msg_p).rnti,
                                     NR_DTCH_DATA_REQ(msg_p).frame,
                                     0,
                                     NR_DTCH_DATA_REQ(msg_p).gNB_index);

      if(ctxt_pP->enb_flag) {
        LOG_I(PDCP, "Sending validated PDCP SDU to DBR task at gNB, rb_id=%ld\n", NR_DTCH_DATA_REQ(msg_p).rb_id);

        MessageDef *message_p = itti_alloc_new_message(TASK_SS_DRB, 0, SS_DRB_PDU_IND);
        AssertFatal(message_p != NULL, "Failed to allocate msg\n");

        SS_DRB_PDU_IND(message_p).drb_id = NR_DTCH_DATA_REQ(msg_p).rb_id;
        SS_DRB_PDU_IND(message_p).frame = NR_DTCH_DATA_REQ(msg_p).frame;
        SS_DRB_PDU_IND(message_p).subframe = 0;
        /* SS_DRB_PDU_IND(message_p).physCellId left unset as not critical for the message*/
        SS_DRB_PDU_IND(message_p).sdu_size = NR_DTCH_DATA_REQ(msg_p).sdu_size;
        memcpy(SS_DRB_PDU_IND(message_p).sdu, NR_DTCH_DATA_REQ(msg_p).sdu_p, NR_DTCH_DATA_REQ(msg_p).sdu_size);

        result = itti_send_msg_to_task(TASK_SS_DRB, NR_DTCH_DATA_REQ(msg_p).module_id, message_p);
        if (result < 0) {
          LOG_E(PDCP, "Error in itti_send_msg_to_task!\n");
        }
      } else {
        result = pdcp_data_req(&ctxt,
                            SRB_FLAG_NO,
                            NR_DTCH_DATA_REQ(msg_p).rb_id,
                            NR_DTCH_DATA_REQ(msg_p).muip,
                            NR_DTCH_DATA_REQ(msg_p).confirmp,
                            NR_DTCH_DATA_REQ(msg_p).sdu_size,
                            NR_DTCH_DATA_REQ(msg_p).sdu_p,
                            NR_DTCH_DATA_REQ(msg_p).mode,
                            NULL, NULL);
        if (result != true) {
          LOG_E(PDCP, "PDCP data request failed!\n");
        }
      }
      result = itti_free(ITTI_MSG_ORIGIN_ID(msg_p), NR_DTCH_DATA_REQ(msg_p).sdu_p);
      AssertFatal(result == EXIT_SUCCESS, "Failed to free memory (%d)!\n", result);
    }
    break;

    case NR_SDAP_DATA_REQ:
    {
      int result = 0;
      LOG_I(PDCP, "Received NR_SDAP_DATA_REQ type at PDCP task \n");
      PROTOCOL_CTXT_SET_BY_MODULE_ID(&ctxt,
                                     NR_SDAP_DATA_REQ(msg_p).module_id,
                                     NR_SDAP_DATA_REQ(msg_p).gnb_flag,
                                     NR_SDAP_DATA_REQ(msg_p).rnti,
                                     nr_pdcp_current_time_last_frame,
                                     nr_pdcp_current_time_last_subframe,
                                     NR_SDAP_DATA_REQ(msg_p).gNB_index);


      if(ctxt_pP->enb_flag) {
        if(RC.nr_drb_data_type == DRB_SdapSdu){
          LOG_I(PDCP, "Sending SDAP SDU to DBR task at gNB.\n");

          MessageDef *message_p = itti_alloc_new_message(TASK_SS_DRB, 0, SS_DRB_PDU_IND);
          AssertFatal(message_p != NULL, "Failed to allocate msg\n");
          SS_DRB_PDU_IND(message_p).frame = nr_pdcp_current_time_last_frame;
          SS_DRB_PDU_IND(message_p).subframe = nr_pdcp_current_time_last_subframe;
          SS_DRB_PDU_IND(message_p).data_type = DRB_SdapSdu;
          /*TODO: physCellId shall be filled if multicell through ue_id(rnti) */
          //SS_DRB_PDU_IND(message_p).physCellId =
          SS_DRB_PDU_IND(message_p).sdu_size = NR_SDAP_DATA_REQ(msg_p).sdu_size;
          memcpy(SS_DRB_PDU_IND(message_p).sdu, NR_SDAP_DATA_REQ(msg_p).sdu_p, NR_SDAP_DATA_REQ(msg_p).sdu_size);
          SS_DRB_PDU_IND(message_p).pdu_sessionId = NR_SDAP_DATA_REQ(msg_p).pdu_sessionId;
          SS_DRB_PDU_IND(message_p).qfi = NR_SDAP_DATA_REQ(msg_p).qfi;

          itti_send_msg_to_task(TASK_SS_DRB, 0, message_p);

          result = itti_send_msg_to_task(TASK_SS_DRB, NR_SDAP_DATA_REQ(msg_p).module_id, message_p);
          if (result < 0) {
            LOG_E(PDCP, "Error in itti_send_msg_to_task!\n");
          }
        }
      } else {
        result = sdap_data_req(&ctxt,
                        NR_SDAP_DATA_REQ(msg_p).rnti,
                        SRB_FLAG_NO,
                        NR_SDAP_DATA_REQ(msg_p).rb_id,
                        NR_SDAP_DATA_REQ(msg_p).muip,
                        NR_SDAP_DATA_REQ(msg_p).confirmp,
                        NR_SDAP_DATA_REQ(msg_p).sdu_size,
                        (unsigned char *)NR_SDAP_DATA_REQ(msg_p).sdu_p,
                        NR_SDAP_DATA_REQ(msg_p).mode,
                        NULL,
                        NULL,
                        NR_SDAP_DATA_REQ(msg_p).qfi,
                        NR_SDAP_DATA_REQ(msg_p).rqi,
                        NR_SDAP_DATA_REQ(msg_p).pdu_sessionId);
        if (result != true) {
          LOG_E(PDCP, "NR_SDAP_DATA_REQ data request failed!\n");
        }
      }
      result = itti_free(ITTI_MSG_ORIGIN_ID(msg_p), NR_SDAP_DATA_REQ(msg_p).sdu_p);
      AssertFatal(result == EXIT_SUCCESS, "Failed to free memory (%d)!\n", result);

    }
    break;

    case RRC_PCCH_DATA_REQ:
      LOG_I(PDCP, "PDCP Received RRC_PCCH_DATA_REQ CC_id %d length %d \n", RRC_PCCH_DATA_REQ(msg_p).CC_id, RRC_PCCH_DATA_REQ(msg_p).sdu_size);

      RC.nrrrc[ctxt_pP->module_id]->carrier.sizeof_paging = RRC_PCCH_DATA_REQ(msg_p).sdu_size;
      memcpy(RC.nrrrc[ctxt_pP->module_id]->carrier.paging, RRC_PCCH_DATA_REQ(msg_p).sdu_p, RRC_PCCH_DATA_REQ(msg_p).sdu_size);
      result = itti_free(ITTI_MSG_ORIGIN_ID(msg_p), RRC_PCCH_DATA_REQ(msg_p).sdu_p);
      AssertFatal(result == EXIT_SUCCESS, "Failed to free memory (%d)!\n", result);
      break;
    default:
      LOG_E(PDCP, "Received unexpected message %s\n", ITTI_MSG_NAME(msg_p));
      break;
    }

    result = itti_free(ITTI_MSG_ORIGIN_ID(msg_p), msg_p);
    AssertFatal(result == EXIT_SUCCESS, "Failed to free memory (%d)!\n", result);
    msg_p = NULL;
  }
}

static void add_srb(int is_gnb, ue_id_t rntiMaybeUEid, struct NR_SRB_ToAddMod *s, int ciphering_algorithm, int integrity_algorithm, unsigned char *ciphering_key, unsigned char *integrity_key)
{
  nr_pdcp_entity_t *pdcp_srb;
  nr_pdcp_ue_t *ue;
  int t_Reordering=3000;

  int srb_id = s->srb_Identity;
  if (s->pdcp_Config == NULL ||
      s->pdcp_Config->t_Reordering == NULL) t_Reordering = 3000;
  else t_Reordering = decode_t_reordering(*s->pdcp_Config->t_Reordering);

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, rntiMaybeUEid);
  if (ue->srb[srb_id-1] != NULL) {
    LOG_D(PDCP, "%s:%d:%s: warning SRB %d already exist for UE ID/RNTI %ld, do nothing\n", __FILE__, __LINE__, __FUNCTION__, srb_id, rntiMaybeUEid);
  } else {
    pdcp_srb = new_nr_pdcp_entity(NR_PDCP_SRB, is_gnb, srb_id,
                                  0, false, false, // sdap parameters
                                  deliver_sdu_srb, ue, deliver_pdu_srb, ue,
                                  12, t_Reordering, -1,
                                  ciphering_algorithm,
                                  integrity_algorithm,
                                  ciphering_key,
                                  integrity_key);
    nr_pdcp_ue_add_srb_pdcp_entity(ue, srb_id, pdcp_srb);

    LOG_D(PDCP, "%s:%d:%s: added srb %d to UE ID/RNTI %ld\n", __FILE__, __LINE__, __FUNCTION__, srb_id, rntiMaybeUEid);
  }
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

void add_drb_am(int is_gnb, ue_id_t rntiMaybeUEid, ue_id_t reestablish_ue_id, struct NR_DRB_ToAddMod *s, int ciphering_algorithm, int integrity_algorithm, unsigned char *ciphering_key, unsigned char *integrity_key)
{
  nr_pdcp_entity_t *pdcp_drb;
  nr_pdcp_ue_t *ue;

  int drb_id = s->drb_Identity;
  int sn_size_ul = decode_sn_size_ul(*s->pdcp_Config->drb->pdcp_SN_SizeUL);
  int sn_size_dl = decode_sn_size_dl(*s->pdcp_Config->drb->pdcp_SN_SizeDL);
  int discard_timer = decode_discard_timer(*s->pdcp_Config->drb->discardTimer);

  int has_integrity;
  int has_ciphering;

  /* if pdcp_Config->t_Reordering is not present, it means infinity (-1) */
  int t_reordering = -1;
  if (s->pdcp_Config->t_Reordering != NULL) {
    t_reordering = decode_t_reordering(*s->pdcp_Config->t_Reordering);
  }

  if (s->pdcp_Config->drb != NULL
      && s->pdcp_Config->drb->integrityProtection != NULL)
    has_integrity = 1;
  else
    has_integrity = 0;

  if (s->pdcp_Config->ext1 != NULL
     && s->pdcp_Config->ext1->cipheringDisabled != NULL)
    has_ciphering = 0;
  else
    has_ciphering = 1;

  if ((!s->cnAssociation) || s->cnAssociation->present == NR_DRB_ToAddMod__cnAssociation_PR_NOTHING) {
    LOG_E(PDCP,"%s:%d:%s: fatal, cnAssociation is missing or present is NR_DRB_ToAddMod__cnAssociation_PR_NOTHING\n",__FILE__,__LINE__,__FUNCTION__);
    exit(-1);
  }

  int pdusession_id;
  bool has_sdap_rx = false;
  bool has_sdap_tx = false;
  bool is_sdap_DefaultDRB = false;
  NR_QFI_t *mappedQFIs2Add = NULL;
  uint8_t mappedQFIs2AddCount=0;
  if (s->cnAssociation->present == NR_DRB_ToAddMod__cnAssociation_PR_eps_BearerIdentity)
     pdusession_id = s->cnAssociation->choice.eps_BearerIdentity;
  else {
    if (!s->cnAssociation->choice.sdap_Config) {
      LOG_E(PDCP,"%s:%d:%s: fatal, sdap_Config is null",__FILE__,__LINE__,__FUNCTION__);
      exit(-1);
    }
    pdusession_id = s->cnAssociation->choice.sdap_Config->pdu_Session;
    if (is_gnb) {
      has_sdap_rx = s->cnAssociation->choice.sdap_Config->sdap_HeaderUL == NR_SDAP_Config__sdap_HeaderUL_present;
      has_sdap_tx = s->cnAssociation->choice.sdap_Config->sdap_HeaderDL == NR_SDAP_Config__sdap_HeaderDL_present;
    } else {
      has_sdap_tx = s->cnAssociation->choice.sdap_Config->sdap_HeaderUL == NR_SDAP_Config__sdap_HeaderUL_present;
      has_sdap_rx = s->cnAssociation->choice.sdap_Config->sdap_HeaderDL == NR_SDAP_Config__sdap_HeaderDL_present;
    }
    is_sdap_DefaultDRB = s->cnAssociation->choice.sdap_Config->defaultDRB == true ? 1 : 0;
    mappedQFIs2Add = (NR_QFI_t*)s->cnAssociation->choice.sdap_Config->mappedQoS_FlowsToAdd->list.array[0]; 
    mappedQFIs2AddCount = s->cnAssociation->choice.sdap_Config->mappedQoS_FlowsToAdd->list.count;
    LOG_D(SDAP, "Captured mappedQoS_FlowsToAdd from RRC: %ld \n", *mappedQFIs2Add);
  }
  /* TODO(?): accept different UL and DL SN sizes? */
  if (sn_size_ul != sn_size_dl) {
    LOG_E(PDCP, "%s:%d:%s: fatal, bad SN sizes, must be same. ul=%d, dl=%d\n",
          __FILE__, __LINE__, __FUNCTION__, sn_size_ul, sn_size_dl);
    exit(1);
  }

  if (drb_id != 1 && drb_id != 2) {
    LOG_E(PDCP, "%s:%d:%s: fatal, bad drb id %d\n",
          __FILE__, __LINE__, __FUNCTION__, drb_id);
    exit(1);
  }

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, rntiMaybeUEid);
  if (ue->drb[drb_id-1] != NULL) {
    LOG_W(PDCP, "%s:%d:%s: warning DRB %d already exist for UE ID/RNTI %ld, do nothing\n", __FILE__, __LINE__, __FUNCTION__, drb_id, rntiMaybeUEid);
  } else {
    pdcp_drb = new_nr_pdcp_entity(NR_PDCP_DRB_AM, is_gnb, drb_id, pdusession_id,
                                  has_sdap_rx, has_sdap_tx,
                                  deliver_sdu_drb, ue, deliver_pdu_drb, ue,
                                  sn_size_dl, t_reordering, discard_timer,
                                  ciphering_algorithm,
                                  integrity_algorithm,
                                  ciphering_key,
                                  integrity_key);
    nr_pdcp_ue_add_drb_pdcp_entity(ue, drb_id, pdcp_drb);

    if (reestablish_ue_id > 0) {
      nr_pdcp_ue_t *reestablish_ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, reestablish_ue_id);
      if (reestablish_ue != NULL) {
        pdcp_drb->tx_next = reestablish_ue->drb[drb_id - 1]->tx_next;
        LOG_I(PDCP, "Applying tx_next %d in DRB %d from old UEid %lx to new UEid %lx\n", reestablish_ue->drb[drb_id - 1]->tx_next, drb_id, reestablish_ue_id, rntiMaybeUEid);
      }
    }

    LOG_D(PDCP, "%s:%d:%s: added drb %d to UE ID/RNTI %ld\n", __FILE__, __LINE__, __FUNCTION__, drb_id, rntiMaybeUEid);

    new_nr_sdap_entity(is_gnb, has_sdap_rx, has_sdap_tx, rntiMaybeUEid, pdusession_id, is_sdap_DefaultDRB, drb_id, mappedQFIs2Add, mappedQFIs2AddCount);
  }
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

static void add_drb(int is_gnb,
                    ue_id_t rntiMaybeUEid,
                    ue_id_t reestablish_ue_id,
                    struct NR_DRB_ToAddMod *s,
                    NR_RLC_Config_t *rlc_Config,
                    int ciphering_algorithm,
                    int integrity_algorithm,
                    unsigned char *ciphering_key,
                    unsigned char *integrity_key)
{

  if (rlc_Config == NULL) {
    LOG_E(PDCP, "%s:%d:%s: fatal: NULL RLC config\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  switch (rlc_Config->present) {
  case NR_RLC_Config_PR_am:
    add_drb_am(is_gnb, rntiMaybeUEid, reestablish_ue_id, s, ciphering_algorithm, integrity_algorithm, ciphering_key, integrity_key);
    break;
  case NR_RLC_Config_PR_um_Bi_Directional:
    LOG_I(PDCP, "NR_RLC_Config_PR_um_Bi_Directional calls add_drb_am()\n");
    // add_drb_um(rntiMaybeUEid, s);
    /* hack */
    add_drb_am(is_gnb, rntiMaybeUEid, reestablish_ue_id, s, ciphering_algorithm, integrity_algorithm, ciphering_key, integrity_key);
    break;
  default:
    LOG_E(PDCP, "%s:%d:%s: fatal: unhandled DRB type\n",
          __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
  LOG_D(PDCP, "%s: added DRB for UE ID/RNTI %ld\n", __FUNCTION__, rntiMaybeUEid);

}

void nr_pdcp_add_srbs(eNB_flag_t enb_flag, ue_id_t rntiMaybeUEid, NR_SRB_ToAddModList_t *const srb2add_list, const uint8_t security_modeP, uint8_t *const kRRCenc, uint8_t *const kRRCint)
{
  if (srb2add_list != NULL) {
    for (int i = 0; i < srb2add_list->list.count; i++) {
      add_srb(enb_flag, rntiMaybeUEid, srb2add_list->list.array[i], security_modeP & 0x0f, (security_modeP >> 4) & 0x0f, kRRCenc, kRRCint);
    }
  } else {
    LOG_W(PDCP, "nr_pdcp_add_srbs() with void list\n");
  }
  if (kRRCenc) {
    free(kRRCenc);
  }
  if (kRRCint) {
    free(kRRCint);
  }
}

void nr_pdcp_add_drbs(eNB_flag_t enb_flag,
                      ue_id_t rntiMaybeUEid,
                      ue_id_t reestablish_ue_id,
                      NR_DRB_ToAddModList_t *const drb2add_list,
                      const uint8_t security_modeP,
                      uint8_t *const kUPenc,
                      uint8_t *const kUPint,
                      struct NR_CellGroupConfig__rlc_BearerToAddModList *rlc_bearer2add_list)
{
  if (drb2add_list != NULL) {
    for (int i = 0; i < drb2add_list->list.count; i++) {
      if (rlc_bearer2add_list != NULL) {
        for (int j = 0; j < rlc_bearer2add_list->list.count; j++){
          if (rlc_bearer2add_list->list.array[j]->servedRadioBearer != NULL){
            if (rlc_bearer2add_list->list.array[j]->servedRadioBearer->present == NR_RLC_BearerConfig__servedRadioBearer_PR_drb_Identity){
              if (drb2add_list->list.array[i]->drb_Identity == rlc_bearer2add_list->list.array[j]->servedRadioBearer->choice.drb_Identity){
                add_drb(enb_flag, rntiMaybeUEid, reestablish_ue_id, drb2add_list->list.array[i], rlc_bearer2add_list->list.array[i]->rlc_Config, security_modeP & 0x0f, (security_modeP >> 4) & 0x0f, kUPenc, kUPint);
              }
            }
          }
        }
      }
    }
  } else {
    LOG_W(PDCP, "nr_pdcp_add_drbs() with void list\n");
  }
  if (kUPenc) {
    free(kUPenc);
  }
  if (kUPint) {
    free(kUPint);
  }

}

/* Dummy function due to dependency from LTE libraries */
bool rrc_pdcp_config_asn1_req(const protocol_ctxt_t *const  ctxt_pP,
                              LTE_SRB_ToAddModList_t  *const srb2add_list,
                              LTE_DRB_ToAddModList_t  *const drb2add_list,
                              LTE_DRB_ToReleaseList_t *const drb2release_list,
                              const uint8_t                   security_modeP,
                              uint8_t                  *const kRRCenc,
                              uint8_t                  *const kRRCint,
                              uint8_t                  *const kUPenc,
                              LTE_PMCH_InfoList_r9_t  *pmch_InfoList_r9,
                              rb_id_t                 *const defaultDRB)
{
  return 0;
}

void nr_DRB_preconfiguration(ue_id_t crntiMaybeUEid)
{

  NR_RadioBearerConfig_t             *rbconfig = NULL;
  struct NR_CellGroupConfig__rlc_BearerToAddModList *Rlc_Bearer_ToAdd_list = NULL;
  protocol_ctxt_t ctxt;
  //fill_default_rbconfig(rb_config, 5, 1);
  rbconfig = calloc(1, sizeof(*rbconfig));

  rbconfig->srb_ToAddModList = NULL;
  rbconfig->srb3_ToRelease = NULL;
  rbconfig->drb_ToAddModList = calloc(1,sizeof(*rbconfig->drb_ToAddModList));
  NR_DRB_ToAddMod_t *drb_ToAddMod = calloc(1,sizeof(*drb_ToAddMod));
  drb_ToAddMod->cnAssociation = calloc(1,sizeof(*drb_ToAddMod->cnAssociation));
  drb_ToAddMod->cnAssociation->present = NR_DRB_ToAddMod__cnAssociation_PR_eps_BearerIdentity;
  drb_ToAddMod->cnAssociation->choice.eps_BearerIdentity= 10;
  drb_ToAddMod->drb_Identity = 1;
  drb_ToAddMod->reestablishPDCP = NULL;
  drb_ToAddMod->recoverPDCP = NULL;
  drb_ToAddMod->pdcp_Config = calloc(1,sizeof(*drb_ToAddMod->pdcp_Config));
  asn1cCalloc(drb_ToAddMod->pdcp_Config->drb, drb);
  asn1cCallocOne(drb->discardTimer, NR_PDCP_Config__drb__discardTimer_infinity);
  asn1cCallocOne(drb->pdcp_SN_SizeUL, NR_PDCP_Config__drb__pdcp_SN_SizeUL_len18bits);
  asn1cCallocOne(drb->pdcp_SN_SizeDL, NR_PDCP_Config__drb__pdcp_SN_SizeDL_len18bits);
  drb->headerCompression.present = NR_PDCP_Config__drb__headerCompression_PR_notUsed;
  drb->headerCompression.choice.notUsed = 0;

  drb->integrityProtection = NULL;
  drb->statusReportRequired = NULL;
  drb->outOfOrderDelivery = NULL;
  drb_ToAddMod->pdcp_Config->moreThanOneRLC = NULL;

  asn1cCallocOne(drb_ToAddMod->pdcp_Config->t_Reordering, NR_PDCP_Config__t_Reordering_ms0);
  drb_ToAddMod->pdcp_Config->ext1 = NULL;

  asn1cSeqAdd(&rbconfig->drb_ToAddModList->list,drb_ToAddMod);

  rbconfig->drb_ToReleaseList = NULL;

  asn1cCalloc(rbconfig->securityConfig, secConf);
  asn1cCalloc(secConf->securityAlgorithmConfig, secConfAlgo);
  secConfAlgo->cipheringAlgorithm = NR_CipheringAlgorithm_nea0;
  secConfAlgo->integrityProtAlgorithm = NULL;
  asn1cCallocOne(secConf->keyToUse, NR_SecurityConfig__keyToUse_master);

  if ( LOG_DEBUGFLAG(DEBUG_ASN1) ) {
    xer_fprint(stdout, &asn_DEF_NR_RadioBearerConfig, (const void*)rbconfig);
  }

  NR_RLC_BearerConfig_t *RLC_BearerConfig = calloc(1,sizeof(*RLC_BearerConfig));
  nr_rlc_bearer_init(RLC_BearerConfig,NR_RLC_BearerConfig__servedRadioBearer_PR_drb_Identity);
  nr_drb_config(RLC_BearerConfig->rlc_Config, NR_RLC_Config_PR_um_Bi_Directional);
  //nr_drb_config(RLC_BearerConfig->rlc_Config, NR_RLC_Config_PR_am);
  nr_rlc_bearer_init_ul_spec(RLC_BearerConfig->mac_LogicalChannelConfig);

  Rlc_Bearer_ToAdd_list = calloc(1,sizeof(*Rlc_Bearer_ToAdd_list));
  asn1cSeqAdd(&Rlc_Bearer_ToAdd_list->list, RLC_BearerConfig);

  if (ENB_NAS_USE_TUN){
    PROTOCOL_CTXT_SET_BY_MODULE_ID(&ctxt, 0, ENB_FLAG_YES, crntiMaybeUEid, 0, 0, 0);
  }
  else{
    PROTOCOL_CTXT_SET_BY_MODULE_ID(&ctxt, 0, ENB_FLAG_NO, crntiMaybeUEid, 0, 0, 0);
  }

  nr_pdcp_add_drbs(ctxt.enb_flag, ctxt.rntiMaybeUEid, 0, rbconfig->drb_ToAddModList, 0, NULL, NULL, Rlc_Bearer_ToAdd_list);

  nr_rrc_rlc_config_asn1_req(&ctxt, (NR_SRB_ToAddModList_t *)NULL, rbconfig->drb_ToAddModList, rbconfig->drb_ToReleaseList, Rlc_Bearer_ToAdd_list);

  LOG_D(PDCP, "%s:%d: done RRC PDCP/RLC ASN1 request for UE ID/RNTI %ld\n", __FUNCTION__, __LINE__, ctxt.rntiMaybeUEid);
}

uint64_t get_pdcp_optmask(void)
{
  return pdcp_optmask;
}

bool pdcp_remove_UE(const protocol_ctxt_t *const ctxt_pP)
{
  ue_id_t rntiMaybeUEid = ctxt_pP->rntiMaybeUEid;

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  nr_pdcp_manager_remove_ue(nr_pdcp_ue_manager, rntiMaybeUEid);
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);

  return 1;
}

void pdcp_config_set_security(
        const protocol_ctxt_t* const  ctxt_pP,
        pdcp_t *const pdcp_pP,
        const rb_id_t rb_id,
        const uint16_t lc_idP,
        const uint8_t security_modeP,
        uint8_t *const kRRCenc_pP,
        uint8_t *const kRRCint_pP,
        uint8_t *const kUPenc_pP,
        uint8_t *const kUPint_pP)
{
  nr_pdcp_ue_t *ue;
  nr_pdcp_entity_t *rb;
  ue_id_t ue_id = ctxt_pP->rntiMaybeUEid;
  int integrity_algorithm;
  int ciphering_algorithm;

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);

  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);

  /* TODO: proper handling of DRBs, for the moment only SRBs are handled */

  if (rb_id >= 1 && rb_id <= 2) {
    rb = ue->srb[rb_id - 1];

    if (rb == NULL) {
      LOG_E(PDCP, "%s:%d:%s: no SRB found (ue_id %ld, rb_id %ld)\n", __FILE__, __LINE__, __FUNCTION__, ue_id, rb_id);
      nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
      return;
    }

    integrity_algorithm = (security_modeP>>4) & 0xf;
    ciphering_algorithm = security_modeP & 0x0f;
    LOG_DUMPMSG(PDCP, DEBUG_PDCP, kRRCint_pP, 16, "%s: (%d) kRRCint_pP: ", __FUNCTION__, integrity_algorithm);
    LOG_DUMPMSG(PDCP, DEBUG_PDCP, kRRCenc_pP, 16, "%s: (%d) kRRCenc_pP: ", __FUNCTION__, ciphering_algorithm);
    rb->set_security(rb, integrity_algorithm, kRRCint_pP, ciphering_algorithm, kRRCenc_pP);
  } else {
    LOG_E(PDCP, "%s:%d:%s: TODO\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

static bool pdcp_data_req_srb(protocol_ctxt_t  *ctxt_pP,
                              const rb_id_t rb_id,
                              const mui_t muiP,
                              const confirm_t confirmP,
                              const sdu_size_t sdu_buffer_size,
                              unsigned char *const sdu_buffer)
{
  LOG_D(PDCP, "%s() called, size %d\n", __func__, sdu_buffer_size);
  nr_pdcp_ue_t *ue;
  nr_pdcp_entity_t *rb;
  ue_id_t ue_id = ctxt_pP->rntiMaybeUEid;

  if (ctxt_pP->module_id != 0 ||
      //ctxt_pP->enb_flag != 1 ||
      ctxt_pP->instance != 0 ||
      ctxt_pP->eNB_index != 0 /*||
      ctxt_pP->configured != 1 ||
      ctxt_pP->brOption != 0*/) {
    LOG_E(PDCP, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);

  if (rb_id < 1 || rb_id > 2)
    rb = NULL;
  else
    rb = ue->srb[rb_id - 1];


  /** TRACE PDCP PDU */
  if (NULL != ue && NULL != rb) {
    nr_pdcp_pkt_info_t pdcp_pkt;
    memset(&pdcp_pkt, 0, sizeof(pdcp_pkt));
    pdcp_pkt.direction 	= 1; //PDCP_NR_DIRECTION_DOWNLINK
//    pdcp_pkt.ueid      	= ue->rntiMaybeUEid;
    pdcp_pkt.bearerType 	= 8; //TODO
    pdcp_pkt.bearerId 	= rb_id - 1;
    pdcp_pkt.plane     	= (rb_id == 1)?4:1;

    pdcp_pkt.seqnum_length 	= rb->sn_size;
    pdcp_pkt.maci_present 	= (rb->has_integrity)?1:0;
    pdcp_pkt.ciphering_disabled 	= (rb->has_ciphering)?1:0;
    pdcp_pkt.sdap_header 		= (rb->has_sdap_rx)?1:0;
    pdcp_pkt.is_retx 		= 0;
    pdcp_pkt.pdu_length 		= sdu_buffer_size;

    LOG_PDCP_P(OAILOG_INFO, "DL_PDCP_PDU", -1, -1, (pdcp_pkt), (unsigned char *)sdu_buffer, sdu_buffer_size);
  }


  if (rb == NULL) {
    LOG_E(PDCP, "%s:%d:%s: no SRB found (ue_id %ld, rb_id %ld)\n", __FILE__, __LINE__, __FUNCTION__, ue_id, rb_id);
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    return 0;
  }


  rb->recv_sdu(rb, (char *)sdu_buffer, sdu_buffer_size, muiP);
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);

  return 1;
}

static bool pdcp_data_req_drb(protocol_ctxt_t  *ctxt_pP,
                              const rb_id_t rb_id,
                              const mui_t muiP,
                              const confirm_t confirmP,
                              const sdu_size_t sdu_buffer_size,
                              unsigned char *const sdu_buffer)
{
  LOG_I(PDCP, "%s() called, size %d\n", __func__, sdu_buffer_size);
  nr_pdcp_ue_t *ue;
  nr_pdcp_entity_t *rb;
  ue_id_t ue_id = ctxt_pP->rntiMaybeUEid;

  if (ctxt_pP->module_id != 0 ||
      //ctxt_pP->enb_flag != 1 ||
      ctxt_pP->instance != 0 ||
      ctxt_pP->eNB_index != 0 /*||
      ctxt_pP->configured != 1 ||
      ctxt_pP->brOption != 0*/) {
    LOG_E(PDCP, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);

  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);

  if (rb_id < 1 || rb_id > MAX_DRBS_PER_UE)
    rb = NULL;
  else
    rb = ue->drb[rb_id - 1];

  if (rb == NULL) {
    LOG_E(PDCP, "%s:%d:%s: no DRB found (ue_id %ld, rb_id %ld)\n", __FILE__, __LINE__, __FUNCTION__, ue_id, rb_id);
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    return 0;
  }

  /** TRACE PDCP PDU */
  nr_pdcp_pkt_info_t pdcp_pkt;
  pdcp_pkt.direction  = 1; //PDCP_NR_DIRECTION_DOWNLINK
  if (ue != NULL)
  {
    pdcp_pkt.ueid       = ue->rntiMaybeUEid;
  }
  pdcp_pkt.bearerType = 8; //TODO
  pdcp_pkt.bearerId   = rb_id - 1;
  pdcp_pkt.plane      = 2;

  if (rb != NULL)
  {
    pdcp_pkt.seqnum_length      = rb->sn_size;
    pdcp_pkt.maci_present       = (rb->has_integrity)?1:0;
    pdcp_pkt.ciphering_disabled = (rb->has_ciphering)?1:0;
    pdcp_pkt.sdap_header        = (rb->has_sdap_rx)?1:0;
  }
  pdcp_pkt.is_retx            = 0;
  pdcp_pkt.pdu_length         = sdu_buffer_size;

  LOG_PDCP_P(OAILOG_INFO, "DL_PDCP_PDU", -1, -1, (pdcp_pkt), (unsigned char *)sdu_buffer, sdu_buffer_size);
  LOG_DUMPMSG(PDCP, DEBUG_PDCP, (unsigned char *)sdu_buffer, sdu_buffer_size, "%s: sz=%d: ", __FUNCTION__, sdu_buffer_size);
  rb->recv_sdu(rb, (char *)sdu_buffer, sdu_buffer_size, muiP);

  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);

  return 1;
}

bool cu_f1u_data_req(protocol_ctxt_t  *ctxt_pP,
                     const srb_flag_t srb_flagP,
                     const rb_id_t rb_id,
                     const mui_t muiP,
                     const confirm_t confirmP,
                     const sdu_size_t sdu_buffer_size,
                     unsigned char *const sdu_buffer,
                     const pdcp_transmission_mode_t mode,
                     const uint32_t *const sourceL2Id,
                     const uint32_t *const destinationL2Id) {
  //Force instance id to 0, OAI incoherent instance management
  ctxt_pP->instance=0;
  mem_block_t *memblock = get_free_mem_block(sdu_buffer_size, __func__);
  if (memblock == NULL) {
    LOG_E(RLC, "%s:%d:%s: ERROR: get_free_mem_block failed\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
  memcpy(memblock->data,sdu_buffer, sdu_buffer_size);
  int ret=pdcp_data_ind(ctxt_pP,srb_flagP, false, rb_id, sdu_buffer_size, memblock, NULL, NULL);
  if (!ret) {
    LOG_E(RLC, "%s:%d:%s: ERROR: pdcp_data_ind failed\n", __FILE__, __LINE__, __FUNCTION__);
    /* what to do in case of failure? for the moment: nothing */
  }
  return ret;
}

bool pdcp_data_req(protocol_ctxt_t  *ctxt_pP,
                   const srb_flag_t srb_flagP,
                   const rb_id_t rb_id,
                   const mui_t muiP,
                   const confirm_t confirmP,
                   const sdu_size_t sdu_buffer_size,
                   unsigned char *const sdu_buffer,
                   const pdcp_transmission_mode_t mode,
                   const uint32_t *const sourceL2Id,
                   const uint32_t *const destinationL2Id)
{
  // Trace NR PDCP PDU here.
  if (srb_flagP) {
   return pdcp_data_req_srb(ctxt_pP, rb_id, muiP, confirmP, sdu_buffer_size, sdu_buffer);
  }
  else{
    return pdcp_data_req_drb(ctxt_pP, rb_id, muiP, confirmP, sdu_buffer_size, sdu_buffer);
  }
}

void pdcp_set_pdcp_data_ind_func(pdcp_data_ind_func_t pdcp_data_ind)
{
  /* nothing to do */
}

void pdcp_set_rlc_data_req_func(send_rlc_data_req_func_t send_rlc_data_req)
{
  /* nothing to do */
}

//Dummy function needed due to LTE dependencies
void
pdcp_mbms_run ( const protocol_ctxt_t *const  ctxt_pP){
  /* nothing to do */
}

void nr_pdcp_tick(int frame, int subframe)
{
  if (frame != nr_pdcp_current_time_last_frame ||
      subframe != nr_pdcp_current_time_last_subframe) {
    nr_pdcp_current_time_last_frame = frame;
    nr_pdcp_current_time_last_subframe = subframe;
    nr_pdcp_current_time++;
    nr_pdcp_wakeup_timer_thread(nr_pdcp_current_time);
  }
}

/*
 * For the SDAP API
 */
nr_pdcp_ue_manager_t *nr_pdcp_sdap_get_ue_manager() {
  return nr_pdcp_ue_manager;
}

/* returns false in case of error, true if everything ok */
const bool nr_pdcp_get_statistics(ue_id_t ue_id, int srb_flag, int rb_id, nr_pdcp_statistics_t *out)
{
  nr_pdcp_ue_t     *ue;
  nr_pdcp_entity_t *rb;
  bool             ret;

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);

  if (srb_flag == 1) {
    if (rb_id < 1 || rb_id > 2)
      rb = NULL;
    else
      rb = ue->srb[rb_id - 1];
  } else {
    if (rb_id < 1 || rb_id > 5)
      rb = NULL;
    else
      rb = ue->drb[rb_id - 1];
  }

  if (rb != NULL) {
    rb->get_stats(rb, out);
    ret = true;
  } else {
    ret = false;
  }

  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);

  return ret;
}

void nr_pdcp_config_set_smc(ue_id_t crntiMaybeUEid, bool complete)
{
  nr_pdcp_ue_t *ue;
  nr_pdcp_entity_t *rb;

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, crntiMaybeUEid);
  if (ue && ue->srb[0]) {
    rb = ue->srb[0];
    if (rb->ciphering_algorithm > 0) {
      rb->has_ciphering = complete ? NR_PDCP_ENTITY_CIPHERING_ON : NR_PDCP_ENTITY_CIPHERING_SMC;
    }
  }
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}
