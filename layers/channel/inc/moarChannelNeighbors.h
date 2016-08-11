//
// Created by svalov on 7/27/16.
//

#ifndef MOARSTACK_MOARCHANNELNEIGHBORS_H
#define MOARSTACK_MOARCHANNELNEIGHBORS_H

#endif //MOARSTACK_MOARCHANNELNEIGHBORS_H

int neighborsInit(ChannelLayer_T* layer);
ChannelNeighbor_T* neighborFindByAddress(ChannelLayer_T* layer, ChannelAddr_T* address);
RemoteInterface_T* neighborFindRemoteInterface(ChannelNeighbor_T* neighbor);
int neighborAdd(ChannelLayer_T* layer, ChannelAddr_T* address, UnIfaceAddr_T* remoteAddress, int localSocket);
int neighborRemove(ChannelLayer_T* layer, UnIfaceAddr_T remoteAddress);
int neighborsRemoveAssociated(ChannelLayer_T* layer, int localSocket);
//non resolved processing
int neighborNonResolvedProcess(ChannelLayer_T* layer);
int neighborNonResolvedRemove(ChannelLayer_T* layer, UnIfaceAddr_T* address); // may be unused
NonResolvedNeighbor_T* neighborFindNonResolvedByAddress(ChannelLayer_T* layer, UnIfaceAddr_T* address); // may be unused
