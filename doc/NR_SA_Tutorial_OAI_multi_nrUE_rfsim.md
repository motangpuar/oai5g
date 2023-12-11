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

## 3.1 Build OAI gNB and OAI nrUE

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
./build_oai -w USRP --ninja --nrUE --gNB --build-lib "nrscope telnetsrv" -C
```

# 4. Run OAI CN5G and OAI gNB

## 4.1 Run OAI CN5G

```bash
cd ~/oai-cn5g
docker compose up -d
```

## 4.2 Run OAI gNB 


### RFsimulator

- For the gNB configuration file, follow the path to openairinterface5g/ci-scripts/conf_files and edit the gnb.sa.band78.106prb.rfsim.conf as:
```bash
min_rxtxtime                                              = 6;
```
and add this line to the bottom of your conf file. 

```bash
@include "channelmod_rfsimu.conf"
```

You can check the example run 2 in this link:(../radio/rfsimulator/README.md)


- For the channel model configuration, follow the path to openairinterface5g/ci-scripts/conf_files and edit the channelmod_rfsimu.conf as:

In the rfsimu_channel_enB0 model part, edit the noise power as the following:
        noise_power_dB                   = -10;
and in the rfsimu_channel_ue0 model part, edit the noise power as the following:
        noise_power_dB                   = -20; 
and the rest stays the same. 

You can check the example run 1 in this link:(../radio/rfsimulator/README.md)


# 5. OAI  UE 
```bash
cd cmake_targets/ran_build/build
sudo ./nr-uesoftmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/ue.conf -r 106 --numerology 1 --band 78 -C 3619200000 --rfsim --sa --ue-fo-compensation --uicc0.imsi 001010000000001 --nokrnmod -E --rfsimulator.options chanmod --rfsimulator.serveraddr 127.0.0.1 --telnetsrv --telnetsrv.listenport 9095
```


- After editing your configuration files, now you can deploy your UE in RFsimulator as
```bash
cd ~/openairinterface5g/cmake_targets/ran_build/build
sudo ./nr-uesoftmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/ue.conf -r 106 --numerology 1 --band 78 -C 3619200000 --rfsim --sa --uicc0.imsi 001010000000001 --nokrnmod -E --rfsimulator.options chanmod --rfsimulator.serveraddr 127.0.0.1 --telnetsrv --telnetsrv.listenport 9095
```
# 5.2 OAI multiple UE 


## 5.1 Testing OAI nrUE with multiple UEs in RFsimulator
Important notes:
- This should be run on the same host as the OAI gNB
- It only applies when running OAI gNB with RFsimulator
- Use the script (multi-ue.sh) in openairinterface/radio/rfsimulator to make namespaces for 
multiple UEs.  

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
