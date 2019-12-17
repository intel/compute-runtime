#!/bin/bash
#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if [ ! -d "$1" ]; then
  echo "Directory "$1" does not exist."
  exit 1
fi

clang-format --version
err=$?
if [$err -ne 0]
  then
  echo "clang-format not found"
  exit 1
fi

git --version
err=$?
if [$err -ne 0]
  then
  echo "git not found"
  exit 1
fi

pushd $1

if git rev-parse --git-dir > /dev/null 2>&1; 
  then
  files=$(git diff HEAD --name-only)
  for i in $files; do
    if [[ $i =~ .*\.(h|cpp|inl) ]]
    then
      clang-format -i -style=file $i
    fi
  done
else
  echo Not a git repository.
  exit 1
fi

popd
exit 0
