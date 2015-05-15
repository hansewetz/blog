// (C) Copyright Hans Ewetz 2010,2011,2012,2013,2014,2015. All rights reserved.

#ifndef __QUEUE_CREATION_H__
#define __QUEUE_CREATION_H__
#include "QueueDefs.h"
#include <memory>
#include <string>

// struct holding queues to be used
struct queue_struct{
  std::shared_ptr<QueueDeqBase>qserviceReceive;      // service receives msg
  std::shared_ptr<QueueEnqBase>qserviceReply;        // service restd::shared_ptronds on message
  std::shared_ptr<QueueEnqBase>qclientRequest;       // client sends request
  std::shared_ptr<QueueDeqBase>qclientReceive;       // client receives restd::shared_ptronse
};
// create memory queues
queue_struct createMemQueues(){
  queue_struct ret;
  // we only need to create two queues
  size_t maxq{10};
  std::shared_ptr<QueueBase>q1=std::make_shared<MemQueue>(maxq);
  std::shared_ptr<QueueBase>q2=std::make_shared<MemQueue>(maxq);
  ret.qserviceReceive=q1;
  ret.qclientRequest=q1;
  ret.qserviceReply=q2;
  ret.qclientReceive=q2;
  return ret;
}
// create ip queues with Service being both an ip-server and ip-client
queue_struct createAsymetricQueues(){
  queue_struct ret;

  // ip data
  constexpr int serverListenPort=7787;
  constexpr int clientListenPort=7788;
  std::string const server{"localhost"};

  // create 4 queues
  ret.qserviceReceive=std::make_shared<SockServQueue>(serverListenPort,msgDeserializer,msgSerializer,sep);
  ret.qclientRequest=std::make_shared<SockClientQueue>(server,serverListenPort,msgDeserializer,msgSerializer,sep);

  ret.qserviceReply=std::make_shared<SockClientQueue>(server,clientListenPort,msgDeserializer,msgSerializer,sep);
  ret.qclientReceive=std::make_shared<SockServQueue>(clientListenPort,msgDeserializer,msgSerializer,sep);
  return ret;
}
// create ip queues with Service being both an ip-server and ip-client
queue_struct createSymetricIpQueues(){
  queue_struct ret;

  // ip data
  constexpr int serverListenPortReceive=7787;
  constexpr int serverListenPortSend=7788;
  std::string const server{"localhost"};

  // create 4 queues
  ret.qserviceReceive=std::make_shared<SockServQueue>(serverListenPortReceive,msgDeserializer,msgSerializer,sep);
  ret.qserviceReply=std::make_shared<SockServQueue>(serverListenPortSend,msgDeserializer,msgSerializer,sep);

  ret.qclientRequest=std::make_shared<SockClientQueue>(server,serverListenPortReceive,msgDeserializer,msgSerializer,sep);
  ret.qclientReceive=std::make_shared<SockClientQueue>(server,serverListenPortSend,msgDeserializer,msgSerializer,sep);
  return ret;
}
#endif
