// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <QApplication>

#include "tools/viewer/viewer_window.h"

int main(int argc, char** argv) {
  QApplication application(argc, argv);
  QStringList arguments = application.arguments();
  arguments.removeFirst();

  jpegxl::tools::ViewerWindow window;
  window.show();

  if (!arguments.empty()) {
    window.loadFilesAndDirectories(arguments);
  }

  return application.exec();
}
