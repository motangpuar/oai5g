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
In this tutorial we describe how to configure and run a 5G end-to-end setup with OAI CN5G, OAI gNB and multiple OAI nrUE in RFSIM.

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

## 4.2 Run OAI gNB with single/multiple UE(s) in RFSIM


### RFsimulator
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

Run OAI nrUE with RFsimulator
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

Run OAI first nrUE with RFsimulator
- For the first UE, create the namespace ue1 (-c1) and then execute bash inside (-e):
```bash
sudo ./multi-ue.sh -c1 -e
sudo ./multi-ue.sh -o1
```
After entering the #bash environment:
```bash
sudo ./nr-uesoftmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/ue.conf -r 106 --numerology 1 --band 78 -C 3619200000 --rfsim --sa --uicc0.imsi 001010000000001 --nokrnmod -E --rfsimulator.options chanmod --rfsimulator.serveraddr 10.201.1.100 --telnetsrv --telnetsrv.listenport 9095
```
- For the second UE, follow the same procedure as the first ue and run the following command in the bash environment for the second UE
```bash
sudo ./nr-uesoftmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/ue.conf -r 106 --numerology 1 --band 78 -C 3619200000 --rfsim --sa --uicc0.imsi 001010000000001 --nokrnmod -E --rfsimulator.options chanmod --rfsimulator.serveraddr 10.202.1.100 --telnetsrv --telnetsrv.listenport 9096
```

### 5.3 Ping test
- UE host
```bash
ping 192.168.70.135 -I oaitun_ue1
```

### 5.4 Telnet server

-Testing OAI nrUE with single UE in RFsimulator

- gNB host
```bash
telnet 127.0.0.1 9099
```
- UE host
```bash
telnet 127.0.0.1 9095
```
-Testing OAI nrUE with multiple UEs in RFsimulator

- gNB host
```bash
telnet 127.0.0.1 9099
```
- UE host
```bash
telnet 10.201.1.1 9095 ### For accessing to the first UE
telnet 10.202.1.2 9096 ### For accessing to the second UE
```
