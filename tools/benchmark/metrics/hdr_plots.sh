#!/usr/bin/env bash
# Copyright (c) the JPEG XL Project Authors.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

"$(dirname "$0")/run_all_hdr_metrics.sh" "$@" | sed -n '/```/q;p' > hdr_results.csv
mkdir -p hdr_plots/
rm -rf hdr_plots/*
python3 "$(dirname "$0")/plots.py" hdr_results.csv hdr_plots
