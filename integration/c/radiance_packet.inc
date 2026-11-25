/* Included by the Radiance camera bridge in the experimental guest build. */
static void world_radiance_packet(unsigned layer,unsigned position,unsigned age)
{
    const char *hex="0123456789ABCDEF";
    char packet[14]={'R','F','X','1',hex[layer]};
    for(unsigned i=0;i<6;++i)packet[5+i]=hex[(position>>(20-4*i))&15];
    for(unsigned i=0;i<3;++i)packet[11+i]=hex[(age>>(8-4*i))&15];
    register const char *r0 __asm__("r0")=packet;
    register unsigned r1 __asm__("r1")=sizeof(packet);
    __asm__ volatile("svc #0x27":"+r"(r0),"+r"(r1)::"memory","cc");
}
