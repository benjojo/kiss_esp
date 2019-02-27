#include <ESP8266WiFi.h>
extern "C" {
#include "user_interface.h"
  typedef void (*freedom_outside_cb_t)(uint8 status);
  int  wifi_register_send_pkt_freedom_cb(freedom_outside_cb_t cb);
  void wifi_unregister_send_pkt_freedom_cb(void);
  int  wifi_send_pkt_freedom(uint8 *buf, uint16 len, bool sys_seq);
}

unsigned int channel = 1;
uint8_t txbuf[1500];
uint16_t txbuflen;
int rxescapemode = 0;
int txescapemode = 0;

const int RED = 15;
const int GREEN = 12;
const int BLUE = 13;


void setup() {
  pinMode(RED, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(BLUE, OUTPUT);

  Serial.begin(57600);
  wifi_set_opmode(STATION_MODE); // For various features to work, we have to be in station mode
  wifi_set_channel(channel);
  wifi_promiscuous_enable(0);
  // It's worth being aware here that this function can only
  // read 128 bytes of frame, the rest is truncated off. Very annoying
  // SDK limitiation.
  wifi_set_promiscuous_rx_cb(rh);
  wifi_promiscuous_enable(1);
}

// Function that handles incoming "wifi" frames into the device
// The goal here is for it to confirm it's a "AX.25" packet and
// if so, encode it out on the serial line for the host to use.
void rh(uint8_t *buf, uint16_t len) {
  analogWrite(GREEN, 1024);

  if (len < 50) {
    analogWrite(GREEN, 0);
    return;
  }

  // Find out AAAAAA MAC packets, and then "decode" them
  for (uint16_t i=16; i <= 16+12; i++){
    if (buf[i] != 0xAA) {
      analogWrite(GREEN, 0);
      return;
    }
  }

  // So here is the thing, if we leave promiscuous on
  // while we are decoding the packet, we will likely
  // run out of heap/ram/stack/whatever and panic.
  // so while decoding, we will drop everything coming in
  wifi_promiscuous_enable(0);


  analogWrite(BLUE, 1024);

  // Start the KISS packet over serial
  Serial.write(0xC0);

  for (uint16_t i=37; i <= len; i++) {

    // 0xDB is a reserved escape value in KISS, So if we
    // are about to send a 0xDB we need to escape it
    if(rxescapemode == 0 && buf[i] == 0xDB) {
      rxescapemode = 1;
    } else if ( rxescapemode == 0 && buf[i] != 0xDB) {
        Serial.write(buf[i]);
    } else if ( rxescapemode == 1 ) {
      // 0xC0 and 0xDB are escape chars in the KISS proto
      if (buf[i] == 0xDC) {
        Serial.write(0xC0);
      }
      if (buf[i] == 0xDD) {
        Serial.write(0xDB);
      }
      rxescapemode = 0;
    }
  }
  // End the packet
  Serial.write(0xC0);

  wifi_set_promiscuous_rx_cb(rh);
  wifi_promiscuous_enable(1);
  analogWrite(BLUE, 0);
  analogWrite(GREEN, 0);
}

void loop() {
  if (Serial.available() > 0) {
        analogWrite(RED, 1024);

    //    && (Serial.read() == '\n')
    char in = Serial.read();
    if (txescapemode == 0) {
      txbuflen++;
      txbuf[txbuflen] = in;
      if (in == 0xC0) {
        if (txbuflen > 24+8+10) {
          // Send the packet on the air
          wifi_send_pkt_freedom(txbuf,txbuflen,0);
        }
        txbuf[0] == 0x80;
        txbuf[1] == 0x62;
        
        // Set the outbound MAC to 0xAA's
        for (int i=3; i <= 21; i++){
          txbuf[i] = 0xAA;
        }

        // Fix up some of the 802.11 frame
        txbuf[22] == 0x90;
        txbuf[23] == 0x71;
        for (int i=24; i <= 24+8; i++){
          // Set inbound MAC to 0xAA's
          txbuf[i] = 0xAA;
        }
        txbuflen = 24;
      }
    } else if (txescapemode == 1) {
      // De-escape the KISS packet
      if (in == 0xDC) {
        txbuflen++;
        txbuf[txbuflen] = 0xC0;
      }
      if (in == 0xDD) {
        txbuflen++;
        txbuf[txbuflen] = 0xDB;
      }
      txescapemode = 0;
    } else if (txescapemode == 0 && in == 0xDB) {
      txescapemode = 1;
    }
    analogWrite(RED, 0);
  }
  delay(1);
}
