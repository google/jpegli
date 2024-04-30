#!/usr/bin/env bash
# Copyright (c) the JPEG XL Project Authors.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

"$(dirname "$0")/run_all_sdr_metrics.sh" "$@" | sed -n '/```/q;p' > sdr_results.csv
mkdir -p sdr_plots/
rm -rf sdr_plots/*
python3 "$(dirname "$0")/plots.py" sdr_results.csv sdr_plots
