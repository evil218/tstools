#!/bin/bash

git rev-list HEAD | sort > config.git-hash
LOCALVER=`wc -l config.git-hash | awk '{print $1}'`
if [ $LOCALVER \> 1 ] ; then
    VER=`git rev-list origin/master | sort | join config.git-hash - | wc -l | awk '{print $1}'`
    if [ $VER != $LOCALVER ] ; then
        VER="$VER+$(($LOCALVER-$VER))"
    fi
    if git status | grep -q "modified:" ; then
        VER="${VER}m"
    fi
    VER="$VER $(git rev-list HEAD -n 1 | cut -c 1-8)"
    GIT_VERSION=$VER
else
    GIT_VERSION=
    VER="x"
fi
rm -f config.git-hash
 
echo "#define VER_MAJOR \"1\""
echo "#define VER_MINOR \"0\""
echo "#define VER_RELEA \"$GIT_VERSION\""
