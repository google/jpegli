#!/usr/bin/env bash
# Copyright (c) the JPEG XL Project Authors.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

"$(dirname "$0")"/compute_octave_metric.sh "$@" \
  --path "$(dirname "$0")"/../../../third_party/hdrvdp-2.2.2/ \
  "$(dirname "$0")"/compute-hdrvdp.m
