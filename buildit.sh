#!/bin/bash

cmake .. \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=10.14 \
    -DCMAKE_FIND_FRAMEWORK=LAST \
    -DCMAKE_INSTALL_PREFIX=../build/macos/ \
    -DRUN_IN_PLACE=FALSE -DENABLE_GETTEXT=TRUE

make -j$(sysctl -n hw.logicalcpu)
make install
