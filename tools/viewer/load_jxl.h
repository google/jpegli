// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef TOOLS_VIEWER_LOAD_JXL_H_
#define TOOLS_VIEWER_LOAD_JXL_H_

#include <QByteArray>
#include <QImage>
#include <QString>

namespace jpegxl {
namespace tools {

QImage loadJxlImage(const QString& filename, const QByteArray& targetIccProfile,
                    qint64* elapsed, bool* usedRequestedProfile = nullptr);

}  // namespace tools
}  // namespace jpegxl

#endif  // TOOLS_VIEWER_LOAD_JXL_H_
