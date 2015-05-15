// (C) Copyright Hans Ewetz 2010,2011,2012,2013,2014,2015. All rights reserved.

#ifndef __SERVICE_H__
#define __SERVICE_H__
#include "QueueDefs.h"
#include <memory>

// service class
class Service{
public:
  Service(boost::asio::io_service&ios,std::shared_ptr<QueueDeqBase>qserviceReceive,std::shared_ptr<QueueEnqBase>qserviceReply);
private:
  std::shared_ptr<QueueListener>serviceReceiver_;
  std::shared_ptr<QueueSender>serviceSender_;

  // handler for received messages
  void recvHandler(boost::system::error_code const&ec,QueueValueType const&msg);
};
#endif
