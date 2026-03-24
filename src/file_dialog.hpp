#ifndef APIKULTURE_FILE_DIALOG_H
#define APIKULTURE_FILE_DIALOG_H

#include <string>

namespace apikulture::file_dialog {

/// Native “open file” for OpenAPI JSON. Empty if cancelled or unavailable.
std::string pick_openapi_json_file();

}  // namespace apikulture::file_dialog

#endif
