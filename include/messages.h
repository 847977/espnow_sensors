// include/messages.h
#pragma once
#include <Arduino.h>
#pragma pack(push,1)

// typ + príznaky
enum : uint8_t { MSG_BATCH = 1 };
enum : uint8_t { FLAG_DHT = 0x01, FLAG_MPL = 0x02, FLAG_HC = 0x04 };

// hlavička
struct MsgHdr {
  uint8_t  type;   // = MSG_BATCH
  uint32_t seq;    // poradie batchu
  uint32_t t_ms;   // millis() na TX pri tvorbe batchu
  uint8_t  flags;  // bity: DHT/MPL/HC prítomné
};

// kompaktné payloady (škálované celé čísla)
struct PayDHT {   // ~4 B
  int16_t  t_dC;        // °C x10
  uint16_t rh_permille; // % x10
};
struct PayMPL {   // ~8 B
  int32_t  p_Pa;        // Pascaly
  int16_t  alt_cm;      // cm
  int16_t  t_dC;        // °C x10
};
struct PayHC {    // ~2 B
  uint16_t dist_mm;     // mm
};

// zostavenie dynamickej správy do bufferu out[]
inline size_t build_batch(
  uint8_t* out, size_t out_cap,
  const PayDHT* dht, const PayMPL* mpl, const PayHC* hc,
  uint32_t seq, uint32_t t_ms
){
  uint8_t flags = 0;
  size_t  pos   = sizeof(MsgHdr);
  if (pos > out_cap) return 0;

  if (dht) { flags |= FLAG_DHT; if (pos + sizeof(PayDHT) > out_cap) return 0; memcpy(out + pos, dht, sizeof(PayDHT)); pos += sizeof(PayDHT); }
  if (mpl) { flags |= FLAG_MPL; if (pos + sizeof(PayMPL) > out_cap) return 0; memcpy(out + pos, mpl, sizeof(PayMPL)); pos += sizeof(PayMPL); }
  if (hc)  { flags |= FLAG_HC;  if (pos + sizeof(PayHC)  > out_cap) return 0; memcpy(out + pos,  hc,  sizeof(PayHC));  pos += sizeof(PayHC);  }

  MsgHdr hdr{ MSG_BATCH, seq, t_ms, flags };
  memcpy(out, &hdr, sizeof(MsgHdr));
  return pos; // celková dĺžka
}

#pragma pack(pop)
