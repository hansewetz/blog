# Copyright (c) 2003-2015 Hans Ewetz (hansewetz at hotmail dot com)
# Distributed under the Boost Software License, Version 1.0. 

# To be built
LOCAL_TARGET    =  test-simpleq3
LOCAL_SOTARGET  =
LOCAL_OBJS      = test-simpleq3.o

# Control.
LOCAL_LIBS      = -lboost_log -lboost_system -lboost_filesystem -lpthread -lboost_iostreams
LOCAL_LIBPATH   = -L$(PROJECT_ROOT)/lib/lib -L$(BOOST_LIB)
LOCAL_INCPATH   = -I${BOOST_INC}  -I$(ENV_ROOT)/include
LOCAL_DEFINES   = -DBOOST_ALL_DYN_LINK -DASIO_STANDALONE
LOCAL_CXXFLAGS  =
LOCAL_CPPFLAGS  = 
LOCAL_LDFLAGS   =
LOCAL_LDSOFLAGS =

include $(ENV_ROOT)/makerules/build.rules
