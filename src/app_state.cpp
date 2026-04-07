#include "app_state.h"
#include "clipboard.hpp"
#include "environment.hpp"
#include "file_dialog.hpp"
#include "http_client.h"
#include "query_string.hpp"
#include "url_import.hpp"
#include "window_state.hpp"
#include "response_json_highlighter.hpp"
#include <jsoncons/json.hpp>
#include <jsoncons_ext/jsonpath/jsonpath.hpp>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string_view>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <optional>
#include <slint_models.h>

namespace {

std::string format_elapsed_since(std::chrono::steady_clock::time_point start) {
	using namespace std::chrono;
	const auto ms = duration_cast<milliseconds>(steady_clock::now() - start).count();
	char buf[48];
	std::snprintf(buf, sizeof(buf), "%.2f s", static_cast<double>(ms) / 1000.0);
	return buf;
}

std::vector<std::pair<std::string, std::string>> parse_headers(const std::string& text) {
	std::vector<std::pair<std::string, std::string>> out;
	std::istringstream stream(text);
	std::string line;
	while (std::getline(stream, line)) {
		size_t colon = line.find(':');
		if (colon != std::string::npos) {
			std::string key = line.substr(0, colon);
			std::string value = line.substr(colon + 1);
			auto trim = [](std::string& s) {
				s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c) { return !std::isspace(c); }));
				s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c) { return !std::isspace(c); }).base(), s.end());
			};
			trim(key);
			trim(value);
			if (!key.empty()) out.emplace_back(std::move(key), std::move(value));
		}
	}
	return out;
}

std::string format_headers(const std::vector<std::pair<std::string, std::string>>& headers) {
	std::ostringstream out;
	for (const auto& h : headers) {
		out << h.first << ": " << h.second << "\n";
	}
	return out.str();
}

std::string try_pretty_print_json(const std::string& body) {
	try {
		auto j = nlohmann::json::parse(body);
		return j.dump(2);
	} catch (...) {
		return body;
	}
}

std::string format_response_with_jsonpath(const std::string& raw_body, const std::string& path_expr) {
	if (path_expr.empty()) {
		return try_pretty_print_json(raw_body);
	}
	try {
		jsoncons::json j = jsoncons::json::parse(raw_body);
		jsoncons::json result = jsoncons::jsonpath::json_query(j, path_expr);
		std::string out;
		result.dump_pretty(out);
		return out;
	} catch (const std::exception& ex) {
		return std::string("JSONPath error: ") + ex.what();
	}
}

std::string to_std_string(const slint::SharedString& ss) {
	std::string_view v;
	try {
		v = static_cast<std::string_view>(ss);
	} catch (...) {
		return std::string();
	}
	if (v.data() == nullptr || v.size() == 0) return std::string();
	return std::string(v.data(), v.size());
}

void trim_in_place(std::string& s) {
	auto not_space = [](unsigned char c) { return !std::isspace(c); };
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
	s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
}

std::string content_type_value_from_headers(const std::vector<std::pair<std::string, std::string>>& headers) {
	for (const auto& kv : headers) {
		std::string k = kv.first;
		for (auto& c : k) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		if (k == "content-type") return kv.second;
	}
	return {};
}

}  // namespace

apikulture::RequestItem* AppState::mutable_current_request_item() {
	if (workspace_.collections.empty()) return nullptr;
	if (collection_index_ < 0 || collection_index_ >= static_cast<int>(workspace_.collections.size())) return nullptr;
	auto& col = workspace_.collections[static_cast<size_t>(collection_index_)];
	if (col.items.empty()) return nullptr;
	if (request_index_ < 0 || request_index_ >= static_cast<int>(col.items.size())) return nullptr;
	return &col.items[static_cast<size_t>(request_index_)];
}

apikulture::Collection* AppState::mutable_current_collection() {
	if (workspace_.collections.empty()) return nullptr;
	if (collection_index_ < 0 || collection_index_ >= static_cast<int>(workspace_.collections.size())) return nullptr;
	return &workspace_.collections[static_cast<size_t>(collection_index_)];
}

apikulture::Environment* AppState::mutable_active_environment() {
	apikulture::Collection* col = mutable_current_collection();
	if (!col || col->environments.empty()) return nullptr;
	int i = col->active_environment_index;
	if (i < 0 || i >= static_cast<int>(col->environments.size())) i = 0;
	return &col->environments[static_cast<size_t>(i)];
}

void AppState::push_response_body(const std::string& text) {
	auto& g = ui_->global<AppLogic>();
	g.set_response_body(slint::SharedString(text));
	auto lines = std::make_shared<slint::VectorModel<ResponseLine>>();
	if (text.empty()) {
		g.set_response_body_lines(lines);
		return;
	}
	if (auto json_lines = apikulture::response_highlight::highlight_json(text)) {
		for (const auto& ln : *json_lines) {
			auto inner = std::make_shared<slint::VectorModel<TokenSpan>>();
			for (const auto& sp : ln) {
				inner->push_back(TokenSpan{slint::SharedString(sp.text), sp.kind});
			}
			ResponseLine rl;
			rl.spans = inner;
			lines->push_back(rl);
		}
	} else {
		for (const auto& ln : apikulture::response_highlight::highlight_plain_lines(text)) {
			auto inner = std::make_shared<slint::VectorModel<TokenSpan>>();
			for (const auto& sp : ln) {
				inner->push_back(TokenSpan{slint::SharedString(sp.text), sp.kind});
			}
			ResponseLine rl;
			rl.spans = inner;
			lines->push_back(rl);
		}
	}
	g.set_response_body_lines(lines);
}

AppState::AppState(const MainWindowHandle& ui) : ui_(ui) {
	workspace_ = apikulture::collections_io::load_workspace_or_default();
	collection_index_ = workspace_.last_selected_collection_index;
	if (collection_index_ < 0 || collection_index_ >= static_cast<int>(workspace_.collections.size())) {
		collection_index_ = 0;
	}
	restore_request_index_for_current_collection();
}

AppState::~AppState() {
	commit_form_to_current_item();
	apikulture::collections_io::save_workspace(workspace_);
	cancel_request();
	{
		std::lock_guard<std::mutex> lock(mutex_);
		shutdown_ = true;
	}
	cv_.notify_one();
	if (worker_.joinable()) worker_.join();
}

std::shared_ptr<slint::VectorModel<slint::SharedString>> AppState::make_name_model(
		const std::vector<std::string>& names) {
	auto m = std::make_shared<slint::VectorModel<slint::SharedString>>();
	for (const auto& n : names) {
		m->push_back(slint::SharedString(n));
	}
	return m;
}

void AppState::refresh_collection_names_model() {
	std::vector<std::string> names;
	names.reserve(workspace_.collections.size());
	for (const auto& c : workspace_.collections) {
		names.push_back(c.name);
	}
	auto& g = ui_->global<AppLogic>();
	g.set_collection_names(make_name_model(names));
}

void AppState::refresh_request_names_model() {
	std::vector<std::string> names;
	if (collection_index_ >= 0 && collection_index_ < static_cast<int>(workspace_.collections.size())) {
		for (const auto& r : workspace_.collections[collection_index_].items) {
			names.push_back(r.name);
		}
	}
	auto& g = ui_->global<AppLogic>();
	g.set_request_names(make_name_model(names));
}

void AppState::refresh_environment_names_model() {
	std::vector<std::string> names;
	if (collection_index_ >= 0 && collection_index_ < static_cast<int>(workspace_.collections.size())) {
		for (const auto& e : workspace_.collections[static_cast<size_t>(collection_index_)].environments) {
			names.push_back(e.name);
		}
	}
	auto& g = ui_->global<AppLogic>();
	g.set_environment_names(make_name_model(names));
}

void AppState::commit_environment_fields_to_active() {
	if (apikulture::Environment* env = mutable_active_environment()) {
		auto& g = ui_->global<AppLogic>();
		env->base_url = to_std_string(g.get_environment_base_url());
		trim_in_place(env->base_url);
		env->variables = apikulture::parse_environment_lines(to_std_string(g.get_environment_variables()));
	}
}

void AppState::apply_environment_fields_to_ui() {
	auto& g = ui_->global<AppLogic>();
	if (apikulture::Environment* env = mutable_active_environment()) {
		g.set_environment_base_url(slint::SharedString(env->base_url));
		g.set_environment_variables(slint::SharedString(apikulture::format_environment_lines(env->variables)));
		g.set_environment_name_edit(slint::SharedString(env->name));
		g.set_active_environment_name(slint::SharedString(env->name));
	} else {
		g.set_environment_base_url(slint::SharedString(""));
		g.set_environment_variables(slint::SharedString(""));
		g.set_environment_name_edit(slint::SharedString(""));
		g.set_active_environment_name(slint::SharedString(""));
	}
}

void AppState::push_name_edits_to_ui() {
	auto& g = ui_->global<AppLogic>();
	if (workspace_.collections.empty()) {
		g.set_collection_name_edit(slint::SharedString(""));
		g.set_request_name_edit(slint::SharedString(""));
		return;
	}
	if (collection_index_ >= 0 && collection_index_ < static_cast<int>(workspace_.collections.size())) {
		g.set_collection_name_edit(slint::SharedString(workspace_.collections[static_cast<size_t>(collection_index_)].name));
		const auto& col = workspace_.collections[static_cast<size_t>(collection_index_)];
		if (!col.items.empty() && request_index_ >= 0
				&& request_index_ < static_cast<int>(col.items.size())) {
			g.set_request_name_edit(slint::SharedString(col.items[static_cast<size_t>(request_index_)].name));
		} else {
			g.set_request_name_edit(slint::SharedString(""));
		}
	}
}

void AppState::restore_request_index_for_current_collection() {
	if (collection_index_ < 0 || collection_index_ >= static_cast<int>(workspace_.collections.size())) {
		request_index_ = 0;
		return;
	}
	auto& col = workspace_.collections[static_cast<size_t>(collection_index_)];
	if (col.items.empty()) {
		request_index_ = 0;
		return;
	}
	request_index_ = col.last_selected_request_index;
	if (request_index_ < 0 || request_index_ >= static_cast<int>(col.items.size())) {
		request_index_ = 0;
	}
}

void AppState::push_selection_to_ui() {
	auto& g = ui_->global<AppLogic>();
	if (collection_index_ >= 0 && collection_index_ < static_cast<int>(workspace_.collections.size())) {
		workspace_.last_selected_collection_index = collection_index_;
		auto& col = workspace_.collections[static_cast<size_t>(collection_index_)];
		if (!col.items.empty() && request_index_ >= 0 && request_index_ < static_cast<int>(col.items.size())) {
			col.last_selected_request_index = request_index_;
		}
	}
	g.set_selected_collection_index(collection_index_);
	g.set_selected_request_index(request_index_);
	push_name_edits_to_ui();
}

void AppState::sync_url_field_to_query_table_if_changed() {
	auto& g = ui_->global<AppLogic>();
	std::string url_now = to_std_string(g.get_url());
	trim_in_place(url_now);
	if (url_now == last_url_field_sync_) {
		g.set_import_status(slint::SharedString(""));
		return;
	}
	if (try_apply_url_import_from_text(url_now)) {
		last_url_field_sync_ = to_std_string(g.get_url());
		trim_in_place(last_url_field_sync_);
	}
}

void AppState::commit_form_to_current_item() {
	commit_environment_fields_to_active();
	if (workspace_.collections.empty()) return;
	if (collection_index_ < 0 || collection_index_ >= static_cast<int>(workspace_.collections.size())) return;
	auto& col = workspace_.collections[collection_index_];
	if (col.items.empty()) return;
	if (request_index_ < 0 || request_index_ >= static_cast<int>(col.items.size())) return;

	auto& g = ui_->global<AppLogic>();
	auto& item = col.items[static_cast<size_t>(request_index_)];
	item.method = to_std_string(g.get_method());
	item.url = to_std_string(g.get_url());
	item.body = to_std_string(g.get_request_body());
	item.jsonpath = to_std_string(g.get_response_jsonpath());
	// Keep list label in sync if user only edited URL (optional: derive name from URL — skip for MVP)
}

void AppState::apply_form_from_current_item() {
	if (workspace_.collections.empty()) return;
	if (collection_index_ < 0 || collection_index_ >= static_cast<int>(workspace_.collections.size())) return;
	auto& col = workspace_.collections[collection_index_];
	if (col.items.empty()) return;
	if (request_index_ < 0 || request_index_ >= static_cast<int>(col.items.size())) return;

	const auto& item = col.items[static_cast<size_t>(request_index_)];
	auto& g = ui_->global<AppLogic>();
	g.set_request_elapsed(slint::SharedString(""));
	g.set_method(slint::SharedString(item.method));
	g.set_url(slint::SharedString(item.url));
	g.set_request_body(slint::SharedString(item.body));
	g.set_response_jsonpath(slint::SharedString(item.jsonpath));
	refresh_query_param_models();
	refresh_request_header_models();

	last_url_field_sync_ = item.url;
	trim_in_place(last_url_field_sync_);

	g.set_response_status(slint::SharedString(item.last_response_status));
	g.set_response_headers(slint::SharedString(item.last_response_headers));
	if (!item.last_response_error.empty()) {
		last_success_response_body_ = std::nullopt;
		last_success_content_type_ = std::nullopt;
		push_response_body(item.last_response_error);
	} else if (!item.last_response_body_raw.empty()) {
		last_success_response_body_ = item.last_response_body_raw;
		last_success_content_type_ = item.last_response_content_type.empty()
				? std::nullopt
				: std::optional<std::string>(item.last_response_content_type);
		refresh_response_display();
	} else {
		last_success_response_body_ = std::nullopt;
		last_success_content_type_ = std::nullopt;
		push_response_body("");
	}
}

void AppState::init_collections_ui() {
	refresh_collection_names_model();
	refresh_request_names_model();
	refresh_environment_names_model();
	push_selection_to_ui();
	apply_environment_fields_to_ui();
	apply_form_from_current_item();
}

void AppState::select_collection(int index) {
	if (index < 0 || index >= static_cast<int>(workspace_.collections.size())) return;
	commit_form_to_current_item();
	collection_index_ = index;
	restore_request_index_for_current_collection();
	refresh_request_names_model();
	refresh_environment_names_model();
	push_selection_to_ui();
	apply_environment_fields_to_ui();
	apply_form_from_current_item();
}

void AppState::select_request(int index) {
	if (collection_index_ < 0 || collection_index_ >= static_cast<int>(workspace_.collections.size())) return;
	auto& col = workspace_.collections[static_cast<size_t>(collection_index_)];
	if (index < 0 || index >= static_cast<int>(col.items.size())) return;
	commit_form_to_current_item();
	request_index_ = index;
	push_selection_to_ui();
	apply_form_from_current_item();
}

void AppState::new_collection() {
	commit_form_to_current_item();
	apikulture::Collection c;
	c.name = "Collection " + std::to_string(workspace_.collections.size() + 1);
	apikulture::RequestItem r;
	r.name = "Request 1";
	c.items.push_back(std::move(r));
	workspace_.collections.push_back(std::move(c));
	collection_index_ = static_cast<int>(workspace_.collections.size()) - 1;
	request_index_ = 0;
	refresh_collection_names_model();
	refresh_request_names_model();
	refresh_environment_names_model();
	push_selection_to_ui();
	apply_environment_fields_to_ui();
	apply_form_from_current_item();
}

void AppState::delete_collection() {
	auto& g = ui_->global<AppLogic>();
	if (workspace_.collections.size() <= 1) {
		pending_delete_collection_index_ = -1;
		g.set_collection_delete_dialog_info_only(true);
		g.set_collection_delete_dialog_text(slint::SharedString("You must keep at least one collection."));
		g.set_collection_delete_dialog_open(true);
		return;
	}
	commit_form_to_current_item();
	pending_delete_collection_index_ = collection_index_;
	const std::string& name = workspace_.collections[static_cast<size_t>(collection_index_)].name;
	g.set_collection_delete_dialog_info_only(false);
	g.set_collection_delete_dialog_text(slint::SharedString(
			std::string("Delete collection \"") + name + "\"? This cannot be undone."));
	g.set_collection_delete_dialog_open(true);
}

void AppState::collection_delete_dialog_cancel() {
	auto& g = ui_->global<AppLogic>();
	g.set_collection_delete_dialog_open(false);
	pending_delete_collection_index_ = -1;
}

void AppState::collection_delete_dialog_confirm() {
	auto& g = ui_->global<AppLogic>();
	const bool info_only = g.get_collection_delete_dialog_info_only();
	g.set_collection_delete_dialog_open(false);
	if (info_only) {
		pending_delete_collection_index_ = -1;
		return;
	}
	const int idx = pending_delete_collection_index_;
	pending_delete_collection_index_ = -1;
	if (idx < 0 || idx >= static_cast<int>(workspace_.collections.size())) return;
	workspace_.collections.erase(workspace_.collections.begin() + idx);
	if (collection_index_ == idx) {
		if (collection_index_ >= static_cast<int>(workspace_.collections.size())) {
			collection_index_ = static_cast<int>(workspace_.collections.size()) - 1;
		}
	} else if (collection_index_ > idx) {
		collection_index_--;
	}
	restore_request_index_for_current_collection();
	refresh_collection_names_model();
	refresh_request_names_model();
	refresh_environment_names_model();
	push_selection_to_ui();
	apply_environment_fields_to_ui();
	apply_form_from_current_item();
	refresh_query_param_models();
	refresh_request_header_models();
	(void)apikulture::collections_io::save_workspace(workspace_);
}

void AppState::new_request() {
	if (workspace_.collections.empty()) return;
	commit_form_to_current_item();
	auto& col = workspace_.collections[static_cast<size_t>(collection_index_)];
	apikulture::RequestItem r;
	r.name = std::string("Request ") + std::to_string(col.items.size() + 1);
	col.items.push_back(std::move(r));
	request_index_ = static_cast<int>(col.items.size()) - 1;
	refresh_request_names_model();
	push_selection_to_ui();
	apply_form_from_current_item();
}

void AppState::delete_request() {
	if (workspace_.collections.empty()) return;
	if (collection_index_ < 0 || collection_index_ >= static_cast<int>(workspace_.collections.size())) return;
	auto& col = workspace_.collections[static_cast<size_t>(collection_index_)];
	if (col.items.empty()) return;
	if (col.items.size() <= 1) {
		col.items[0] = apikulture::RequestItem{};
		col.items[0].name = "Request 1";
		request_index_ = 0;
	} else {
		col.items.erase(col.items.begin() + request_index_);
		if (request_index_ >= static_cast<int>(col.items.size())) {
			request_index_ = static_cast<int>(col.items.size()) - 1;
		}
	}
	refresh_request_names_model();
	push_selection_to_ui();
	apply_form_from_current_item();
}

void AppState::duplicate_request() {
	if (workspace_.collections.empty()) return;
	commit_form_to_current_item();
	auto& col = workspace_.collections[static_cast<size_t>(collection_index_)];
	if (request_index_ < 0 || request_index_ >= static_cast<int>(col.items.size())) return;
	apikulture::RequestItem copy = col.items[static_cast<size_t>(request_index_)];
	copy.name = copy.name + " copy";
	col.items.insert(col.items.begin() + request_index_ + 1, std::move(copy));
	request_index_++;
	refresh_request_names_model();
	push_selection_to_ui();
	apply_form_from_current_item();
}

void AppState::save_collections() {
	commit_form_to_current_item();
	(void)apikulture::collections_io::save_workspace(workspace_);
}

void AppState::apply_openapi_import_result(apikulture::openapi::ImportResult&& result) {
	auto& g = ui_->global<AppLogic>();
	if (!result.error.empty()) {
		g.set_import_status(slint::SharedString(result.error));
		return;
	}
	const size_t n_req = result.collection.items.size();
	if (n_req == 0) {
		g.set_import_status(slint::SharedString("Import produced no requests."));
		return;
	}
	workspace_.collections.push_back(std::move(result.collection));
	collection_index_ = static_cast<int>(workspace_.collections.size()) - 1;
	request_index_ = 0;
	if (result.primary_server_url) {
		if (apikulture::Collection* col = mutable_current_collection()) {
			for (auto& env : col->environments) {
				if (env.name == "Default") {
					env.base_url = *result.primary_server_url;
					break;
				}
			}
		}
	}
	refresh_collection_names_model();
	refresh_request_names_model();
	refresh_environment_names_model();
	push_selection_to_ui();
	apply_environment_fields_to_ui();
	apply_form_from_current_item();
	refresh_query_param_models();
	refresh_request_header_models();
	g.set_import_status(slint::SharedString("Imported " + std::to_string(n_req) + " request(s) from OpenAPI."));
}

void AppState::import_openapi_from_path(const std::string& path_utf8) {
	commit_form_to_current_item();
	ui_->global<AppLogic>().set_import_status(slint::SharedString(""));
	apply_openapi_import_result(apikulture::openapi::import_from_file(path_utf8));
}

void AppState::import_openapi_from_url_string(const std::string& url_utf8) {
	commit_form_to_current_item();
	auto& g = ui_->global<AppLogic>();
	g.set_import_status(slint::SharedString(""));
	std::string url = url_utf8;
	trim_in_place(url);
	if (url.empty()) {
		g.set_import_status(slint::SharedString("Enter an https:// URL to an OpenAPI 3.x JSON document."));
		return;
	}
	if (url.size() < 8
			|| (url.compare(0, 7, "http://") != 0 && url.compare(0, 8, "https://") != 0)) {
		url.insert(0, "https://");
	}
	HttpResponse res = execute("GET", url, {}, "", nullptr);
	if (!res.error.empty()) {
		g.set_import_status(slint::SharedString("OpenAPI download failed: " + res.error));
		return;
	}
	if (res.status < 200 || res.status >= 300) {
		g.set_import_status(slint::SharedString("OpenAPI URL returned HTTP " + std::to_string(res.status) + "."));
		return;
	}
	apply_openapi_import_result(
			apikulture::openapi::import_from_json_text(std::move(res.body), std::optional<std::string>(url)));
}

void AppState::import_openapi_from_url_ui() {
	import_openapi_from_url_string(to_std_string(ui_->global<AppLogic>().get_openapi_import_url()));
}

void AppState::import_openapi_dialog() {
	const std::string path = apikulture::file_dialog::pick_openapi_json_file();
	if (path.empty()) return;
	import_openapi_from_path(path);
}

void AppState::commit_collection_name() {
	if (workspace_.collections.empty()) return;
	if (collection_index_ < 0 || collection_index_ >= static_cast<int>(workspace_.collections.size())) return;
	auto& g = ui_->global<AppLogic>();
	std::string name = to_std_string(g.get_collection_name_edit());
	trim_in_place(name);
	if (name.empty()) {
		push_name_edits_to_ui();
		return;
	}
	workspace_.collections[static_cast<size_t>(collection_index_)].name = std::move(name);
	refresh_collection_names_model();
	push_selection_to_ui();
}

void AppState::response_jsonpath_changed() {
	commit_form_to_current_item();
	refresh_response_display();
}

void AppState::apply_response_font_index(int index) {
	if (index < 0 || index >= static_cast<int>(apikulture::window_state::k_response_font_choices.size())) {
		return;
	}
	auto& g = ui_->global<AppLogic>();
	const char* fam = apikulture::window_state::k_response_font_choices[static_cast<size_t>(index)];
	g.set_response_body_font_family(slint::SharedString(fam));
	apikulture::window_state::persist_response_body_font_family(fam);
}

void AppState::adjust_response_font_size(int delta) {
	auto& g = ui_->global<AppLogic>();
	const float cur = g.get_response_body_font_size();
	const float next = std::clamp(cur + static_cast<float>(delta), 8.f, 24.f);
	if (next == cur) return;
	g.set_response_body_font_size(next);
	apikulture::window_state::persist_response_body_font_size(next);
}

void AppState::commit_response_font_size(float size_px) {
	const float v = std::clamp(size_px, 8.f, 24.f);
	auto& g = ui_->global<AppLogic>();
	g.set_response_body_font_size(v);
	apikulture::window_state::persist_response_body_font_size(v);
}

void AppState::refresh_response_display() {
	if (!last_success_response_body_) {
		return;
	}
	auto& g = ui_->global<AppLogic>();
	std::string path = to_std_string(g.get_response_jsonpath());
	trim_in_place(path);
	std::string plain = format_response_with_jsonpath(*last_success_response_body_, path);
	push_response_body(plain);
}

void AppState::commit_request_name() {
	if (workspace_.collections.empty()) return;
	if (collection_index_ < 0 || collection_index_ >= static_cast<int>(workspace_.collections.size())) return;
	auto& col = workspace_.collections[static_cast<size_t>(collection_index_)];
	if (col.items.empty()) return;
	if (request_index_ < 0 || request_index_ >= static_cast<int>(col.items.size())) return;
	auto& g = ui_->global<AppLogic>();
	std::string name = to_std_string(g.get_request_name_edit());
	trim_in_place(name);
	if (name.empty()) {
		push_name_edits_to_ui();
		return;
	}
	col.items[static_cast<size_t>(request_index_)].name = std::move(name);
	refresh_request_names_model();
	push_selection_to_ui();
}

void AppState::send_request() {
	if (worker_busy_) return;
	commit_form_to_current_item();
	sync_url_field_to_query_table_if_changed();

	auto& g = ui_->global<AppLogic>();
	pending_method_ = to_std_string(g.get_method());
	const std::string raw_url = to_std_string(g.get_url());
	const std::string raw_body = to_std_string(g.get_request_body());

	std::map<std::string, std::string> var_map;
	std::string base_subst;
	if (apikulture::Environment* env = mutable_active_environment()) {
		auto local = apikulture::collections_io::load_local_overrides();
		var_map = apikulture::effective_variable_map(*env, local);
		std::string base_raw = apikulture::effective_base_url_field(*env, local);
		if (base_raw.empty()) {
			if (auto it = var_map.find("base_url"); it != var_map.end()) base_raw = it->second;
		}
		base_subst = apikulture::substitute_variables(base_raw, var_map);
		if (!base_subst.empty()) var_map["base_url"] = base_subst;
	}

	std::string url_after_vars = apikulture::substitute_variables(raw_url, var_map);
	pending_url_ = apikulture::resolve_url_with_base(base_subst, url_after_vars);
	apikulture::normalize_http_url_delimiters_inplace(pending_url_);
	apikulture::RequestItem* req_item = mutable_current_request_item();
	if (req_item) {
		if (apikulture::has_query_params_to_append(req_item->query_params, var_map)) {
			pending_url_ = apikulture::strip_url_query_preserving_fragment(std::move(pending_url_));
		}
		pending_url_ = apikulture::append_query_params_to_url(pending_url_, req_item->query_params, var_map);
	}
	std::ostringstream hdr_lines;
	if (req_item) {
		for (const auto& h : req_item->request_headers) {
			if (!h.enabled) continue;
			std::string hk = apikulture::substitute_variables(h.key, var_map);
			std::string hv = apikulture::substitute_variables(h.value, var_map);
			trim_in_place(hk);
			trim_in_place(hv);
			if (hk.empty()) continue;
			hdr_lines << hk << ": " << hv << "\n";
		}
	}
	pending_headers_ = hdr_lines.str();
	pending_body_ = apikulture::substitute_variables(raw_body, var_map);

	if (pending_url_.empty()) {
		last_success_response_body_ = std::nullopt;
		last_success_content_type_ = std::nullopt;
		g.set_response_status(slint::SharedString(""));
		g.set_response_headers(slint::SharedString(""));
		g.set_request_elapsed(slint::SharedString(""));
		push_response_body("Enter a URL or path");
		return;
	}
	if (!apikulture::is_absolute_http_url(pending_url_)) {
		last_success_response_body_ = std::nullopt;
		last_success_content_type_ = std::nullopt;
		g.set_response_status(slint::SharedString(""));
		g.set_response_headers(slint::SharedString(""));
		g.set_request_elapsed(slint::SharedString(""));
		push_response_body(
				"Relative URL needs a Base URL for this environment (sidebar), or use an absolute https://… URL.");
		return;
	}

	cancelled_ = false;
	last_success_response_body_ = std::nullopt;
	last_success_content_type_ = std::nullopt;
	request_elapsed_start_ = std::chrono::steady_clock::now();
	g.set_request_elapsed(slint::SharedString("0.00 s"));
	g.set_loading(true);
	g.set_response_status(slint::SharedString("Loading..."));
	g.set_response_headers(slint::SharedString(""));
	push_response_body("");

	{
		std::lock_guard<std::mutex> lock(mutex_);
		pending_work_ = true;
	}
	cv_.notify_one();

	if (!worker_.joinable()) {
		worker_ = std::thread(&AppState::worker_run, this);
	}
}

void AppState::cancel_request() {
	cancelled_ = true;
}

void AppState::tick_request_elapsed() {
	auto& g = ui_->global<AppLogic>();
	if (!g.get_loading() || !request_elapsed_start_) return;
	g.set_request_elapsed(slint::SharedString(format_elapsed_since(*request_elapsed_start_)));
}

bool AppState::try_apply_url_import_from_text(const std::string& raw_utf8) {
	auto& g = ui_->global<AppLogic>();
	apikulture::RequestItem* item = mutable_current_request_item();
	if (!item) return false;

	std::map<std::string, std::string> var_map;
	std::string base_subst;
	if (apikulture::Environment* env = mutable_active_environment()) {
		const auto local = apikulture::collections_io::load_local_overrides();
		var_map = apikulture::effective_variable_map(*env, local);
		std::string base_raw = apikulture::effective_base_url_field(*env, local);
		if (base_raw.empty()) {
			if (auto it = var_map.find("base_url"); it != var_map.end()) base_raw = it->second;
		}
		base_subst = apikulture::substitute_variables(base_raw, var_map);
	}

	std::string url_out;
	std::vector<apikulture::QueryParam> params;
	if (!apikulture::url_import::parse_pasted_url_into_request(raw_utf8, base_subst, url_out, params)) {
		g.set_import_status(slint::SharedString("URL: not a usable address or query string."));
		return false;
	}

	item->url = std::move(url_out);
	item->query_params = std::move(params);
	if (item->query_params.empty()) {
		item->query_params.push_back({});
	}
	item->method = "GET";
	g.set_import_status(slint::SharedString(""));
	g.set_method(slint::SharedString("GET"));
	g.set_url(slint::SharedString(item->url));
	refresh_query_param_models();
	(void)apikulture::collections_io::save_workspace(workspace_);
	return true;
}

void AppState::request_url_accepted() {
	auto& g = ui_->global<AppLogic>();
	if (g.get_loading()) {
		cancel_request();
		return;
	}
	commit_form_to_current_item();
	sync_url_field_to_query_table_if_changed();
	send_request();
}

void AppState::copy_response_body() {
	auto& g = ui_->global<AppLogic>();
	const std::string body = to_std_string(g.get_response_body());
	(void)apikulture::clipboard::set_utf8(body);
}

void AppState::worker_run() {
	while (true) {
		std::string method, url, headers_text, body;
		{
			std::unique_lock<std::mutex> lock(mutex_);
			cv_.wait(lock, [this] { return pending_work_ || shutdown_; });
			if (shutdown_) break;
			if (!pending_work_) continue;
			worker_busy_ = true;
			method = pending_method_;
			url = pending_url_;
			headers_text = pending_headers_;
			body = pending_body_;
			pending_work_ = false;
		}

		auto headers = parse_headers(headers_text);
		HttpResponse res = execute(method, url, headers, body, &cancelled_);
		const std::string response_content_type = content_type_value_from_headers(res.headers);

		slint::invoke_from_event_loop([this, res, response_content_type]() {
			auto& g = ui_->global<AppLogic>();
			if (request_elapsed_start_) {
				g.set_request_elapsed(slint::SharedString(format_elapsed_since(*request_elapsed_start_)));
				request_elapsed_start_.reset();
			}
			g.set_loading(false);
			if (apikulture::RequestItem* item = mutable_current_request_item()) {
				item->jsonpath = to_std_string(g.get_response_jsonpath());
				if (!res.error.empty()) {
					item->last_response_body_raw.clear();
					item->last_response_content_type.clear();
					item->last_response_error = res.error;
					item->last_response_status = "Error: " + res.error;
					item->last_response_headers.clear();
				} else {
					item->last_response_error.clear();
					item->last_response_status = res.status_line;
					item->last_response_headers = format_headers(res.headers);
					item->last_response_body_raw = res.body;
					item->last_response_content_type = response_content_type;
				}
						(void)apikulture::collections_io::save_workspace(workspace_);
			}
			if (!res.error.empty()) {
				last_success_response_body_ = std::nullopt;
				last_success_content_type_ = std::nullopt;
				g.set_response_status(slint::SharedString("Error: " + res.error));
				g.set_response_headers(slint::SharedString(""));
				push_response_body(res.error);
			} else {
				last_success_response_body_ = res.body;
				last_success_content_type_ = response_content_type.empty()
						? std::nullopt
						: std::optional<std::string>(response_content_type);
				g.set_response_status(slint::SharedString(res.status_line));
				g.set_response_headers(slint::SharedString(format_headers(res.headers)));
				refresh_response_display();
			}
		});

		worker_busy_ = false;
	}
}

void AppState::environment_changed(const std::string& selected_name) {
	commit_environment_fields_to_active();
	apikulture::Collection* col = mutable_current_collection();
	if (!col) return;
	for (size_t i = 0; i < col->environments.size(); ++i) {
		if (col->environments[i].name == selected_name) {
			col->active_environment_index = static_cast<int>(i);
			apply_environment_fields_to_ui();
			(void)apikulture::collections_io::save_workspace(workspace_);
			return;
		}
	}
}

void AppState::new_environment() {
	commit_environment_fields_to_active();
	apikulture::Collection* col = mutable_current_collection();
	if (!col) return;
	apikulture::Environment e;
	e.name = "Environment " + std::to_string(col->environments.size() + 1);
	for (;;) {
		bool clash = false;
		for (const auto& x : col->environments) {
			if (x.name == e.name) {
				clash = true;
				break;
			}
		}
		if (!clash) break;
		e.name += "_";
	}
	col->environments.push_back(std::move(e));
	col->active_environment_index = static_cast<int>(col->environments.size()) - 1;
	refresh_environment_names_model();
	apply_environment_fields_to_ui();
	(void)apikulture::collections_io::save_workspace(workspace_);
}

void AppState::delete_environment() {
	apikulture::Collection* col = mutable_current_collection();
	if (!col || col->environments.size() <= 1) return;
	commit_environment_fields_to_active();
	col->environments.erase(col->environments.begin() + col->active_environment_index);
	if (col->active_environment_index >= static_cast<int>(col->environments.size())) {
		col->active_environment_index = static_cast<int>(col->environments.size()) - 1;
	}
	refresh_environment_names_model();
	apply_environment_fields_to_ui();
	(void)apikulture::collections_io::save_workspace(workspace_);
}

void AppState::commit_environment_name() {
	if (!mutable_active_environment()) return;
	auto& g = ui_->global<AppLogic>();
	std::string name = to_std_string(g.get_environment_name_edit());
	trim_in_place(name);
	if (name.empty()) {
		apply_environment_fields_to_ui();
		return;
	}
	apikulture::Collection* col = mutable_current_collection();
	if (!col) return;
	for (size_t i = 0; i < col->environments.size(); ++i) {
		if (static_cast<int>(i) != col->active_environment_index
				&& col->environments[i].name == name) {
			apply_environment_fields_to_ui();
			return;
		}
	}
	mutable_active_environment()->name = std::move(name);
	refresh_environment_names_model();
	apply_environment_fields_to_ui();
	(void)apikulture::collections_io::save_workspace(workspace_);
}

void AppState::commit_environment_variables() {
	commit_environment_fields_to_active();
}

void AppState::refresh_query_param_models() {
	std::vector<std::string> keys;
	std::vector<std::string> vals;
	std::vector<bool> enabled;
	if (apikulture::RequestItem* it = mutable_current_request_item()) {
		/// Match stored rows to the visible editor: one blank row is always persisted so "+ param"
		/// adds a real second row (previously the list showed a synthetic row while `query_params` stayed empty).
		if (it->query_params.empty()) {
			it->query_params.push_back({});
		}
		for (const auto& p : it->query_params) {
			keys.push_back(p.key);
			vals.push_back(p.value);
			enabled.push_back(p.enabled);
		}
	}
	if (keys.empty()) {
		keys.push_back("");
		vals.push_back("");
		enabled.push_back(true);
	}
	query_param_keys_model_ = make_name_model(keys);
	query_param_values_model_ = make_name_model(vals);
	query_param_enabled_model_ = std::make_shared<slint::VectorModel<bool>>();
	for (bool e : enabled) {
		query_param_enabled_model_->push_back(e);
	}
	auto& g = ui_->global<AppLogic>();
	g.set_query_param_keys(query_param_keys_model_);
	g.set_query_param_values(query_param_values_model_);
	g.set_query_param_enabled(query_param_enabled_model_);
}

void AppState::query_param_key_edited(int index, slint::SharedString text) {
	auto* item = mutable_current_request_item();
	if (!item || index < 0) return;
	while (static_cast<size_t>(index) >= item->query_params.size()) {
		item->query_params.push_back({});
	}
	item->query_params[static_cast<size_t>(index)].key = to_std_string(text);
	if (query_param_keys_model_) {
		const int rc = query_param_keys_model_->row_count();
		if (index < rc) {
			query_param_keys_model_->set_row_data(index, text);
		}
	}
}

void AppState::query_param_value_edited(int index, slint::SharedString text) {
	auto* item = mutable_current_request_item();
	if (!item || index < 0) return;
	while (static_cast<size_t>(index) >= item->query_params.size()) {
		item->query_params.push_back({});
	}
	item->query_params[static_cast<size_t>(index)].value = to_std_string(text);
	if (query_param_values_model_) {
		const int rc = query_param_values_model_->row_count();
		if (index < rc) {
			query_param_values_model_->set_row_data(index, text);
		}
	}
}

void AppState::query_param_enabled_changed(int index, bool enabled) {
	auto* item = mutable_current_request_item();
	if (!item || index < 0) return;
	while (static_cast<size_t>(index) >= item->query_params.size()) {
		item->query_params.push_back({});
	}
	item->query_params[static_cast<size_t>(index)].enabled = enabled;
	if (query_param_enabled_model_) {
		const int rc = query_param_enabled_model_->row_count();
		if (index < rc) {
			query_param_enabled_model_->set_row_data(index, enabled);
		}
	}
}

void AppState::add_query_param() {
	auto* item = mutable_current_request_item();
	if (!item) return;
	if (item->query_params.empty()) {
		item->query_params.push_back({});
	}
	item->query_params.push_back({});
	refresh_query_param_models();
}

void AppState::remove_query_param(int index) {
	auto* item = mutable_current_request_item();
	if (!item || index < 0) return;
	if (item->query_params.size() <= 1) {
		item->query_params[0] = {};
		refresh_query_param_models();
		return;
	}
	if (static_cast<size_t>(index) < item->query_params.size()) {
		item->query_params.erase(item->query_params.begin() + index);
	}
	if (item->query_params.empty()) item->query_params.push_back({});
	refresh_query_param_models();
}

void AppState::refresh_request_header_models() {
	std::vector<std::string> keys;
	std::vector<std::string> vals;
	std::vector<bool> enabled;
	if (apikulture::RequestItem* it = mutable_current_request_item()) {
		if (it->request_headers.empty()) {
			it->request_headers.push_back({});
		}
		for (const auto& h : it->request_headers) {
			keys.push_back(h.key);
			vals.push_back(h.value);
			enabled.push_back(h.enabled);
		}
	}
	if (keys.empty()) {
		keys.push_back("");
		vals.push_back("");
		enabled.push_back(true);
	}
	request_header_keys_model_ = make_name_model(keys);
	request_header_values_model_ = make_name_model(vals);
	request_header_enabled_model_ = std::make_shared<slint::VectorModel<bool>>();
	for (bool e : enabled) {
		request_header_enabled_model_->push_back(e);
	}
	auto& g = ui_->global<AppLogic>();
	g.set_request_header_keys(request_header_keys_model_);
	g.set_request_header_values(request_header_values_model_);
	g.set_request_header_enabled(request_header_enabled_model_);
}

void AppState::request_header_key_edited(int index, slint::SharedString text) {
	auto* item = mutable_current_request_item();
	if (!item || index < 0) return;
	while (static_cast<size_t>(index) >= item->request_headers.size()) {
		item->request_headers.push_back({});
	}
	item->request_headers[static_cast<size_t>(index)].key = to_std_string(text);
	if (request_header_keys_model_) {
		const int rc = request_header_keys_model_->row_count();
		if (index < rc) {
			request_header_keys_model_->set_row_data(index, text);
		}
	}
}

void AppState::request_header_value_edited(int index, slint::SharedString text) {
	auto* item = mutable_current_request_item();
	if (!item || index < 0) return;
	while (static_cast<size_t>(index) >= item->request_headers.size()) {
		item->request_headers.push_back({});
	}
	item->request_headers[static_cast<size_t>(index)].value = to_std_string(text);
	if (request_header_values_model_) {
		const int rc = request_header_values_model_->row_count();
		if (index < rc) {
			request_header_values_model_->set_row_data(index, text);
		}
	}
}

void AppState::request_header_enabled_changed(int index, bool enabled) {
	auto* item = mutable_current_request_item();
	if (!item || index < 0) return;
	while (static_cast<size_t>(index) >= item->request_headers.size()) {
		item->request_headers.push_back({});
	}
	item->request_headers[static_cast<size_t>(index)].enabled = enabled;
	if (request_header_enabled_model_) {
		const int rc = request_header_enabled_model_->row_count();
		if (index < rc) {
			request_header_enabled_model_->set_row_data(index, enabled);
		}
	}
}

void AppState::add_request_header() {
	auto* item = mutable_current_request_item();
	if (!item) return;
	if (item->request_headers.empty()) {
		item->request_headers.push_back({});
	}
	item->request_headers.push_back({});
	refresh_request_header_models();
}

void AppState::remove_request_header(int index) {
	auto* item = mutable_current_request_item();
	if (!item || index < 0) return;
	if (item->request_headers.size() <= 1) {
		item->request_headers[0] = {};
		refresh_request_header_models();
		return;
	}
	if (static_cast<size_t>(index) < item->request_headers.size()) {
		item->request_headers.erase(item->request_headers.begin() + index);
	}
	if (item->request_headers.empty()) item->request_headers.push_back({});
	refresh_request_header_models();
}
