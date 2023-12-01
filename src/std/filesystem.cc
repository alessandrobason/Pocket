#include "filesystem.h"

static fs::Path base_folder = StrView("");

void fs::setBaseFolder(StrView folder) {
    if (folder.back() != '/' && folder.back() != '\\') {
        base_folder = fs::Path::cat({ folder, "/" });
    }
    else {
        base_folder = folder;
    }
}

fs::Path fs::getPath(StrView relative) {
    return fs::Path::cat({ base_folder, relative });
}
