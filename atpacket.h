// ALPHA-T trigger data UDP packet

#ifndef atpacket_H
#define atpacket_H

struct AlphaTPacket
{
   uint32_t num_bytes = 0;
   uint32_t packet_no = 0;
   uint32_t trig_no_header = 0;
   //uint32_t trig_no_footer = 0;
   uint32_t ts_625 = 0;

   void Print() const;
   void Unpack(const char* buf, int bufsize);
};

void AlphaTPacket::Print() const
{
   printf("AlphaTPacket: packet %d, trig %d, ts 0x%08x, %d bytes", packet_no, trig_no_header, ts_625, num_bytes);
}

void AlphaTPacket::Unpack(const char* buf, int bufsize)
{
   if (bufsize >= 40) {
      const uint32_t *p32 = (uint32_t*)buf;
      num_bytes = bufsize;
      packet_no = p32[0];
      trig_no_header = p32[1] & 0x3FFFFFF;
      ts_625 = p32[2];
      //trig_no_footer = p32[9] & 0x3FFFFFF;
   }
}

#endif
/* emacs
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 3
 * indent-tabs-mode: nil
 * End:
 */
