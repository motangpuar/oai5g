<table style="border-collapse: collapse; border: none;">
  <tr style="border-collapse: collapse; border: none;">
    <td style="border-collapse: collapse; border: none;">
      <a href="http://www.openairinterface.org/">
         <img src="./images/oai_final_logo.png" alt="" border=3 height=50 width=150>
         </img>
      </a>
    </td>
    <td style="border-collapse: collapse; border: none; vertical-align: center;">
      <b><font size = "5">OAI 5G NR SA tutorial with multiple OAI nrUE in RFSIM</font></b>
    </td>
  </tr>
</table>

**Table of Contents**

[[_TOC_]]

#  1. Scenario
In this tutorial we describe how to configure and run a 5G end-to-end setup with OAI CN5G, OAI gNB with single/multiple OAI nrUE(s) in the RFsimulator.

Minimum hardware requirements:
- Laptop/Desktop/Server for OAI CN5G and OAI gNB and UE
    - Operating System: [Ubuntu 22.04 LTS](https://releases.ubuntu.com/22.04/ubuntu-22.04.3-desktop-amd64.iso)
    - CPU: 8 cores x86_64 @ 3.5 GHz
    - RAM: 32 GB




# 2. OAI CN5G

## 2.1 OAI CN5G pre-requisites

Please install and configure OAI CN5G as described here:
[OAI CN5G](NR_SA_Tutorial_OAI_CN5G.md)


# 3. OAI gNB and OAI nrUE

## 3.1 OAI gNB and OAI nrUE pre-requisites

### Build UHD from source
```bash
sudo apt install -y libboost-all-dev libusb-1.0-0-dev doxygen python3-docutils python3-mako python3-numpy python3-requests python3-ruamel.yaml python3-setuptools cmake build-essential

git clone https://github.com/EttusResearch/uhd.git ~/uhd
cd ~/uhd
git checkout v4.5.0.0
cd host
mkdir build
cd build
cmake ../
make -j $(nproc)
make test # This step is optional
sudo make install
sudo ldconfig
sudo uhd_images_downloader
```

## 3.2 Build OAI gNB and OAI nrUE

```bash
# Get openairinterface5g source code
git clone https://gitlab.eurecom.fr/oai/openairinterface5g.git ~/openairinterface5g
cd ~/openairinterface5g
git checkout develop

# Install OAI dependencies
cd ~/openairinterface5g/cmake_targets
./build_oai -I

# nrscope dependencies
sudo apt install -y libforms-dev libforms-bin

# Build OAI gNB
cd ~/openairinterface5g
source oaienv
cd cmake_targets
./build_oai -w USRP --ninja --nrUE --gNB --build-lib telnetsrv "nrscope" -C
```

# 4. Run OAI CN5G and OAI gNB

## 4.1 Run OAI CN5G

```bash
cd ~/oai-cn5g
docker compose up -d
```

## 4.2 Run OAI gNB 


### RFsimulator
- Add the channel models configuration file to where your other configuration files are (as an example openairinterface5g/targets/PROJECTS/GENERIC-NR-5GC/CONF/channelmod_rfsimu.conf).
- Below is the tested configuration for channelmod_rfsimu.conf with RFsimulator.

```bash
#/* configuration for channel modelisation */
#/* To be included in main config file when */
#/* channel modelisation is used (rfsimulator with chanmod options enabled) */
channelmod = { 
  max_chan=10;
  modellist="modellist_rfsimu_1";
  modellist_rfsimu_1 = (
    {
        model_name                       = "rfsimu_channel_enB0"
      	type                             = "AWGN";			  
      	ploss_dB                         = 0;
        noise_power_dB                   = -10; 
        forgetfact                       = 0;  
        offset                           = 0;      
        ds_tdl                           = 0;      
    },
    {
        model_name                       = "rfsimu_channel_ue0"
      	type                             = "AWGN";			  
      	ploss_dB                         = 0;
        noise_power_dB                   = -20; 
        forgetfact                       = 0;  
        offset                           = 0;      
        ds_tdl                           = 0;      
    }    
  );
  modellist_rfsimu_2 = (
    {
        model_name                       = "rfsimu_channel_ue0"
      	type                             = "AWGN";			  
      	ploss_dB                         = 0;
        noise_power_dB                   = 0; 
        forgetfact                       = 0;  
        offset                           = 0;      
        ds_tdl                           = 0;      
    },
    {
        model_name                       = "rfsimu_channel_ue1"
      	type                             = "AWGN";			  
      	ploss_dB                         = 0;
        noise_power_dB                   = 0; 
        forgetfact                       = 0;  
        offset                           = 0;      
        ds_tdl                           = 0;      
    },
    {
        model_name                       = "rfsimu_channel_ue2"
      	type                             = "AWGN";			  
      	ploss_dB                         = 0;
        noise_power_dB                   = 0; 
        forgetfact                       = 0;  
        offset                           = 0;      
        ds_tdl                           = 0;      
    }    
  );  
};

```

- Edit your gnb.sa.band78.fr1.106PRB.usrpb210.conf (as an example openairinterface5g/targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band78.fr1.106PRB.usrpb210.conf) by adding
```bash
min_rxtxtime                                              = 6;
```
and

```bash
@include "channelmod_rfsimu.conf"
```
to your configuration file.

- Below is the tested configuration for gnb.sa.band78.fr1.106PRB.usrpb210.conf with RFsimulator.
```bash
Active_gNBs = ( "gNB-OAI");
# Asn1_verbosity, choice in: none, info, annoying
Asn1_verbosity = "none";

gNBs =
(
 {
    ////////// Identification parameters:
    gNB_ID    =  0xe00;
    gNB_name  =  "gNB-OAI";

    // Tracking area code, 0x0000 and 0xfffe are reserved values
    tracking_area_code  =  1;
    plmn_list = ({ mcc = 001; mnc = 01; mnc_length = 2; snssaiList = ({ sst = 1; }) });

    nr_cellid = 12345678L;

    ////////// Physical parameters:

    do_CSIRS                                                  = 1;
    do_SRS                                                    = 0;
    min_rxtxtime                                              = 6;
    servingCellConfigCommon = (
    {
 #spCellConfigCommon

      physCellId                                                    = 0;

#  downlinkConfigCommon
    #frequencyInfoDL
      # this is 3600 MHz + 43 PRBs@30kHz SCS (same as initial BWP)
      absoluteFrequencySSB                                             = 641280;
      dl_frequencyBand                                                 = 78;
      # this is 3600 MHz
      dl_absoluteFrequencyPointA                                       = 640008;
      #scs-SpecificCarrierList
        dl_offstToCarrier                                              = 0;
# subcarrierSpacing
# 0=kHz15, 1=kHz30, 2=kHz60, 3=kHz120
        dl_subcarrierSpacing                                           = 1;
        dl_carrierBandwidth                                            = 106;
     #initialDownlinkBWP
      #genericParameters
        # this is RBstart=27,L=48 (275*(L-1))+RBstart
        initialDLBWPlocationAndBandwidth                               = 28875; # 6366 12925 12956 28875 12952
# subcarrierSpacing
# 0=kHz15, 1=kHz30, 2=kHz60, 3=kHz120
        initialDLBWPsubcarrierSpacing                                   = 1;
      #pdcch-ConfigCommon
        initialDLBWPcontrolResourceSetZero                              = 12;
        initialDLBWPsearchSpaceZero                                     = 0;

  #uplinkConfigCommon
     #frequencyInfoUL
      ul_frequencyBand                                              = 78;
      #scs-SpecificCarrierList
      ul_offstToCarrier                                             = 0;
# subcarrierSpacing
# 0=kHz15, 1=kHz30, 2=kHz60, 3=kHz120
      ul_subcarrierSpacing                                          = 1;
      ul_carrierBandwidth                                           = 106;
      pMax                                                          = 20;
     #initialUplinkBWP
      #genericParameters
        initialULBWPlocationAndBandwidth                            = 28875;
# subcarrierSpacing
# 0=kHz15, 1=kHz30, 2=kHz60, 3=kHz120
        initialULBWPsubcarrierSpacing                               = 1;
      #rach-ConfigCommon
        #rach-ConfigGeneric
          prach_ConfigurationIndex                                  = 98;
#prach_msg1_FDM
#0 = one, 1=two, 2=four, 3=eight
          prach_msg1_FDM                                            = 0;
          prach_msg1_FrequencyStart                                 = 0;
          zeroCorrelationZoneConfig                                 = 13;
          preambleReceivedTargetPower                               = -96;
#preamblTransMax (0...10) = (3,4,5,6,7,8,10,20,50,100,200)
          preambleTransMax                                          = 6;
#powerRampingStep
# 0=dB0,1=dB2,2=dB4,3=dB6
        powerRampingStep                                            = 1;
#ra_ReponseWindow
#1,2,4,8,10,20,40,80
        ra_ResponseWindow                                           = 4;
#ssb_perRACH_OccasionAndCB_PreamblesPerSSB_PR
#1=oneeighth,2=onefourth,3=half,4=one,5=two,6=four,7=eight,8=sixteen
        ssb_perRACH_OccasionAndCB_PreamblesPerSSB_PR                = 4;
#oneHalf (0..15) 4,8,12,16,...60,64
        ssb_perRACH_OccasionAndCB_PreamblesPerSSB                   = 14;
#ra_ContentionResolutionTimer
#(0..7) 8,16,24,32,40,48,56,64
        ra_ContentionResolutionTimer                                = 7;
        rsrp_ThresholdSSB                                           = 19;
#prach-RootSequenceIndex_PR
#1 = 839, 2 = 139
        prach_RootSequenceIndex_PR                                  = 2;
        prach_RootSequenceIndex                                     = 1;
        # SCS for msg1, can only be 15 for 30 kHz < 6 GHz, takes precendence over the one derived from prach-ConfigIndex
        #
        msg1_SubcarrierSpacing                                      = 1,
# restrictedSetConfig
# 0=unrestricted, 1=restricted type A, 2=restricted type B
        restrictedSetConfig                                         = 0,

        msg3_DeltaPreamble                                          = 1;
        p0_NominalWithGrant                                         =-90;

# pucch-ConfigCommon setup :
# pucchGroupHopping
# 0 = neither, 1= group hopping, 2=sequence hopping
        pucchGroupHopping                                           = 0;
        hoppingId                                                   = 40;
        p0_nominal                                                  = -90;
# ssb_PositionsInBurs_BitmapPR
# 1=short, 2=medium, 3=long
      ssb_PositionsInBurst_PR                                       = 2;
      ssb_PositionsInBurst_Bitmap                                   = 1;

# ssb_periodicityServingCell
# 0 = ms5, 1=ms10, 2=ms20, 3=ms40, 4=ms80, 5=ms160, 6=spare2, 7=spare1
      ssb_periodicityServingCell                                    = 2;

# dmrs_TypeA_position
# 0 = pos2, 1 = pos3
      dmrs_TypeA_Position                                           = 0;

# subcarrierSpacing
# 0=kHz15, 1=kHz30, 2=kHz60, 3=kHz120
      subcarrierSpacing                                             = 1;


  #tdd-UL-DL-ConfigurationCommon
# subcarrierSpacing
# 0=kHz15, 1=kHz30, 2=kHz60, 3=kHz120
      referenceSubcarrierSpacing                                    = 1;
      # pattern1
      # dl_UL_TransmissionPeriodicity
      # 0=ms0p5, 1=ms0p625, 2=ms1, 3=ms1p25, 4=ms2, 5=ms2p5, 6=ms5, 7=ms10
      dl_UL_TransmissionPeriodicity                                 = 6;
      nrofDownlinkSlots                                             = 7;
      nrofDownlinkSymbols                                           = 6;
      nrofUplinkSlots                                               = 2;
      nrofUplinkSymbols                                             = 4;

      ssPBCH_BlockPower                                             = -25;
  }

  );


    # ------- SCTP definitions
    SCTP :
    {
        # Number of streams to use in input/output
        SCTP_INSTREAMS  = 2;
        SCTP_OUTSTREAMS = 2;
    };


    ////////// AMF parameters:
    amf_ip_address      = ( { ipv4       = "192.168.70.132";
                              ipv6       = "192:168:30::17";
                              active     = "yes";
                              preference = "ipv4";
                            }
                          );


    NETWORK_INTERFACES :
    {
        GNB_INTERFACE_NAME_FOR_NG_AMF            = "demo-oai";
        GNB_IPV4_ADDRESS_FOR_NG_AMF              = "192.168.70.129/24";
        GNB_INTERFACE_NAME_FOR_NGU               = "demo-oai";
        GNB_IPV4_ADDRESS_FOR_NGU                 = "192.168.70.129/24";
        GNB_PORT_FOR_S1U                         = 2152; # Spec 2152
    };

  }
);

MACRLCs = (
{
  num_cc                      = 1;
  tr_s_preference             = "local_L1";
  tr_n_preference             = "local_RRC";
  pusch_TargetSNRx10          = 150;
  pucch_TargetSNRx10          = 200;
 # pusch_TargetSNRx10          =100;
 # pucch_TargetSNRx10          =150;
  ulsch_max_frame_inactivity  = 0;
  ul_max_mcs                  = 6;
 # ul_max_mcs                  = 28;
}
);

L1s = (
{
  num_cc = 1;
  tr_n_preference       = "local_mac";
  prach_dtx_threshold   = 120;
  pucch0_dtx_threshold  = 100;
  ofdm_offset_divisor   = 8; #set this to UINT_MAX for offset 0
}
);

RUs = (
{
  local_rf       = "yes"
  nb_tx          = 1
  nb_rx          = 1
  att_tx         = 12;
  att_rx         = 12;
  bands          = [78];
  max_pdschReferenceSignalPower = -27;
  max_rxgain                    = 114;
  eNB_instances  = [0];
  #beamforming 1x4 matrix:
  bf_weights = [0x00007fff, 0x0000, 0x0000, 0x0000];
  clock_src = "internal";
}
);

THREAD_STRUCT = (
{
  #three config for level of parallelism "PARALLEL_SINGLE_THREAD", "PARALLEL_RU_L1_SPLIT", or "PARALLEL_RU_L1_TRX_SPLIT"
  parallel_config    = "PARALLEL_SINGLE_THREAD";
  #two option for worker "WORKER_DISABLE" or "WORKER_ENABLE"
  worker_config      = "WORKER_ENABLE";
}
);

rfsimulator :
{
  serveraddr = "server";
  serverport = "4043";
  options = (); #("saviq"); or/and "chanmod"
  modelname = "AWGN";
  IQfile = "/tmp/rfsimulator.iqs";
};

security = {
  # preferred ciphering algorithms
  # the first one of the list that an UE supports in chosen
  # valid values: nea0, nea1, nea2, nea3
  ciphering_algorithms = ( "nea0" );

  # preferred integrity algorithms
  # the first one of the list that an UE supports in chosen
  # valid values: nia0, nia1, nia2, nia3
  integrity_algorithms = ( "nia2", "nia0" );

  # setting 'drb_ciphering' to "no" disables ciphering for DRBs, no matter
  # what 'ciphering_algorithms' configures; same thing for 'drb_integrity'
  drb_ciphering = "yes";
  drb_integrity = "no";
};

log_config :
{
  global_log_level                      ="info";
  hw_log_level                          ="info";
  phy_log_level                         ="info";
  mac_log_level                         ="info";
  rlc_log_level                         ="info";
  pdcp_log_level                        ="info";
  rrc_log_level                         ="info";
  ngap_log_level                        ="debug";
  f1ap_log_level                        ="debug";
};

e2_agent = {
  near_ric_ip_addr = "127.0.0.1";
  #sm_dir = "/path/where/the/SMs/are/located/"
  sm_dir = "/usr/local/lib/flexric/"
};

@include "channelmod_rfsimu.conf"


```
- After editing your configuration files, now you can deploy your gNB in RFsimulator as 

```bash
cd ~/openairinterface5g
source oaienv
cd cmake_targets/ran_build/build
sudo ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band78.fr1.106PRB.usrpb210.conf --rfsim --sa --nokrnmod -E --rfsimulator.options chanmod --rfsimulator.serveraddr server --telnetsrv --telnetsrv.listenport 9099
```


# 5. OAI  UE 


## 5.1 Testing OAI nrUE with single UE in RFsimulator
Important notes:
- This should be run on the same host as the OAI gNB
- It only applies when running OAI gNB with RFsimulator
- Edit the ue.conf file by adding: 
```bash
@include "channelmod_rfsimu.conf"
```
- Below is the tested configuration for ue.conf with RFsimulator.

```bash
uicc0 = {
imsi = "001010000000001";
key = "fec86ba6eb707ed08905757b1bb44b8f";
opc= "C42449363BBAD02B66D16BC975D77CC1";
dnn= "oai";
nssai_sst=1;
#nssai_sd=1;
}
@include "channelmod_rfsimu.conf"

```

- After editing your configuration files, now you can deploy your UE in RFsimulator as
```bash
cd ~/openairinterface5g
source oaienv
cd cmake_targets/ran_build/build
sudo ./nr-uesoftmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/ue.conf -r 106 --numerology 1 --band 78 -C 3619200000 --rfsim --sa --uicc0.imsi 001010000000001 --nokrnmod -E --rfsimulator.options chanmod --rfsimulator.serveraddr 127.0.0.1 --telnetsrv --telnetsrv.listenport 9095
```
# 5.2 OAI multiple UE 


## 5.1 Testing OAI nrUE with multiple UEs in RFsimulator
Important notes:
- This should be run on the same host as the OAI gNB
- It only applies when running OAI gNB with RFsimulator
- Follow the link https://www.eurecom.fr/~schmidtr/blog/2023/09/15/multiple-ues-in-rfsimulator/ and use the script (multi-ue.sh) in order to make namespaces for multiple UEs.  

- For the first UE, create the namespace ue1 (-c1) and then execute bash inside (-e):
```bash
sudo ./multi-ue.sh -c1 -e
sudo ./multi-ue.sh -o1
```
- After entering the bash environment, run the following command to deploy your first UE
```bash
cd ~/openairinterface5g
source oaienv
cd cmake_targets/ran_build/build
sudo ./nr-uesoftmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/ue.conf -r 106 --numerology 1 --band 78 -C 3619200000 --rfsim --sa --uicc0.imsi 001010000000001 --nokrnmod -E --rfsimulator.options chanmod --rfsimulator.serveraddr 10.201.1.100 --telnetsrv --telnetsrv.listenport 9095
```
- For the second UE, create the namespace ue2 (-c2) and then execute bash inside (-e):
```bash
sudo ./multi-ue.sh -c2 -e
sudo ./multi-ue.sh -o2
```
- After entering the bash environment, run the following command to deploy your second UE
```bash
cd ~/openairinterface5g
source oaienv
cd cmake_targets/ran_build/build
sudo ./nr-uesoftmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/ue.conf -r 106 --numerology 1 --band 78 -C 3619200000 --rfsim --sa --uicc0.imsi 001010000000001 --nokrnmod -E --rfsimulator.options chanmod --rfsimulator.serveraddr 10.202.1.100 --telnetsrv --telnetsrv.listenport 9096
```


### 5.3 Telnet server

-single UE in RFsimulator

- gNB host
```bash
telnet 127.0.0.1 9099
```
- UE host
```bash
telnet 127.0.0.1 9095
```
-Multiple UEs in RFsimulator

- gNB host
```bash
telnet 127.0.0.1 9099
```
- UE host
```bash
telnet 10.201.1.1 9095 ### For accessing to the first UE
telnet 10.202.1.2 9096 ### For accessing to the second UE
```
After entering to the bash environment you can type help and see the possible options to change the channelmodels and other available options in RFSIM. 
gNB side
```bash
5g@5gtestbed ~ $ telnet 127.0.0.1 9099
Trying 127.0.0.1...
Connected to 127.0.0.1.
Escape character is '^]'.

softmodem_gnb> help
   module 0 = telnet:
      telnet [get set] debug <value>
      telnet [get set] prio <value>
      telnet [get set] loopc <value>
      telnet [get set] loopd <value>
      telnet [get set] phypb <value>
      telnet [get set] hsize <value>
      telnet [get set] hfile <value>
      telnet [get set] logfile <value>
      telnet redirlog [here,file,off]
      telnet param [prio]
      telnet history [list,reset]
   module 1 = softmodem:
      softmodem show loglvl|thread|config
      softmodem log (enter help for details)
      softmodem thread (enter help for details)
      softmodem exit 
      softmodem restart 
   module 2 = loader:
      loader [get set] mainversion <value>
      loader [get set] defpath <value>
      loader [get set] maxshlibs <value>
      loader [get set] numshlibs <value>
      loader show [params,modules]
   module 3 = measur:
      measur show groups | <group name> | inq
      measur cpustats [enable | disable]
      measur async [enable | disable]
   module 4 = channelmod:
      channelmod help 
      channelmod show <predef,current>
      channelmod modify <channelid> <param> <value>
      channelmod show params <channelid> <param> <value>
   module 5 = rfsimu:
      rfsimu setmodel <model name> <model type>
      rfsimu setdistance <model name> <distance>
      rfsimu getdistance <model name>
      rfsimu vtime 
softmodem_gnb> 
```
UE side 
```bash
5g@5gtestbed ~ $ telnet 127.0.0.1 9095
Trying 127.0.0.1...
Connected to 127.0.0.1.
Escape character is '^]'.

softmodem_5Gue> help
   module 0 = telnet:
      telnet [get set] debug <value>
      telnet [get set] prio <value>
      telnet [get set] loopc <value>
      telnet [get set] loopd <value>
      telnet [get set] phypb <value>
      telnet [get set] hsize <value>
      telnet [get set] hfile <value>
      telnet [get set] logfile <value>
      telnet redirlog [here,file,off]
      telnet param [prio]
      telnet history [list,reset]
   module 1 = softmodem:
      softmodem show loglvl|thread|config
      softmodem log (enter help for details)
      softmodem thread (enter help for details)
      softmodem exit 
      softmodem restart 
   module 2 = loader:
      loader [get set] mainversion <value>
      loader [get set] defpath <value>
      loader [get set] maxshlibs <value>
      loader [get set] numshlibs <value>
      loader show [params,modules]
   module 3 = measur:
      measur show groups | <group name> | inq
      measur cpustats [enable | disable]
      measur async [enable | disable]
   module 4 = channelmod:
      channelmod help 
      channelmod show <predef,current>
      channelmod modify <channelid> <param> <value>
      channelmod show params <channelid> <param> <value>
   module 5 = rfsimu:
      rfsimu setmodel <model name> <model type>
      rfsimu setdistance <model name> <distance>
      rfsimu getdistance <model name>
      rfsimu vtime 
softmodem_5Gue> 
```