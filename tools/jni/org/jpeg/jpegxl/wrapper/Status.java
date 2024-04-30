// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

package org.jpeg.jpegxl.wrapper;

public enum Status {
  /** Operation was successful. */
  OK,

  /** So far stream was valid, but incomplete. */
  NOT_ENOUGH_INPUT,

  /** Stream is corrupted. */
  INVALID_STREAM
}
