#ifndef _NETPKTOR_H
#define _NETPKTOR_H

#include "byteswap.h"
#include "netdrv.h"
#include "network.h"

//Returns a checksum of a block of memory.
extern ushort netmisc_calc_checksum( void * vptr, int len );

ubyte out_buffer[MAX_DATA_SIZE];		// used for tmp netgame packets as well as sending object data
extern frame_info 	MySyncPack;
void send_d1x_netgame_packet(ubyte *server, ubyte *node);
void receive_d1x_netgame_packet(ubyte *data, netgame_info *netgame);
//end change

void receive_netplayer_info(ubyte *data, netplayer_info *info, int d1x);
void send_sequence_packet(sequence_packet seq, ubyte *server, ubyte *node, ubyte *net_address);
void receive_sequence_packet(ubyte *data, sequence_packet *seq);
void send_netgame_packet(ubyte *server, ubyte *node);
void receive_netgame_packet(ubyte *data, netgame_info *netgame, int d1x);
void send_frameinfo_packet(ubyte *server, ubyte *node, ubyte *address, int short_packet);
void receive_frameinfo_packet(ubyte *data, frame_info *info, int short_packet);
void send_endlevel_packet(endlevel_info *info, ubyte *server, ubyte *node, ubyte *address);
void receive_endlevel_packet(ubyte *data, endlevel_info *info);
void swap_object(object *obj);

#endif
