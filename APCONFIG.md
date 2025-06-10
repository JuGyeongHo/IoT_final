# Rasbperry pi AP 설정
***
## 패키지 설치

```bash
sudo apt update
sudo apt install hostapd dnsmasq
```
***
## 1. hostapd.conf 설정


`/etc/hostapd/hostapd.conf` 설정

```conf
interface=wlan0
driver=nl80211
ssid="ap_A" # ssid 
wpa_passphrase="qeq12345" # password
hw_mode=g
channel=6
auth_algs=1
wmm_enabled=1
wpa=2
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
```

`/etc/default/hostapd` 수정

```bash
sudo nano /etc/default/hostapd
```

```conf
DAEMON_CONF="/etc/hostapd/hostapd.conf"
```
---
## 2. dnsmasq.con 설정

```bash
#기존 파일 백업
sudo mv /etc/dnsmasq.conf /etc/dnsmasq.conf.orig
#새로운 파일 설정 
sudo nano /etc/dnsmasq.conf
```

```conf
interface=wlan0
dhcp-range=192.168.105.10,192.168.105.100,12h
```
---
## 3. wlan0 IP 고정

`/etc/dhcpcd.conf` 또는 `network/interfaces`에 추가:

```conf
interface wlan0
    static ip_address=192.168.105.1/24
    nohook wpa_supplicant
```

IP forwarding 및 NAT 설정:

```bash
sudo sh -c "echo 1 > /proc/sys/net/ipv4/ip_forward"
sudo iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
```

```bash
sudo nano /etc/sysctl.conf
```

```conf
net.ipv4.ip_forward=1
```
---
## 4. Reboot 및 확인

```bash
sudo systemctl unmask hostapd
sudo systemctl enable hostapd
sudo systemctl enable dnsmasq

sudo systemctl restart dhcpcd
sudo systemctl restart hostapd
sudo systemctl restart dnsmasq
```

```bash
ip a show wlan0
journalctl -u hostapd
```