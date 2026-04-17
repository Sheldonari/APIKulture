#ifndef APIKULTURE_APP_STATE_H
#define APIKULTURE_APP_STATE_H

#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <map>
#include "MainWindow.h"
#include "collections_io.hpp"
#include "openapi_import.hpp"

class AppState {
public:
	using MainWindowHandle = slint::ComponentHandle<MainWindow>;

	explicit AppState(const MainWindowHandle& ui);
	~AppState();

	/// Commit editor fields into the in-memory workspace and write `collections.json`.
	/// Call after the event loop ends while the window API may still reflect last edits.
	void save_collections_state();

	void send_request();
	void cancel_request();

	void select_collection(int index);
	void select_request(int index);
	void new_collection();
	void delete_collection();
	void collection_delete_dialog_cancel();
	void collection_delete_dialog_confirm();
	void new_request();
	void delete_request();
	void duplicate_request();
	void save_collections();
	void import_openapi_dialog();
	void import_openapi_from_path(const std::string& path_utf8);
	void import_openapi_from_url_string(const std::string& url_utf8);
	void import_openapi_from_url_ui();
	void commit_collection_name();
	void commit_request_name();
	void response_jsonpath_changed();
	void apply_response_font_index(int index);
	void adjust_response_font_size(int delta);
	void commit_response_font_size(float size_px);

	void environment_changed(const std::string& selected_name);
	void new_environment();
	void delete_environment();
	void commit_environment_name();
	void commit_environment_variables();
	void commit_request_body();

	void query_param_key_edited(int index, slint::SharedString text);
	void query_param_value_edited(int index, slint::SharedString text);
	void query_param_enabled_changed(int index, bool enabled);
	void add_query_param();
	void remove_query_param(int index);

	void request_header_key_edited(int index, slint::SharedString text);
	void request_header_value_edited(int index, slint::SharedString text);
	void request_header_enabled_changed(int index, bool enabled);
	void add_request_header();
	void remove_request_header(int index);
	/// UI timer callback: updates live elapsed time while `loading` is true.
	void tick_request_elapsed();
	/// Enter in URL field: cancel if loading; else if URL changed since last import, parse `?query` into the table; then send.
	void request_url_accepted();
	/// URL field text changed (live); updates the effective-URL preview (query params, base URL, vars).
	void request_url_edited();
	void copy_response_body();
	void copy_request_url();

	/// Call after MainWindow is created to load data into UI models.
	void init_collections_ui();
	/// Persist method, body format, URL, and body text from the UI into the current request item.
	void persist_request_editor_to_item();

private:
	void worker_run();
	void commit_form_to_current_item();
	void apply_form_from_current_item();
	void refresh_response_display();
	void push_response_body(const std::string& text);
	/// Non-null when collections/request indices refer to a valid item.
	apikulture::RequestItem* mutable_current_request_item();
	apikulture::Collection* mutable_current_collection();
	apikulture::Environment* mutable_active_environment();
	void refresh_collection_names_model();
	void refresh_request_names_model();
	void refresh_environment_names_model();
	void commit_environment_fields_to_active();
	void apply_environment_fields_to_ui();
	void push_name_edits_to_ui();
	void push_selection_to_ui();
	void restore_request_index_for_current_collection();
	void refresh_query_param_models();
	void refresh_request_header_models();
	/// Copy header rows from Slint models into the current request (Send must see latest LineEdit text).
	void sync_request_headers_from_ui_models_into_item();
	void apply_openapi_import_result(apikulture::openapi::ImportResult&& result);
	/// If URL field differs from \ref last_url_field_sync_, parse `?query` into the table (Enter / Send).
	void sync_url_field_to_query_table_if_changed();
	void sync_request_editor_before_http();
	void refresh_resolved_request_url_display();
	/// Parses \a raw (full URL or path with query) into the current item; updates UI. Returns false if unusable.
	bool try_apply_url_import_from_text(const std::string& raw_utf8);
	std::shared_ptr<slint::VectorModel<slint::SharedString>> make_name_model(
			const std::vector<std::string>& names);

	MainWindowHandle ui_;
	std::atomic<bool> cancelled_{false};
	std::atomic<bool> worker_busy_{false};
	std::atomic<bool> shutdown_{false};
	std::thread worker_;
	std::mutex mutex_;
	std::condition_variable cv_;
	bool pending_work_ = false;
	std::string pending_method_;
	std::string pending_url_;
	std::string pending_headers_;
	std::string pending_body_;
	std::string pending_body_kind_{"json"};

	/// Set only after a successful HTTP response; used with JSONPath to build `response-body`.
	std::optional<std::string> last_success_response_body_;
	/// `Content-Type` value for the last successful response (for highlighter when body is JSONPath-filtered).
	std::optional<std::string> last_success_content_type_;

	apikulture::Workspace workspace_;
	int collection_index_{0};
	int request_index_{0};
	std::shared_ptr<slint::VectorModel<slint::SharedString>> query_param_keys_model_;
	std::shared_ptr<slint::VectorModel<slint::SharedString>> query_param_values_model_;
	std::shared_ptr<slint::VectorModel<bool>> query_param_enabled_model_;
	std::shared_ptr<slint::VectorModel<slint::SharedString>> request_header_keys_model_;
	std::shared_ptr<slint::VectorModel<slint::SharedString>> request_header_values_model_;
	std::shared_ptr<slint::VectorModel<bool>> request_header_enabled_model_;
	/// Monotonic start time for the in-flight HTTP request (UI-thread only).
	std::optional<std::chrono::steady_clock::time_point> request_elapsed_start_;
	/// Collection index to remove after Slint delete confirmation; -1 when not applicable.
	int pending_delete_collection_index_{-1};
	/// Last URL field value we already split into path + query table (trimmed); Enter skips re-import when unchanged.
	std::string last_url_field_sync_;
};

#endif  // APIKULTURE_APP_STATE_H
