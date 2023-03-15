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

/*! \file PHY/NR_TRANSPORT/nr_ulsch_llr_computation.c
 * \brief Top-level routines for LLR computation of the PDSCH physical channel
 * \author Ahmed Hussein
 * \date 2019
 * \version 0.1
 * \company Fraunhofer IIS
 * \email: ahmed.hussein@iis.fraunhofer.de
 * \note
 * \warning
 */

#include "PHY/defs_nr_common.h"
#include "PHY/sse_intrin.h"
#include "PHY/impl_defs_top.h"



//----------------------------------------------------------------------------------------------
// QPSK
//----------------------------------------------------------------------------------------------
void nr_ulsch_qpsk_llr(int32_t *rxdataF_comp,
                      int16_t  *ulsch_llr,
                      uint32_t nb_re,
                      uint8_t  symbol)
{
  c16_t *rxF   = (c16_t *)rxdataF_comp;
  c16_t *llr32 = (c16_t *)ulsch_llr;

  if (!llr32) {
    LOG_E(PHY,"nr_ulsch_qpsk_llr: llr is null, symbol %d, llr32 = %p\n",symbol, llr32);
  }
  for (int i = 0; i < nb_re; i++) {
    //*llr32 = *rxF;
    llr32->r = rxF->r >> 3;
    llr32->i = rxF->i >> 3;
    rxF++;
    llr32++;
  }
}

//----------------------------------------------------------------------------------------------
// 16-QAM
//----------------------------------------------------------------------------------------------

void nr_ulsch_16qam_llr(int32_t *rxdataF_comp,
                        int32_t *ul_ch_mag,
                        int16_t  *ulsch_llr,
                        uint32_t nb_rb,
                        uint32_t nb_re,
                        uint8_t  symbol)
{

#if defined(__x86_64__) || defined(__i386__)
  __m256i *rxF = (__m256i*)rxdataF_comp;
  __m256i *ch_mag;
  __m256i llr256[2];
  register __m256i xmm0;
  uint32_t *llr32;
#elif defined(__arm__) || defined(__aarch64__)
  int16x8_t *rxF = (int16x8_t*)&rxdataF_comp;
  int16x8_t *ch_mag;
  int16x8_t xmm0;
  int16_t *llr16;
#endif


  int i;

  int off = ((nb_rb&1) == 1)? 4:0;

#if defined(__x86_64__) || defined(__i386__)
    llr32 = (uint32_t*)ulsch_llr;
#elif defined(__arm__) || defined(__aarch64__)
    llr16 = (int16_t*)ulsch_llr;
#endif

#if defined(__x86_64__) || defined(__i386__)
    ch_mag = (__m256i*)&ul_ch_mag[(symbol*(off+(nb_rb*12)))];
#elif defined(__arm__) || defined(__aarch64__)
  ch_mag = (int16x8_t*)&ul_ch_mag[(symbol*nb_rb*12)];
#endif
  unsigned char len_mod8 = nb_re&7;
  nb_re >>= 3;  // length in quad words (4 REs)
  nb_re += (len_mod8 == 0 ? 0 : 1);

  for (i=0; i<nb_re; i++) {
#if defined(__x86_64__) || defined(__i386)
    xmm0 = simde_mm256_abs_epi16(rxF[i]); // registers of even index in xmm0-> |y_R|, registers of odd index in xmm0-> |y_I|
    xmm0 = simde_mm256_subs_epi16(ch_mag[i],xmm0); // registers of even index in xmm0-> |y_R|-|h|^2, registers of odd index in xmm0-> |y_I|-|h|^2
 
    llr256[0] = simde_mm256_unpacklo_epi32(rxF[i],xmm0); // llr128[0] contains the llrs of the 1st,2nd,5th and 6th REs
    llr256[1] = simde_mm256_unpackhi_epi32(rxF[i],xmm0); // llr128[1] contains the llrs of the 3rd, 4th, 7th and 8th REs
    
    // 1st RE
    llr32[0] = simde_mm256_extract_epi32(llr256[0],0); // llr32[0] low 16 bits-> y_R        , high 16 bits-> y_I
    llr32[1] = simde_mm256_extract_epi32(llr256[0],1); // llr32[1] low 16 bits-> |h|-|y_R|^2, high 16 bits-> |h|-|y_I|^2

    // 2nd RE
    llr32[2] = simde_mm256_extract_epi32(llr256[0],2); // llr32[2] low 16 bits-> y_R        , high 16 bits-> y_I
    llr32[3] = simde_mm256_extract_epi32(llr256[0],3); // llr32[3] low 16 bits-> |h|-|y_R|^2, high 16 bits-> |h|-|y_I|^2

    // 3rd RE
    llr32[4] = simde_mm256_extract_epi32(llr256[1],0); // llr32[4] low 16 bits-> y_R        , high 16 bits-> y_I
    llr32[5] = simde_mm256_extract_epi32(llr256[1],1); // llr32[5] low 16 bits-> |h|-|y_R|^2, high 16 bits-> |h|-|y_I|^2

    // 4th RE
    llr32[6] = simde_mm256_extract_epi32(llr256[1],2); // llr32[6] low 16 bits-> y_R        , high 16 bits-> y_I
    llr32[7] = simde_mm256_extract_epi32(llr256[1],3); // llr32[7] low 16 bits-> |h|-|y_R|^2, high 16 bits-> |h|-|y_I|^2

    // 5th RE
    llr32[8] = simde_mm256_extract_epi32(llr256[0],4); // llr32[8] low 16 bits-> y_R        , high 16 bits-> y_I
    llr32[9] = simde_mm256_extract_epi32(llr256[0],5); // llr32[9] low 16 bits-> |h|-|y_R|^2, high 16 bits-> |h|-|y_I|^2

    // 6th RE
    llr32[10] = simde_mm256_extract_epi32(llr256[0],6); // llr32[10] low 16 bits-> y_R        , high 16 bits-> y_I
    llr32[11] = simde_mm256_extract_epi32(llr256[0],7); // llr32[11] low 16 bits-> |h|-|y_R|^2, high 16 bits-> |h|-|y_I|^2

    // 7th RE
    llr32[12] = simde_mm256_extract_epi32(llr256[1],4); // llr32[12] low 16 bits-> y_R        , high 16 bits-> y_I
    llr32[13] = simde_mm256_extract_epi32(llr256[1],5); // llr32[13] low 16 bits-> |h|-|y_R|^2, high 16 bits-> |h|-|y_I|^2

    // 8th RE
    llr32[14] = simde_mm256_extract_epi32(llr256[1],6); // llr32[14] low 16 bits-> y_R        , high 16 bits-> y_I
    llr32[15] = simde_mm256_extract_epi32(llr256[1],7); // llr32[15] low 16 bits-> |h|-|y_R|^2, high 16 bits-> |h|-|y_I|^2

    llr32+=16;
#elif defined(__arm__) || defined(__aarch64__)
    xmm0 = vabsq_s16(rxF[i]);
    xmm0 = vqsubq_s16((*(__m128i*)&ones[0]),xmm0);

    llr16[0]  = vgetq_lane_s16(rxF[i],0);
    llr16[1]  = vgetq_lane_s16(rxF[i],1);
    llr16[2]  = vgetq_lane_s16(xmm0,0);
    llr16[3]  = vgetq_lane_s16(xmm0,1);
    llr16[4]  = vgetq_lane_s16(rxF[i],2);
    llr16[5]  = vgetq_lane_s16(rxF[i],3);
    llr16[6]  = vgetq_lane_s16(xmm0,2);
    llr16[7]  = vgetq_lane_s16(xmm0,3);
    llr16[8]  = vgetq_lane_s16(rxF[i],4);
    llr16[9]  = vgetq_lane_s16(rxF[i],5);
    llr16[10] = vgetq_lane_s16(xmm0,4);
    llr16[11] = vgetq_lane_s16(xmm0,5);
    llr16[12] = vgetq_lane_s16(rxF[i],6);
    llr16[13] = vgetq_lane_s16(rxF[i],6);
    llr16[14] = vgetq_lane_s16(xmm0,7);
    llr16[15] = vgetq_lane_s16(xmm0,7);
    llr16+=16;
#endif

  }

#if defined(__x86_64__) || defined(__i386__)
  _mm_empty();
  _m_empty();
#endif
}

//----------------------------------------------------------------------------------------------
// 64-QAM
//----------------------------------------------------------------------------------------------

void nr_ulsch_64qam_llr(int32_t *rxdataF_comp,
                        int32_t *ul_ch_mag,
                        int32_t *ul_ch_magb,
                        int16_t  *ulsch_llr,
                        uint32_t nb_rb,
                        uint32_t nb_re,
                        uint8_t  symbol)
{
  int off = ((nb_rb&1) == 1)? 4:0;

#if defined(__x86_64__) || defined(__i386__)
  __m256i *rxF = (__m256i*)rxdataF_comp;
  __m256i *ch_mag,*ch_magb;
  register __m256i xmm0,xmm1,xmm2;
#elif defined(__arm__) || defined(__aarch64__)
  int16x8_t *rxF = (int16x8_t*)&rxdataF_comp;
  int16x8_t *ch_mag,*ch_magb; // [hna] This should be uncommented once channel estimation is implemented
  int16x8_t xmm0,xmm1,xmm2;
#endif

  int i;

#if defined(__x86_64__) || defined(__i386__)
  ch_mag = (__m256i*)&ul_ch_mag[(symbol*(off+(nb_rb*12)))];
  ch_magb = (__m256i*)&ul_ch_magb[(symbol*(off+(nb_rb*12)))];
#elif defined(__arm__) || defined(__aarch64__)
  ch_mag = (int16x8_t*)&ul_ch_mag[(symbol*nb_rb*12)];
  ch_magb = (int16x8_t*)&ul_ch_magb[(symbol*nb_rb*12)];
#endif

  int len_mod8 = nb_re&7;
  nb_re    = nb_re>>3;  // length in quad words (4 REs)
  nb_re   += ((len_mod8 == 0) ? 0 : 1);

  for (i=0; i<nb_re; i++) {
    xmm0 = rxF[i];
#if defined(__x86_64__) || defined(__i386__)
    xmm1 = simde_mm256_abs_epi16(xmm0);
    xmm1 = simde_mm256_subs_epi16(ch_mag[i],xmm1);
    xmm2 = simde_mm256_abs_epi16(xmm1);
    xmm2 = simde_mm256_subs_epi16(ch_magb[i],xmm2);
#elif defined(__arm__) || defined(__aarch64__)
    xmm1 = vabsq_s16(xmm0);
    xmm1 = vsubq_s16(ch_mag[i],xmm1);
    xmm2 = vabsq_s16(xmm1);
    xmm2 = vsubq_s16(ch_magb[i],xmm2);
#endif
    
    // ---------------------------------------
    // 1st RE
    // ---------------------------------------
#if defined(__x86_64__) || defined(__i386__)
    ulsch_llr[0] = simde_mm256_extract_epi16(xmm0,0);
    ulsch_llr[1] = simde_mm256_extract_epi16(xmm0,1);
    ulsch_llr[2] = simde_mm256_extract_epi16(xmm1,0);
    ulsch_llr[3] = simde_mm256_extract_epi16(xmm1,1);
    ulsch_llr[4] = simde_mm256_extract_epi16(xmm2,0);
    ulsch_llr[5] = simde_mm256_extract_epi16(xmm2,1);
#elif defined(__arm__) || defined(__aarch64__)
    ulsch_llr[0] = vgetq_lane_s16(xmm0,0);
    ulsch_llr[1] = vgetq_lane_s16(xmm0,1);
    ulsch_llr[2] = vgetq_lane_s16(xmm1,0);
    ulsch_llr[3] = vgetq_lane_s16(xmm1,1);
    ulsch_llr[4] = vgetq_lane_s16(xmm2,0);
    ulsch_llr[5] = vgetq_lane_s16(xmm2,1);
#endif
    // ---------------------------------------

    ulsch_llr+=6;
    
    // ---------------------------------------
    // 2nd RE
    // ---------------------------------------
#if defined(__x86_64__) || defined(__i386__)
    ulsch_llr[0] = simde_mm256_extract_epi16(xmm0,2);
    ulsch_llr[1] = simde_mm256_extract_epi16(xmm0,3);
    ulsch_llr[2] = simde_mm256_extract_epi16(xmm1,2);
    ulsch_llr[3] = simde_mm256_extract_epi16(xmm1,3);
    ulsch_llr[4] = simde_mm256_extract_epi16(xmm2,2);
    ulsch_llr[5] = simde_mm256_extract_epi16(xmm2,3);
#elif defined(__arm__) || defined(__aarch64__)
    ulsch_llr[2] = vgetq_lane_s16(xmm0,2);
    ulsch_llr[3] = vgetq_lane_s16(xmm0,3);
    ulsch_llr[2] = vgetq_lane_s16(xmm1,2);
    ulsch_llr[3] = vgetq_lane_s16(xmm1,3);
    ulsch_llr[4] = vgetq_lane_s16(xmm2,2);
    ulsch_llr[5] = vgetq_lane_s16(xmm2,3);
#endif
    // ---------------------------------------

    ulsch_llr+=6;
    
    // ---------------------------------------
    // 3rd RE
    // ---------------------------------------
#if defined(__x86_64__) || defined(__i386__)
    ulsch_llr[0] = simde_mm256_extract_epi16(xmm0,4);
    ulsch_llr[1] = simde_mm256_extract_epi16(xmm0,5);
    ulsch_llr[2] = simde_mm256_extract_epi16(xmm1,4);
    ulsch_llr[3] = simde_mm256_extract_epi16(xmm1,5);
    ulsch_llr[4] = simde_mm256_extract_epi16(xmm2,4);
    ulsch_llr[5] = simde_mm256_extract_epi16(xmm2,5);
#elif defined(__arm__) || defined(__aarch64__)
    ulsch_llr[0] = vgetq_lane_s16(xmm0,4);
    ulsch_llr[1] = vgetq_lane_s16(xmm0,5);
    ulsch_llr[2] = vgetq_lane_s16(xmm1,4);
    ulsch_llr[3] = vgetq_lane_s16(xmm1,5);
    ulsch_llr[4] = vgetq_lane_s16(xmm2,4);
    ulsch_llr[5] = vgetq_lane_s16(xmm2,5);
#endif
    // ---------------------------------------

    ulsch_llr+=6;
    
    // ---------------------------------------
    // 4th RE
    // ---------------------------------------
#if defined(__x86_64__) || defined(__i386__)
    ulsch_llr[0] = simde_mm256_extract_epi16(xmm0,6);
    ulsch_llr[1] = simde_mm256_extract_epi16(xmm0,7);
    ulsch_llr[2] = simde_mm256_extract_epi16(xmm1,6);
    ulsch_llr[3] = simde_mm256_extract_epi16(xmm1,7);
    ulsch_llr[4] = simde_mm256_extract_epi16(xmm2,6);
    ulsch_llr[5] = simde_mm256_extract_epi16(xmm2,7);
#elif defined(__arm__) || defined(__aarch64__)
    ulsch_llr[0] = vgetq_lane_s16(xmm0,6);
    ulsch_llr[1] = vgetq_lane_s16(xmm0,7);
    ulsch_llr[2] = vgetq_lane_s16(xmm1,6);
    ulsch_llr[3] = vgetq_lane_s16(xmm1,7);
    ulsch_llr[4] = vgetq_lane_s16(xmm2,6);
    ulsch_llr[5] = vgetq_lane_s16(xmm2,7);
#endif
    // ---------------------------------------

    ulsch_llr+=6;
    ulsch_llr[0] = simde_mm256_extract_epi16(xmm0,8);
    ulsch_llr[1] = simde_mm256_extract_epi16(xmm0,9);
    ulsch_llr[2] = simde_mm256_extract_epi16(xmm1,8);
    ulsch_llr[3] = simde_mm256_extract_epi16(xmm1,9);
    ulsch_llr[4] = simde_mm256_extract_epi16(xmm2,8);
    ulsch_llr[5] = simde_mm256_extract_epi16(xmm2,9);

    ulsch_llr[6] = simde_mm256_extract_epi16(xmm0,10);
    ulsch_llr[7] = simde_mm256_extract_epi16(xmm0,11);
    ulsch_llr[8] = simde_mm256_extract_epi16(xmm1,10);
    ulsch_llr[9] = simde_mm256_extract_epi16(xmm1,11);
    ulsch_llr[10] = simde_mm256_extract_epi16(xmm2,10);
    ulsch_llr[11] = simde_mm256_extract_epi16(xmm2,11);

    ulsch_llr[12] = simde_mm256_extract_epi16(xmm0,12);
    ulsch_llr[13] = simde_mm256_extract_epi16(xmm0,13);
    ulsch_llr[14] = simde_mm256_extract_epi16(xmm1,12);
    ulsch_llr[15] = simde_mm256_extract_epi16(xmm1,13);
    ulsch_llr[16] = simde_mm256_extract_epi16(xmm2,12);
    ulsch_llr[17] = simde_mm256_extract_epi16(xmm2,13);

    ulsch_llr[18] = simde_mm256_extract_epi16(xmm0,14);
    ulsch_llr[19] = simde_mm256_extract_epi16(xmm0,15);
    ulsch_llr[20] = simde_mm256_extract_epi16(xmm1,14);
    ulsch_llr[21] = simde_mm256_extract_epi16(xmm1,15);
    ulsch_llr[22] = simde_mm256_extract_epi16(xmm2,14);
    ulsch_llr[23] = simde_mm256_extract_epi16(xmm2,15);

    ulsch_llr+=24;
  }

#if defined(__x86_64__) || defined(__i386__)
  _mm_empty();
  _m_empty();
#endif
}

void nr_ulsch_256qam_llr(int32_t *rxdataF_comp,
                         int32_t *ul_ch_mag,
                         int32_t *ul_ch_magb,
	                 int32_t *ul_ch_magc,
	                 int16_t  *ulsch_llr,
	                 uint32_t nb_rb,
	                 uint32_t nb_re,
	                 uint8_t  symbol)
{
  int off = ((nb_rb&1) == 1)? 4:0;

  simde__m256i *rxF = (simde__m256i*)rxdataF_comp;
  simde__m256i *ch_mag,*ch_magb,*ch_magc;
  register simde__m256i xmm0,xmm1,xmm2,xmm3,xmm4,xmm5,xmm6;
  simde__m256i *llr256=(simde__m256i*)ulsch_llr;

  ch_mag  = (simde__m256i*)&ul_ch_mag[(symbol*(off+(nb_rb*12)))];
  ch_magb = (simde__m256i*)&ul_ch_magb[(symbol*(off+(nb_rb*12)))];
  ch_magc = (simde__m256i*)&ul_ch_magc[(symbol*(off+(nb_rb*12)))];
  int len_mod8 = nb_re&7;
  int nb_re256    = nb_re>>3;  // length in 256-bit words (8 REs)

  for (int i=0; i<nb_re256; i++) {
       xmm0 = simde_mm256_abs_epi16(rxF[i]); // registers of even index in xmm0-> |y_R|, registers of odd index in xmm0-> |y_I|
       xmm0 = simde_mm256_subs_epi16(ch_mag[i],xmm0); // registers of even index in xmm0-> |y_R|-|h|^2, registers of odd index in xmm0-> |y_I|-|h|^2
      //  xmmtmpD2 contains 16 LLRs
       xmm1 = simde_mm256_abs_epi16(xmm0);
       xmm1 = simde_mm256_subs_epi16(ch_magb[i],xmm1); // contains 16 LLRs
       xmm2 = simde_mm256_abs_epi16(xmm1);
       xmm2 = simde_mm256_subs_epi16(ch_magc[i],xmm2); // contains 16 LLRs
        // rxF[i] A0 A1 A2 A3 A4 A5 A6 A7 bits 7,6
        // xmm0   B0 B1 B2 B3 B4 B5 B6 B7 bits 5,4
        // xmm1   C0 C1 C2 C3 C4 C5 C6 C7 bits 3,2
        // xmm2   D0 D1 D2 D3 D4 D5 D6 D7 bits 1,0
       xmm3 = simde_mm256_unpacklo_epi32(rxF[i],xmm0); // A0 B0 A1 B1 A4 B4 A5 B5
       xmm4 = simde_mm256_unpackhi_epi32(rxF[i],xmm0); // A2 B2 A3 B3 A6 B6 A7 B7
       xmm5 = simde_mm256_unpacklo_epi32(xmm1,xmm2);   // C0 D0 C1 D1 C4 D4 C5 D5
       xmm6 = simde_mm256_unpackhi_epi32(xmm1,xmm2);   // C2 D2 C3 D3 C6 D6 C7 D7

       xmm0 = simde_mm256_unpacklo_epi64(xmm3,xmm5); // A0 B0 C0 D0 A4 B4 C4 D4
       xmm1 = simde_mm256_unpackhi_epi64(xmm3,xmm5); // A1 B1 C1 D1 A5 B5 C5 D5
       xmm2 = simde_mm256_unpacklo_epi64(xmm4,xmm6); // A2 B2 C2 D2 A6 B6 C6 D6
       xmm3 = simde_mm256_unpackhi_epi64(xmm4,xmm6); // A3 B3 C3 D3 A7 B7 C7 D7
       llr256[0] = simde_mm256_permute2x128_si256(xmm0, xmm1, 0x20); // A0 B0 C0 D0 A1 B1 C1 D1
       llr256[1] = simde_mm256_permute2x128_si256(xmm2, xmm3, 0x20); // A2 B2 C2 D2 A3 B3 C3 D3
       llr256[2] = simde_mm256_permute2x128_si256(xmm0, xmm1, 0x31); // A4 B4 C4 D4 A5 B5 C5 D5
       llr256[3] = simde_mm256_permute2x128_si256(xmm2, xmm3, 0x31); // A6 B6 C6 D6 A7 B7 C7 D7
       llr256+=4;

  }
  simde__m128i *llr128 = (simde__m128i*)llr256;
  if (len_mod8 >= 4) {
     int nb_re128 = nb_re>>2;
     simde__m128i xmm0,xmm1,xmm2,xmm3,xmm4,xmm5,xmm6;
     simde__m128i *rxF = (simde__m128i*)rxdataF_comp;
     simde__m128i *ch_mag  = (simde__m128i*)&ul_ch_mag[(symbol*(off+(nb_rb*12)))];
     simde__m128i *ch_magb = (simde__m128i*)&ul_ch_magb[(symbol*(off+(nb_rb*12)))];
     simde__m128i *ch_magc = (simde__m128i*)&ul_ch_magc[(symbol*(off+(nb_rb*12)))];

     xmm0 = simde_mm_abs_epi16(rxF[nb_re128-1]); // registers of even index in xmm0-> |y_R|, registers of odd index in xmm0-> |y_I|
     xmm0 = simde_mm_subs_epi16(ch_mag[nb_re128-1],xmm0); // registers of even index in xmm0-> |y_R|-|h|^2, registers of odd index in xmm0-> |y_I|-|h|^2
      //  xmmtmpD2 contains 8 LLRs
     xmm1 = simde_mm_abs_epi16(xmm0);
     xmm1 = simde_mm_subs_epi16(ch_magb[nb_re128-1],xmm1); // contains 8 LLRs
     xmm2 = simde_mm_abs_epi16(xmm1);
     xmm2 = simde_mm_subs_epi16(ch_magc[nb_re128-1],xmm2); // contains 8 LLRs
     // rxF[i] A0 A1 A2 A3
     // xmm0   B0 B1 B2 B3
     // xmm1   C0 C1 C2 C3
     // xmm2   D0 D1 D2 D3
     xmm3 = simde_mm_unpacklo_epi32(rxF[nb_re128-1],xmm0); // A0 B0 A1 B1
     xmm4 = simde_mm_unpackhi_epi32(rxF[nb_re128-1],xmm0); // A2 B2 A3 B3
     xmm5 = simde_mm_unpacklo_epi32(xmm1,xmm2);   // C0 D0 C1 D1
     xmm6 = simde_mm_unpackhi_epi32(xmm1,xmm2);   // C2 D2 C3 D3

     llr128[0] = simde_mm_unpacklo_epi64(xmm3,xmm5); // A0 B0 C0 D0
     llr128[1] = simde_mm_unpackhi_epi64(xmm3,xmm5); // A1 B1 C1 D1
     llr128[2] = simde_mm_unpacklo_epi64(xmm4,xmm6); // A2 B2 C2 D2
     llr128[3] = simde_mm_unpackhi_epi64(xmm4,xmm6); // A3 B3 C3 D3
     llr128+=4;
  }
  if (len_mod8 == 6) {
     int nb_re64 = nb_re>>1;
     simde__m64 *llr64 = (simde__m64 *)llr128;
     simde__m64 xmm0,xmm1,xmm2;
     simde__m64 *rxF = (simde__m64*)rxdataF_comp;
     simde__m64 *ch_mag  = (simde__m64*)&ul_ch_mag[(symbol*(off+(nb_rb*12)))];
     simde__m64 *ch_magb = (simde__m64*)&ul_ch_magb[(symbol*(off+(nb_rb*12)))];
     simde__m64 *ch_magc = (simde__m64*)&ul_ch_magc[(symbol*(off+(nb_rb*12)))];

     xmm0 = simde_mm_abs_pi16(rxF[nb_re64-1]); // registers of even index in xmm0-> |y_R|, registers of odd index in xmm0-> |y_I|
     xmm0 = simde_mm_subs_pi16(ch_mag[nb_re-1],xmm0); // registers of even index in xmm0-> |y_R|-|h|^2, registers of odd index in xmm0-> |y_I|-|h|^2
      //  xmmtmpD2 contains 4 LLRs
     xmm1 = simde_mm_abs_pi16(xmm0);
     xmm1 = simde_mm_subs_pi16(ch_magb[nb_re64-1],xmm1); // contains 4 LLRs
     xmm2 = simde_mm_abs_pi16(xmm1);
     xmm2 = simde_mm_subs_pi16(ch_magc[nb_re64-1],xmm2); // contains 4 LLRs
     // rxF[i] A0 A1
     // xmm0   B0 B1
     // xmm1   C0 C1
     // xmm2   D0 D1
     llr64[0] = simde_m_punpckldq(rxF[nb_re64-1],xmm0); // A0 B0
     llr64[2] = simde_m_punpckhdq(rxF[nb_re64-1],xmm0);  // A1 B1
     llr64[1] = simde_m_punpckldq(xmm1,xmm2);         // C0 D0
     llr64[3] = simde_m_punpckhdq(xmm1,xmm2);         // C1 D1
  }

}
void nr_ulsch_compute_llr(int32_t *rxdataF_comp,
                          int32_t *ul_ch_mag,
                          int32_t *ul_ch_magb,
                          int32_t *ul_ch_magc,
                          int16_t *ulsch_llr,
                          uint32_t nb_rb,
                          uint32_t nb_re,
                          uint8_t  symbol,
                          uint8_t  mod_order)
{
  switch(mod_order){
    case 2:
      nr_ulsch_qpsk_llr(rxdataF_comp,
                        ulsch_llr,
                        nb_re,
                        symbol);
      break;
    case 4:
      nr_ulsch_16qam_llr(rxdataF_comp,
                         ul_ch_mag,
                         ulsch_llr,
                         nb_rb,
                         nb_re,
                         symbol);
      break;
    case 6:
    nr_ulsch_64qam_llr(rxdataF_comp,
                       ul_ch_mag,
                       ul_ch_magb,
                       ulsch_llr,
                       nb_rb,
                       nb_re,
                       symbol);
      break;
    case 8:
    nr_ulsch_256qam_llr(rxdataF_comp,
                        ul_ch_mag,
                        ul_ch_magb,
                        ul_ch_magc,
                        ulsch_llr,
                        nb_rb,
                        nb_re,
                        symbol);
      break;
    default:
      AssertFatal(1==0,"nr_ulsch_compute_llr: invalid Qm value, symbol = %d, Qm = %d\n",symbol, mod_order);
      break;
  }
}

/*
 * This function computes the LLRs of stream 0 (s_0) in presence of the interfering stream 1 (s_1) assuming that both symbols are
 * QPSK. It can be used for both MU-MIMO interference-aware receiver or for SU-MIMO receivers.
 *
 * Input:
 *   stream0_in:  MF filter output for 1st stream, i.e., y0' = h0'*y0
 *   stream1_in:  MF filter output for 2nd stream, i.e., y1' = h1'*y0
 *   rho01:       Channel cross correlation, i.e., rho01 = h0'*h1
 *   length:      Number of resource elements
 *
 * Output:
 *   stream0_out: Output LLRs for 1st stream
 */
void nr_ulsch_qpsk_qpsk(c16_t *stream0_in, c16_t *stream1_in, c16_t *stream0_out, c16_t *rho01, uint32_t length)
{
  __m128i *rho01_128i = (__m128i *)rho01;
  __m128i *stream0_128i_in = (__m128i *)stream0_in;
  __m128i *stream1_128i_in = (__m128i *)stream1_in;
  __m128i *stream0_128i_out = (__m128i *)stream0_out;
  __m128i ONE_OVER_2_SQRT_2 = _mm_set1_epi16(23170); // round(2 ^ 16 / (2 * sqrt(2)))

  // In each iteration, we take 8 complex symbols
  for (int i = 0; i < length >> 2; i += 2) {

    /// Compute real and imaginary parts of MF output for stream 0 (desired stream)

    // Put xmm0 = [Re(0,1) Re(2,3) Im(0,1) Im(2,3)]
    __m128i xmm0 = stream0_128i_in[i];            // 4 symbols
    xmm0 = simde_mm_shufflelo_epi16(xmm0, 0xd8);  //_MM_SHUFFLE(0,2,1,3));
    xmm0 = simde_mm_shufflehi_epi16(xmm0, 0xd8);  //_MM_SHUFFLE(0,2,1,3));
    xmm0 = simde_mm_shuffle_epi32(xmm0, 0xd8);    //_MM_SHUFFLE(0,2,1,3));

    // Put xmm1 = [Re(4,5) Re(6,7) Im(4,5) Im(6,7)]
    __m128i xmm1 = stream0_128i_in[i + 1];        // 4 symbols
    xmm1 = simde_mm_shufflelo_epi16(xmm1, 0xd8);  //_MM_SHUFFLE(0,2,1,3));
    xmm1 = simde_mm_shufflehi_epi16(xmm1, 0xd8);  //_MM_SHUFFLE(0,2,1,3));
    xmm1 = simde_mm_shuffle_epi32(xmm1, 0xd8);    //_MM_SHUFFLE(0,2,1,3));

    __m128i y0r = simde_mm_unpacklo_epi64(xmm0, xmm1);  // y0r = Re(y0)
    __m128i y0i = simde_mm_unpackhi_epi64(xmm0, xmm1);  // y0i = Im(y0)

    __m128i y0r_over2 = simde_mm_mulhi_epi16(y0r, ONE_OVER_2_SQRT_2);
    y0r_over2 = _mm_slli_epi16(y0r_over2, 1); // y0r_over2 = Re(y0) / sqrt(2)
    __m128i y0i_over2 = simde_mm_mulhi_epi16(y0i, ONE_OVER_2_SQRT_2);
    y0i_over2 = _mm_slli_epi16(y0i_over2, 1); // y0i_over2 = Im(y0) / sqrt(2)

    /// Compute real and imaginary parts of MF output for stream 1 (interference stream)

    // Put xmm0 = [Re(0,1) Re(2,3) Im(0,1) Im(2,3)]
    xmm0 = stream1_128i_in[i];                    // 4 symbols
    xmm0 = simde_mm_shufflelo_epi16(xmm0, 0xd8);  //_MM_SHUFFLE(0,2,1,3));
    xmm0 = simde_mm_shufflehi_epi16(xmm0, 0xd8);  //_MM_SHUFFLE(0,2,1,3));
    xmm0 = simde_mm_shuffle_epi32(xmm0, 0xd8);    //_MM_SHUFFLE(0,2,1,3));

    // Put xmm1 = [Re(4,5) Re(6,7) Im(4,5) Im(6,7)]
    xmm1 = stream1_128i_in[i + 1];                // 4 symbols
    xmm1 = simde_mm_shufflelo_epi16(xmm1, 0xd8);  //_MM_SHUFFLE(0,2,1,3));
    xmm1 = simde_mm_shufflehi_epi16(xmm1, 0xd8);  //_MM_SHUFFLE(0,2,1,3));
    xmm1 = simde_mm_shuffle_epi32(xmm1, 0xd8);    //_MM_SHUFFLE(0,2,1,3));

    __m128i y1r = simde_mm_unpacklo_epi64(xmm0, xmm1);  // y1r = Re(y1)
    __m128i y1i = simde_mm_unpackhi_epi64(xmm0, xmm1);  // y1i = Im(y1)
    __m128i y1r_over2 = simde_mm_srai_epi16(y1r, 1);          // y1r_over2 = Re(y1) / 2
    __m128i y1i_over2 = simde_mm_srai_epi16(y1i, 1);          // y1i_over2 = Im(y1) / 2

    /// Get real and imaginary parts of rho

    // Put xmm0 = [Re(0,1) Re(2,3) Im(0,1) Im(2,3)]
    xmm0 = rho01_128i[i];                         // 4 symbols
    xmm0 = simde_mm_shufflelo_epi16(xmm0, 0xd8);  //_MM_SHUFFLE(0,2,1,3));
    xmm0 = simde_mm_shufflehi_epi16(xmm0, 0xd8);  //_MM_SHUFFLE(0,2,1,3));
    xmm0 = simde_mm_shuffle_epi32(xmm0, 0xd8);    //_MM_SHUFFLE(0,2,1,3));

    // Put xmm1 = [Re(4,5) Re(6,7) Im(4,5) Im(6,7)]
    xmm1 = rho01_128i[i + 1];             // 4 symbols
    xmm1 = simde_mm_shufflelo_epi16(xmm1, 0xd8);  //_MM_SHUFFLE(0,2,1,3));
    xmm1 = simde_mm_shufflehi_epi16(xmm1, 0xd8);  //_MM_SHUFFLE(0,2,1,3));
    xmm1 = simde_mm_shuffle_epi32(xmm1, 0xd8);    //_MM_SHUFFLE(0,2,1,3));

    __m128i rhor = simde_mm_unpacklo_epi64(xmm0, xmm1); // rhor = Re(rho)
    __m128i rhoi = simde_mm_unpackhi_epi64(xmm0, xmm1); // rhoi = Im(rho)

    /// Compute |psi_r| and |psi_i|

    // psi_r = rhor * xR + rhoi * xI
    // psi_i = rhor * xI - rhoi * xR

    // Put (rho_r + rho_i)/(2*sqrt(2)) in rho_p
    // rhor * xR + rhoi * xI  --> xR = 1/sqrt(2) and xI = 1/sqrt(2)
    // rhor * xI - rhoi * xR  --> xR = -1/sqrt(2) and xI = 1/sqrt(2)
    __m128i rho_p = simde_mm_adds_epi16(rhor, rhoi);        // rho_p = Re(rho) + Im(rho)
    rho_p = simde_mm_mulhi_epi16(rho_p, ONE_OVER_2_SQRT_2); // rho_p = rho_p / (2*sqrt(2))

    // Put (rho_r - rho_i)/(2*sqrt(2)) in rho_m
    // rhor * xR + rhoi * xI  --> xR = 1/sqrt(2) and xI = -1/sqrt(2)
    // rhor * xI - rhoi * xR  --> xR = 1/sqrt(2) and xI = 1/sqrt(2)
    __m128i rho_m = simde_mm_subs_epi16(rhor, rhoi);        // rho_m = Re(rho) - Im(rho)
    rho_m = simde_mm_mulhi_epi16(rho_m, ONE_OVER_2_SQRT_2); // rho_m = rho_m / (2*sqrt(2))

    // xR = 1/sqrt(2) and xI = 1/sqrt(2)
    __m128i abs_psi_rpm = simde_mm_subs_epi16(rho_p, y1r_over2);  // psi_rpm = rho_p - y1r/2
    abs_psi_rpm = simde_mm_abs_epi16(abs_psi_rpm);                   // abs_psi_rpm = |psi_rpm|

    // xR = 1/sqrt(2) and xI = 1/sqrt(2)
    __m128i abs_psi_imm = simde_mm_subs_epi16(rho_m, y1i_over2);  // psi_imm = rho_m - y1i/2
    abs_psi_imm = simde_mm_abs_epi16(abs_psi_imm);                   // abs_psi_imm = |psi_imm|

    // xR = 1/sqrt(2) and xI = -1/sqrt(2)
    __m128i abs_psi_rmm = simde_mm_subs_epi16(rho_m, y1r_over2);  // psi_rmm = rho_m - y1r/2
    abs_psi_rmm = simde_mm_abs_epi16(abs_psi_rmm);                   // abs_psi_rmm = |psi_rmm|

    // xR = -1/sqrt(2) and xI = 1/sqrt(2)
    __m128i abs_psi_ipm = simde_mm_subs_epi16(rho_p, y1i_over2);  // psi_ipm = rho_p - y1i/2
    abs_psi_ipm = simde_mm_abs_epi16(abs_psi_ipm);                   // abs_psi_ipm = |psi_ipm|

    // xR = -1/sqrt(2) and xI = -1/sqrt(2)
    __m128i abs_psi_rpp = simde_mm_adds_epi16(rho_p, y1r_over2);  // psi_rpp = rho_p + y1r/2
    abs_psi_rpp = simde_mm_abs_epi16(abs_psi_rpp);                   // abs_psi_rpp = |psi_rpp|

    // xR = -1/sqrt(2) and xI = -1/sqrt(2)
    __m128i abs_psi_imp = simde_mm_adds_epi16(rho_m, y1i_over2);  // psi_imp = rho_m + y1i/2
    abs_psi_imp = simde_mm_abs_epi16(abs_psi_imp);                   // abs_psi_imp = |psi_imp|

    // xR = -1/sqrt(2) and xI = 1/sqrt(2)
    __m128i abs_psi_rmp = simde_mm_adds_epi16(rho_m, y1r_over2);  // psi_rmp = rho_m + y1r/2
    abs_psi_rmp = simde_mm_abs_epi16(abs_psi_rmp);                   // abs_psi_rmp = |psi_rmp|

    // xR = 1/sqrt(2) and xI = -1/sqrt(2)
    __m128i abs_psi_ipp = simde_mm_adds_epi16(rho_p, y1i_over2);  // psi_ipm = rho_p + y1i/2
    abs_psi_ipp = simde_mm_abs_epi16(abs_psi_ipp);                   // abs_psi_ipp = |psi_ipm|

    /// Compute bit metrics (lambda)

    // lambda = max { |psi_r - y1r| * |x2R| + |psi_i - y1i| * |x2I| + y0r * xR + y0i * xI}

    // xR = 1/sqrt(2) and xI = 1/sqrt(2)
    // For numerator: bit_met_num_re_p = abs_psi_rpm + abs_psi_imm + y0r/sqrt(2) + y0i/sqrt(2)
    __m128i bit_met_num_re_p = simde_mm_adds_epi16(abs_psi_rpm, abs_psi_imm);
    bit_met_num_re_p = simde_mm_adds_epi16(bit_met_num_re_p, y0r_over2);
    bit_met_num_re_p = simde_mm_adds_epi16(bit_met_num_re_p, y0i_over2);

    // xR = 1/sqrt(2) and xI = -1/sqrt(2)
    // For numerator: bit_met_num_re_m = abs_psi_rmm + abs_psi_ipp + y0r/sqrt(2) - y0i/sqrt(2)
    __m128i bit_met_num_re_m = simde_mm_adds_epi16(abs_psi_rmm, abs_psi_ipp);
    bit_met_num_re_m = simde_mm_adds_epi16(bit_met_num_re_m, y0r_over2);
    bit_met_num_re_m = simde_mm_subs_epi16(bit_met_num_re_m, y0i_over2);

    // xR = -1/sqrt(2) and xI = 1/sqrt(2)
    // For denominator: bit_met_den_re_p = abs_psi_rmp + abs_psi_ipm - y0r/sqrt(2) + y0i/sqrt(2)
    __m128i bit_met_den_re_p = simde_mm_adds_epi16(abs_psi_rmp, abs_psi_ipm);
    bit_met_den_re_p = simde_mm_subs_epi16(bit_met_den_re_p, y0r_over2);
    bit_met_den_re_p = simde_mm_adds_epi16(bit_met_den_re_p, y0i_over2);

    // xR = -1/sqrt(2) and xI = -1/sqrt(2)
    // For denominator: bit_met_den_re_m = abs_psi_rpp + abs_psi_imp - y0r/sqrt(2) - y0i/sqrt(2)
    __m128i bit_met_den_re_m = simde_mm_adds_epi16(abs_psi_rpp, abs_psi_imp);
    bit_met_den_re_m = simde_mm_subs_epi16(bit_met_den_re_m, y0r_over2);
    bit_met_den_re_m = simde_mm_subs_epi16(bit_met_den_re_m, y0i_over2);

    // xR = 1/sqrt(2) and xI = 1/sqrt(2)
    // For numerator: bit_met_num_im_p = abs_psi_rpm + abs_psi_imm + y0r/sqrt(2) + y0i/sqrt(2)
    __m128i bit_met_num_im_p = simde_mm_adds_epi16(abs_psi_rpm, abs_psi_imm);
    bit_met_num_im_p = simde_mm_adds_epi16(bit_met_num_im_p, y0r_over2);
    bit_met_num_im_p = simde_mm_adds_epi16(bit_met_num_im_p, y0i_over2);

    // xR = -1/sqrt(2) and xI = 1/sqrt(2)
    // For numerator: bit_met_num_im_m = abs_psi_rmp + abs_psi_ipm - y0r/sqrt(2) + y0i/sqrt(2)
    __m128i bit_met_num_im_m = simde_mm_adds_epi16(abs_psi_rmp, abs_psi_ipm);
    bit_met_num_im_m = simde_mm_subs_epi16(bit_met_num_im_m, y0r_over2);
    bit_met_num_im_m = simde_mm_adds_epi16(bit_met_num_im_m, y0i_over2);

    // xR = 1/sqrt(2) and xI = -1/sqrt(2)
    // For denominator: bit_met_den_im_p = abs_psi_rmm + abs_psi_ipp + y0r/sqrt(2) - y0i/sqrt(2)
    __m128i bit_met_den_im_p = simde_mm_adds_epi16(abs_psi_rmm, abs_psi_ipp);
    bit_met_den_im_p = simde_mm_adds_epi16(bit_met_den_im_p, y0r_over2);
    bit_met_den_im_p = simde_mm_subs_epi16(bit_met_den_im_p, y0i_over2);

    // xR = -1/sqrt(2) and xI = -1/sqrt(2)
    // For denominator: bit_met_den_im_m = abs_psi_rpp + abs_psi_imp - y0r/sqrt(2)- y0i/sqrt(2)
    __m128i bit_met_den_im_m = simde_mm_adds_epi16(abs_psi_rpp, abs_psi_imp);
    bit_met_den_im_m = simde_mm_subs_epi16(bit_met_den_im_m, y0r_over2);
    bit_met_den_im_m = simde_mm_subs_epi16(bit_met_den_im_m, y0i_over2);

    /// Compute the LLRs

    // LLR = lambda(c==1) - lambda(c==0)

    __m128i logmax_num_re0 = simde_mm_max_epi16(bit_met_num_re_p, bit_met_num_re_m); // LLR of the first bit: Bit = 1
    __m128i logmax_den_re0 = simde_mm_max_epi16(bit_met_den_re_p, bit_met_den_re_m); // LLR of the first bit: Bit = 0
    __m128i logmax_num_im0 = simde_mm_max_epi16(bit_met_num_im_p, bit_met_num_im_m); // LLR of the second bit: Bit = 1
    __m128i logmax_den_im0 = simde_mm_max_epi16(bit_met_den_im_p, bit_met_den_im_m); // LLR of the second bit: Bit = 0

    y0r = simde_mm_subs_epi16(logmax_num_re0, logmax_den_re0);  // LLR of first bit [L1(1), L1(2), L1(3), L1(4)]
    y0i = simde_mm_subs_epi16(logmax_num_im0, logmax_den_im0);  // LLR of second bit [L2(1), L2(2), L2(3), L2(4)]

    // [L1(1), L2(1), L1(2), L2(2)]
    simde_mm_storeu_si128(&stream0_128i_out[i], simde_mm_unpacklo_epi16(y0r, y0i));

    // false if only 2 REs remain
    if (i < ((length >> 1) - 1)) {
      simde_mm_storeu_si128(&stream0_128i_out[i + 1], simde_mm_unpackhi_epi16(y0r, y0i));
    }
  }

  _mm_empty();
  _m_empty();
}


void nr_ulsch_compute_ML_llr(int32_t **rxdataF_comp,
                             int32_t ***rho,
                             int16_t **llr_layers,
                             uint8_t nb_antennas_rx,
                             uint32_t rb_size,
                             uint32_t nb_re,
                             uint8_t symbol,
                             uint32_t rxdataF_ext_offset,
                             uint8_t mod_order)
{
  int off = ((rb_size & 1) == 1) ? 4 : 0;
  c16_t *rxdataF_comp0 = (c16_t *)&rxdataF_comp[0][symbol * (off + (rb_size * NR_NB_SC_PER_RB))];
  c16_t *rxdataF_comp1 = (c16_t *)&rxdataF_comp[nb_antennas_rx][symbol * (off + (rb_size * NR_NB_SC_PER_RB))];
  c16_t *llr_layers0 = (c16_t *)&llr_layers[0][rxdataF_ext_offset * mod_order];
  c16_t *llr_layers1 = (c16_t *)&llr_layers[1][rxdataF_ext_offset * mod_order];
  c16_t *rho0 = (c16_t *)&rho[0][1][symbol * (off + (rb_size * NR_NB_SC_PER_RB))];
  c16_t *rho1 = (c16_t *)&rho[0][2][symbol * (off + (rb_size * NR_NB_SC_PER_RB))];

  switch (mod_order) {
    case 2:
      nr_ulsch_qpsk_qpsk(rxdataF_comp0, rxdataF_comp1, llr_layers0, rho0, nb_re);
      nr_ulsch_qpsk_qpsk(rxdataF_comp1, rxdataF_comp0, llr_layers1, rho1, nb_re);
      break;
    case 4:
    case 6:
      AssertFatal(1 == 0, "LLR computation is not implemented yet for ML with Qm = %d\n", mod_order);
    default:
      AssertFatal(1 == 0, "nr_ulsch_compute_llr: invalid Qm value, symbol = %d, Qm = %d\n", symbol, mod_order);
  }
}

void nr_ulsch_shift_llr(int16_t **llr_layers, uint32_t nb_re, uint32_t rxdataF_ext_offset, uint8_t mod_order, int shift)
{
  __m128i *llr_layers0 = (__m128i *)&llr_layers[0][rxdataF_ext_offset * mod_order];
  __m128i *llr_layers1 = (__m128i *)&llr_layers[1][rxdataF_ext_offset * mod_order];

  uint8_t mem_offset = ((16 - ((long)llr_layers0)) & 0xF) >> 2;

  if (mem_offset > 0) {
    c16_t *llr_layers0_c16 = (c16_t *)&llr_layers[0][rxdataF_ext_offset * mod_order];
    c16_t *llr_layers1_c16 = (c16_t *)&llr_layers[1][rxdataF_ext_offset * mod_order];
    for (int i = 0; i < mem_offset; i++) {
      llr_layers0_c16[i] = c16Shift(llr_layers0_c16[i], shift);
      llr_layers1_c16[i] = c16Shift(llr_layers1_c16[i], shift);
    }
    llr_layers0 = (__m128i *)&llr_layers[0][rxdataF_ext_offset * mod_order + (mem_offset << 1)];
    llr_layers1 = (__m128i *)&llr_layers[1][rxdataF_ext_offset * mod_order + (mem_offset << 1)];
  }

  for (int i = 0; i < nb_re >> 2; i++) {
    llr_layers0[i] = simde_mm_srai_epi16(llr_layers0[i], shift);
    llr_layers1[i] = simde_mm_srai_epi16(llr_layers1[i], shift);
  }
}