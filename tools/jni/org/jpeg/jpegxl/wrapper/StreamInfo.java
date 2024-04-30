// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

package org.jpeg.jpegxl.wrapper;

/** POJO that wraps some fields of JxlBasicInfo. */
public class StreamInfo {
  public Status status;
  public int width;
  public int height;
  public int alphaBits;

  // package-private
  int pixelsSize;
  int iccSize;
}
