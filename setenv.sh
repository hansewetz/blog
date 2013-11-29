# setup installation directories for includes and libs
#export INSTALL_HOME=
#export INSTALL_EXTRA_HOME=

# build environment root
export ENV_ROOT=`pwd`

# must include boost stuff
export BOOST_INC=${INSTALL_HOME}/include
export BOOST_LIB=${INSTALL_HOME}/lib

# occi stuff
export OCCI_INC=${INSTALL_HOME}/include/occi
export OCCI_LIB=${INSTALL_HOME}/lib/occi

# oracle stuff
export ORACLE_HOME=/ec/sw/oracle/client/product/11.2.0.2/

# set path and ld library path
export PATH=${PATH}:${ENV_ROOT}/bin
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${ENV_ROOT}/lib
