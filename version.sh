#!/bin/bash

MAJOR=1
MINOR=1

git rev-list HEAD | sort > config.git-hash
PATCH=`wc -l config.git-hash | awk '{print $1}'`
if [ $PATCH \> 1 ] ; then
    VER=`git rev-list origin/master | sort | join config.git-hash - | wc -l | awk '{print $1}'`
    if [ $VER != $PATCH ] ; then
        VER="$VER+$(($PATCH-$VER))"
    fi
    if git status | grep -q "modified:" ; then
        VER="${VER}m"
    fi
    VER="$VER $(git rev-list HEAD -n 1 | cut -c 1-8)"
    GIT_VERSION=$VER
else
    GIT_VERSION=git_not_found
    VER="x"
fi
rm -f config.git-hash
 
echo "#define VER_MAJOR $MAJOR"
echo "#define VER_MINOR $MINOR"
echo "#define VER_PATCH $PATCH"
echo "#define VERSION $MAJOR.$MINOR.$PATCH"
echo "#define VERSION_STR \"$MAJOR.$MINOR.$PATCH\""
echo "#define REVISION \"$GIT_VERSION\""
