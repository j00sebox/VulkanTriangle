#!/usr/bin/env python3

import os
import shutil

if not os.path.exists("build"):
    os.makedirs("build")
   # shutil.copy("imgui.ini", "build/")

os.system("cmake -DGLFW_BUILD_DOCS=OFF -S . -B build")