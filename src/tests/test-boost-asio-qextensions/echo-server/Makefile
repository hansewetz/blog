# To be built
LOCAL_TARGET    =  echo-test
LOCAL_SOTARGET  =
LOCAL_OBJS      = echo-test.o Service.o

# Control.
LOCAL_LIBS      = -lboost_system -lpthread -lboost_iostreams
LOCAL_LIBPATH   = -L$(PROJECT_ROOT)/lib/lib -L$(BOOST_LIB)
LOCAL_INCPATH   = -I${BOOST_INC} -I$(PROJECT_ROOT)/include
LOCAL_DEFINES   = -DBOOST_ALL_DYN_LINK -DASIO_STANDALONE
LOCAL_CXXFLAGS  =
LOCAL_CPPFLAGS  = 
LOCAL_LDFLAGS   =
LOCAL_LDSOFLAGS =

include $(PROJECT_ROOT)/makerules/build.rules
