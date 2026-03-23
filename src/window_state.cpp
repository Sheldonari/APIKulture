#include "window_state.hpp"

#include "collections_io.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace apikulture::window_state {

static fs::path window_state_file_path() {
	fs::path p(collections_io::default_config_path());
	p.replace_filename("window-state.json");
	return p;
}

bool load_main_window_maximized() {
	const fs::path path = window_state_file_path();
	std::ifstream in(path);
	if (!in) return false;
	try {
		nlohmann::json j;
		in >> j;
		return j.value("maximized", false);
	} catch (...) {
		return false;
	}
}

void save_main_window_maximized(bool maximized) {
	try {
		const fs::path path = window_state_file_path();
		fs::create_directories(path.parent_path());
		nlohmann::json j;
		j["version"] = 1;
		j["maximized"] = maximized;
		std::ofstream out(path, std::ios::trunc);
		if (!out) return;
		out << j.dump(2);
	} catch (...) {
	}
}

}  // namespace apikulture::window_state
