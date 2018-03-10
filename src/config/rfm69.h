// RFM96 - ESP8266
// https://bitbucket.org/xoseperez/rfm69gw/overview

#define RFM69_CS 15 // GPIO15/HCS/D8
#define RFM69_IRQ 4 // GPIO04/D2
#define RFM69_IRQN digitalPinToInterrupt(RFM69_IRQ)
#define RFM69_RST 2 // GPIO02/D4
#define LED 2 // GPIO02/D4