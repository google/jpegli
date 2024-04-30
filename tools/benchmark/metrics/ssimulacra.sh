#!/usr/bin/env bash
# Copyright (c) the JPEG XL Project Authors.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

"$(dirname "$0")"/../../../build/tools/ssimulacra_main "$1" "$2" > "$3" 2>/dev/null
